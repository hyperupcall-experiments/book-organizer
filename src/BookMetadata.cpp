#include "BookMetadata.h"
#include <regex>
#include <sstream>

void BookMetadata::parseFromFilename() {
    std::string filename = filePath.stem().string();

    // Try to parse format: "TITLE [Author Name] (Year)" where author and year are optional
    std::regex pattern(R"(^(.*?)(?:\s*\[([^\]]+)\])?(?:\s*\((\d{4})\))?$)");
    std::smatch matches;

    if (std::regex_match(filename, matches, pattern)) {
        title = matches[1].str();
        author = matches[2].str();
        publishYear = matches[3].str();

        // Trim whitespace from title
        title.erase(0, title.find_first_not_of(" \t"));
        title.erase(title.find_last_not_of(" \t") + 1);

        // Trim whitespace from author if present
        if (!author.empty()) {
            author.erase(0, author.find_first_not_of(" \t"));
            author.erase(author.find_last_not_of(" \t") + 1);
        }
    } else {
        // If parsing fails, use filename as title
        title = filename;
        author = "";
        publishYear = "";
    }
}

std::string BookMetadata::generateFilename() const {
    if (title.empty() && author.empty() && publishYear.empty()) {
        return filePath.stem().string();
    }

    std::ostringstream oss;

    if (!title.empty()) {
        oss << title;
    } else {
        oss << "Untitled";
    }

    if (!author.empty()) {
        oss << " [" << author << "]";
    }

    if (!publishYear.empty()) {
        oss << " (" << publishYear << ")";
    }

    return oss.str();
}

void BookMetadata::updateLastModified() {
    if (std::filesystem::exists(filePath)) {
        lastModified = std::filesystem::last_write_time(filePath);
    }
}

bool BookMetadata::hasChanged() const {
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    auto currentTime = std::filesystem::last_write_time(filePath);
    return currentTime != lastModified;
}
