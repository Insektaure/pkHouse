#include "ui.h"
#include "led.h"
#include "species_converter.h"

#include <switch.h>
#include <string>
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
