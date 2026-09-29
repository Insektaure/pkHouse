// First-launch tour: four pages shown once per SD card - what pkHouse is,
// the two launch modes, how saves are kept safe, the main keys - and again on
// demand with A in the About screen.
//
// Pages go through showPages, a small blocking pager meant to serve the
// "What's new after an update" screen too.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "gts.h"   // Gts::CONFIG_DIR, where the other one-off settings live

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

namespace {

// Bump to show the tour once more to everyone who saw an older one.
constexpr const char* TOUR_MARK = "tour_v1.cfg";

constexpr int TP_W = 960, TP_H = 560, TP_PAD = 44;

bool pathExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

std::string configFile(const char* name) { return std::string(Gts::CONFIG_DIR) + "/" + name; }

std::vector<std::string> paragraphs(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) nl = s.size();
        out.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

// A line of a page: text, with keys written as tokens - "[A]", "[+]", "[-]",
// "[L R]" - that are drawn as the footer's key caps. Unclosed brackets stay text.
struct Segment { std::string text; bool key; };
std::vector<Segment> segments(const std::string& line) {
    std::vector<Segment> out;
    size_t i = 0;
    while (i < line.size()) {
        const size_t open = line.find('[', i);
        const size_t close = open == std::string::npos ? std::string::npos : line.find(']', open);
        if (close == std::string::npos) { out.push_back({line.substr(i), false}); break; }
        if (open > i) out.push_back({line.substr(i, open - i), false});
        out.push_back({line.substr(open + 1, close - open - 1), true});
        i = close + 1;
    }
    return out;
}

constexpr int KEY_GAP = 6;   // between a key cap and the text beside it

// The spaces next to a key are the gap's job: the text's own are dropped, so
// "hold [R] while" and "[A]." read evenly.
void tidyAroundKeys(std::vector<Segment>& sg) {
    for (size_t i = 0; i < sg.size(); i++) {
        if (sg[i].key) continue;
        std::string& t = sg[i].text;
        if (i > 0 && sg[i - 1].key)
            while (!t.empty() && t.front() == ' ') t.erase(t.begin());
        if (i + 1 < sg.size() && sg[i + 1].key)
            while (!t.empty() && t.back() == ' ') t.pop_back();
    }
}

// The gap before segment i: none at the start, less before punctuation.
int gapBefore(const std::vector<Segment>& sg, size_t i) {
    if (i == 0 || (!sg[i].key && !sg[i - 1].key)) return 0;
    if (!sg[i].key) {
        // Latin and full-width punctuation (UTF-8, so compared as strings).
        static const char* const PUNCT[] = {".", ",", ":", ";", "!", "?", ")",
                                            "\xe3\x80\x82", "\xe3\x80\x81",       // 。 、
                                            "\xef\xbc\x8c", "\xef\xbc\x9a",       // ， ：
                                            "\xef\xbc\x81", "\xef\xbc\x9f"};      // ！ ？
        for (const char* p : PUNCT)
            if (sg[i].text.rfind(p, 0) == 0) return 2;
    }
    return KEY_GAP;
}

} // anonymous namespace

// Not seen yet on this SD card: new installs, and players coming from a
// version without the tour, who see it once.
bool UI::tourNotSeen() const {
    return !pathExists(configFile(TOUR_MARK));
}

void UI::markTourSeen() {
    mkdir("sdmc:/config", 0755);
    mkdir(Gts::CONFIG_DIR, 0755);
    if (FILE* f = std::fopen(configFile(TOUR_MARK).c_str(), "wb")) std::fclose(f);
}

// --- The pager ---------------------------------------------------------------------

void UI::drawInfoPage(const InfoPage& page, int index, int count) {
    drawDialogBackdrop();

    const int x = (SCREEN_W - TP_W) / 2, y = (SCREEN_H - TP_H) / 2;
    fillRounded(x, y, TP_W, TP_H, 22, T().panelBg);
    strokeRounded(x, y, TP_W, TP_H, 22, 1, T().panelBorder);
    const int cx = x + TP_W / 2, inW = TP_W - 2 * TP_PAD;

    // Icon, title, body - centred.
    drawIconDisc(page.icon.c_str(), cx, y + 76, 32, page.color);   // icon 48 px: the file, 1:1
    TTF_Font* fTitle = uiFont(30, true);
    drawTextCentered(fitText(page.title, fTitle, inW), cx, y + 152, T().text, fTitle);

    // A line's width with its keys drawn as caps, and the line itself.
    auto split = [](const std::string& line) {
        std::vector<Segment> sg = segments(line);
        tidyAroundKeys(sg);
        return sg;
    };
    auto lineWidth = [&](const std::string& line, TTF_Font* f) {
        const std::vector<Segment> sg = split(line);
        int w = 0;
        for (size_t i = 0; i < sg.size(); i++)
            w += gapBefore(sg, i) + (sg[i].key ? drawFooterKey(0, -100, sg[i].text.c_str(), true)
                                               : textWidth(sg[i].text, f));
        return w;
    };
    auto drawLine = [&](const std::string& line, int lcy, TTF_Font* f) {
        const std::vector<Segment> sg = split(line);
        int lx = cx - lineWidth(line, f) / 2;
        for (size_t i = 0; i < sg.size(); i++) {
            lx += gapBefore(sg, i);
            if (sg[i].key) {
                lx += drawFooterKey(lx, lcy, sg[i].text.c_str(), false);
            } else {
                drawText(sg[i].text, lx, lcy - TTF_FontHeight(f) / 2, T().textDim, f);
                lx += textWidth(sg[i].text, f);
            }
        }
    };

    // The body is written one short sentence per line. Each stays whole: the
    // page takes the largest size at which every line fits. Only a line too
    // long even at the smallest is wrapped, and only if it holds no key, since
    // a key cap cannot be split.
    const std::vector<std::string> lines = paragraphs(page.body);
    int size = 20;
    for (; size > 16; size--) {
        TTF_Font* f = uiFont(size);
        bool fits = true;
        for (const std::string& l : lines)
            if (lineWidth(l, f) > inW) { fits = false; break; }
        if (fits) break;
    }
    TTF_Font* fBody = uiFont(size);
    const int pitch = size + 12, gap = 14;   // line, and the blank line between groups
    std::vector<std::pair<std::string, bool>> rows;   // text, or a gap when empty
    for (const std::string& l : lines) {
        if (l.empty()) { rows.push_back({std::string(), true}); continue; }
        if (l.find('[') != std::string::npos || lineWidth(l, fBody) <= inW) { rows.push_back({l, false}); continue; }
        for (const std::string& w : wrapText(l, fBody, inW, 2)) rows.push_back({w, false});
    }
    int blockH = 0;
    for (const auto& r : rows) blockH += r.second ? gap : pitch;
    const int top = y + 186, bottom = y + TP_H - 110;   // title above, page dots below
    int ly = top + std::max(0, (bottom - top - blockH) / 2);
    for (const auto& r : rows) {
        if (r.second) { ly += gap; continue; }
        drawLine(r.first, ly + pitch / 2, fBody);
        ly += pitch;
    }

    // Page dots.
    {
        constexpr int R = 5, GAP = 18;
        const int dy = y + TP_H - 88;
        int dx = cx - (count - 1) * GAP / 2;
        for (int i = 0; i < count; i++, dx += GAP)
            fillDisc(dx, dy, R, i == index ? T().accent : T().buttonBorder);
    }

    // Keys: B skip on the left, A next (or done) on the right, the arrows between.
    {
        const int fy = y + TP_H - 58;
        drawRect(x, fy, TP_W, 1, T().panelBorder);
        TTF_Font* f = uiFont(17, true);
        const int cy = fy + 29;
        const bool last = index == count - 1;

        const std::string skip = i18n::get(StrKey::TourSkip);
        int kx = x + TP_PAD;
        kx += drawFooterKey(kx, cy, "B", false) + 8;
        drawText(skip, kx, cy - TTF_FontHeight(f) / 2, last ? T().textMuted : T().text, f);

        const std::string next = i18n::get(last ? StrKey::TourDone : StrKey::TourNext);
        const int keyW = drawFooterKey(0, -100, "A", true);
        const int nx = x + TP_W - TP_PAD - (keyW + 8 + textWidth(next, f));
        drawFooterKey(nx, cy, "A", false);
        drawText(next, nx + keyW + 8, cy - TTF_FontHeight(f) / 2, T().accent, f);

        if (count > 1) {
            const int arrowsW = drawFooterKey(0, -100, "L R", true);
            drawFooterKey(cx - arrowsW / 2, cy, "L R", false);
        }
    }
}

void UI::showPages(const std::vector<InfoPage>& pages) {
    if (!renderer_ || pages.empty()) return;
    markDirty();
    const int count = static_cast<int>(pages.size());
    int index = 0;
    updateStick(0, 0);

    bool open = true, redraw = true;
    auto step = [&](int d) {
        const int next = std::clamp(index + d, 0, count - 1);
        if (next != index) { index = next; redraw = true; }
    };
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
                case SDL_CONTROLLER_BUTTON_B:             // Switch A = next, or done
                    if (index == count - 1) open = false;
                    else step(+1);
                    break;
                case SDL_CONTROLLER_BUTTON_A:             // Switch B = skip
                    open = false;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: step(+1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  step(-1); break;
            }
        }
        // The stick turns pages once per tilt: no repeat, a page is read.
        if (open && stickDirX_ != 0 && !stickMoved_) {
            step(stickDirX_);
            stickMoved_ = true;
        }
        if (redraw && open) {
            drawInfoPage(pages[index], index, count);
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
    updateStick(0, 0);
    markDirty();
}

// --- The tour ----------------------------------------------------------------------

void UI::showTour() {
    const std::vector<InfoPage> pages = {
        {"house", T().accent,
         i18n::get(StrKey::TourWelcomeTitle), i18n::get(StrKey::TourWelcomeBody)},
        {"gamepad", T().accentBank,
         i18n::get(StrKey::TourModesTitle),
         i18n::get(StrKey::TourModesBody) + "\n\n"
             + i18n::get(appletMode_ ? StrKey::TourModeApplet : StrKey::TourModeTitle)},
        {"rollback", T().statusOk,
         i18n::get(StrKey::TourSafeTitle), i18n::get(StrKey::TourSafeBody)},
        {"bank", T().accent,
         i18n::get(StrKey::TourKeysTitle), i18n::get(StrKey::TourKeysBody)},
    };
    showPages(pages);
    markTourSeen();
}
