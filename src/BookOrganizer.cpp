#include "BookOrganizer.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

BookOrganizer::BookOrganizer() {
    // Initialize buffers
    titleBuffer[0] = '\0';
    authorBuffer[0] = '\0';
    yearBuffer[0] = '\0';
}

BookOrganizer::~BookOrganizer() {
    shutdown();
}

bool BookOrganizer::initialize(const std::string& watchDir) {
    if (initialized) {
        return true;
    }

    if (!std::filesystem::exists(watchDir)) {
        std::cerr << "Watch directory does not exist: " << watchDir << std::endl;
        return false;
    }

    watchDirectory = watchDir;

    fileWatcher = std::make_unique<FileWatcher>(watchDirectory);
    fileWatcher->setCallback([this](const std::filesystem::path& path, bool isNew) {
        onFileChanged(path, isNew);
    });

    fileWatcher->start();
    refreshBookList();

    initialized = true;
    return true;
}

void BookOrganizer::shutdown() {
    if (fileWatcher) {
        fileWatcher->stop();
        fileWatcher.reset();
    }
    initialized = false;
}

void BookOrganizer::onFileChanged(const std::filesystem::path& path, bool isNew) {
    refreshBookList();
}

void BookOrganizer::refreshBookList() {
    if (!fileWatcher) {
        return;
    }

    auto files = fileWatcher->getCurrentFiles();

    std::lock_guard<std::mutex> lock(booksMutex);
    books.clear();
    books.reserve(files.size());

    for (const auto& file : files) {
        books.emplace_back(file);
    }

    // Sort books by title
    std::sort(books.begin(), books.end(), [](const BookMetadata& a, const BookMetadata& b) {
        std::string titleA = a.title.empty() ? a.filePath.stem().string() : a.title;
        std::string titleB = b.title.empty() ? b.filePath.stem().string() : b.title;
        std::transform(titleA.begin(), titleA.end(), titleA.begin(), ::tolower);
        std::transform(titleB.begin(), titleB.end(), titleB.begin(), ::tolower);
        return titleA < titleB;
    });

    // Reset selection if it's out of bounds
    if (selectedBookIndex >= static_cast<int>(books.size())) {
        selectedBookIndex = -1;
    }
}

void BookOrganizer::render() {
    if (!initialized) {
        // Show error window
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Error##BookOrganizer")) {
            ImGui::Text("Application not initialized");
            ImGui::Text("Please check the directory path and try again.");
        }
        ImGui::End();
        return;
    }

    // Main window - fullscreen to viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Book Organizer", nullptr, window_flags)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("Watching: %s", watchDirectory.c_str());
            ImGui::EndMenuBar();
        }

        ImGui::Separator();

        // Create two-column layout
        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, leftPanelWidth);

        renderFileList();

        ImGui::NextColumn();

        renderMetadataPanel();

        ImGui::Columns(1);
    }
    ImGui::End();

    // Show ImGui demo for debugging (optional)
    static bool show_demo = false;
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F1))) {
        show_demo = !show_demo;
    }
    if (show_demo) {
        ImGui::ShowDemoWindow(&show_demo);
    }
}

void BookOrganizer::renderFileList() {
    ImGui::Text("Books (%zu)", books.size());
    ImGui::Separator();

    if (ImGui::BeginChild("FileList", ImVec2(0, 0), true)) {
        std::lock_guard<std::mutex> lock(booksMutex);

        for (int i = 0; i < static_cast<int>(books.size()); ++i) {
            const auto& book = books[i];

            std::string displayName = book.title.empty() ?
                book.filePath.stem().string() : book.title;

            if (!book.author.empty()) {
                displayName += " - " + book.author;
            }

            if (!book.publishYear.empty()) {
                displayName += " (" + book.publishYear + ")";
            }

            // Add file extension for clarity
            displayName += " [" + book.filePath.extension().string() + "]";

            bool isSelected = (selectedBookIndex == i);

            if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                selectBook(i);
            }

            // Show tooltip with full path
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", book.filePath.string().c_str());
            }
        }
    }
    ImGui::EndChild();
}

void BookOrganizer::renderMetadataPanel() {
    ImGui::Text("Metadata Editor");
    ImGui::Separator();

    if (selectedBookIndex < 0 || selectedBookIndex >= static_cast<int>(books.size())) {
        ImGui::Text("No book selected");
        return;
    }

    std::lock_guard<std::mutex> lock(booksMutex);
    const auto& book = books[selectedBookIndex];

    ImGui::Text("File: %s", book.filePath.filename().string().c_str());
    ImGui::Text("Path: %s", book.filePath.parent_path().string().c_str());
    ImGui::Separator();

    // Title input
    ImGui::Text("Title:");
    if (ImGui::InputText("##Title", titleBuffer, sizeof(titleBuffer))) {
        metadataChanged = true;
    }

    // Author input
    ImGui::Text("Author:");
    if (ImGui::InputText("##Author", authorBuffer, sizeof(authorBuffer))) {
        metadataChanged = true;
    }

    // Publish year input
    ImGui::Text("Publish Year:");
    if (ImGui::InputText("##Year", yearBuffer, sizeof(yearBuffer))) {
        metadataChanged = true;
    }

    ImGui::Separator();

    // Preview new filename
    if (metadataChanged) {
        BookMetadata tempBook = book;
        tempBook.title = titleBuffer;
        tempBook.author = authorBuffer;
        tempBook.publishYear = yearBuffer;

        std::string newFilename = tempBook.generateFilename();
        ImGui::Text("New filename: %s%s", newFilename.c_str(),
                   book.filePath.extension().string().c_str());
    }

    ImGui::Spacing();

    // Update button
    if (ImGui::Button("Update Metadata") && metadataChanged) {
        updateBookMetadata();
    }

    ImGui::SameLine();

    // Reset button
    if (ImGui::Button("Reset")) {
        selectBook(selectedBookIndex); // Reload original values
    }
}

void BookOrganizer::selectBook(int index) {
    if (index < 0 || index >= static_cast<int>(books.size())) {
        selectedBookIndex = -1;
        return;
    }

    selectedBookIndex = index;
    metadataChanged = false;

    const auto& book = books[index];

    // Copy current metadata to edit buffers
    strncpy(titleBuffer, book.title.c_str(), sizeof(titleBuffer) - 1);
    titleBuffer[sizeof(titleBuffer) - 1] = '\0';

    strncpy(authorBuffer, book.author.c_str(), sizeof(authorBuffer) - 1);
    authorBuffer[sizeof(authorBuffer) - 1] = '\0';

    strncpy(yearBuffer, book.publishYear.c_str(), sizeof(yearBuffer) - 1);
    yearBuffer[sizeof(yearBuffer) - 1] = '\0';
}

void BookOrganizer::updateBookMetadata() {
    if (selectedBookIndex < 0 || selectedBookIndex >= static_cast<int>(books.size())) {
        return;
    }

    auto& book = books[selectedBookIndex];

    // Update metadata
    book.title = titleBuffer;
    book.author = authorBuffer;
    book.publishYear = yearBuffer;

    // Generate new filename
    std::string newFilename = book.generateFilename();
    std::string extension = book.filePath.extension().string();
    std::string fullNewFilename = newFilename + extension;

    // Rename file if the filename changed
    std::string currentFilename = book.filePath.filename().string();
    if (fullNewFilename != currentFilename) {
        if (renameBookFile(book, fullNewFilename)) {
            std::cout << "Successfully renamed: " << currentFilename
                     << " -> " << fullNewFilename << std::endl;
        } else {
            std::cerr << "Failed to rename file: " << currentFilename << std::endl;
        }
    }

    metadataChanged = false;
}

bool BookOrganizer::renameBookFile(const BookMetadata& book, const std::string& newFilename) {
    std::filesystem::path newPath = book.filePath.parent_path() / newFilename;

    // Check if target file already exists
    if (std::filesystem::exists(newPath)) {
        std::cerr << "Target file already exists: " << newPath << std::endl;
        return false;
    }

    try {
        std::filesystem::rename(book.filePath, newPath);

        // Update the book's file path in our data structure
        const_cast<BookMetadata&>(book).filePath = newPath;

        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to rename file: " << e.what() << std::endl;
        return false;
    }
}
