#include "ui.h"
#include "led.h"
#include "species_converter.h"
#include "i18n.h"

#include <switch.h>
#include <string>
#include <fstream>
#include <cstdio>

int main(int argc, char* argv[]) {
    romfsInit();

    // Determine base path
    // On Switch, the NRO runs from sdmc:/switch/pkHouse/
    // Save file "main" and bank files are in the same directory
    std::string basePath;
    // Get the path of the NRO itself
    if (argc > 0 && argv[0]) {
        basePath = argv[0];
        auto lastSlash = basePath.rfind('/');
        if (lastSlash != std::string::npos)
            basePath = basePath.substr(0, lastSlash + 1);
        else
            basePath = "sdmc:/switch/pkHouse/";
    } else {
        basePath = "sdmc:/switch/pkHouse/";
    }
    ledInitWithPath(basePath.c_str());

    std::string savePath = basePath + "main";

    // Detect language: check override file first, then system setting
    {
        std::string lang = "en";
        std::string overridePath = basePath + "language.txt";
        std::ifstream ifs(overridePath);
        if (ifs.good()) {
            std::string line;
            if (std::getline(ifs, line) && !line.empty())
                lang = line;
        } else {
            setInitialize();
            u64 langCode;
            setGetSystemLanguage(&langCode);
            SetLanguage sysLang;
            setMakeLanguage(langCode, &sysLang);
            switch (sysLang) {
                case SetLanguage_JA:    lang = "ja"; break;
                case SetLanguage_FR:
                case SetLanguage_FRCA:  lang = "fr"; break;
                case SetLanguage_DE:    lang = "de"; break;
                case SetLanguage_ES:
                case SetLanguage_ES419: lang = "es"; break;
                case SetLanguage_IT:    lang = "it"; break;
                case SetLanguage_NL:    lang = "nl"; break;
                case SetLanguage_PT:
                case SetLanguage_PTBR:  lang = "pt"; break;
                case SetLanguage_RU:    lang = "ru"; break;
                // Korean and Chinese: translation files exist but are disabled
                // because PlSharedFontType_Standard lacks CJK/Korean glyphs.
                // case SetLanguage_KO:    lang = "ko"; break;
                // case SetLanguage_ZHCN:
                // case SetLanguage_ZHHANS:lang = "zh-Hans"; break;
                // case SetLanguage_ZHTW:
                // case SetLanguage_ZHHANT:lang = "zh-Hant"; break;
                default:                lang = "en"; break;
            }
            setExit();
        }
        i18n::init(lang);
    }

    // Load text data
    SpeciesName::load("romfs:/data/species_en.txt");
    MoveName::load("romfs:/data/moves_en.txt");
    NatureName::load("romfs:/data/natures_en.txt");
    AbilityName::load("romfs:/data/abilities_en.txt");
    ItemName::load("romfs:/data/items_en.txt");

    // Initialize UI first so we can show errors
    UI ui;
    if (!ui.init()) {
        romfsExit();
        return 1;
    }

    // Show splash screen while loading
    ui.showSplash();

    // Detect applet mode on Switch — bank-only access without save data
    {
        AppletType at = appletGetAppletType();
        if (at != AppletType_Application && at != AppletType_SystemApplication)
            ui.setAppletMode(true);
    }

    // Run main loop — game selection, bank selection, and save loading all handled inside
    ui.run(basePath, savePath);

    // Cleanup
    ui.shutdown();
    ledExit();

    romfsExit();
    return 0;
}
