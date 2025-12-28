#include "FileWatcher.h"
#include <iostream>
#include <algorithm>

FileWatcher::FileWatcher(const std::string& watchDirectory)
    : watchDir(watchDirectory) {
    fileWatcher = std::make_unique<efsw::FileWatcher>();
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start() {
    if (running) {
        return;
    }

    if (!std::filesystem::exists(watchDir)) {
        std::cerr << "Watch directory does not exist: " << watchDir << std::endl;
        return;
    }

    std::cout << "Starting file watcher for: " << watchDir << std::endl;

    // Initial scan to populate known files
    scanDirectory();

    // Start watching
    watchID = fileWatcher->addWatch(watchDir, this, true); // true = recursive

    if (watchID < 0) {
        std::cerr << "Failed to add watch for directory: " << watchDir << std::endl;
        return;
    }

    fileWatcher->watch();
    running = true;

    std::cout << "File watcher started successfully" << std::endl;
}

void FileWatcher::stop() {
    if (!running) {
        return;
    }

    if (watchID >= 0) {
        fileWatcher->removeWatch(watchID);
        watchID = -1;
    }

    running = false;
    std::cout << "File watcher stopped" << std::endl;
}

void FileWatcher::setCallback(FileChangeCallback callback) {
    changeCallback = callback;
}

std::vector<std::filesystem::path> FileWatcher::getCurrentFiles() const {
    std::lock_guard<std::mutex> lock(filesMutex);
    std::vector<std::filesystem::path> files;
    files.reserve(knownFiles.size());

    for (const auto& filepath : knownFiles) {
        files.emplace_back(filepath);
    }

    std::sort(files.begin(), files.end());
    return files;
}

void FileWatcher::handleFileAction(efsw::WatchID watchid, const std::string& dir,
                                  const std::string& filename, efsw::Action action,
                                  std::string oldFilename) {
    std::filesystem::path fullPath = std::filesystem::path(dir) / filename;

    // Only handle book files
    if (!isBookFile(fullPath)) {
        return;
    }

    std::lock_guard<std::mutex> lock(filesMutex);
    std::string pathStr = fullPath.string();

    switch (action) {
        case efsw::Actions::Add: {
            if (knownFiles.find(pathStr) == knownFiles.end()) {
                knownFiles.insert(pathStr);
                if (changeCallback) {
                    changeCallback(fullPath, true); // true = new file
                }
                std::cout << "File added: " << filename << std::endl;
            }
            break;
        }

        case efsw::Actions::Delete: {
            auto it = knownFiles.find(pathStr);
            if (it != knownFiles.end()) {
                knownFiles.erase(it);
                std::cout << "File deleted: " << filename << std::endl;
                // Note: We could add a callback for deletions if needed
            }
            break;
        }

        case efsw::Actions::Modified: {
            if (knownFiles.find(pathStr) != knownFiles.end()) {
                if (changeCallback) {
                    changeCallback(fullPath, false); // false = modified file
                }
                std::cout << "File modified: " << filename << std::endl;
            }
            break;
        }

        case efsw::Actions::Moved: {
            if (!oldFilename.empty()) {
                std::filesystem::path oldPath = std::filesystem::path(dir) / oldFilename;
                std::string oldPathStr = oldPath.string();

                // Remove old path
                auto it = knownFiles.find(oldPathStr);
                if (it != knownFiles.end()) {
                    knownFiles.erase(it);
                }

                // Add new path if it's a book file
                if (isBookFile(fullPath)) {
                    knownFiles.insert(pathStr);
                    if (changeCallback) {
                        changeCallback(fullPath, false); // treat as modified
                    }
                }

                std::cout << "File moved: " << oldFilename << " -> " << filename << std::endl;
            }
            break;
        }
    }
}

void FileWatcher::scanDirectory() {
    if (!std::filesystem::exists(watchDir)) {
        return;
    }

    std::lock_guard<std::mutex> lock(filesMutex);
    knownFiles.clear();

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(watchDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto& path = entry.path();
            if (isBookFile(path)) {
                knownFiles.insert(path.string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error during scan: " << e.what() << std::endl;
    }

    std::cout << "Initial scan found " << knownFiles.size() << " book files" << std::endl;
}

bool FileWatcher::isBookFile(const std::filesystem::path& path) const {
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
