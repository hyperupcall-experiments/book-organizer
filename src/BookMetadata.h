#pragma once
#include <string>
#include <filesystem>

struct BookMetadata {
    std::string title;
    std::string author;
    std::string publishYear;
    std::string publisher;
    std::string comments;
    std::filesystem::path filePath;
    std::filesystem::file_time_type lastModified;
    bool metadataLoaded = false;

    BookMetadata() = default;

    BookMetadata(const std::filesystem::path& path)
        : filePath(path) {
        parseFromFilename();
        updateLastModified();
    }

    void parseFromFilename();
    std::string generateFilename() const;
    void updateLastModified();
    bool hasChanged() const;
};
