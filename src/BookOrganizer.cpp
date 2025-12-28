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

    if (ImGui::BeginChild("FileList", ImVec2(0, 0), true)) {
        std::lock_guard<std::mutex> lock(booksMutex);

        if (rootNode) {
            for (auto& child : rootNode->children) {
                renderTreeNode(child.get(), 0);
            }
        }
    }
    ImGui::EndChild();
}

void BookOrganizer::renderTreeNode(TreeNode* node, int depth) {
    if (!node) return;

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
            displayName = node->bookData->title.empty() ?
                node->bookData->filePath.stem().string() : node->bookData->title;

            if (!node->bookData->author.empty()) {
                displayName += " - " + node->bookData->author;
            }

            if (!node->bookData->publishYear.empty()) {
                displayName += " (" + node->bookData->publishYear + ")";
            }

            // Add file extension for clarity
            displayName += " [" + node->bookData->filePath.extension().string() + "]";
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
    ImGui::Spacing();

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

    ImGui::Spacing();

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
        selectBook(selectedBook); // Reload original values
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
