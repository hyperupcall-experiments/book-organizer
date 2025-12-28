#include "FallbackFileWatcher.h"
#include <algorithm>
#include <iostream>

FallbackFileWatcher::FallbackFileWatcher(const std::string& watchDirectory)
    : watchDir(watchDirectory) {
}

FallbackFileWatcher::~FallbackFileWatcher() {
    stop();
}

void FallbackFileWatcher::start() {
    if (running) {
        return;
    }

    if (!std::filesystem::exists(watchDir)) {
        std::cerr << "Watch directory does not exist: " << watchDir << std::endl;
        return;
    }

    std::cout << "Starting fallback file watcher for: " << watchDir << std::endl;

    running = true;
    scanDirectory(); // Initial scan
    watchThread = std::thread(&FallbackFileWatcher::watchLoop, this);

    std::cout << "Fallback file watcher started successfully" << std::endl;
}

void FallbackFileWatcher::stop() {
    if (!running) {
        return;
    }

    running = false;
    if (watchThread.joinable()) {
        watchThread.join();
    }

    std::cout << "Fallback file watcher stopped" << std::endl;
}

void FallbackFileWatcher::setCallback(FileChangeCallback callback) {
    changeCallback = callback;
}

std::vector<std::filesystem::path> FallbackFileWatcher::getCurrentFiles() const {
    std::lock_guard<std::mutex> lock(fileStatesMutex);
    std::vector<std::filesystem::path> files;

    for (const auto& [filepath, _] : fileStates) {
        files.emplace_back(filepath);
    }

    std::sort(files.begin(), files.end());
    return files;
}

void FallbackFileWatcher::watchLoop() {
    while (running) {
        scanDirectory();
        std::this_thread::sleep_for(pollInterval);
    }
}

void FallbackFileWatcher::scanDirectory() {
    if (!std::filesystem::exists(watchDir)) {
        return;
    }

    std::lock_guard<std::mutex> lock(fileStatesMutex);
    std::unordered_map<std::string, std::filesystem::file_time_type> currentFiles;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(watchDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto& path = entry.path();
            if (!isBookFile(path)) {
                continue;
            }

            auto lastWrite = std::filesystem::last_write_time(path);
            std::string pathStr = path.string();
            currentFiles[pathStr] = lastWrite;

            auto it = fileStates.find(pathStr);
            if (it == fileStates.end()) {
                // New file
                if (changeCallback) {
                    changeCallback(path, true);
                }
                std::cout << "File added: " << path.filename() << std::endl;
            } else if (it->second != lastWrite) {
                // Modified file
                if (changeCallback) {
                    changeCallback(path, false);
                }
                std::cout << "File modified: " << path.filename() << std::endl;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    // Check for deleted files
    for (const auto& [pathStr, _] : fileStates) {
        if (currentFiles.find(pathStr) == currentFiles.end()) {
            std::cout << "File deleted: " << std::filesystem::path(pathStr).filename() << std::endl;
        }
    }

    fileStates = std::move(currentFiles);
}

bool FallbackFileWatcher::isBookFile(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    return extension == ".pdf" ||
           extension == ".epub" ||
           extension == ".mobi" ||
           extension == ".azw" ||
           extension == ".azw3" ||
           extension == ".txt" ||
           extension == ".doc" ||
           extension == ".docx" ||
           extension == ".fb2" ||
           extension == ".djvu" ||
           extension == ".cbr" ||
           extension == ".cbz";
}
