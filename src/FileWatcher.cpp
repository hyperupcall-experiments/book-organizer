#include "FileWatcher.h"

FileWatcher::FileWatcher(const std::string& watchDirectory) {
#if EFSW_AVAILABLE
    impl = std::make_unique<EfswFileWatcher>(watchDirectory);
#else
    impl = std::make_unique<FallbackFileWatcher>(watchDirectory);
#endif
}

FileWatcher::~FileWatcher() = default;

void FileWatcher::start() {
    if (impl) {
        impl->start();
    }
}

void FileWatcher::stop() {
    if (impl) {
        impl->stop();
    }
}

void FileWatcher::setCallback(FileChangeCallback callback) {
    if (impl) {
        impl->setCallback(callback);
    }
}

std::vector<std::filesystem::path> FileWatcher::getCurrentFiles() const {
    if (impl) {
        return impl->getCurrentFiles();
    }
    return {};
}

bool FileWatcher::isRunning() const {
    if (impl) {
        return impl->isRunning();
    }
    return false;
}
