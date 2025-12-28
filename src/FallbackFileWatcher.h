#pragma once
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

class FallbackFileWatcher {
public:
    using FileChangeCallback = std::function<void(const std::filesystem::path&, bool)>; // path, isNew

    FallbackFileWatcher(const std::string& watchDirectory);
    ~FallbackFileWatcher();

    void start();
    void stop();
    void setCallback(FileChangeCallback callback);
    std::vector<std::filesystem::path> getCurrentFiles() const;
    bool isRunning() const { return running; }

private:
    void watchLoop();
    void scanDirectory();
    bool isBookFile(const std::filesystem::path& path) const;

    std::string watchDir;
    std::atomic<bool> running{false};
    std::thread watchThread;
    FileChangeCallback changeCallback;

    std::unordered_map<std::string, std::filesystem::file_time_type> fileStates;
    mutable std::mutex fileStatesMutex;

    std::chrono::milliseconds pollInterval{1000}; // Poll every 1 second
};
