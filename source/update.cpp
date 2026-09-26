// Self-update. See include/update.h for the shape of it; nx-plaza's
// source/net/update.cpp is where the careful parts come from.

#define JSON_NOEXCEPTION
#include "json.hpp"

#include "update.h"
#include "gts.h"

#include <switch.h>
#include <curl/curl.h>
#include <minizip/unzip.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <strings.h>
#include <sys/stat.h>
#include <vector>

// The repository whose releases are pkHouse's. Overridable from the Makefile.
#ifndef PKHOUSE_REPO
#define PKHOUSE_REPO "Insektaure/pkHouse"
#endif

using nlohmann::json;

namespace {

const char* const API_URL = "https://api.github.com/repos/" PKHOUSE_REPO "/releases/latest";

// Where the zip keeps the app, and so the part of each entry name that maps
// to the running app's own folder.
const char* const ZIP_APP_DIR = "switch/pkHouse/";

// Staged in the config folder rather than beside the running NRO: a half
// finished download has no business showing up in the homebrew menu.
std::string configFile(const char* name) { return std::string(Gts::CONFIG_DIR) + "/" + name; }
const char* const ZIP_PART    = "update.zip.part";
const char* const NRO_PART    = "update.nro.part";
const char* const NRO_BACKUP  = "update.nro.backup";

// A release is a few megabytes. These guard against a redirect to something
// enormous or a zip that expands without end, not real limits.
constexpr curl_off_t MAX_DOWNLOAD   = 128ll * 1024 * 1024;
constexpr uint64_t   MAX_ENTRY      = 128ull * 1024 * 1024;
constexpr size_t     MAX_API_BODY   = 1024 * 1024;

std::mutex        g_mutex;
Update::State     g_state = Update::State::Idle;
std::string       g_version;
std::string       g_assetUrl;
std::atomic<bool> g_busy{false};
Thread            g_thread{};
bool              g_threadLive = false;

void setState(Update::State s) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = s;
}

bool endsWithNoCase(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// --- versions ------------------------------------------------------------------

std::vector<int> versionParts(const std::string& raw) {
    std::vector<int> out;
    size_t i = 0;
    if (i < raw.size() && (raw[i] == 'v' || raw[i] == 'V')) i++;   // "v1.2.3"
    int value = 0;
    bool any = false;
    for (; i <= raw.size(); i++) {
        if (i < raw.size() && raw[i] >= '0' && raw[i] <= '9') {
            if (value < 100000) value = value * 10 + (raw[i] - '0');   // saturates
            any = true;
            continue;
        }
        if (any) out.push_back(value);
        value = 0;
        any = false;
        // Anything but a dot ends the version: "1.2.3-beta4" is 1.2.3.
        if (i < raw.size() && raw[i] != '.') break;
    }
    return out;
}

// --- http ----------------------------------------------------------------------

size_t appendBody(char* data, size_t size, size_t nmemb, void* userdata) {
    std::string* out = static_cast<std::string*>(userdata);
    const size_t total = size * nmemb;
    if (out->size() + total > MAX_API_BODY) return 0;
    out->append(data, total);
    return total;
}

size_t writeFile(char* data, size_t size, size_t nmemb, void* userdata) {
    return fwrite(data, size, nmemb, static_cast<FILE*>(userdata)) * size;
}

struct DownloadProgress { const Update::Progress* cb; uint64_t lastTick; };

int onTransfer(void* userdata, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    auto* p = static_cast<DownloadProgress*>(userdata);
    if (total > MAX_DOWNLOAD || now > MAX_DOWNLOAD) return 1;   // aborts the transfer
    // A redraw costs a frame; a few a second is plenty for a bar.
    const uint64_t tick = armGetSystemTick();
    if (p->cb && *p->cb && armTicksToNs(tick - p->lastTick) > 100000000ull) {
        p->lastTick = tick;
        (*p->cb)(Update::StepDownload, total > 0 ? static_cast<float>(double(now) / double(total)) : -1.0f);
    }
    return 0;
}

void commonOptions(CURL* curl, const char* url, long timeoutMs) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "pkHouse/" APP_VERSION);   // GitHub requires one
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);   // a release file is a redirect
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 20000L);
    if (timeoutMs > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
    // Verification stays on: what comes down here replaces the app.
    const std::string& ca = Gts::caBundle();
    if (!ca.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}

// --- the launch check ----------------------------------------------------------

void checkNow() {
    CURL* curl = curl_easy_init();
    if (!curl) { setState(Update::State::Failed); return; }

    std::string body;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    // Twenty seconds: a cold handshake, with the CA bundle parsed first.
    commonOptions(curl, API_URL, 20000L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || status != 200) { setState(Update::State::Failed); return; }

    // Checked before every read: this build compiles with JSON_NOEXCEPTION,
    // where a failed conversion is an abort, not an exception.
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) { setState(Update::State::Failed); return; }
    auto tagIt = root.find("tag_name");
    if (tagIt == root.end() || !tagIt->is_string()) { setState(Update::State::Failed); return; }
    std::string tag = tagIt->get<std::string>();
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) tag.erase(0, 1);

    // The zip: releases carry the NRO and its folders in one. A name that
    // says pkHouse beats one that does not, should a release ever carry more.
    std::string url;
    int best = 0;
    auto assets = root.find("assets");
    if (assets != root.end() && assets->is_array()) {
        for (const auto& a : *assets) {
            if (!a.is_object()) continue;
            auto n = a.find("name");
            auto u = a.find("browser_download_url");
            if (n == a.end() || u == a.end() || !n->is_string() || !u->is_string()) continue;
            const std::string name = n->get<std::string>();
            if (!endsWithNoCase(name, ".zip")) continue;
            std::string lower = name;
            for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            const int score = lower.find("pkhouse") != std::string::npos ? 2 : 1;
            if (score > best) { best = score; url = u->get<std::string>(); }
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_version = tag;
        g_assetUrl = url;
    }
    if (Update::compareVersions(tag, APP_VERSION) <= 0 || url.empty())
        setState(Update::State::UpToDate);   // nothing newer, or nothing to install
    else
        setState(Update::State::Available);
}

void threadEntry(void*) {
    checkNow();
    g_busy.store(false);
}

// --- files ---------------------------------------------------------------------

// mkdir -p for everything above `path`'s last slash, past the "sdmc:/" prefix.
bool ensureParentDir(const std::string& path) {
    const size_t end = path.find_last_of('/');
    if (end == std::string::npos) return true;
    size_t at = path.find(":/");
    at = at == std::string::npos ? 0 : at + 2;
    for (size_t slash = path.find('/', at); slash != std::string::npos && slash <= end;
         slash = path.find('/', slash + 1)) {
        const std::string dir = path.substr(0, slash);
        if (!dir.empty() && mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) return false;
    }
    return true;
}

// Only ever writes files that are ours and open nowhere else (the backup, a
// staged file). The running NRO goes through overwriteFile() instead.
bool copyFile(const std::string& from, const std::string& to) {
    FILE* in = fopen(from.c_str(), "rb");
    if (!in) return false;
    remove(to.c_str());
    FILE* out = fopen(to.c_str(), "wb");
    if (!out) { fclose(in); return false; }
    std::vector<char> buf(64 * 1024);
    bool ok = true;
    for (;;) {
        const size_t got = fread(buf.data(), 1, buf.size(), in);
        if (got == 0) { ok = feof(in) != 0; break; }
        if (fwrite(buf.data(), 1, got, out) != got) { ok = false; break; }
    }
    if (fflush(out) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    fclose(in);
    if (!ok) remove(to.c_str());
    return ok;
}

// Writes `from` over `to` in place. fopen("wb") truncates on open, and this
// filesystem answers EIO to truncating the NRO that is running; creating the
// file (harmless when it is there), opening it for writing and setting the
// length explicitly never asks for that. Committed before returning, because
// this file has to survive the relaunch.
bool overwriteFile(const std::string& from, const std::string& to, std::string& why) {
    FILE* in = fopen(from.c_str(), "rb");
    if (!in) { why = "the new version could not be read"; return false; }
    fseek(in, 0, SEEK_END);
    const long total = ftell(in);
    rewind(in);
    if (total <= 0) { fclose(in); why = "the new version is empty"; return false; }

    FsFileSystem* sd = fsdevGetDeviceFileSystem("sdmc:");
    if (!sd) { fclose(in); why = "the SD card could not be opened"; return false; }
    std::string path = to;
    const size_t colon = path.find(':');
    if (colon != std::string::npos) path.erase(0, colon + 1);   // the native API wants "/switch/..."

    fsFsCreateFile(sd, path.c_str(), total, 0);   // fails when it exists, which is fine
    FsFile file{};
    Result rc = fsFsOpenFile(sd, path.c_str(), FsOpenMode_Write, &file);
    if (R_FAILED(rc)) { fclose(in); why = "the app could not be opened for writing"; return false; }
    // Shrinks as well as grows, so a smaller build leaves no tail behind.
    rc = fsFileSetSize(&file, total);
    if (R_FAILED(rc)) { fsFileClose(&file); fclose(in); why = "the app's size could not be set"; return false; }

    std::vector<char> buf(64 * 1024);
    s64 offset = 0;
    bool ok = true;
    while (offset < total) {
        const size_t got = fread(buf.data(), 1, buf.size(), in);
        if (got == 0) { ok = false; why = "the new version could not be read"; break; }
        rc = fsFileWrite(&file, offset, buf.data(), got, FsWriteOption_None);
        if (R_FAILED(rc)) { ok = false; why = "the app could not be written"; break; }
        offset += static_cast<s64>(got);
    }
    fsFileClose(&file);
    fclose(in);
    if (ok && R_FAILED(fsFsCommit(sd))) { ok = false; why = "the SD card did not commit the write"; }
    return ok;
}

// An entry name that can only land inside the app's folder: relative, no
// drive, no backslashes, no "." or ".." anywhere in it.
bool safeRelative(const std::string& rel) {
    if (rel.empty() || rel[0] == '/') return false;
    if (rel.find('\\') != std::string::npos || rel.find(':') != std::string::npos) return false;
    size_t start = 0;
    for (;;) {
        const size_t end = rel.find('/', start);
        const std::string part = rel.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "." || part == "..") return false;
        if (part.empty() && end != std::string::npos) return false;   // "a//b"
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

// Writes the zip's current entry to `to`, stopping at MAX_ENTRY whatever the
// header claims: a zip bomb is stopped by what comes out, not by what it said.
bool extractCurrent(unzFile zip, const std::string& to) {
    if (unzOpenCurrentFile(zip) != UNZ_OK) return false;
    FILE* out = fopen(to.c_str(), "wb");
    if (!out) { unzCloseCurrentFile(zip); return false; }
    std::vector<char> buf(64 * 1024);
    bool ok = true;
    uint64_t written = 0;
    for (;;) {
        const int got = unzReadCurrentFile(zip, buf.data(), static_cast<unsigned>(buf.size()));
        if (got < 0) { ok = false; break; }
        if (got == 0) break;
        written += static_cast<uint64_t>(got);
        if (written > MAX_ENTRY || fwrite(buf.data(), 1, static_cast<size_t>(got), out) != static_cast<size_t>(got)) {
            ok = false;
            break;
        }
    }
    if (fflush(out) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    // The CRC is only checked when the entry was read to its end.
    if (unzCloseCurrentFile(zip) != UNZ_OK) ok = false;
    if (!ok) remove(to.c_str());
    return ok;
}

// The zip's entries, each with the name relative to the app's folder, or
// empty when it is outside it (and so not ours to write).
struct Entry { std::string rel; bool dir; };

} // anonymous namespace

namespace Update {

bool autoCheckEnabled() {
    const std::string on  = configFile("autoUPD_on.cfg");
    const std::string off = configFile("autoUPD_off.cfg");
    if (fileExists(off)) return false;
    if (fileExists(on)) return true;
    // Neither: on, and say so where it can be switched, by renaming the file.
    mkdir("sdmc:/config", 0755);
    mkdir(Gts::CONFIG_DIR, 0755);
    if (FILE* f = fopen(on.c_str(), "wb")) fclose(f);
    return true;
}

bool setAutoCheck(bool on) {
    const std::string onPath  = configFile("autoUPD_on.cfg");
    const std::string offPath = configFile("autoUPD_off.cfg");
    const std::string& want = on ? onPath : offPath;
    const std::string& other = on ? offPath : onPath;
    mkdir("sdmc:/config", 0755);
    mkdir(Gts::CONFIG_DIR, 0755);
    if (fileExists(want)) {
        remove(other.c_str());   // both there: the one asked for stays
    } else if (fileExists(other)) {
        if (rename(other.c_str(), want.c_str()) != 0) return false;
    } else {
        FILE* f = fopen(want.c_str(), "wb");
        if (!f) return false;
        fclose(f);
    }
    return fileExists(want) && !fileExists(other);
}

void beginCheck(const std::string& basePath) {
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true)) return;   // one at a time
    if (!Gts::startNetwork(basePath)) {
        g_busy.store(false);
        setState(State::Failed);
        return;
    }
    if (g_threadLive) {
        threadWaitForExit(&g_thread);
        threadClose(&g_thread);
        g_threadLive = false;
    }
    setState(State::Checking);
    // A low priority, off the main core: the check must never cost a frame.
    // 256 KB, for mbedTLS's handshake.
    Result rc = threadCreate(&g_thread, threadEntry, nullptr, nullptr, 256 * 1024, 0x3B, -2);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&g_thread);
        if (R_FAILED(rc)) threadClose(&g_thread);
    }
    if (R_FAILED(rc)) {
        g_busy.store(false);
        setState(State::Failed);
        return;
    }
    g_threadLive = true;
}

void shutdown() {
    if (g_threadLive) {
        threadWaitForExit(&g_thread);
        threadClose(&g_thread);
        g_threadLive = false;
    }
}

State state() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::string latestVersion() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_version;
}

int compareVersions(const std::string& a, const std::string& b) {
    const std::vector<int> l = versionParts(a), r = versionParts(b);
    const size_t n = std::max(l.size(), r.size());
    for (size_t i = 0; i < n; i++) {
        const int x = i < l.size() ? l[i] : 0, y = i < r.size() ? r[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

std::string nroDisplayVersion(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    auto done = [&f]() { fclose(f); return std::string(); };

    // The header sits after the 0x10-byte start stub; the asset section (icon,
    // NACP, RomFS) is appended after the code image.
    NroHeader header{};
    if (fseek(f, sizeof(NroStart), SEEK_SET) != 0 || fread(&header, 1, sizeof(header), f) != sizeof(header)
        || header.magic != NROHEADER_MAGIC)
        return done();
    NroAssetHeader assets{};
    if (fseek(f, static_cast<long>(header.size), SEEK_SET) != 0
        || fread(&assets, 1, sizeof(assets), f) != sizeof(assets)
        || assets.magic != NROASSETHEADER_MAGIC || assets.nacp.size < sizeof(NacpStruct))
        return done();
    NacpStruct nacp{};
    if (fseek(f, static_cast<long>(header.size + assets.nacp.offset), SEEK_SET) != 0
        || fread(&nacp, 1, sizeof(nacp), f) != sizeof(nacp))
        return done();
    fclose(f);
    // A fixed-size field that need not be terminated.
    char version[sizeof(nacp.display_version) + 1] = {};
    memcpy(version, nacp.display_version, sizeof(nacp.display_version));
    return std::string(version);
}

bool install(const std::string& exePath, const std::string& appDir,
             const Progress& progress, std::string& error) {
    std::string url, wanted;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        url = g_assetUrl;
        wanted = g_version;
    }
    if (url.empty() || wanted.empty()) { error = "no release to install"; return false; }
    if (exePath.empty() || !endsWithNoCase(exePath, ".nro")) {
        error = "pkHouse does not know where its own NRO is, so it cannot replace it";
        return false;
    }

    const std::string zipPath = configFile(ZIP_PART);
    const std::string nroPart = configFile(NRO_PART);
    const std::string backup  = configFile(NRO_BACKUP);
    remove(zipPath.c_str());
    remove(nroPart.c_str());
    remove(backup.c_str());

    // 1. The zip, to a staging file. Nothing that is in use is ever the
    //    target of a transfer that might stop half way.
    {
        FILE* out = fopen(zipPath.c_str(), "wb");
        if (!out) { error = "the download could not be written to the SD card"; return false; }
        CURL* curl = curl_easy_init();
        if (!curl) { fclose(out); remove(zipPath.c_str()); error = "the network client did not start"; return false; }
        DownloadProgress dp{&progress, armGetSystemTick()};
        commonOptions(curl, url.c_str(), 0L);
        // Give up on a transfer that has crawled below 1 KB/s for a minute,
        // rather than capping the whole download at a time a slow network misses.
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onTransfer);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &dp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        const CURLcode rc = curl_easy_perform(curl);
        long status = 0;
        if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(curl);
        const bool closed = fclose(out) == 0;
        if (rc != CURLE_OK || !closed || status < 200 || status >= 300) {
            remove(zipPath.c_str());
            error = rc != CURLE_OK ? std::string("the download did not finish: ") + curl_easy_strerror(rc)
                  : !closed        ? std::string("the download could not be written to the SD card")
                                   : "the server answered " + std::to_string(status);
            return false;
        }
    }

    // 2. What is in it. Only entries under switch/pkHouse/ are ours; each is
    //    mapped onto the running app's folder, and anything that could step
    //    outside it is refused rather than written.
    if (progress) progress(StepUnpack, -1.0f);
    unzFile zip = unzOpen(zipPath.c_str());
    if (!zip) { remove(zipPath.c_str()); error = "the download is not a readable zip"; return false; }
    auto closeZip = [&]() { unzClose(zip); remove(zipPath.c_str()); };

    const size_t prefixLen = strlen(ZIP_APP_DIR);
    std::vector<Entry> entries;
    int nroIndex = -1;
    for (int step = unzGoToFirstFile(zip), i = 0; step == UNZ_OK; step = unzGoToNextFile(zip), i++) {
        unz_file_info64 info{};
        char name[512] = {};
        if (unzGetCurrentFileInfo64(zip, &info, name, sizeof(name) - 1, nullptr, 0, nullptr, 0) != UNZ_OK) {
            closeZip();
            error = "the zip could not be read";
            return false;
        }
        std::string n(name);
        Entry e{std::string(), !n.empty() && n.back() == '/'};
        if (n.compare(0, prefixLen, ZIP_APP_DIR) == 0) {
            std::string rel = n.substr(prefixLen);
            if (e.dir && !rel.empty()) rel.pop_back();
            if (!rel.empty() && safeRelative(rel)) e.rel = rel;
        }
        // The app itself: an .nro directly in the app folder.
        if (!e.dir && !e.rel.empty() && e.rel.find('/') == std::string::npos && endsWithNoCase(e.rel, ".nro"))
            nroIndex = i;
        entries.push_back(e);
    }
    if (nroIndex < 0) { closeZip(); error = "the zip holds no pkHouse build"; return false; }

    // 3. The build first, to a staging file, checked before anything else is
    //    touched: a wrong or broken download changes nothing at all.
    {
        unzGoToFirstFile(zip);
        for (int i = 0; i < nroIndex; i++) unzGoToNextFile(zip);
        if (!extractCurrent(zip, nroPart)) { closeZip(); error = "the build in the zip could not be unpacked"; return false; }
        const std::string v = nroDisplayVersion(nroPart);
        if (v.empty() || compareVersions(v, wanted) != 0) {
            remove(nroPart.c_str());
            closeZip();
            error = v.empty() ? "the build in the zip is not a valid NRO"
                              : "the build in the zip is version " + v + ", not " + wanted;
            return false;
        }
    }

    // 4. Everything else in the app folder (the bundled wondercards, and
    //    whatever later releases add). Each file through a temporary one, so a
    //    failed write never leaves a truncated file in place of a good one.
    //    Nothing the zip does not carry is touched, banks and backups included.
    {
        const int total = static_cast<int>(entries.size());
        unzGoToFirstFile(zip);
        for (int i = 0; i < total; i++, unzGoToNextFile(zip)) {
            const Entry& e = entries[i];
            if (e.rel.empty() || i == nroIndex) continue;
            const std::string target = appDir + e.rel;
            if (e.dir) {
                ensureParentDir(target + "/");
                continue;
            }
            const std::string tmp = target + ".upd";
            if (!ensureParentDir(target) || !extractCurrent(zip, tmp)) {
                remove(nroPart.c_str());
                closeZip();
                error = "could not write " + e.rel;
                return false;
            }
            // FAT's rename does not replace, so the old file goes first.
            remove(target.c_str());
            if (rename(tmp.c_str(), target.c_str()) != 0) {
                remove(tmp.c_str());
                remove(nroPart.c_str());
                closeZip();
                error = "could not write " + e.rel;
                return false;
            }
            if (progress && (i % 16) == 0) progress(StepUnpack, static_cast<float>(i) / static_cast<float>(total));
        }
    }
    closeZip();

    // 5. The build over the running one, last. The caller shows the working
    //    dialog before this and draws nothing during it: RomFS is released
    //    here, since it is mounted out of this very file and would otherwise
    //    lock it against the write (0xE02), and is put back however this ends.
    if (progress) progress(StepInstall, -1.0f);
    struct RomfsRelease {
        RomfsRelease() { romfsExit(); }
        ~RomfsRelease() { romfsInit(); }
    } releaseRomfs;

    if (!copyFile(exePath, backup)) {
        remove(nroPart.c_str());
        error = "the current version could not be backed up";
        return false;
    }
    auto restore = [&]() {
        std::string why;
        if (overwriteFile(backup, exePath, why)) remove(backup.c_str());
        remove(nroPart.c_str());
    };
    std::string why;
    if (!overwriteFile(nroPart, exePath, why)) {
        restore();
        error = "the update could not be written over the current version (" + why + ")";
        return false;
    }
    // Read back off the card: a write that went wrong is still recoverable
    // here, and not one step later.
    if (compareVersions(nroDisplayVersion(exePath), wanted) != 0) {
        restore();
        error = "the installed update did not verify, so it was rolled back";
        return false;
    }
    remove(backup.c_str());
    remove(nroPart.c_str());
    return true;
}

void restartInto(const std::string& exePath) {
    // The loader starts this file when the app exits, instead of the menu.
    envSetNextLoad(exePath.c_str(), exePath.c_str());
}

} // namespace Update
