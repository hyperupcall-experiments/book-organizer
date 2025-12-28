#include "BookOrganizer.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

BookOrganizer::BookOrganizer() {
    // Initialize buffers
    titleBuffer[0] = '\0';
    authorBuffer[0] = '\0';
    yearBuffer[0] = '\0';
    publisherBuffer[0] = '\0';
    commentsBuffer[0] = '\0';
    filenameTitle[0] = '\0';
    filenameAuthor[0] = '\0';
    filenamePublisher[0] = '\0';
    filenameYear[0] = '\0';
    searchBuffer[0] = '\0';
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
        // Read actual metadata from ebook files
        if (isEbookFile(file)) {
            readMetadataFromFile(books.back());
        }
    }

    // Sort books by title
    std::sort(books.begin(), books.end(), [](const BookMetadata& a, const BookMetadata& b) {
        std::string titleA = a.title.empty() ? a.filePath.stem().string() : a.title;
        std::string titleB = b.title.empty() ? b.filePath.stem().string() : b.title;
        std::transform(titleA.begin(), titleA.end(), titleA.begin(), ::tolower);
        std::transform(titleB.begin(), titleB.end(), titleB.begin(), ::tolower);
        return titleA < titleB;
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
            ImGui::Text("Watching: %s", watchDirectory.c_str());
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

    // Search input
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) {
        currentSearch = searchBuffer;
        // Clear visibility cache when search changes
        nodeVisibility.clear();
        // If search is empty, clear the current search to show all files
        if (strlen(searchBuffer) == 0) {
            currentSearch.clear();
        }
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

        bool nodeClicked = ImGui::TreeNodeEx(displayName.c_str(), leafFlags);

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

        if (!node->isDirectory && ImGui::MenuItem("Open in Folder")) {
            openFileInFolderNonBlocking(node->fullPath);
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

    ImGui::Spacing();

    bool isEbook = isEbookFile(book.filePath);

    // Title input
    ImGui::Text("Title:");
    if (ImGui::InputText("##Title", titleBuffer, sizeof(titleBuffer))) {
        metadataChanged = true;
        if (useTitleFromMetadata) {
            strncpy(filenameTitle, titleBuffer, sizeof(filenameTitle) - 1);
            filenameTitle[sizeof(filenameTitle) - 1] = '\0';
        }
    }

    // Author input
    ImGui::Text("Authors:");
    if (ImGui::InputText("##Author", authorBuffer, sizeof(authorBuffer))) {
        metadataChanged = true;
        if (useAuthorFromMetadata) {
            strncpy(filenameAuthor, authorBuffer, sizeof(filenameAuthor) - 1);
            filenameAuthor[sizeof(filenameAuthor) - 1] = '\0';
        }
    }

    // Publish year input (both ebooks and PDFs)
    ImGui::Text("Publish Year:");
    if (ImGui::InputText("##Year", yearBuffer, sizeof(yearBuffer))) {
        metadataChanged = true;
        if (useYearFromMetadata) {
            strncpy(filenameYear, yearBuffer, sizeof(filenameYear) - 1);
            filenameYear[sizeof(filenameYear) - 1] = '\0';
        }
    }

    if (isEbook) {
        // Publisher input (ebooks only)
        ImGui::Text("Publisher:");
        if (ImGui::InputText("##Publisher", publisherBuffer, sizeof(publisherBuffer))) {
            metadataChanged = true;
            if (usePublisherFromMetadata) {
                strncpy(filenamePublisher, publisherBuffer, sizeof(filenamePublisher) - 1);
                filenamePublisher[sizeof(filenamePublisher) - 1] = '\0';
            }
        }

        // Comments input (ebooks only)
        ImGui::Text("Comments:");
        if (ImGui::InputTextMultiline("##Comments", commentsBuffer, sizeof(commentsBuffer), ImVec2(-1, 120), ImGuiInputTextFlags_CtrlEnterForNewLine)) {
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
            strncpy(filenameTitle, titleBuffer, sizeof(filenameTitle) - 1);
            filenameTitle[sizeof(filenameTitle) - 1] = '\0';
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##FilenameTitle", filenameTitle, sizeof(filenameTitle))) {
            if (strcmp(filenameTitle, titleBuffer) != 0) {
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
            strncpy(filenameAuthor, authorBuffer, sizeof(filenameAuthor) - 1);
            filenameAuthor[sizeof(filenameAuthor) - 1] = '\0';
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##FilenameAuthor", filenameAuthor, sizeof(filenameAuthor))) {
            if (strcmp(filenameAuthor, authorBuffer) != 0) {
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
            strncpy(filenamePublisher, publisherBuffer, sizeof(filenamePublisher) - 1);
            filenamePublisher[sizeof(filenamePublisher) - 1] = '\0';
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##FilenamePublisher", filenamePublisher, sizeof(filenamePublisher))) {
            if (strcmp(filenamePublisher, publisherBuffer) != 0) {
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
            strncpy(filenameYear, yearBuffer, sizeof(filenameYear) - 1);
            filenameYear[sizeof(filenameYear) - 1] = '\0';
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##FilenameYear", filenameYear, sizeof(filenameYear))) {
            if (strcmp(filenameYear, yearBuffer) != 0) {
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

    // Copy current metadata to edit buffers
    strncpy(titleBuffer, book->title.c_str(), sizeof(titleBuffer) - 1);
    titleBuffer[sizeof(titleBuffer) - 1] = '\0';

    strncpy(authorBuffer, book->author.c_str(), sizeof(authorBuffer) - 1);
    authorBuffer[sizeof(authorBuffer) - 1] = '\0';

    strncpy(yearBuffer, book->publishYear.c_str(), sizeof(yearBuffer) - 1);
    yearBuffer[sizeof(yearBuffer) - 1] = '\0';

    strncpy(publisherBuffer, book->publisher.c_str(), sizeof(publisherBuffer) - 1);
    publisherBuffer[sizeof(publisherBuffer) - 1] = '\0';

    strncpy(commentsBuffer, book->comments.c_str(), sizeof(commentsBuffer) - 1);
    commentsBuffer[sizeof(commentsBuffer) - 1] = '\0';

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
    if (strlen(titleBuffer) > 0) {
        command += " --title=\"" + std::string(titleBuffer) + "\"";
    }

    // Add authors
    if (strlen(authorBuffer) > 0) {
        command += " --authors=\"" + std::string(authorBuffer) + "\"";
    }

    // Add publisher (ebooks only)
    if (isEbookFile(book.filePath) && strlen(publisherBuffer) > 0) {
        command += " --publisher=\"" + std::string(publisherBuffer) + "\"";
    }

    // Add comments (ebooks only)
    if (isEbookFile(book.filePath) && strlen(commentsBuffer) > 0) {
        command += " --comments=\"" + std::string(commentsBuffer) + "\"";
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

    if (includeTitle && strlen(filenameTitle) > 0) {
        filename += filenameTitle;
    }

    if (includeAuthor && strlen(filenameAuthor) > 0) {
        if (!filename.empty()) filename += " ";
        filename += "[" + std::string(filenameAuthor) + "]";
    }

    if (includePublisher && strlen(filenamePublisher) > 0) {
        if (!filename.empty()) filename += " ";
        filename += "[" + std::string(filenamePublisher) + "]";
    }

    if (includeYear && strlen(filenameYear) > 0) {
        if (!filename.empty()) filename += " ";
        filename += "(" + std::string(filenameYear) + ")";
    }

    return filename;
}

void BookOrganizer::syncFilenameFieldsWithMetadata() {
    if (useTitleFromMetadata) {
        strncpy(filenameTitle, titleBuffer, sizeof(filenameTitle) - 1);
        filenameTitle[sizeof(filenameTitle) - 1] = '\0';
    }

    if (useAuthorFromMetadata) {
        strncpy(filenameAuthor, authorBuffer, sizeof(filenameAuthor) - 1);
        filenameAuthor[sizeof(filenameAuthor) - 1] = '\0';
    }

    if (usePublisherFromMetadata) {
        strncpy(filenamePublisher, publisherBuffer, sizeof(filenamePublisher) - 1);
        filenamePublisher[sizeof(filenamePublisher) - 1] = '\0';
    }

    if (useYearFromMetadata) {
        strncpy(filenameYear, yearBuffer, sizeof(filenameYear) - 1);
        filenameYear[sizeof(filenameYear) - 1] = '\0';
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
}
