#include "BookOrganizer.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <iomanip>
#include <stdexcept>


// Helper function to handle ImGui string input properly
namespace {
    bool ImGuiStringInput(const char* label, std::string& str, size_t maxSize = 256, ImGuiInputTextFlags flags = 0) {
        str.resize(maxSize);
        bool changed = ImGui::InputText(label, str.data(), str.size(), flags);
        if (changed) {
            str.resize(strlen(str.data()));
        }
        return changed;
    }

    bool ImGuiStringInputMultiline(const char* label, std::string& str, const ImVec2& size, size_t maxSize = 1024, ImGuiInputTextFlags flags = 0) {
        str.resize(maxSize);
        bool changed = ImGui::InputTextMultiline(label, str.data(), str.size(), size, flags);
        if (changed) {
            str.resize(strlen(str.data()));
        }
        return changed;
    }
}

BookOrganizer::BookOrganizer() {
    // Strings are initialized empty by default
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
    watchDirBuffer = watchDirectory; // Initialize UI buffer

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
        // Metadata will be loaded on-demand when needed
    }

    // Sort books by filename (avoid triggering metadata loading)
    std::sort(books.begin(), books.end(), [](const BookMetadata& a, const BookMetadata& b) {
        std::string nameA = a.filePath.stem().string();
        std::string nameB = b.filePath.stem().string();
        std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
        std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
        return nameA < nameB;
    });

    // Build tree structure
    buildTree();

    // Reset selection if the selected book no longer exists
    if (selectedBook) {
        bool found = false;
        for (const auto& book : books) {
            if (&book == selectedBook) {
                found = true;
                break;
            }
        }
        if (!found) {
            selectedBook = nullptr;
        }
    }
}

void BookOrganizer::buildTree() {
    rootNode = std::make_unique<TreeNode>("Root", watchDirectory, true);
    pathToNode.clear();
    pathToNode[watchDirectory] = rootNode.get();

    std::filesystem::path rootPath(watchDirectory);

    for (auto& book : books) {
        std::filesystem::path relativePath = std::filesystem::relative(book.filePath, rootPath);
        TreeNode* currentNode = rootNode.get();

        // Build path to file
        std::filesystem::path buildPath = rootPath;
        for (const auto& component : relativePath.parent_path()) {
            buildPath /= component;
            TreeNode* childNode = findOrCreateNode(buildPath);
            if (!childNode) {
                // Create directory node
                auto newNode = std::make_unique<TreeNode>(component.string(), buildPath, true, currentNode);
                childNode = newNode.get();
                pathToNode[buildPath.string()] = childNode;
                currentNode->children.push_back(std::move(newNode));
                currentNode = childNode;
            } else {
                currentNode = childNode;
            }
        }

        // Create file node
        auto fileNode = std::make_unique<TreeNode>(relativePath.filename().string(), book.filePath, false, currentNode);
        fileNode->bookData = &book;
        currentNode->children.push_back(std::move(fileNode));
    }

    // Sort all nodes
    sortTreeNode(rootNode.get());
}

TreeNode* BookOrganizer::findOrCreateNode(const std::filesystem::path& path) {
    auto it = pathToNode.find(path.string());
    return (it != pathToNode.end()) ? it->second : nullptr;
}

void BookOrganizer::sortTreeNode(TreeNode* node) {
    if (!node) return;

    // Sort children: directories first, then files, both alphabetically
    std::sort(node->children.begin(), node->children.end(),
              [](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
                  if (a->isDirectory != b->isDirectory) {
                      return a->isDirectory > b->isDirectory; // directories first
                  }

                  std::string nameA = a->name;
                  std::string nameB = b->name;
                  std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
                  std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
                  return nameA < nameB;
              });

    // Recursively sort children
    for (auto& child : node->children) {
        sortTreeNode(child.get());
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
            ImGui::Text("Book Organizer (%zu total)", books.size());
            ImGui::EndMenuBar();
        }

        // Create two-column layout
        ImGui::Columns(2, "MainColumns", false);
        ImGui::SetColumnWidth(0, leftPanelWidth);

        renderFileList();

        ImGui::NextColumn();

        renderMetadataPanel();

        ImGui::Columns(1);
    }
    ImGui::End();

    // Render unpaired books window if open
    if (showUnpairedWindow) {
        renderUnpairedBooksWindow();
    }

    // Show ImGui demo for debugging (optional)
    static bool show_demo = false;
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
        show_demo = !show_demo;
    }
    if (show_demo) {
        ImGui::ShowDemoWindow(&show_demo);
    }
}

void BookOrganizer::renderFileList() {
    // Watch directory input
    ImGui::Text("Watch Directory:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-200);
    if (ImGuiStringInput("##WatchDir", watchDirBuffer, 1024)) {
        setWatchDirectory(watchDirBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##Watch")) {
        // Simple implementation using zenity for directory selection
        FILE* pipe = popen("zenity --file-selection --directory 2>/dev/null", "r");
        if (pipe) {
            char selectedPath[1024];
            if (fgets(selectedPath, sizeof(selectedPath), pipe) != nullptr) {
                // Remove newline at end
                size_t len = strlen(selectedPath);
                if (len > 0 && selectedPath[len - 1] == '\n') {
                    selectedPath[len - 1] = '\0';
                }
                watchDirBuffer = selectedPath;
                setWatchDirectory(watchDirBuffer);
            }
            pclose(pipe);
        }
    }

    ImGui::Spacing();

    // Target directory input
    ImGui::Text("Target Directory:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-200);
    if (ImGuiStringInput("##TargetDir", targetDirBuffer, 1024)) {
        setTargetDirectory(targetDirBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##Target")) {
        // Simple implementation using zenity for directory selection
        FILE* pipe = popen("zenity --file-selection --directory 2>/dev/null", "r");
        if (pipe) {
            char selectedPath[1024];
            if (fgets(selectedPath, sizeof(selectedPath), pipe) != nullptr) {
                // Remove newline at end
                size_t len = strlen(selectedPath);
                if (len > 0 && selectedPath[len - 1] == '\n') {
                    selectedPath[len - 1] = '\0';
                }
                targetDirBuffer = selectedPath;
                setTargetDirectory(targetDirBuffer);
            }
            pclose(pipe);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Search input
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGuiStringInput("##Search", searchBuffer)) {
        currentSearch = searchBuffer;
        // Clear visibility cache when search changes
        nodeVisibility.clear();
        // If search is empty, clear the current search to show all files
        if (searchBuffer.empty()) {
            currentSearch.clear();
        }
    }

    ImGui::Spacing();

    // Show book statuses checkbox
    if (ImGui::Checkbox("Show book statuses", &showBookStatuses)) {
        if (showBookStatuses && !targetDirectory.empty()) {
            updateBookStatuses();
        } else if (!showBookStatuses) {
            // Clear status cache when disabled
            bookStatusCache.clear();
        }
    }

    // Find Unpaired Books button
    if (ImGui::Button("Find Unpaired Books") && !targetDirectory.empty()) {
        if (!scanningInProgress) {
            findUnpairedBooks();
        }
    }
    if (targetDirectory.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.6f, 1.0f), "(Set target directory first)");
    } else if (scanningInProgress) {
        ImGui::SameLine();
        ImGui::Text("Scanning...");
    }

    ImGui::Spacing();

    if (ImGui::BeginChild("FileList", ImVec2(0, 0), true)) {
        std::lock_guard<std::mutex> lock(booksMutex);

        if (rootNode) {
            // Update visibility based on search
            if (!currentSearch.empty()) {
                nodeVisibility.clear();
                for (auto& child : rootNode->children) {
                    shouldShowNode(child.get());
                }
            }

            for (auto& child : rootNode->children) {
                renderTreeNode(child.get(), 0);
            }
        }
    }
    ImGui::EndChild();
}

void BookOrganizer::renderTreeNode(TreeNode* node, int depth) {
    if (!node) return;

    // Check if this node should be shown based on search
    if (!currentSearch.empty() && nodeVisibility.find(node) != nodeVisibility.end() && !nodeVisibility[node]) {
        return;
    }

    if (node->isDirectory) {
        // Directory node
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if (node->isExpanded) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);
        node->isExpanded = nodeOpen;

        if (nodeOpen) {
            for (auto& child : node->children) {
                renderTreeNode(child.get(), depth + 1);
            }
            ImGui::TreePop();
        }
    } else {
        // File node - use TreeNodeEx as leaf
        std::string displayName;
        if (node->bookData) {
            // Use exact filename
            displayName = node->bookData->filePath.filename().string();
        } else {
            displayName = node->name;
        }

        ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        bool isSelected = (selectedBook == node->bookData);

        if (isSelected) {
            leafFlags |= ImGuiTreeNodeFlags_Selected;
        }

        // Set text color based on book status if enabled
        bool colorPushed = false;
        if (showBookStatuses && !node->isDirectory && node->bookData) {
            switch (node->status) {
                case BookStatus::Copied:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
                    colorPushed = true;
                    break;
                case BookStatus::CopiedDifferent:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.0f, 1.0f)); // Yellow
                    colorPushed = true;
                    break;
                default:
                    break;
            }
        }

        bool nodeClicked = ImGui::TreeNodeEx(displayName.c_str(), leafFlags);

        if (colorPushed) {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked()) {
            selectBook(node->bookData);
        }

        // Show tooltip with full path
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", node->fullPath.string().c_str());
        }
    }

    // Right-click context menu for both files and directories
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open")) {
            executeNonBlocking("xdg-open", {node->fullPath.string()});
        }

        if (!node->isDirectory) {
            if (ImGui::MenuItem("Open in Folder")) {
                openFileInFolderNonBlocking(node->fullPath);
            }

            // Show Copy option for files when target directory is set
            if (!targetDirectory.empty()) {
                ImGui::Separator();
                if (ImGui::MenuItem("Copy")) {
                    if (node->bookData) {
                        copyBookToTarget(*node->bookData);
                    }
                }
            }
        }

        ImGui::EndPopup();
    }
}

void BookOrganizer::renderMetadataPanel() {
    ImGui::Text("Metadata Editor");
    ImGui::Spacing();

    if (!selectedBook) {
        ImGui::Text("No book selected");
        return;
    }

    std::lock_guard<std::mutex> lock(booksMutex);
    const auto& book = *selectedBook;

    ImGui::Text("File: %s", book.filePath.filename().string().c_str());
    ImGui::Text("Path: %s", book.filePath.parent_path().string().c_str());

    // Open button
    if (ImGui::Button("Open")) {
        executeNonBlocking("xdg-open", {book.filePath.string()});
    }

    ImGui::SameLine();

    // Open in folder button
    if (ImGui::Button("Open in Folder")) {
        openFileInFolderNonBlocking(book.filePath);
    }

    // Show copy button if book status is CopiedDifferent
    if (showBookStatuses && !targetDirectory.empty()) {
        BookStatus status = checkBookStatus(book);
        if (status == BookStatus::CopiedDifferent) {
            ImGui::SameLine();
            if (ImGui::Button("Copy File")) {
                copyBookToTarget(book);
            }
        }
    }

    ImGui::Spacing();

    bool isEbook = isEbookFile(book.filePath);

    // Title input
    ImGui::Text("Title:");
    if (ImGuiStringInput("##Title", titleBuffer)) {
        metadataChanged = true;
        if (useTitleFromMetadata) {
            filenameTitle = titleBuffer;
        }
    }

    // Author input
    ImGui::Text("Authors:");
    if (ImGuiStringInput("##Author", authorBuffer)) {
        metadataChanged = true;
        if (useAuthorFromMetadata) {
            filenameAuthor = authorBuffer;
        }
    }

    // Publish year input (both ebooks and PDFs)
    ImGui::Text("Publish Year:");
    if (ImGuiStringInput("##Year", yearBuffer, 16)) {
        metadataChanged = true;
        if (useYearFromMetadata) {
            filenameYear = yearBuffer;
        }
    }

    if (isEbook) {
        // Publisher input (ebooks only)
        ImGui::Text("Publisher:");
        if (ImGuiStringInput("##Publisher", publisherBuffer)) {
            metadataChanged = true;
            if (usePublisherFromMetadata) {
                filenamePublisher = publisherBuffer;
            }
        }

        // Comments input (ebooks only)
        ImGui::Text("Comments:");
        if (ImGuiStringInputMultiline("##Comments", commentsBuffer, ImVec2(-1, 120), 1024, ImGuiInputTextFlags_CtrlEnterForNewLine)) {
            metadataChanged = true;
        }
    }

    ImGui::Spacing();

    // Update metadata button
    if (ImGui::Button("Update Book Metadata") && metadataChanged) {
        updateMetadataWithEbookMeta();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Filename generation section
    ImGui::Text("Filename Generation Options");
    ImGui::Spacing();

    // Title inclusion
    if (ImGui::Checkbox("##IncludeTitle", &includeTitle)) {
        // Checkbox changed
    }
    ImGui::SameLine();
    ImGui::Text("Title");

    if (includeTitle) {
        ImGui::Indent();
        bool titleMetadataChanged = ImGui::Checkbox("use value from metadata##Title", &useTitleFromMetadata);
        if (titleMetadataChanged && useTitleFromMetadata) {
            filenameTitle = titleBuffer;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGuiStringInput("##FilenameTitle", filenameTitle)) {
            if (filenameTitle != titleBuffer) {
                useTitleFromMetadata = false;
            }
        }
        ImGui::Unindent();
        ImGui::Spacing();
    }

    // Author inclusion
    if (ImGui::Checkbox("##IncludeAuthor", &includeAuthor)) {
        // Checkbox changed
    }
    ImGui::SameLine();
    ImGui::Text("Author");

    if (includeAuthor) {
        ImGui::Indent();
        bool authorMetadataChanged = ImGui::Checkbox("use value from metadata##Author", &useAuthorFromMetadata);
        if (authorMetadataChanged && useAuthorFromMetadata) {
            filenameAuthor = authorBuffer;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGuiStringInput("##FilenameAuthor", filenameAuthor)) {
            if (filenameAuthor != authorBuffer) {
                useAuthorFromMetadata = false;
            }
        }
        ImGui::Unindent();
        ImGui::Spacing();
    }

    // Publisher inclusion
    if (ImGui::Checkbox("##IncludePublisher", &includePublisher)) {
        // Checkbox changed
    }
    ImGui::SameLine();
    ImGui::Text("Publisher");

    if (includePublisher) {
        ImGui::Indent();
        bool publisherMetadataChanged = ImGui::Checkbox("use value from metadata##Publisher", &usePublisherFromMetadata);
        if (publisherMetadataChanged && usePublisherFromMetadata) {
            filenamePublisher = publisherBuffer;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGuiStringInput("##FilenamePublisher", filenamePublisher)) {
            if (filenamePublisher != publisherBuffer) {
                usePublisherFromMetadata = false;
            }
        }
        ImGui::Unindent();
        ImGui::Spacing();
    }

    // Year inclusion
    if (ImGui::Checkbox("##IncludeYear", &includeYear)) {
        // Checkbox changed
    }
    ImGui::SameLine();
    ImGui::Text("Year");

    if (includeYear) {
        ImGui::Indent();
        bool yearMetadataChanged = ImGui::Checkbox("use value from metadata##Year", &useYearFromMetadata);
        if (yearMetadataChanged && useYearFromMetadata) {
            filenameYear = yearBuffer;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGuiStringInput("##FilenameYear", filenameYear, 16)) {
            if (filenameYear != yearBuffer) {
                useYearFromMetadata = false;
            }
        }
        ImGui::Unindent();
        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Preview and apply section
    std::string customFilename = generateCustomFilename();
    if (!customFilename.empty()) {
        ImGui::Text("Preview:");
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "%s%s", customFilename.c_str(), book.filePath.extension().string().c_str());

        ImGui::Spacing();
        if (ImGui::Button("Apply Filename") && customFilename != book.filePath.stem().string()) {
            std::string fullNewFilename = customFilename + book.filePath.extension().string();
            if (renameBookFile(book, fullNewFilename)) {
                std::cout << "Successfully renamed file to: " << fullNewFilename << std::endl;
                // Clear selection to avoid stale reference - file watcher will handle refresh
                selectedBook = nullptr;
            } else {
                std::cerr << "Failed to rename file" << std::endl;
            }
        }
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.6f, 1.0f), "No filename elements selected");
    }
}

void BookOrganizer::selectBook(BookMetadata* book) {
    if (!book) {
        selectedBook = nullptr;
        return;
    }

    selectedBook = book;
    metadataChanged = false;

    // Ensure metadata is loaded before accessing it
    ensureMetadataLoaded(*book);

    // Copy current metadata to edit buffers
    titleBuffer = book->title;
    authorBuffer = book->author;
    yearBuffer = book->publishYear;
    publisherBuffer = book->publisher;
    commentsBuffer = book->comments;

    // Sync filename fields with metadata
    syncFilenameFieldsWithMetadata();
}



void BookOrganizer::updateBookMetadata() {
    if (!selectedBook) {
        return;
    }

    auto& book = *selectedBook;

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

void BookOrganizer::updateMetadataWithEbookMeta() {
    if (!selectedBook) {
        return;
    }

    auto& book = *selectedBook;
    std::string filePath = book.filePath.string();
    std::string command = "ebook-meta";

    // Add title
    if (!titleBuffer.empty()) {
        command += " --title=\"" + titleBuffer + "\"";
    }

    // Add authors
    if (!authorBuffer.empty()) {
        command += " --authors=\"" + authorBuffer + "\"";
    }

    // Add publisher (ebooks only)
    if (isEbookFile(book.filePath) && !publisherBuffer.empty()) {
        command += " --publisher=\"" + publisherBuffer + "\"";
    }

    // Add comments (ebooks only)
    if (isEbookFile(book.filePath) && !commentsBuffer.empty()) {
        command += " --comments=\"" + commentsBuffer + "\"";
    }

    command += " \"" + filePath + "\"";

    // Execute the command
    int result = system(command.c_str());

    if (result == 0) {
        // Update local metadata
        book.title = titleBuffer;
        book.author = authorBuffer;
        book.publishYear = yearBuffer;
        book.publisher = publisherBuffer;
        book.comments = commentsBuffer;
        metadataChanged = false;
        std::cout << "Successfully updated metadata for: " << book.filePath.filename().string() << std::endl;
    } else {
        std::cerr << "Failed to update metadata with ebook-meta command" << std::endl;
    }
}

bool BookOrganizer::isEbookFile(const std::filesystem::path& filePath) const {
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    return extension == ".epub" || extension == ".mobi" || extension == ".azw" ||
           extension == ".azw3" || extension == ".fb2" || extension == ".lit";
}

std::string BookOrganizer::generateCustomFilename() const {
    std::string filename;

    if (includeTitle && !filenameTitle.empty()) {
        filename += filenameTitle;
    }

    if (includeAuthor && !filenameAuthor.empty()) {
        if (!filename.empty()) filename += " ";
        filename += "[" + filenameAuthor + "]";
    }

    if (includePublisher && !filenamePublisher.empty()) {
        if (!filename.empty()) filename += " ";
        filename += "[" + filenamePublisher + "]";
    }

    if (includeYear && !filenameYear.empty()) {
        if (!filename.empty()) filename += " ";
        filename += "(" + filenameYear + ")";
    }

    return filename;
}

void BookOrganizer::syncFilenameFieldsWithMetadata() {
    if (useTitleFromMetadata) {
        filenameTitle = titleBuffer;
    }

    if (useAuthorFromMetadata) {
        filenameAuthor = authorBuffer;
    }

    if (usePublisherFromMetadata) {
        filenamePublisher = publisherBuffer;
    }

    if (useYearFromMetadata) {
        filenameYear = yearBuffer;
    }
}

bool BookOrganizer::shouldShowNode(TreeNode* node) {
    if (!node) return false;

    // If no search, show everything
    if (currentSearch.empty()) {
        nodeVisibility[node] = true;
        return true;
    }

    // Check if this node or any child matches
    bool shouldShow = false;

    if (!node->isDirectory) {
        // For files, check if filename contains search string (case insensitive)
        std::string filename = node->name;
        std::string search = currentSearch;
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);

        if (filename.find(search) != std::string::npos) {
            shouldShow = true;
            // Mark all parent directories as visible
            markParentPathsVisible(node);
        }
    } else {
        // For directories, check if any child should be shown
        for (auto& child : node->children) {
            if (shouldShowNode(child.get())) {
                shouldShow = true;
            }
        }
    }

    nodeVisibility[node] = shouldShow;
    return shouldShow;
}

void BookOrganizer::markParentPathsVisible(TreeNode* node) {
    TreeNode* current = node->parent;
    while (current) {
        nodeVisibility[current] = true;
        current = current->parent;
    }
}

void BookOrganizer::executeNonBlocking(const std::string& program, const std::vector<std::string>& args) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));

        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Redirect stdout and stderr to /dev/null to avoid output
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        execvp(program.c_str(), argv.data());
        _exit(1); // If execvp fails
    } else if (pid > 0) {
        // Parent process - don't wait, let it run in background
        // We could optionally use waitpid with WNOHANG to clean up zombies
    } else {
        // Fork failed
        std::cerr << "Failed to fork process for: " << program << std::endl;
    }
}

bool BookOrganizer::isCommandAvailable(const std::string& command) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execlp("which", "which", command.c_str(), nullptr);
        _exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) == 0;
    }

    return false; // Fork failed
}

void BookOrganizer::openFileInFolderNonBlocking(const std::filesystem::path& filePath) {
    std::string filePathStr = filePath.string();
    std::string folderPath = filePath.parent_path().string();

    // Check file managers in order of preference
    if (isCommandAvailable("nautilus")) {
        executeNonBlocking("nautilus", {"--select", filePathStr});
    } else if (isCommandAvailable("dolphin")) {
        executeNonBlocking("dolphin", {"--select", filePathStr});
    } else if (isCommandAvailable("nemo")) {
        executeNonBlocking("nemo", {folderPath});
    } else {
        // Fallback to opening the folder
        executeNonBlocking("xdg-open", {folderPath});
    }
}

void BookOrganizer::setTargetDirectory(const std::string& targetDir) {
    targetDirectory = targetDir;

    // Update the UI buffer as well for consistency
    targetDirBuffer = targetDirectory;

    if (showBookStatuses && !targetDirectory.empty()) {
        updateBookStatuses();
    }
}

void BookOrganizer::setWatchDirectory(const std::string& watchDir) {
    if (watchDirectory == watchDir) {
        return; // No change needed
    }

    // Stop current file watcher if running
    if (fileWatcher) {
        fileWatcher->stop();
        fileWatcher.reset();
    }

    // Clear current books
    {
        std::lock_guard<std::mutex> lock(booksMutex);
        books.clear();
        selectedBook = nullptr;
    }

    // Clear tree structure
    rootNode.reset();
    pathToNode.clear();

    // Set new watch directory
    watchDirectory = watchDir;
    watchDirBuffer = watchDirectory;

    // Initialize file watcher for new directory
    if (!watchDirectory.empty() && std::filesystem::exists(watchDirectory)) {
        fileWatcher = std::make_unique<FileWatcher>(watchDirectory);
        fileWatcher->setCallback([this](const std::filesystem::path& path, bool isNew) {
            this->onFileChanged(path, isNew);
        });
        fileWatcher->start();

        // Refresh book list for new directory
        refreshBookList();

        std::cout << "Watch directory changed to: " << watchDirectory << std::endl;
    } else if (!watchDirectory.empty()) {
        std::cerr << "Watch directory does not exist: " << watchDirectory << std::endl;
    }
}

BookStatus BookOrganizer::checkBookStatus(const BookMetadata& book) {
    if (targetDirectory.empty()) {
        return BookStatus::NotCopied;
    }

    // Get relative path from watch directory
    std::filesystem::path relativePath = std::filesystem::relative(book.filePath, watchDirectory);
    std::filesystem::path targetPath = std::filesystem::path(targetDirectory) / relativePath;

    if (!std::filesystem::exists(targetPath)) {
        return BookStatus::NotCopied;
    }

    // File exists, compare file sizes and modification times
    try {
        auto sourceSize = std::filesystem::file_size(book.filePath);
        auto targetSize = std::filesystem::file_size(targetPath);

        auto sourceTime = std::filesystem::last_write_time(book.filePath);
        auto targetTime = std::filesystem::last_write_time(targetPath);

        if (sourceSize == targetSize && sourceTime == targetTime) {
            return BookStatus::Copied;
        } else {
            return BookStatus::CopiedDifferent;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error comparing files: " << e.what() << std::endl;
        return BookStatus::Copied; // Assume copied if we can't verify
    }
}



void BookOrganizer::copyBookToTarget(const BookMetadata& book) {
    if (targetDirectory.empty()) {
        std::cerr << "No target directory set" << std::endl;
        return;
    }

    // Get relative path and create target path
    std::filesystem::path relativePath = std::filesystem::relative(book.filePath, watchDirectory);
    std::filesystem::path targetPath = std::filesystem::path(targetDirectory) / relativePath;

    // Create target directory if it doesn't exist
    std::error_code ec_dir;
    std::filesystem::create_directories(targetPath.parent_path(), ec_dir);
    if (ec_dir) {
        std::cerr << "Failed to create target directory: " << ec_dir.message() << std::endl;
        return;
    }

    // Copy the file
    std::error_code ec;
    std::filesystem::copy_file(book.filePath, targetPath,
                              std::filesystem::copy_options::overwrite_existing, ec);

    if (!ec) {
        std::cout << "Successfully copied: " << book.filePath.filename().string()
                  << " to " << targetPath.string() << std::endl;

        // Update status cache immediately
        bookStatusCache[book.filePath.string()] = BookStatus::Copied;

        // Update tree node status immediately
        std::function<void(TreeNode*)> updateNodeStatusImmediate = [&](TreeNode* node) {
            if (node->bookData && node->bookData->filePath == book.filePath) {
                node->status = BookStatus::Copied;
            }
            for (auto& child : node->children) {
                updateNodeStatusImmediate(child.get());
            }
        };

        if (rootNode) {
            updateNodeStatusImmediate(rootNode.get());
        }
    } else {
        std::cerr << "Failed to copy file: " << ec.message() << std::endl;
    }
}

void BookOrganizer::updateBookStatuses() {
    if (targetDirectory.empty()) {
        return;
    }

    bookStatusCache.clear();

    // Update status for all books and their tree nodes
    for (auto& book : books) {
        BookStatus status = checkBookStatus(book);
        bookStatusCache[book.filePath.string()] = status;
    }

    // Update tree node statuses
    std::function<void(TreeNode*)> updateNodeStatus = [&](TreeNode* node) {
        if (node->bookData) {
            auto it = bookStatusCache.find(node->bookData->filePath.string());
            if (it != bookStatusCache.end()) {
                node->status = it->second;
            }
        }
        for (auto& child : node->children) {
            updateNodeStatus(child.get());
        }
    };

    if (rootNode) {
        updateNodeStatus(rootNode.get());
    }
}

void BookOrganizer::findUnpairedBooks() {
    if (targetDirectory.empty()) {
        return;
    }

    scanningInProgress = true;
    unpairedBooks.clear();
    scanTargetDirectory(unpairedBooks);
    scanningInProgress = false;
    showUnpairedWindow = true;
}

void BookOrganizer::scanTargetDirectory(std::vector<UnpairedBook>& unpairedBooks) {
    std::cout << "Scanning target directory: " << targetDirectory << std::endl;

    // Collect all source book paths for comparison
    std::unordered_set<std::string> sourcePaths;
    std::unordered_map<std::string, std::filesystem::path> nameToSourcePath;

    for (const auto& book : books) {
        // Add relative path
        std::filesystem::path relativePath = std::filesystem::relative(book.filePath, watchDirectory);
        sourcePaths.insert(relativePath.string());

        // Map filename to relative path for misplaced file detection
        std::string fileName = relativePath.filename().string();
        nameToSourcePath[fileName] = relativePath;
    }

    // Recursively scan target directory
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(targetDirectory)) {
            if (entry.is_regular_file()) {
                std::filesystem::path filePath = entry.path();
                std::string fileName = filePath.filename().string();

                // Skip hidden files and non-book files
                if (fileName.empty() || fileName[0] == '.') {
                    continue;
                }

                // Check if it's a book-like file
                std::string ext = filePath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".epub" && ext != ".pdf" && ext != ".mobi" && ext != ".azw" &&
                    ext != ".azw3" && ext != ".fb2" && ext != ".lit") {
                    continue;
                }

                // Get relative path from target directory
                std::filesystem::path targetRelativePath = std::filesystem::relative(filePath, targetDirectory);

                // Check if this file exists in source (by path)
                bool foundByPath = sourcePaths.find(targetRelativePath.string()) != sourcePaths.end();

                // Check if this file exists in source (by filename only)
                bool foundByName = nameToSourcePath.find(fileName) != nameToSourcePath.end();

                // If not found by path, it's unpaired
                if (!foundByPath) {
                    UnpairedBook unpaired;
                    unpaired.filePath = filePath;
                    unpaired.fileName = fileName;
                    unpaired.fileSize = std::filesystem::file_size(filePath);
                    unpaired.hasMatchingPath = foundByName;

                    if (foundByName) {
                        unpaired.reason = "Wrong file path (filename matches source file)";
                        // Store the correct source path for fixing
                        auto it = nameToSourcePath.find(fileName);
                        if (it != nameToSourcePath.end()) {
                            unpaired.correctSourcePath = it->second;
                        }
                    } else {
                        unpaired.reason = "File not found in source directory";
                    }

                    unpairedBooks.push_back(unpaired);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning target directory: " << e.what() << std::endl;
    }

    std::cout << "Found " << unpairedBooks.size() << " unpaired books" << std::endl;
}

void BookOrganizer::renderUnpairedBooksWindow() {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Unpaired Books", &showUnpairedWindow)) {
        ImGui::Text("Found %zu unpaired books in target directory", unpairedBooks.size());
        ImGui::Text("These files exist in the target directory but not in the correct location.");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Yellow");
        ImGui::SameLine();
        ImGui::Text("= Wrong location (fixable)");
        ImGui::SameLine(300);
        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Red");
        ImGui::SameLine();
        ImGui::Text("= Not found in source");
        ImGui::Separator();

        if (unpairedBooks.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "No unpaired books found! All files in target are paired with source.");
        } else {
            if (ImGui::BeginChild("UnpairedList", ImVec2(0, 0), true)) {
                for (size_t i = 0; i < unpairedBooks.size(); i++) {
                    const auto& unpaired = unpairedBooks[i];

                    ImGui::PushID(static_cast<int>(i));

                    // File name with color coding
                    if (unpaired.hasMatchingPath) {
                        // Yellow for wrong location
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", unpaired.fileName.c_str());
                    } else {
                        // Red for completely missing
                        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "%s", unpaired.fileName.c_str());
                    }

                    // Show file size on same line
                    ImGui::SameLine();
                    ImGui::Text("(");
                    ImGui::SameLine();
                    if (unpaired.fileSize < 1024) {
                        ImGui::Text("%zu B", unpaired.fileSize);
                    } else if (unpaired.fileSize < 1024 * 1024) {
                        ImGui::Text("%.1f KB", unpaired.fileSize / 1024.0);
                    } else {
                        ImGui::Text("%.1f MB", unpaired.fileSize / (1024.0 * 1024.0));
                    }
                    ImGui::SameLine();
                    ImGui::Text(")");

                    // Show tooltip with details on hover
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Full Path: %s", unpaired.filePath.string().c_str());
                        ImGui::Text("File Size: %zu bytes (%.2f KB)", unpaired.fileSize, unpaired.fileSize / 1024.0);
                        ImGui::Text("Reason: %s", unpaired.reason.c_str());
                        if (unpaired.hasMatchingPath) {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Filename matches source but wrong location");
                            ImGui::Text("Should be at: %s", unpaired.correctSourcePath.string().c_str());
                        }
                        ImGui::EndTooltip();
                    }

                    // Action button on same line
                    ImGui::SameLine(ImGui::GetWindowWidth() - 80);

                    if (unpaired.hasMatchingPath) {
                        // Fix button for files with matching content
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.8f));
                        if (ImGui::Button("Fix")) {
                            fixUnpairedBook(unpaired);
                            // Remove from vector
                            unpairedBooks.erase(unpairedBooks.begin() + i);
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                            break; // Break to avoid iterator invalidation
                        }
                        ImGui::PopStyleColor();
                    } else {
                        // Delete button for files not in source
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
                        if (ImGui::Button("Delete")) {
                            deleteUnpairedBook(unpaired.filePath);
                            // Remove from vector
                            unpairedBooks.erase(unpairedBooks.begin() + i);
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                            break; // Break to avoid iterator invalidation
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void BookOrganizer::fixUnpairedBook(const UnpairedBook& unpaired) {
    try {
        // Calculate the correct target path
        std::filesystem::path correctTargetPath = std::filesystem::path(targetDirectory) / unpaired.correctSourcePath;

        // Create target directory if it doesn't exist
        std::filesystem::create_directories(correctTargetPath.parent_path());

        // Check if target file already exists
        if (std::filesystem::exists(correctTargetPath)) {
            // If target exists and has same size and time, just delete the misplaced file
            try {
                auto existingSize = std::filesystem::file_size(correctTargetPath);
                auto existingTime = std::filesystem::last_write_time(correctTargetPath);
                auto misplacedSize = std::filesystem::file_size(unpaired.filePath);
                auto misplacedTime = std::filesystem::last_write_time(unpaired.filePath);

                if (existingSize == misplacedSize && existingTime == misplacedTime) {
                    std::filesystem::remove(unpaired.filePath);
                    std::cout << "Removed duplicate file: " << unpaired.filePath.filename().string()
                              << " (correct file already exists at " << correctTargetPath.string() << ")" << std::endl;
                    return;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error comparing existing file " << correctTargetPath.string() << ": " << e.what() << std::endl;
            }

            // Different content - backup existing file
            std::filesystem::path backupPath = correctTargetPath.string() + ".backup";
            std::filesystem::rename(correctTargetPath, backupPath);
            std::cout << "Backed up existing file to: " << backupPath.string() << std::endl;
        }

        // Move the file to the correct location
        std::filesystem::rename(unpaired.filePath, correctTargetPath);

        std::cout << "Successfully moved: " << unpaired.filePath.filename().string()
                  << " -> " << correctTargetPath.string() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error fixing file " << unpaired.filePath.string() << ": " << e.what() << std::endl;
    }
}

void BookOrganizer::deleteUnpairedBook(const std::filesystem::path& filePath) {
    try {
        if (std::filesystem::remove(filePath)) {
            std::cout << "Successfully deleted: " << filePath.string() << std::endl;
        } else {
            std::cerr << "Failed to delete: " << filePath.string() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error deleting file " << filePath.string() << ": " << e.what() << std::endl;
    }
}

void BookOrganizer::readMetadataFromFile(BookMetadata& book) {
    if (!isEbookFile(book.filePath)) {
        return;
    }

    std::string command = "ebook-meta \"" + book.filePath.string() + "\" 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return;
    }

    char buffer[1024];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Track if we found any metadata
    bool foundMetadata = false;

    // Parse the ebook-meta output
    std::istringstream stream(result);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("Title               :") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            if (pos < line.length()) {
                book.title = line.substr(pos);
                // Trim whitespace
                book.title.erase(0, book.title.find_first_not_of(" \t"));
                book.title.erase(book.title.find_last_not_of(" \t") + 1);
                foundMetadata = true;
            }
        } else if (line.find("Author(s)           :") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            if (pos < line.length()) {
                book.author = line.substr(pos);
                // Trim whitespace
                book.author.erase(0, book.author.find_first_not_of(" \t"));
                book.author.erase(book.author.find_last_not_of(" \t") + 1);
                foundMetadata = true;
            }
        } else if (line.find("Published           :") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            if (pos < line.length()) {
                std::string published = line.substr(pos);
                // Trim whitespace
                published.erase(0, published.find_first_not_of(" \t"));
                published.erase(published.find_last_not_of(" \t") + 1);

                // Extract year from published date (usually in format YYYY-MM-DD)
                if (published.length() >= 4) {
                    book.publishYear = published.substr(0, 4);
                }
                foundMetadata = true;
            }
        } else if (line.find("Publisher           :") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            if (pos < line.length()) {
                book.publisher = line.substr(pos);
                // Trim whitespace
                book.publisher.erase(0, book.publisher.find_first_not_of(" \t"));
                book.publisher.erase(book.publisher.find_last_not_of(" \t") + 1);
                foundMetadata = true;
            }
        } else if (line.find("Comments            :") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            if (pos < line.length()) {
                book.comments = line.substr(pos);
                // Trim whitespace
                book.comments.erase(0, book.comments.find_first_not_of(" \t"));
                book.comments.erase(book.comments.find_last_not_of(" \t") + 1);
                foundMetadata = true;
            }
        }
    }

    // If no metadata was found, keep the filename-parsed metadata
    if (!foundMetadata) {
        // The parseFromFilename() was already called during BookMetadata construction
        // so we don't need to do anything - just keep the existing values
    }

    // Mark metadata as loaded
    book.metadataLoaded = true;
}

void BookOrganizer::ensureMetadataLoaded(BookMetadata& book) {
    if (!book.metadataLoaded && isEbookFile(book.filePath)) {
        readMetadataFromFile(book);
    }
}
