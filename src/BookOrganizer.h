#pragma once
#include "BookMetadata.h"
#include "FileWatcher.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <filesystem>
#include <unordered_map>
#include <libb2/blake2.h>

enum class BookStatus {
    NotCopied,
    Copied,
    CopiedDifferent
};

struct UnpairedBook {
    std::filesystem::path filePath;
    std::string blake2Hash;
    std::string fileName;
    size_t fileSize;
    bool hasMatchingHash;
    std::string reason;
    std::filesystem::path correctSourcePath;
};

struct TreeNode {
    std::string name;
    std::filesystem::path fullPath;
    bool isDirectory;
    bool isExpanded;
    std::vector<std::unique_ptr<TreeNode>> children;
    TreeNode* parent;
    BookMetadata* bookData; // only for files
    BookStatus status = BookStatus::NotCopied;

    TreeNode(const std::string& n, const std::filesystem::path& path, bool isDir, TreeNode* p = nullptr)
        : name(n), fullPath(path), isDirectory(isDir), isExpanded(true), parent(p), bookData(nullptr) {}
};

class BookOrganizer {
public:
    BookOrganizer();
    ~BookOrganizer();

    bool initialize(const std::string& watchDirectory);
    void render();
    void shutdown();
    void setTargetDirectory(const std::string& targetDir);

    bool isInitialized() const { return initialized; }

private:
    void onFileChanged(const std::filesystem::path& path, bool isNew);
    void refreshBookList();
    void buildTree();
    void renderFileList();
    void renderTreeNode(TreeNode* node, int depth = 0);
    void renderMetadataPanel();
    void selectBook(BookMetadata* book);
    void updateBookMetadata();
    void updateMetadataWithEbookMeta();
    bool renameBookFile(const BookMetadata& book, const std::string& newFilename);
    TreeNode* findOrCreateNode(const std::filesystem::path& path);
    void sortTreeNode(TreeNode* node);
    bool isEbookFile(const std::filesystem::path& filePath) const;
    std::string generateCustomFilename() const;
    void syncFilenameFieldsWithMetadata();
    bool shouldShowNode(TreeNode* node);
    void markParentPathsVisible(TreeNode* node);
    void openFileInFolderNonBlocking(const std::filesystem::path& filePath);
    void executeNonBlocking(const std::string& program, const std::vector<std::string>& args);
    bool isCommandAvailable(const std::string& command);
    void readMetadataFromFile(BookMetadata& book);
    BookStatus checkBookStatus(const BookMetadata& book);
    std::string calculateBlake2Checksum(const std::filesystem::path& filePath);
    void copyBookToTarget(const BookMetadata& book);
    void updateBookStatuses();
    void findUnpairedBooks();
    void renderUnpairedBooksWindow();
    void deleteUnpairedBook(const std::filesystem::path& filePath);
    void fixUnpairedBook(const UnpairedBook& unpaired);
    void scanTargetDirectory(std::vector<UnpairedBook>& unpairedBooks);

    bool initialized = false;
    std::string watchDirectory;
    std::string targetDirectory;
    std::unique_ptr<FileWatcher> fileWatcher;

    std::vector<BookMetadata> books;
    mutable std::mutex booksMutex;

    // Tree structure
    std::unique_ptr<TreeNode> rootNode;
    std::unordered_map<std::string, TreeNode*> pathToNode;
    BookMetadata* selectedBook = nullptr;

    // UI state for editing
    char titleBuffer[256];
    char authorBuffer[256];
    char yearBuffer[16];
    char publisherBuffer[256];
    char commentsBuffer[1024];
    bool metadataChanged = false;

    // Filename generation state
    bool includeTitle = true;
    bool includeAuthor = true;
    bool includePublisher = false;
    bool includeYear = true;
    bool useTitleFromMetadata = true;
    bool useAuthorFromMetadata = true;
    bool usePublisherFromMetadata = true;
    bool useYearFromMetadata = true;
    char filenameTitle[256];
    char filenameAuthor[256];
    char filenamePublisher[256];
    char filenameYear[16];

    // Search functionality
    char searchBuffer[256];
    std::string currentSearch;
    std::unordered_map<TreeNode*, bool> nodeVisibility;

    // Target directory and status functionality
    char targetDirBuffer[1024];
    bool showBookStatuses = false;
    std::unordered_map<std::string, BookStatus> bookStatusCache;

    // Unpaired books functionality
    bool showUnpairedWindow = false;
    std::vector<UnpairedBook> unpairedBooks;
    bool scanningInProgress = false;

    // UI dimensions
    float leftPanelWidth = 750.0f;
};
