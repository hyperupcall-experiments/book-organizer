#pragma once
#include <efsw/efsw.hpp>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <unordered_set>
#include <mutex>

class EfswFileWatcher : public efsw::FileWatchListener {
public:
    using FileChangeCallback = std::function<void(const std::filesystem::path&, bool)>; // path, isNew

    EfswFileWatcher(const std::string& watchDirectory);
    ~EfswFileWatcher();

    void start();
    void stop();
    void setCallback(FileChangeCallback callback);
    std::vector<std::filesystem::path> getCurrentFiles() const;
    bool isRunning() const { return running; }

    // efsw::FileWatchListener interface
    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                         const std::string& filename, efsw::Action action,
                         std::string oldFilename = "") override;

private:
    void scanDirectory();
    bool isBookFile(const std::filesystem::path& path) const;

    std::string watchDir;
    bool running = false;

    std::unique_ptr<efsw::FileWatcher> fileWatcher;
    efsw::WatchID watchID = -1;

    FileChangeCallback changeCallback;

    std::unordered_set<std::string> knownFiles;
    mutable std::mutex filesMutex;
};
