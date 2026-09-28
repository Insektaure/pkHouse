#pragma once
#include <switch.h>
#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

// Save backups on the SD card, and putting one back onto the console.
//
// A backup is the save's folder copied to
//   backups/<profile>/<game>/<profile>_YYYY-MM-DD_HH-MM-SS/
// (UI::buildBackupDir, AccountManager::backupSaveDir). Since 2.1.0 each one
// also carries META_NAME: which save it was taken from (title + account) and
// every file's size and MD5, read back from the SD card after the copy. The
// meta is what lets a restore prove it is putting the right profile's save
// back, and that the SD card still holds what was written. Backups made
// before it existed restore too, with a warning that neither can be checked.
//
// The restore follows JKSV (tasks/backup.cpp and
// fs/io.cpp) where it has to: the journal size from the save's extra data,
// files created at their final size before being written, and a commit
// before the journal fills. Where JKSV is not transactional it goes further:
//
//  - Everything is read, checked and held in memory before the save is
//    touched. A backup that fails any check is refused, not half-restored.
//  - When the whole backup fits in the save's journal - the normal case: the
//    save files of these games are written in a single commit every time
//    pkHouse saves - the old files are removed and the new ones written
//    without any commit in between, then committed once. Until that commit
//    nothing is changed; a failure before it leaves the save exactly as it
//    was, because a save filesystem closed without a commit discards the
//    writes.
//  - Only a backup too large for one commit takes JKSV's path: wipe and
//    commit, then write committing whenever the journal is about to fill.
//    The caller takes a fresh backup first, so even that path can be undone.
//  - After the commit every file is read back from the console and compared
//    with the backup, byte for byte.
namespace SaveBackup {

// Dot-named: AccountManager::backupSaveDir skips dot entries, so a save can
// never have put a file of that name into a backup, and a restore skips it.
constexpr const char* META_NAME = ".pkhouse_backup";

struct FileEntry {
    std::string rel;     // path inside the backup, "/"-separated, no leading "/"
    int64_t     size = 0;
    std::string md5;     // lowercase hex
};

// What was in the save, read when the backup was made, so the list can say it
// without decrypting every backup. Empty / -1 when it could not be read.
struct Summary {
    std::string trainer;           // OT name, UTF-8
    std::string tid;               // as the game shows it
    int         pokemon = -1;      // Pokemon in the boxes
};

struct Meta {
    bool        present = false;   // found and parsed
    uint64_t    titleId = 0;
    AccountUid  uid{};
    time_t      created = 0;
    std::string reason;            // "open" (opening a game), "restore" (before one)
    std::string restoredFrom;      // reason "restore": the folder of the backup restored over it
    std::string version;           // pkHouse version that wrote it
    Summary     summary;
    std::vector<FileEntry> files;
};

// One backup folder, as listed.
struct Entry {
    std::string dir;     // full path, trailing "/"
    std::string name;    // folder name
    time_t      when = 0;   // from the folder name
    int64_t     bytes = 0;
    int         files = 0;
    Meta        meta;
    std::string fingerprint;   // from the meta; empty for a backup without one
};

// Reads back every file of a backup just written and records it in the meta.
// False when a file cannot be read back or the meta cannot be written; the
// backup is then left as a legacy one.
bool writeMeta(const std::string& backupDir, uint64_t titleId, const AccountUid& uid,
               const char* reason, const Summary& summary = Summary(),
               const std::string& restoredFrom = std::string());
Meta readMeta(const std::string& backupDir);

// One hash for a whole save: every file's path, size and MD5, in path order.
// Two backups with the same fingerprint hold the same save, and a backup
// whose fingerprint is the console's is what the console holds now.
std::string fingerprint(const std::vector<FileEntry>& files);
// The fingerprint of a folder's files, read and hashed now - "save:/" for the
// save on the console (mounted by the caller). Empty when it cannot be read.
std::string fingerprintDir(const std::string& dir);

// The backups of one profile and game (backups/<profile>/<game>/), newest first.
std::vector<Entry> list(const std::string& gameDir);

// Deletes a backup folder from the SD card.
bool remove(const std::string& backupDir);

// The size of every backup of one game folder (backups/<profile>/<game>/),
// counted as list() counts them. 0 when there is none.
int64_t bytesIn(const std::string& gameDir);

enum class Error {
    None,
    Empty,          // no files in the backup
    NoMainFile,     // the game's save file is missing from it
    ReadFailed,     // a backup file could not be read from the SD card
    Corrupt,        // files differ from what the meta recorded
    OtherSave,      // the meta names another game or another profile
    OpenSave,       // the console's save could not be opened
    SaveInfo,       // the save's journal size could not be read
    TooLarge,       // the backup does not fit in this save
    WriteFailed,    // a write to the console failed
    CommitFailed,   // the console refused the commit
    VerifyFailed,   // what was read back differs from the backup
};

// A backup read, checked and ready to be written. Holds every file in memory:
// the saves of these games are a few MB, and what is written is then exactly
// what was checked.
struct Prepared {
    Error       error = Error::None;
    std::string detail;            // the file or call that failed, for the message
    std::string dir;
    bool        legacy = false;    // no meta: origin and integrity not checkable
    std::vector<std::string> dirs;       // sub-folders, parents first
    std::vector<FileEntry>   files;
    std::vector<std::vector<uint8_t>> data;   // one per file
    int64_t     totalBytes = 0;
    int64_t     journalSize = 0;
    bool        atomic = false;    // fits in one commit: nothing changes unless it all works
};

// Everything that can be checked without writing: the backup read and
// hashed, its meta compared, the console's save opened to read its journal
// size, free space and current files. `mainFile` is the game's save file
// name (saveFileNameOf). The "save" device must NOT be mounted: the save is
// opened here directly.
Prepared prepare(uint64_t titleId, const AccountUid& uid, const std::string& backupDir,
                 const std::string& mainFile);

struct Outcome {
    Error       error = Error::None;
    std::string detail;
    bool        saveChanged = false;   // the save may differ from before (committed)
};

// Writes a prepared backup onto the console's save, commits and verifies.
// `progress` gets 0..1 over the write and the read-back. The "save" device
// must NOT be mounted.
Outcome apply(const Prepared& p, uint64_t titleId, const AccountUid& uid,
              const std::function<void(float)>& progress = nullptr);

} // namespace SaveBackup
