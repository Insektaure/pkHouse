#include "save_backup.h"
#include "md5.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace {

// JKSV's buffer (fs/io.cpp SIZE_FILE_BUFFER): what is written between two
// looks at the journal.
constexpr int64_t CHUNK = 0x80000;
// Room left in the journal for what is not file data: the deletes, creates and
// directory entries that go into the same commit.
constexpr int64_t JOURNAL_MARGIN = 0x10000;
// Allocation slack per file or folder when comparing sizes with free space:
// the save filesystem hands out whole blocks.
constexpr int64_t BLOCK_SLACK = 0x4000;

constexpr const char* META_HEADER = "pkhouse-backup 1";

std::string md5Hex(const uint8_t* data, size_t len) {
    uint8_t h[16];
    MD5::hash(data, len, h);
    char out[33];
    for (int i = 0; i < 16; i++) std::snprintf(out + i * 2, 3, "%02x", h[i]);
    return std::string(out, 32);
}

std::string md5Hex(const std::vector<uint8_t>& d) { return md5Hex(d.data(), d.size()); }

// A meta value is the rest of its line: no line breaks, nothing else to escape.
std::string oneLine(std::string v) {
    for (char& c : v) if (c == '\n' || c == '\r') c = ' ';
    return v;
}

std::string join(const std::string& dirRel, const char* name) {
    return dirRel.empty() ? std::string(name) : dirRel + "/" + name;
}

bool sameUid(const AccountUid& a, const AccountUid& b) {
    return std::memcmp(&a, &b, sizeof(AccountUid)) == 0;
}

// --- SD card -------------------------------------------------------------------

// Every folder and file under root (trailing "/"), dot entries left out as
// backupSaveDir leaves them out. Sorted, so a backup lists the same way twice.
bool walkSd(const std::string& root, const std::string& dirRel,
            std::vector<std::string>& dirs, std::vector<SaveBackup::FileEntry>& files) {
    const std::string path = dirRel.empty() ? root : root + dirRel + "/";
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    struct Item { std::string name; bool dir; int64_t size; };
    std::vector<Item> items;
    bool ok = true;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        struct stat st{};
        if (stat((path + e->d_name).c_str(), &st) != 0) { ok = false; break; }
        items.push_back({e->d_name, S_ISDIR(st.st_mode), static_cast<int64_t>(st.st_size)});
    }
    closedir(d);
    if (!ok) return false;
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.name < b.name; });
    for (const Item& it : items) {
        const std::string rel = join(dirRel, it.name.c_str());
        if (it.dir) {
            dirs.push_back(rel);
            if (!walkSd(root, rel, dirs, files)) return false;
        } else {
            files.push_back({rel, it.size, std::string()});
        }
    }
    return true;
}

bool readSdFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = std::fseek(f, 0, SEEK_END) == 0;
    const long size = ok ? std::ftell(f) : -1;
    ok = ok && size >= 0 && std::fseek(f, 0, SEEK_SET) == 0;
    if (ok) {
        out.resize(static_cast<size_t>(size));
        ok = size == 0 || std::fread(out.data(), 1, out.size(), f) == out.size();
    }
    std::fclose(f);
    return ok;
}

bool removeSdTree(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    bool ok = true;
    while (dirent* e = readdir(d)) {
        if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
        const std::string child = path + "/" + e->d_name;
        struct stat st{};
        if (stat(child.c_str(), &st) != 0) { ok = false; continue; }
        if (S_ISDIR(st.st_mode)) ok = removeSdTree(child) && ok;
        else if (unlink(child.c_str()) != 0) ok = false;
    }
    closedir(d);
    return rmdir(path.c_str()) == 0 && ok;
}

// --- Console save ----------------------------------------------------------------

bool openSave(uint64_t titleId, const AccountUid& uid, FsFileSystem& fs) {
    FsSaveDataAttribute attr{};
    attr.application_id = titleId;
    attr.uid = uid;
    attr.save_data_type = FsSaveDataType_Account;
    return R_SUCCEEDED(fsOpenSaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr));
}

// The journal size, as JKSV reads it: from the save's extra data, found
// through the save data info list (tasks/backup.cpp, fs/save_data_functions.cpp).
bool readJournalSize(uint64_t titleId, const AccountUid& uid, int64_t& journal) {
    FsSaveDataFilter filter{};
    filter.filter_by_application_id = true;
    filter.filter_by_save_data_type = true;
    filter.filter_by_user_id = true;
    filter.save_data_rank = FsSaveDataRank_Primary;
    filter.attr.application_id = titleId;
    filter.attr.uid = uid;
    filter.attr.save_data_type = FsSaveDataType_Account;
    filter.attr.save_data_rank = FsSaveDataRank_Primary;

    FsSaveDataInfoReader reader;
    if (R_FAILED(fsOpenSaveDataInfoReaderWithFilter(&reader, FsSaveDataSpaceId_User, &filter)))
        return false;
    bool found = false;
    FsSaveDataInfo info{};
    s64 count = 0;
    while (!found && R_SUCCEEDED(fsSaveDataInfoReaderRead(&reader, &info, 1, &count)) && count > 0) {
        // The filter should be enough; checked again rather than trusted.
        found = info.application_id == titleId && sameUid(info.uid, uid)
             && info.save_data_type == FsSaveDataType_Account;
    }
    fsSaveDataInfoReaderClose(&reader);
    if (!found) return false;

    FsSaveDataExtraData extra{};
    if (R_FAILED(fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(
            &extra, sizeof(extra), static_cast<FsSaveDataSpaceId>(info.save_data_space_id),
            info.save_data_id)))
        return false;
    journal = extra.journal_size;
    return journal > 0;
}

struct LiveEntry {
    std::string rel;
    bool dir;
    int64_t size;
};

std::string savePath(const std::string& rel) { return "/" + rel; }

// Every folder and file of the console's save, parents before children; dot
// entries left out, as they never went into a backup either.
bool walkSave(FsFileSystem* fs, const std::string& dirRel, std::vector<LiveEntry>& out) {
    FsDir dir;
    const std::string path = dirRel.empty() ? std::string("/") : savePath(dirRel);
    if (R_FAILED(fsFsOpenDirectory(fs, path.c_str(), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir)))
        return false;
    std::vector<LiveEntry> here;
    std::vector<FsDirectoryEntry> buf(16);
    bool ok = true;
    for (;;) {
        s64 n = 0;
        if (R_FAILED(fsDirRead(&dir, &n, buf.size(), buf.data()))) { ok = false; break; }
        if (n <= 0) break;
        for (s64 i = 0; i < n; i++) {
            if (buf[i].name[0] == '.') continue;
            here.push_back({join(dirRel, buf[i].name), buf[i].type == FsDirEntryType_Dir, buf[i].file_size});
        }
    }
    fsDirClose(&dir);
    if (!ok) return false;
    for (const LiveEntry& e : here) {
        out.push_back(e);
        if (e.dir && !walkSave(fs, e.rel, out)) return false;
    }
    return true;
}

// Writes `data` into a file already open for writing, from `offset`.
bool writeAll(FsFile& f, int64_t offset, const uint8_t* data, int64_t len) {
    while (len > 0) {
        const int64_t n = std::min(len, CHUNK);
        if (R_FAILED(fsFileWrite(&f, offset, data, static_cast<u64>(n), FsWriteOption_None)))
            return false;
        offset += n;
        data += n;
        len -= n;
    }
    return true;
}

} // anonymous namespace

namespace SaveBackup {

// --- Meta ----------------------------------------------------------------------------

bool writeMeta(const std::string& backupDir, uint64_t titleId, const AccountUid& uid,
               const char* reason, const Summary& summary, const std::string& restoredFrom) {
    std::vector<std::string> dirs;
    std::vector<FileEntry> files;
    if (!walkSd(backupDir, std::string(), dirs, files) || files.empty())
        return false;
    // Read back from the SD card, not taken from the copy: the meta records
    // what the card actually holds.
    for (FileEntry& f : files) {
        std::vector<uint8_t> d;
        if (!readSdFile(backupDir + f.rel, d) || static_cast<int64_t>(d.size()) != f.size)
            return false;
        f.md5 = md5Hex(d);
    }

    FILE* out = std::fopen((backupDir + META_NAME).c_str(), "wb");
    if (!out) return false;
    bool ok = std::fprintf(out, "%s\n", META_HEADER) > 0;
    ok = ok && std::fprintf(out, "title %016llX\n", static_cast<unsigned long long>(titleId)) > 0;
    ok = ok && std::fprintf(out, "uid %016llX%016llX\n", static_cast<unsigned long long>(uid.uid[0]),
                            static_cast<unsigned long long>(uid.uid[1])) > 0;
    ok = ok && std::fprintf(out, "created %lld\n", static_cast<long long>(time(nullptr))) > 0;
    ok = ok && std::fprintf(out, "reason %s\n", reason ? reason : "open") > 0;
    ok = ok && std::fprintf(out, "version %s\n", APP_VERSION) > 0;
    if (!restoredFrom.empty())
        ok = ok && std::fprintf(out, "restored_from %s\n", oneLine(restoredFrom).c_str()) > 0;
    if (!summary.trainer.empty())
        ok = ok && std::fprintf(out, "trainer %s\n", oneLine(summary.trainer).c_str()) > 0;
    if (!summary.tid.empty())
        ok = ok && std::fprintf(out, "tid %s\n", oneLine(summary.tid).c_str()) > 0;
    if (summary.pokemon >= 0)
        ok = ok && std::fprintf(out, "pokemon %d\n", summary.pokemon) > 0;
    for (const FileEntry& f : files)
        ok = ok && std::fprintf(out, "file %lld %s %s\n", static_cast<long long>(f.size),
                                f.md5.c_str(), f.rel.c_str()) > 0;
    ok = std::fclose(out) == 0 && ok;
    if (!ok) std::remove((backupDir + META_NAME).c_str());   // half a meta is worse than none
    return ok;
}

Meta readMeta(const std::string& backupDir) {
    Meta m;
    FILE* in = std::fopen((backupDir + META_NAME).c_str(), "rb");
    if (!in) return m;
    char line[1024];
    bool header = false, title = false, uid = false;
    while (std::fgets(line, sizeof(line), in)) {
        size_t len = std::strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (!header) {
            header = std::strcmp(line, META_HEADER) == 0;
            if (!header) break;
            continue;
        }
        unsigned long long a = 0, b = 0;
        long long n = 0;
        char word[64];
        int used = 0;
        // Values that may hold spaces are the rest of their line.
        auto rest = [&](const char* key, std::string& into) {
            const size_t klen = std::strlen(key);
            if (std::strncmp(line, key, klen) != 0 || line[klen] != ' ') return false;
            into = line + klen + 1;
            return true;
        };
        int count = 0;
        if (rest("restored_from", m.restoredFrom) || rest("trainer", m.summary.trainer)
            || rest("tid", m.summary.tid)) {
            // read
        } else if (std::sscanf(line, "pokemon %d", &count) == 1) {
            m.summary.pokemon = count;
        } else if (std::sscanf(line, "title %llx", &a) == 1) {
            m.titleId = a;
            title = true;
        } else if (std::strncmp(line, "uid ", 4) == 0 && std::strlen(line + 4) == 32) {
            char hi[17] = {}, lo[17] = {};
            std::memcpy(hi, line + 4, 16);
            std::memcpy(lo, line + 20, 16);
            if (std::sscanf(hi, "%llx", &a) == 1 && std::sscanf(lo, "%llx", &b) == 1) {
                m.uid.uid[0] = a;
                m.uid.uid[1] = b;
                uid = true;
            }
        } else if (std::sscanf(line, "created %lld", &n) == 1) {
            m.created = static_cast<time_t>(n);
        } else if (std::sscanf(line, "reason %63s", word) == 1) {
            m.reason = word;
        } else if (std::sscanf(line, "version %63s", word) == 1) {
            m.version = word;
        } else if (std::sscanf(line, "file %lld %32s %n", &n, word, &used) == 2 && used > 0) {
            m.files.push_back({std::string(line + used), static_cast<int64_t>(n), word});
        }
    }
    std::fclose(in);
    m.present = header && title && uid && !m.files.empty();
    return m;
}

// --- Listing ------------------------------------------------------------------------

std::vector<Entry> list(const std::string& gameDir) {
    std::vector<Entry> out;
    DIR* d = opendir(gameDir.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        const std::string name = e->d_name;
        // Named after the time it was taken (UI::buildBackupDir); the same
        // parse as UI::backupStats, so the list and the count agree.
        if (name.size() < 19) continue;
        struct tm t{};
        if (std::sscanf(name.c_str() + name.size() - 19, "%4d-%2d-%2d_%2d-%2d-%2d",
                        &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
            continue;
        const std::string dir = gameDir + name + "/";
        struct stat st{};
        if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        t.tm_isdst = -1;

        Entry en;
        en.dir = dir;
        en.name = name;
        en.when = mktime(&t);
        std::vector<std::string> dirs;
        std::vector<FileEntry> files;
        walkSd(dir, std::string(), dirs, files);
        en.files = static_cast<int>(files.size());
        for (const FileEntry& f : files) en.bytes += f.size;
        en.meta = readMeta(dir);
        if (en.meta.present) en.fingerprint = fingerprint(en.meta.files);
        out.push_back(std::move(en));
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        return a.when != b.when ? a.when > b.when : a.name > b.name;
    });
    return out;
}

int64_t bytesIn(const std::string& gameDir) {
    int64_t total = 0;
    for (const Entry& e : list(gameDir)) total += e.bytes;
    return total;
}

bool remove(const std::string& backupDir) {
    std::string path = backupDir;
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return removeSdTree(path);
}

std::string fingerprint(const std::vector<FileEntry>& files) {
    std::vector<const FileEntry*> sorted;
    for (const FileEntry& f : files) sorted.push_back(&f);
    std::sort(sorted.begin(), sorted.end(),
              [](const FileEntry* a, const FileEntry* b) { return a->rel < b->rel; });
    std::string text;
    for (const FileEntry* f : sorted)
        text += f->rel + "\t" + std::to_string(f->size) + "\t" + f->md5 + "\n";
    return md5Hex(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::string fingerprintDir(const std::string& dir) {
    std::vector<std::string> dirs;
    std::vector<FileEntry> files;
    if (!walkSd(dir, std::string(), dirs, files) || files.empty())
        return std::string();
    std::vector<uint8_t> d;
    for (FileEntry& f : files) {
        if (!readSdFile(dir + f.rel, d) || static_cast<int64_t>(d.size()) != f.size)
            return std::string();
        f.md5 = md5Hex(d);
    }
    return fingerprint(files);
}

// --- Restore -------------------------------------------------------------------------

Prepared prepare(uint64_t titleId, const AccountUid& uid, const std::string& backupDir,
                 const std::string& mainFile) {
    Prepared p;
    p.dir = backupDir;
    auto fail = [&](Error e, const std::string& what) {
        p.error = e;
        p.detail = what;
        p.data.clear();
        return p;
    };

    // The backup, read whole and hashed.
    if (!walkSd(backupDir, std::string(), p.dirs, p.files))
        return fail(Error::ReadFailed, backupDir);
    if (p.files.empty())
        return fail(Error::Empty, backupDir);
    if (std::none_of(p.files.begin(), p.files.end(),
                     [&](const FileEntry& f) { return f.rel == mainFile; }))
        return fail(Error::NoMainFile, mainFile);
    p.data.resize(p.files.size());
    for (size_t i = 0; i < p.files.size(); i++) {
        FileEntry& f = p.files[i];
        if (!readSdFile(backupDir + f.rel, p.data[i]) || static_cast<int64_t>(p.data[i].size()) != f.size)
            return fail(Error::ReadFailed, f.rel);
        f.md5 = md5Hex(p.data[i]);
        p.totalBytes += f.size;
    }

    // Against its meta, when it has one.
    const Meta meta = readMeta(backupDir);
    p.legacy = !meta.present;
    if (meta.present) {
        if (meta.titleId != titleId || !sameUid(meta.uid, uid))
            return fail(Error::OtherSave, std::string());
        if (meta.files.size() != p.files.size())
            return fail(Error::Corrupt, std::string());
        std::unordered_map<std::string, const FileEntry*> recorded;
        for (const FileEntry& f : meta.files) recorded[f.rel] = &f;
        for (const FileEntry& f : p.files) {
            auto it = recorded.find(f.rel);
            if (it == recorded.end() || it->second->size != f.size || it->second->md5 != f.md5)
                return fail(Error::Corrupt, f.rel);
        }
    }

    // The console's save: journal, free space, what it holds now.
    if (!readJournalSize(titleId, uid, p.journalSize))
        return fail(Error::SaveInfo, std::string());
    FsFileSystem fs;
    if (!openSave(titleId, uid, fs))
        return fail(Error::OpenSave, std::string());
    s64 freeSpace = 0;
    std::vector<LiveEntry> live;
    const bool spaceOk = R_SUCCEEDED(fsFsGetFreeSpace(&fs, "/", &freeSpace));
    const bool liveOk = spaceOk && walkSave(&fs, std::string(), live);
    fsFsClose(&fs);
    if (!spaceOk) return fail(Error::SaveInfo, std::string());
    if (!liveOk)  return fail(Error::OpenSave, std::string());

    std::unordered_map<std::string, int64_t> liveFiles;
    int64_t liveBytes = 0;
    for (const LiveEntry& e : live)
        if (!e.dir) { liveFiles[e.rel] = e.size; liveBytes += e.size; }
    // What a single commit has to allocate on top of what is there: every
    // file not already present at the same size (those are written in place).
    int64_t newBytes = 0;
    for (const FileEntry& f : p.files) {
        auto it = liveFiles.find(f.rel);
        if (it == liveFiles.end() || it->second != f.size) newBytes += f.size;
    }
    const int64_t slack = BLOCK_SLACK * static_cast<int64_t>(p.files.size() + p.dirs.size());

    const bool fitsJournal = p.totalBytes + JOURNAL_MARGIN <= p.journalSize;
    const bool fitsBeside = newBytes + slack <= freeSpace;   // old blocks are only freed by the commit
    p.atomic = fitsJournal && fitsBeside;
    if (!p.atomic && p.totalBytes + slack > static_cast<int64_t>(freeSpace) + liveBytes)
        return fail(Error::TooLarge, std::string());
    return p;
}

Outcome apply(const Prepared& p, uint64_t titleId, const AccountUid& uid,
              const std::function<void(float)>& progress) {
    Outcome o;
    if (p.error != Error::None) {
        o.error = p.error;
        o.detail = p.detail;
        return o;
    }

    FsFileSystem fs;
    if (!openSave(titleId, uid, fs)) {
        o.error = Error::OpenSave;
        return o;
    }
    // Closing without a commit throws every uncommitted change away: until
    // the first commit below, failing leaves the save as it was.
    auto fail = [&](Error e, const std::string& what) {
        o.error = e;
        o.detail = what;
        fsFsClose(&fs);
        return o;
    };

    const double total = static_cast<double>(std::max<int64_t>(1, p.totalBytes)) * 2.0;   // write + read back
    double done = 0.0;
    auto advance = [&](int64_t n) {
        done += static_cast<double>(n);
        if (progress) progress(static_cast<float>(std::min(1.0, done / total)));
    };

    std::vector<LiveEntry> live;
    if (!walkSave(&fs, std::string(), live))
        return fail(Error::OpenSave, std::string());

    std::unordered_map<std::string, const FileEntry*> wantFile;
    for (const FileEntry& f : p.files) wantFile[f.rel] = &f;
    const std::unordered_set<std::string> wantDir(p.dirs.begin(), p.dirs.end());

    if (p.atomic) {
        // 1. Out of the way: every file and folder the backup does not have,
        //    and every file it has at another size (recreated at its size,
        //    as JKSV creates each file before writing it).
        std::vector<std::string> gone;             // folders removed with their contents
        std::unordered_set<std::string> inPlace;   // same size: overwritten, not recreated
        std::unordered_set<std::string> keptDirs;
        auto underGone = [&](const std::string& rel) {
            for (const std::string& g : gone)
                if (rel.size() > g.size() && rel.compare(0, g.size(), g) == 0 && rel[g.size()] == '/')
                    return true;
            return false;
        };
        for (const LiveEntry& e : live) {
            if (underGone(e.rel)) continue;
            if (e.dir) {
                if (wantDir.count(e.rel)) { keptDirs.insert(e.rel); continue; }
                if (R_FAILED(fsFsDeleteDirectoryRecursively(&fs, savePath(e.rel).c_str())))
                    return fail(Error::WriteFailed, e.rel);
                gone.push_back(e.rel);
            } else {
                auto it = wantFile.find(e.rel);
                if (it != wantFile.end() && it->second->size == e.size) { inPlace.insert(e.rel); continue; }
                if (R_FAILED(fsFsDeleteFile(&fs, savePath(e.rel).c_str())))
                    return fail(Error::WriteFailed, e.rel);
            }
        }
        // 2. Folders, parents first.
        for (const std::string& d : p.dirs)
            if (!keptDirs.count(d) && R_FAILED(fsFsCreateDirectory(&fs, savePath(d).c_str())))
                return fail(Error::WriteFailed, d);
        // 3. Files, each at its final size before it is written.
        for (size_t i = 0; i < p.files.size(); i++) {
            const FileEntry& f = p.files[i];
            const std::string path = savePath(f.rel);
            if (!inPlace.count(f.rel) && R_FAILED(fsFsCreateFile(&fs, path.c_str(), f.size, 0)))
                return fail(Error::WriteFailed, f.rel);
            FsFile file;
            if (R_FAILED(fsFsOpenFile(&fs, path.c_str(), FsOpenMode_Write, &file)))
                return fail(Error::WriteFailed, f.rel);
            const bool ok = writeAll(file, 0, p.data[i].data(), f.size)
                         && R_SUCCEEDED(fsFileFlush(&file));
            fsFileClose(&file);
            if (!ok) return fail(Error::WriteFailed, f.rel);
            advance(f.size);
        }
        // 4. The one commit. If the console refuses it, nothing was applied.
        if (R_FAILED(fsFsCommit(&fs)))
            return fail(Error::CommitFailed, std::string());
        o.saveChanged = true;
    } else {
        // JKSV's path (tasks/backup.cpp restore_backup_local, fs/io.cpp
        // copy_file_commit), for a backup the journal cannot hold at once.
        // 1. Wipe and commit.
        for (const LiveEntry& e : live) {
            if (e.rel.find('/') != std::string::npos) continue;   // removed with its folder
            const Result rc = e.dir ? fsFsDeleteDirectoryRecursively(&fs, savePath(e.rel).c_str())
                                    : fsFsDeleteFile(&fs, savePath(e.rel).c_str());
            if (R_FAILED(rc)) return fail(Error::WriteFailed, e.rel);
        }
        if (R_FAILED(fsFsCommit(&fs)))
            return fail(Error::CommitFailed, std::string());
        o.saveChanged = true;
        // 2. Folders, committed.
        for (const std::string& d : p.dirs)
            if (R_FAILED(fsFsCreateDirectory(&fs, savePath(d).c_str())))
                return fail(Error::WriteFailed, d);
        if (!p.dirs.empty() && R_FAILED(fsFsCommit(&fs)))
            return fail(Error::CommitFailed, std::string());
        // 3. Files, committing before the journal fills. Unlike JKSV the
        //    count starts over after each commit, which is when the journal
        //    empties; JKSV keeps counting and so commits more often than it
        //    needs to, which is safe but not required.
        const int64_t budget = std::max<int64_t>(BLOCK_SLACK, p.journalSize - JOURNAL_MARGIN);
        const int64_t step = std::min<int64_t>(CHUNK, std::max<int64_t>(BLOCK_SLACK, budget / 2));
        int64_t journalCount = 0;
        for (size_t i = 0; i < p.files.size(); i++) {
            const FileEntry& f = p.files[i];
            const std::string path = savePath(f.rel);
            if (R_FAILED(fsFsCreateFile(&fs, path.c_str(), f.size, 0)))
                return fail(Error::WriteFailed, f.rel);
            FsFile file;
            if (R_FAILED(fsFsOpenFile(&fs, path.c_str(), FsOpenMode_Write, &file)))
                return fail(Error::WriteFailed, f.rel);
            for (int64_t off = 0; off < f.size;) {
                const int64_t n = std::min(step, f.size - off);
                if (journalCount + n > budget) {
                    // Commit with the file closed, as JKSV does, then carry on
                    // where it stopped.
                    const bool flushed = R_SUCCEEDED(fsFileFlush(&file));
                    fsFileClose(&file);
                    if (!flushed || R_FAILED(fsFsCommit(&fs)))
                        return fail(Error::CommitFailed, f.rel);
                    if (R_FAILED(fsFsOpenFile(&fs, path.c_str(), FsOpenMode_Write, &file)))
                        return fail(Error::WriteFailed, f.rel);
                    journalCount = 0;
                }
                if (!writeAll(file, off, p.data[i].data() + off, n)) {
                    fsFileClose(&file);
                    return fail(Error::WriteFailed, f.rel);
                }
                off += n;
                journalCount += n;
                advance(n);
            }
            const bool flushed = R_SUCCEEDED(fsFileFlush(&file));
            fsFileClose(&file);
            if (!flushed) return fail(Error::WriteFailed, f.rel);
        }
        if (R_FAILED(fsFsCommit(&fs)))
            return fail(Error::CommitFailed, std::string());
    }

    // Read back: exactly the backup's files, byte for byte, and nothing else.
    std::vector<LiveEntry> after;
    if (!walkSave(&fs, std::string(), after))
        return fail(Error::VerifyFailed, std::string());
    size_t filesSeen = 0, dirsSeen = 0;
    for (const LiveEntry& e : after) {
        if (e.dir) {
            if (!wantDir.count(e.rel)) return fail(Error::VerifyFailed, e.rel);
            dirsSeen++;
            continue;
        }
        auto it = wantFile.find(e.rel);
        if (it == wantFile.end() || it->second->size != e.size)
            return fail(Error::VerifyFailed, e.rel);
        filesSeen++;
    }
    if (filesSeen != p.files.size() || dirsSeen != p.dirs.size())
        return fail(Error::VerifyFailed, std::string());
    std::vector<uint8_t> back;
    for (size_t i = 0; i < p.files.size(); i++) {
        const FileEntry& f = p.files[i];
        FsFile file;
        if (R_FAILED(fsFsOpenFile(&fs, savePath(f.rel).c_str(), FsOpenMode_Read, &file)))
            return fail(Error::VerifyFailed, f.rel);
        back.assign(static_cast<size_t>(f.size), 0);
        u64 got = 0;
        const bool ok = f.size == 0
            || (R_SUCCEEDED(fsFileRead(&file, 0, back.data(), back.size(), FsReadOption_None, &got))
                && got == back.size());
        fsFileClose(&file);
        if (!ok || (f.size > 0 && std::memcmp(back.data(), p.data[i].data(), back.size()) != 0))
            return fail(Error::VerifyFailed, f.rel);
        advance(f.size);
    }

    fsFsClose(&fs);
    return o;
}

} // namespace SaveBackup
