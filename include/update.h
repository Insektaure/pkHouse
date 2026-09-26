#pragma once
#include <functional>
#include <string>

// Self-update from the project's GitHub releases.
//
// Modelled on nx-plaza's updater, and built around the same rule: never
// destroy the only working copy. The release zip is downloaded to a staging
// file; the NRO inside it is extracted to a staging file of its own and its
// version checked against the release before anything is written; the running
// NRO is copied to a backup before it is replaced, and the replacement is read
// back and checked again before the backup is deleted. A console that loses
// power half way still has an app to boot.
//
// The launch check runs on a worker thread, so a slow or absent network never
// holds the screen up. Installing blocks behind the working dialog, like every
// other long operation in pkHouse, and is only ever offered on the profile and
// game selectors, where nothing is open that could have unsaved changes.
namespace Update {

enum class State {
    Idle,       // no check started (auto-check off, or not yet)
    Checking,   // asking GitHub
    UpToDate,   // this build is the newest release
    Available,  // a newer release exists and has a zip to install
    Failed,     // no network, or GitHub did not answer; said nowhere
};

// sdmc:/config/pkHouse/autoUPD_on.cfg or autoUPD_off.cfg: an empty file whose
// name is the setting. Off wins when both are there. With neither, the check
// is on and autoUPD_on.cfg is created, so the setting is there to rename.
bool autoCheckEnabled();

// Switches the setting by renaming the file (autoUPD_on.cfg <-> autoUPD_off.cfg),
// creating it when neither is there and removing the other when both are.
// False when the SD card would not take the change.
bool setAutoCheck(bool on);

// Starts the launch check on a worker thread. Brings the network up first
// (Gts::startNetwork). Does nothing when a check is already running.
void beginCheck(const std::string& basePath);

// Waits for the worker. Call before the network is torn down.
void shutdown();

State state();
std::string latestVersion();   // the release tag, without a leading "v"

// Downloads the release the check found and installs it:
//   - every file under switch/pkHouse/ in the zip is written to the matching
//     place in `appDir` (the running NRO's folder), each through a temporary
//     file, never deleting anything the zip does not replace;
//   - the NRO itself replaces `exePath` last, with a backup and a read-back.
// `progress` is called with a step and a 0..1 fraction (-1 when unknown).
// On failure `error` says why, in English, and the running build is intact.
using Progress = std::function<void(int step, float fraction)>;
enum Step { StepDownload = 0, StepUnpack = 1, StepInstall = 2 };
bool install(const std::string& exePath, const std::string& appDir,
             const Progress& progress, std::string& error);

// Points the homebrew loader at `exePath`, so leaving the app starts the new
// build instead of returning to the menu. The caller still has to exit.
void restartInto(const std::string& exePath);

// Dotted versions ("1.2.3", "v1.2"): positive when `a` is newer than `b`,
// negative when older, zero when they are the same. Missing parts count as 0.
int compareVersions(const std::string& a, const std::string& b);

// The DisplayVersion in an NRO's embedded NACP; empty when the file is not an
// NRO or carries none - either way, not something to install.
std::string nroDisplayVersion(const std::string& path);

} // namespace Update
