// Offering and installing a newer release (see include/update.h).
//
// Only ever from the profile and game selectors: nothing is open there that
// could have unsaved changes, and the app restarts straight into the new build.

#include "ui.h"
#include "update.h"
#include "i18n.h"

// The About popup's switch. Renames the setting file; turning it on also
// checks straight away, so the pill beside the version has something to say.
void UI::toggleAutoUpdate() {
    const bool next = !autoUpdate_;
    if (!Update::setAutoCheck(next)) {
        showMessageAndWait(i18n::get(StrKey::UpdToggleFailedTitle), i18n::get(StrKey::UpdToggleFailedBody));
        return;
    }
    autoUpdate_ = next;
    if (next && Update::state() == Update::State::Idle)
        Update::beginCheck(basePath_);
}

// X in the About popup: a check now, on or off. A release it finds is offered
// again on the selectors, even one that was turned down at launch.
void UI::checkUpdatesNow() {
    if (Update::state() == Update::State::Checking) return;
    updateOffered_ = false;
    Update::beginCheck(basePath_);
}

void UI::offerUpdate(bool& running) {
    const std::string latest = Update::latestVersion();
    ConfirmStyle st;
    st.confirmKey = StrKey::UpdInstall;
    if (!showConfirmDialog(i18n::get(StrKey::UpdAvailableTitle),
                           i18n::fmt(StrKey::UpdAvailableBody, latest, std::string(APP_VERSION)), st))
        return;

    std::string error;
    const bool ok = Update::install(exePath_, basePath_,
        [&](int step, float fraction) {
            const char* key = step == Update::StepDownload ? StrKey::UpdDownloading
                            : step == Update::StepUnpack   ? StrKey::UpdUnpacking
                                                           : StrKey::UpdInstalling;
            // Writing from the unpack on: that is when stopping would hurt.
            showWorking(i18n::fmt(key, latest), step != Update::StepDownload, fraction);
        },
        error);
    markDirty();

    if (!ok) {
        showMessageAndWait(i18n::get(StrKey::UpdFailedTitle), i18n::fmt(StrKey::UpdFailedBody, error));
        return;
    }
    // Straight into the new build: the one running now has had its RomFS
    // replaced underneath it, and should not carry on.
    showMessageAndWait(i18n::get(StrKey::UpdDoneTitle), i18n::fmt(StrKey::UpdDoneBody, latest),
                       DialogKind::Success);
    Update::restartInto(exePath_);
    running = false;
}
