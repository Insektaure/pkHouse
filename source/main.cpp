#include "ui.h"
#include "save_file.h"
#include "bank.h"
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
    // Save file "main" and bank file are in the same directory
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
    std::string bankPath = basePath + "bank.bin";

    // Load species names
#ifdef __SWITCH__
    SpeciesName::load("romfs:/data/species_en.txt");
#else
    SpeciesName::load("romfs/data/species_en.txt");
#endif

    // Initialize UI first so we can show errors
    UI ui;
    if (!ui.init()) {
#ifdef __SWITCH__
        romfsExit();
#endif
        return 1;
    }

    // Load save file
    SaveFile save;
    if (!save.load(savePath)) {
        // Could not load save - still start with empty state for testing
        // In production, show error and exit
    }

    // Load or create bank
    Bank bank;
    bank.load(bankPath);

    // Run main loop - returns true if user chose save & exit
    bool shouldSave = ui.run(save, bank);

    // Save changes only if user chose to
    if (shouldSave) {
        if (save.isLoaded())
            save.save(savePath);
        bank.save(bankPath);
    }

    // Cleanup
    ui.shutdown();

#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}
