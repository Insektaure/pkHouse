#include "ui.h"
#include "species_converter.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <string>
#include <cstdio>

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    romfsInit();
#endif

    // Determine base path
    // On Switch, the NRO runs from sdmc:/switch/pkHouse/
    // Save file "main" and bank files are in the same directory
    std::string basePath;
#ifdef __SWITCH__
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
#else
    basePath = "./";
#endif

    std::string savePath = basePath + "main";

    // Load text data
#ifdef __SWITCH__
    SpeciesName::load("romfs:/data/species_en.txt");
    MoveName::load("romfs:/data/moves_en.txt");
    NatureName::load("romfs:/data/natures_en.txt");
    AbilityName::load("romfs:/data/abilities_en.txt");
#else
    SpeciesName::load("romfs/data/species_en.txt");
    MoveName::load("romfs/data/moves_en.txt");
    NatureName::load("romfs/data/natures_en.txt");
    AbilityName::load("romfs/data/abilities_en.txt");
#endif

    // Initialize UI first so we can show errors
    UI ui;
    if (!ui.init()) {
#ifdef __SWITCH__
        romfsExit();
#endif
        return 1;
    }

    // Show splash screen while loading
    ui.showSplash();

    // Check for applet mode on Switch — require title takeover
#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication) {
        ui.showMessageAndWait("Applet Mode Not Supported",
                              "Please run this app in title takeover mode.");
        ui.shutdown();
        romfsExit();
        return 1;
    }
#endif

    // Run main loop — game selection, bank selection, and save loading all handled inside
    ui.run(basePath, savePath);

    // Cleanup
    ui.shutdown();

#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}
