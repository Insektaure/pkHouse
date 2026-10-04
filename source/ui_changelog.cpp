// Changelog: every release's notes, on one scrolling page.
//
// Opened with + in the About screen, and once by itself on the first launch
// after an update ("What's new"), at the top, where the version just installed
// is. A fresh install gets the tour instead: there is nothing new to someone
// who has never seen the app.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "gts.h"   // Gts::CONFIG_DIR, where the other one-off settings live

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

namespace {

// The version that last ran, so the next launch knows it has been updated.
constexpr const char* LAST_VERSION_FILE = "last_version.cfg";

constexpr int CL_X = 32, CL_Y = 84, CL_W = 1216, CL_H = 568;
constexpr int CL_PAD = 30;
constexpr int CL_INDENT = 24;   // per bullet level

enum class Kind { Version, Section, Text, Bullet, More, Gap };

struct Line {
    Kind kind;
    std::string text;
    int indent = 0;          // bullet level, 1 for a top-level bullet
    bool current = false;    // the heading of the version running now
};

int lineHeight(Kind k) {
    switch (k) {
        case Kind::Version: return 50;
        case Kind::Section: return 34;
        case Kind::Gap:     return 10;
        default:            return 24;
    }
}

std::string configFile(const char* name) { return std::string(Gts::CONFIG_DIR) + "/" + name; }

bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

std::string firstLine(const std::string& path) {
    std::string s;
    if (!readFile(path, s)) return std::string();
    const size_t nl = s.find_first_of("\r\n");
    if (nl != std::string::npos) s.erase(nl);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

} // anonymous namespace

// --- After an update -----------------------------------------------------------------

void UI::checkWhatsNew() {
    const std::string path = configFile(LAST_VERSION_FILE);
    const std::string last = firstLine(path);
    if (last == APP_VERSION) return;

    // Someone who has run pkHouse before: a version on record, or - for an
    // update from a version older than this record - the tour already seen.
    // Nobody else has anything to call new.
    whatsNewPending_ = !last.empty() || !tourPending_;

    mkdir("sdmc:/config", 0755);
    mkdir(Gts::CONFIG_DIR, 0755);
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fputs(APP_VERSION, f);
        std::fclose(f);
    }
}

// --- The page ------------------------------------------------------------------------

void UI::showChangelog(bool afterUpdate) {
    if (!renderer_) return;

    TTF_Font* fVersion = uiFont(22, true);
    TTF_Font* fSection = uiFont(16, true);
    TTF_Font* fText    = uiFont(15);
    const int width = CL_W - 2 * CL_PAD - 14;   // room for the scrollbar

    // Parse and wrap once; the page only scrolls through the result.
    std::vector<Line> lines;
    std::string text;
    if (!readFile("romfs:/changelog.txt", text)) {
        lines.push_back({Kind::Text, i18n::get(StrKey::ChangelogMissing)});
    } else {
        const std::string currentHeading = std::string("Version ") + APP_VERSION;
        bool lastGap = true;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t nl = text.find('\n', pos);
            if (nl == std::string::npos) nl = text.size();
            std::string line = text.substr(pos, nl - pos);
            pos = nl + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            size_t lead = 0;
            while (lead < line.size() && line[lead] == ' ') lead++;
            const std::string body = line.substr(lead);

            if (body.empty()) {
                if (!lastGap) lines.push_back({Kind::Gap, ""});
                lastGap = true;
                continue;
            }
            lastGap = false;

            if (body[0] == '#') {
                size_t h = 0;
                while (h < body.size() && body[h] == '#') h++;
                std::string title = body.substr(h);
                while (!title.empty() && title[0] == ' ') title.erase(0, 1);
                if (h <= 2) {
                    // A version starts a block: a gap above it, unless first.
                    if (!lines.empty() && lines.back().kind != Kind::Gap)
                        lines.push_back({Kind::Gap, ""});
                    const bool current = title.compare(0, currentHeading.size(), currentHeading) == 0
                        && (title.size() == currentHeading.size() || title[currentHeading.size()] == ' ');
                    lines.push_back({Kind::Version, title, 0, current});
                } else {
                    for (const auto& l : wrapText(title, fSection, width, 8))
                        lines.push_back({Kind::Section, l});
                }
            } else if (body.compare(0, 2, "- ") == 0 || body.compare(0, 2, "* ") == 0) {
                const int indent = static_cast<int>(lead / 4) + 1;
                const auto wrapped = wrapText(body.substr(2), fText, width - indent * CL_INDENT, 50);
                for (size_t i = 0; i < wrapped.size(); i++)
                    lines.push_back({i == 0 ? Kind::Bullet : Kind::More, wrapped[i], indent});
            } else {
                const int indent = static_cast<int>(lead / 4);
                for (const auto& l : wrapText(body, fText, width - indent * CL_INDENT, 50))
                    lines.push_back({Kind::Text, l, indent});
            }
        }
    }

    // How far down it can go: the last lines filling the card, no further.
    const int n = static_cast<int>(lines.size());
    int maxScroll = 0;
    {
        int used = 0;
        for (int i = n - 1; i >= 0; i--) {
            used += lineHeight(lines[i].kind);
            if (used > CL_H - 2 * 18) { maxScroll = i + 1; break; }
        }
    }
    // A page for L / R: about as many lines as the card shows.
    constexpr int PAGE = 20;

    int scroll = 0;
    auto draw = [&]() {
        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);
        drawFlowTopBar(i18n::get(StrKey::ChangelogTitle), {}, 0);

        // After an update, say so on the right of the top bar.
        if (afterUpdate) {
            TTF_Font* f = uiFont(14, true);
            const std::string msg = i18n::fmt(StrKey::ChangelogUpdated, APP_VERSION);
            const int w = textWidth(msg, f) + 28;
            const int bx = SCREEN_W - 32 - w;
            fillRounded(bx, 24, w, 32, 10, T().badgeBg);
            fillDisc(bx + 14, 40, 4, T().accent);
            drawText(msg, bx + 24, 40 - TTF_FontHeight(f) / 2, T().accent, f);
        }

        fillRounded(CL_X, CL_Y, CL_W, CL_H, 16, T().panelBg);
        strokeRounded(CL_X, CL_Y, CL_W, CL_H, 16, 1, T().panelBorder);
        SDL_Rect clip = {CL_X + 1, CL_Y + 12, CL_W - 2, CL_H - 24};
        SDL_RenderSetClipRect(renderer_, &clip);

        int y = CL_Y + 18;
        for (int i = scroll; i < n && y < CL_Y + CL_H; i++) {
            const Line& l = lines[i];
            const int x = CL_X + CL_PAD + l.indent * CL_INDENT;
            switch (l.kind) {
                case Kind::Version: {
                    const int ty = y + 10;
                    drawText(l.text, CL_X + CL_PAD, ty, l.current ? T().accent : T().text, fVersion);
                    drawRect(CL_X + CL_PAD, y + lineHeight(l.kind) - 6, CL_W - 2 * CL_PAD - 14, 1, T().divider);
                    break;
                }
                case Kind::Section:
                    drawText(l.text, CL_X + CL_PAD, y + 8, T().accentBank, fSection);
                    break;
                case Kind::Bullet:
                    fillDisc(x - 12, y + TTF_FontHeight(fText) / 2, 3,
                             l.indent > 1 ? T().textMuted : T().accent);
                    drawText(l.text, x, y, T().text, fText);
                    break;
                case Kind::More:
                    drawText(l.text, x, y, T().text, fText);
                    break;
                case Kind::Text:
                    drawText(l.text, x, y, T().textDim, fText);
                    break;
                case Kind::Gap:
                    break;
            }
            y += lineHeight(l.kind);
        }
        SDL_RenderSetClipRect(renderer_, nullptr);

        // Scrollbar.
        if (maxScroll > 0) {
            const int trackY = CL_Y + 18, trackH = CL_H - 36;
            const int thumbH = std::max(30, trackH * 20 / std::max(20, n));
            const int thumbY = trackY + (trackH - thumbH) * scroll / maxScroll;
            fillRounded(CL_X + CL_W - 16, trackY, 4, trackH, 2, T().buttonBg);
            fillRounded(CL_X + CL_W - 16, thumbY, 4, thumbH, 2, T().textMuted);
        }

        const ButtonHint hints[] = {
            {HINT_DPAD, StrKey::HintScroll},
            {"L R", StrKey::HintPage2},
            {"B", StrKey::HintClose},
        };
        drawFooterBar(hints, 3);
        SDL_RenderPresent(renderer_);
    };

    markDirty();
    updateStick(0, 0);
    bool open = true, redraw = true;
    auto step = [&](int d) {
        const int next = std::clamp(scroll + d, 0, maxScroll);
        if (next != scroll) { scroll = next; redraw = true; }
    };
    uint32_t repeatAt = 0;
    while (open) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { open = false; break; }
            if (event.type == SDL_CONTROLLERAXISMOTION &&
                (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                 event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
                updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                            SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
                continue;
            }
            if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:       step(-1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     step(+1); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  step(-PAGE); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: step(+PAGE); break;
                case SDL_CONTROLLER_BUTTON_A:       // Switch B = close
                case SDL_CONTROLLER_BUTTON_B:       // Switch A, as on the tour's last page
                case SDL_CONTROLLER_BUTTON_START:   // + closes it too, as it opened it
                    open = false;
                    break;
            }
        }
        // Held D-pad or stick: keep scrolling, a line at a time. The D-pad's
        // first line came with its press above; the stick's comes here.
        if (open) {
            const bool padUp = SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_UP);
            const bool padDown = SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            const bool up = padUp || stickDirY_ < 0;
            const bool down = padDown || stickDirY_ > 0;
            const uint32_t now = SDL_GetTicks();
            if (up || down) {
                if (repeatAt == 0) {
                    if (!padUp && !padDown) step(up ? -1 : +1);
                    repeatAt = now + STICK_INITIAL_DELAY;
                } else if (now >= repeatAt) {
                    step(up ? -1 : +1);
                    repeatAt = now + 60;
                }
            } else {
                repeatAt = 0;
            }
        }
        if (redraw && open) {
            draw();
            redraw = false;
        }
        SDL_Delay(16);
    }
    updateStick(0, 0);
    markDirty();
}
