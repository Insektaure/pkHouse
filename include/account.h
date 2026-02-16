#pragma once
#include "game_type.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdint>

#ifdef __SWITCH__
#include <switch.h>
#endif

struct UserProfile {
    std::string nickname;
    std::string pathSafeName;
#ifdef __SWITCH__
    AccountUid uid{};
#endif
    SDL_Texture* iconTexture = nullptr;
};

class AccountManager {
public:
    bool init();
    void shutdown();

    // Load all Switch profiles (names + icons). Returns false on PC or if no profiles.
    bool loadProfiles(SDL_Renderer* renderer);

    // Check if a profile has save data for a given game.
    bool hasSaveData(int profileIndex, GameType game) const;

    // Mount save filesystem for profile + game. Returns "save:/" on success, "" on failure.
    std::string mountSave(int profileIndex, GameType game);
    void unmountSave();

    // Commit writes to mounted save (required after save_.save() on Switch).
    void commitSave();

    // Copy all files from a save directory to a backup directory.
    static bool backupSaveDir(const std::string& srcDir, const std::string& dstDir);

    // Recursively calculate total size of all files in a directory.
    static size_t calculateDirSize(const std::string& dir);

    const std::vector<UserProfile>& profiles() const { return users_; }
    int profileCount() const { return (int)users_.size(); }

    void freeTextures();

private:
    std::vector<UserProfile> users_;
    bool mounted_ = false;

#ifdef __SWITCH__
    FsFileSystem saveFs_{};

    bool loadProfile(AccountUid uid, SDL_Renderer* renderer, UserProfile& out);
    static std::string sanitizeForPath(const char* nickname);
#endif
};
