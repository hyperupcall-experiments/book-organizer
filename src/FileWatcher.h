#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <memory>

#if EFSW_AVAILABLE
#include "EfswFileWatcher.h"
#else
#include "FallbackFileWatcher.h"
#endif

class FileWatcher {
public:
    using FileChangeCallback = std::function<void(const std::filesystem::path&, bool)>; // path, isNew

    FileWatcher(const std::string& watchDirectory);
    ~FileWatcher();

    void start();
    void stop();
    void setCallback(FileChangeCallback callback);
    std::vector<std::filesystem::path> getCurrentFiles() const;
    bool isRunning() const;

private:
#if EFSW_AVAILABLE
    std::unique_ptr<EfswFileWatcher> impl;
#else
    std::unique_ptr<FallbackFileWatcher> impl;
#endif
};
