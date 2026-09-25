// Transport for the GTS board. See include/gts.h for what the calls mean.
//
// One curl easy handle for the life of the app, reset between requests rather
// than recreated: a fresh handle per request costs a TCP connect, a full TLS
// handshake and a re-read of the CA bundle off the SD card every time, and the
// console is slow at all three. Resetting keeps the connection, the TLS
// session and the DNS cache, so the second page of a browse is noticeably
// quicker than the first.

#define JSON_NOEXCEPTION
#include "json.hpp"

#include "gts.h"
#include "card_payload.h"

#include <switch.h>
#include <curl/curl.h>

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <fstream>
#include <string>

using nlohmann::json;

namespace {

bool        g_ready      = false;
bool        g_socketsUp  = false;
CURL*       g_handle     = nullptr;
std::string g_baseUrl;
std::string g_caBundle;
std::string g_error;
std::string g_trainerId;   // 32 hex characters
std::string g_token;       // 64 hex characters
bool        g_saidHello   = false;

constexpr size_t MAX_BODY = 256 * 1024;
constexpr long   TIMEOUT_MS = 12000;


// --- reading JSON that came off a network ------------------------------------
//
// nlohmann's value() fetches the key and then converts it, and a conversion it
// cannot make is a type_error. This build compiles with JSON_NOEXCEPTION, which
// turns a thrown type_error into abort() -- so a server answering
// {"species": "abc"} would not produce a wrong listing, it would take the
// console down. Everything below checks the type before it reads, and falls
// back rather than failing.

int jsonInt(const json& j, const char* key, int fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer())
        return fallback;
    return it->get<int>();
}

bool jsonBool(const json& j, const char* key, bool fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_boolean())
        return fallback;
    return it->get<bool>();
}

std::string jsonStr(const json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string())
        return std::string();
    return it->get<std::string>();
}

// --- small helpers ------------------------------------------------------------

std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string readFirstLine(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.good()) return "";
    std::string line;
    std::getline(ifs, line);
    return trimmed(line);
}

bool fileExists(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    return ifs.good();
}

std::string randomHex(int bytes) {
    std::string out;
    out.reserve(static_cast<size_t>(bytes) * 2);
    for (int i = 0; i < bytes; i++) {
        uint8_t b = 0;
        // The console's own CSPRNG. A token that anybody can guess is a token
        // that lets anybody withdraw somebody else's deposits.
        randomGet(&b, 1);
        static const char* HEX = "0123456789abcdef";
        out.push_back(HEX[b >> 4]);
        out.push_back(HEX[b & 0x0F]);
    }
    return out;
}

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        uint32_t n = (static_cast<uint32_t>(data[i]) << 16)
                   | (static_cast<uint32_t>(data[i + 1]) << 8)
                   | data[i + 2];
        out.push_back(B64[(n >> 18) & 0x3F]);
        out.push_back(B64[(n >> 12) & 0x3F]);
        out.push_back(B64[(n >> 6) & 0x3F]);
        out.push_back(B64[n & 0x3F]);
    }
    if (i < len) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        bool two = (i + 1 < len);
        if (two) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        out.push_back(B64[(n >> 18) & 0x3F]);
        out.push_back(B64[(n >> 12) & 0x3F]);
        out.push_back(two ? B64[(n >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

// Strict: anything outside the alphabet fails the whole string rather than
// being skipped. A payload with junk spliced through it would otherwise decode
// to something shorter that still passed every check downstream.
bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    out.clear();
    if (in.empty() || in.size() % 4 != 0) return false;

    for (size_t i = 0; i < in.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int k = 0; k < 4; k++) {
            char c = in[i + k];
            if (c == '=') {
                // Padding is only ever the last one or two characters.
                if (i + 4 != in.size() || k < 2) return false;
                v[k] = 0;
                pad++;
            } else {
                v[k] = value(c);
                if (v[k] < 0 || pad != 0) return false;
            }
        }
        uint32_t n = (static_cast<uint32_t>(v[0]) << 18) | (static_cast<uint32_t>(v[1]) << 12)
                   | (static_cast<uint32_t>(v[2]) << 6)  | static_cast<uint32_t>(v[3]);
        out.push_back(static_cast<uint8_t>((n >> 16) & 0xFF));
        if (pad < 2) out.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
        if (pad < 1) out.push_back(static_cast<uint8_t>(n & 0xFF));
    }
    return true;
}

// --- identity -----------------------------------------------------------------

// Trust on first use: this console makes an id and a token once and keeps
// them. The board stores the id and an HMAC of the token, never the token, and
// that pair is the whole of the identity - enough to attribute a deposit and
// let whoever made it withdraw it again, and nothing more.
// Reads an identity file into g_trainerId / g_token. False when it is missing
// or does not hold one.
bool readIdentity(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.good())
        return false;

    std::string id, token;
    std::getline(ifs, id);
    std::getline(ifs, token);
    id = trimmed(id);
    token = trimmed(token);

    if (id.size() != 32 || token.size() != 64)
        return false;   // half-written or hand-edited; not an identity

    g_trainerId = id;
    g_token = token;
    return true;
}

bool writeIdentity(const std::string& path) {
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.good())
        return false;
    ofs << g_trainerId << "\n" << g_token << "\n";
    return ofs.good();
}

void ensureConfigDir() {
    // Both levels, because sdmc:/config may not exist on a fresh SD card.
    // Failures are ignored: the directory already being there is the usual
    // reason, and everything below reports its own problem if it is not.
    mkdir("sdmc:/config", 0755);
    mkdir(Gts::CONFIG_DIR, 0755);
}

std::string configPath(const char* name) {
    return std::string(Gts::CONFIG_DIR) + "/" + name;
}

void loadOrCreateIdentity(const std::string& basePath) {
    const std::string path = configPath("gts_id.txt");
    if (readIdentity(path))
        return;

    // An identity from before this moved, beside the NRO. Brought across rather
    // than replaced: the board attributes every deposit to the id that made it,
    // so generating a fresh one here would quietly orphan everything this
    // console has ever put on the board.
    const std::string legacy = basePath + "gts_id.txt";
    if (readIdentity(legacy)) {
        if (writeIdentity(path)) {
            // Only once the new copy is on disk and reads back, so a failed
            // write cannot lose the identity altogether.
            if (readIdentity(path))
                std::remove(legacy.c_str());
        }
        return;
    }

    g_trainerId = randomHex(16);
    g_token     = randomHex(32);
    writeIdentity(path);
}

// --- http ---------------------------------------------------------------------

size_t writeCallback(char* data, size_t size, size_t nmemb, void* userdata) {
    std::string* out = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    // A hostile or broken server must not be able to make us allocate forever.
    if (out->size() + total > MAX_BODY)
        return 0;
    out->append(data, total);
    return total;
}

struct Response {
    long        status = 0;
    std::string body;
    bool        transportFailed = false;
};

Response postJson(const std::string& route, const std::string& body) {
    Response r;

    CURL* curl = g_handle;
    bool disposable = false;
    if (curl) {
        curl_easy_reset(curl);
    } else {
        curl = curl_easy_init();
        disposable = true;
    }
    if (!curl) {
        r.transportFailed = true;
        return r;
    }

    const std::string url = g_baseUrl + route;

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    const std::string idHeader = "X-PkHouse-Id: " + g_trainerId;
    const std::string authHeader = "Authorization: Bearer " + g_token;
    headers = curl_slist_append(headers, idHeader.c_str());
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TIMEOUT_MS);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, TIMEOUT_MS);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "pkHouse/" APP_VERSION);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    if (g_baseUrl.compare(0, 8, "https://") == 0) {
        // The console has no trust store to borrow, so pkHouse carries its own
        // (see init). Verification stays ON either way: if the bundle is
        // somehow unreadable the handshake fails loudly, which is the right
        // outcome - a board carries bearer tokens and Pokemon data, and
        // silently not checking who we are talking to would be worse than not
        // connecting at all.
        if (!g_caBundle.empty() && fileExists(g_caBundle))
            curl_easy_setopt(curl, CURLOPT_CAINFO, g_caBundle.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef CURLOPT_CA_CACHE_TIMEOUT
        curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 24L * 60L * 60L);
#endif
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
    } else {
        r.transportFailed = true;
        g_error = curl_easy_strerror(rc);
        if (rc == CURLE_SSL_CACERT || rc == CURLE_PEER_FAILED_VERIFICATION) {
            // pkHouse ships the Let's Encrypt roots and trusts nothing else, so
            // this is nearly always a board whose certificate comes from
            // somewhere else - another CA, or a proxy in front of it.
            g_error += " (this board's certificate is not from Let's Encrypt;"
                       " put a CA bundle at cacert.pem next to the NRO)";
        }
    }

    curl_slist_free_all(headers);
    if (disposable)
        curl_easy_cleanup(curl);
    return r;
}

// Parses a reply and checks it says ok. Sets g_error and returns false
// otherwise, so every caller can just `if (!accept(...)) return false;`.
bool accept(const Response& r, json& out) {
    if (r.transportFailed)
        return false;   // g_error already set by postJson

    out = json::parse(r.body, nullptr, false);
    if (out.is_discarded() || !out.is_object()) {
        g_error = "the board sent something that is not a reply";
        return false;
    }

    if (jsonBool(out, "ok", false))
        return true;

    // The server's own message, which is written to be shown to a person.
    auto it = out.find("error");
    if (it != out.end() && it->is_string() && !it->get<std::string>().empty())
        g_error = it->get<std::string>();
    else
        g_error = "the board refused the request (" + std::to_string(r.status) + ")";
    return false;
}

// One listing row out of JSON. Unknown or out-of-range values are clamped
// rather than trusted: this is data from a server, and a form index of 60000
// would walk straight off the end of a sprite lookup.
Gts::Entry parseEntry(const json& j) {
    Gts::Entry e;

    e.id       = jsonStr(j, "id");
    e.family   = jsonStr(j, "family");
    e.nickname = jsonStr(j, "nickname");
    e.otName   = jsonStr(j, "ot_name");
    e.note     = jsonStr(j, "note");
    e.legality = jsonStr(j, "legality");
    if (e.legality.empty()) e.legality = "pending";
    e.legalityReport = jsonStr(j, "legality_report");

    int game = jsonInt(j, "game", -1);
    if (game >= 0 && game < GAME_TYPE_COUNT) {
        e.game = static_cast<GameType>(game);
        e.gameKnown = true;
    }

    auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

    e.species  = static_cast<uint16_t>(clamp(jsonInt(j, "species", 0), 0, 1100));
    e.form     = static_cast<uint8_t>(clamp(jsonInt(j, "form", 0), 0, 255));
    e.level    = static_cast<uint8_t>(clamp(jsonInt(j, "level", 0), 0, 100));
    e.shiny    = jsonBool(j, "shiny", false);
    e.alpha    = jsonBool(j, "alpha", false);
    e.gender   = static_cast<uint8_t>(clamp(jsonInt(j, "gender", 3), 0, 3));
    e.nature   = static_cast<uint8_t>(clamp(jsonInt(j, "nature", 0), 0, 24));
    e.ball     = static_cast<uint8_t>(clamp(jsonInt(j, "ball", 0), 0, 37));
    e.ability  = static_cast<uint16_t>(clamp(jsonInt(j, "ability", 0), 0, 65535));
    e.heldItem = static_cast<uint16_t>(clamp(jsonInt(j, "held_item", 0), 0, 65535));
    e.language = static_cast<uint8_t>(clamp(jsonInt(j, "language", 0), 0, 255));
    e.egg      = jsonBool(j, "egg", false);
    e.ivTotal  = clamp(jsonInt(j, "iv_total", 0), 0, 186);
    e.evTotal  = clamp(jsonInt(j, "ev_total", 0), 0, 510);
    e.downloads = clamp(jsonInt(j, "downloads", 0), 0, 1 << 30);
    e.created  = static_cast<uint32_t>(jsonInt(j, "created", 0));

    return e;
}

} // anonymous namespace

// --- lifecycle ----------------------------------------------------------------

bool Gts::init(const std::string& basePath) {
    if (g_ready) return true;

    g_error.clear();

    ensureConfigDir();

    // The trust store.
    //
    // romfs:/cacert.pem ships inside the NRO and holds the two Let's Encrypt
    // roots and nothing else - 3 KB against the 184 KB of a full bundle, which
    // matters because mbedTLS parses the whole file on every handshake and the
    // console is slow at it. See tools/gen_ca_bundle.py.
    //
    // A file on the SD card overrides it, so a board behind a different CA, or
    // a root that has rotated since the build, is a copied file rather than a
    // new release. config/ is where that belongs; beside the NRO is still read
    // so that an install predating the move keeps working.
    g_caBundle = configPath("cacert.pem");
    if (!fileExists(g_caBundle))
        g_caBundle = basePath + "cacert.pem";
    if (!fileExists(g_caBundle))
        g_caBundle = "romfs:/cacert.pem";

    // Same order, same reason.
    g_baseUrl = readFirstLine(configPath("gts.txt"));
    if (g_baseUrl.empty())
        g_baseUrl = readFirstLine(basePath + "gts.txt");
    if (g_baseUrl.empty())
        g_baseUrl = DEFAULT_URL;
    while (!g_baseUrl.empty() && g_baseUrl.back() == '/')
        g_baseUrl.pop_back();

    loadOrCreateIdentity(basePath);

    Result rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        g_error = "could not start networking";
        return false;
    }
    g_socketsUp = true;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        g_error = "could not start the network client";
        socketExit();
        g_socketsUp = false;
        return false;
    }

    // A shared handle is an optimisation, not a requirement: without one every
    // request opens its own, which still works.
    g_handle = curl_easy_init();
    g_ready = true;
    return true;
}

void Gts::shutdown() {
    if (g_handle) {
        curl_easy_cleanup(g_handle);
        g_handle = nullptr;
    }
    if (g_ready) {
        curl_global_cleanup();
        g_ready = false;
    }
    if (g_socketsUp) {
        socketExit();
        g_socketsUp = false;
    }
    g_saidHello = false;
}

bool Gts::configured() { return !g_baseUrl.empty(); }
const std::string& Gts::baseUrl() { return g_baseUrl; }
const std::string& Gts::lastError() { return g_error; }

// --- calls --------------------------------------------------------------------

bool Gts::hello() {
    g_error.clear();
    if (!g_ready)    { g_error = "networking is not available"; return false; }
    if (!configured()) { g_error = "no board address configured"; return false; }

    json out;
    if (!accept(postJson("/v1/hello", "{}"), out))
        return false;

    g_saidHello = true;
    return true;
}

bool Gts::search(const Filter& filter, int offset, Page& out) {
    g_error.clear();
    if (!g_ready)      { g_error = "networking is not available"; return false; }
    if (!configured()) { g_error = "no board address configured"; return false; }
    if (!g_saidHello && !hello())
        return false;

    json req;
    req["limit"]  = PAGE_SIZE;
    req["offset"] = offset < 0 ? 0 : offset;
    req["legality"] = filter.onlyLegal ? "legal" : "any";
    req["sort"]     = filter.byPopularity ? "popular" : "recent";

    // Only what was actually asked for: a filter sent as "any" and a filter
    // left out mean the same thing to the board, and leaving it out keeps the
    // request readable in a log.
    if (!filter.family.empty()) req["family"]  = filter.family;
    if (filter.species != 0)    req["species"] = filter.species;
    if (filter.shiny  >= 0)     req["shiny"]   = (filter.shiny == 1);
    if (filter.egg    >= 0)     req["egg"]     = (filter.egg == 1);
    if (filter.ball   >= 0)     req["ball"]    = filter.ball;
    if (filter.minIvs >= 0)     req["min_ivs"] = filter.minIvs;

    json reply;
    if (!accept(postJson("/v1/search", req.dump()), reply))
        return false;

    out.entries.clear();
    out.offset = jsonInt(reply, "offset", offset);
    out.more   = jsonBool(reply, "more", false);

    auto results = reply.find("results");
    if (results == reply.end() || !results->is_array()) {
        g_error = "the board sent a listing with no results";
        return false;
    }
    out.entries.reserve(results->size());
    for (const auto& row : *results) {
        if (!row.is_object()) continue;
        Entry e = parseEntry(row);
        if (e.id.size() == 32)
            out.entries.push_back(std::move(e));
    }
    return true;
}

bool Gts::fetch(const std::string& id, Pokemon& out, GameType& game) {
    g_error.clear();
    if (!g_ready)      { g_error = "networking is not available"; return false; }
    if (!configured()) { g_error = "no board address configured"; return false; }
    if (id.size() != 32) { g_error = "that listing has no id"; return false; }
    if (!g_saidHello && !hello())
        return false;

    json req;
    req["id"] = id;

    json reply;
    if (!accept(postJson("/v1/fetch", req.dump()), reply))
        return false;

    // Which game the board says this is. Used as the target below, so a
    // payload that disagrees with its own listing is rejected rather than
    // reinterpreted through the wrong offsets.
    auto deposit = reply.find("deposit");
    if (deposit == reply.end() || !deposit->is_object()) {
        g_error = "the board sent a download with no listing";
        return false;
    }
    int gameByte = jsonInt(*deposit, "game", -1);
    if (gameByte < 0 || gameByte >= GAME_TYPE_COUNT) {
        g_error = "that Pokemon is from a game this pkHouse does not know";
        return false;
    }
    game = static_cast<GameType>(gameByte);

    std::vector<uint8_t> payload;
    if (!base64Decode(jsonStr(reply, "payload"), payload)
        || payload.size() > static_cast<size_t>(CardPayload::MAX_SIZE)) {
        g_error = "the board sent a damaged download";
        return false;
    }

    // Exactly the checks a card gets on the way in from the SD card. A board
    // is not more trusted than a PNG somebody handed you.
    CardPayload::Parsed parsed =
        CardPayload::parse(payload.data(), payload.size(), game);
    if (parsed.result != CardPayload::Result::Ok) {
        g_error = "the board sent a download that failed its checks";
        return false;
    }

    out = parsed.pkm;
    return true;
}

bool Gts::markDownloaded(const std::string& id) {
    // No g_error, and no hello() if we have not said it: this is bookkeeping
    // that happens after the useful work, and it must never turn a card that
    // saved fine into an error on screen.
    if (!g_ready || !configured() || !g_saidHello || id.size() != 32)
        return false;

    json req;
    req["id"] = id;

    json reply;
    const std::string saved = g_error;
    const bool ok = accept(postJson("/v1/downloaded", req.dump()), reply);
    g_error = saved;   // leave lastError() reporting whatever last mattered
    return ok;
}

bool Gts::deposit(const std::vector<uint8_t>& payload, const std::string& note,
                  std::string& outId, bool& outDuplicate) {
    g_error.clear();
    outId.clear();
    outDuplicate = false;

    if (!g_ready)      { g_error = "networking is not available"; return false; }
    if (!configured()) { g_error = "no board address configured"; return false; }
    if (payload.empty() || payload.size() > static_cast<size_t>(CardPayload::MAX_SIZE)) {
        g_error = "that card has nothing to upload";
        return false;
    }
    if (!g_saidHello && !hello())
        return false;

    json req;
    req["payload"] = base64Encode(payload.data(), payload.size());
    if (!note.empty())
        req["note"] = note;

    json reply;
    if (!accept(postJson("/v1/deposit", req.dump()), reply))
        return false;

    outId        = jsonStr(reply, "id");
    outDuplicate = jsonBool(reply, "duplicate", false);
    return true;
}
