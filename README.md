# Book Organizer

An ImGUI C++ application for managing and syncing eBooks and PDFs.

It's all vibe-coded so use at your own risk.

## Features

- **Recursive Directory Monitoring**: Continuously watches a directory for book files
- **Real-time File Detection**: Automatically detects new and modified book files
- **Metadata Editing**: Edit title, author, and publication year
- **Smart File Naming**: Automatically renames files based on metadata in the format: `[title] [author] (year).extension`
- **Supported Formats**: PDF, EPUB, MOBI, AZW, AZW3, TXT, DOC, DOCX, FB2, DJVU, CBR, CBZ

## Prerequisites

### System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libglfw3-dev libgl1-mesa-dev pkg-config git
```

**RHEL/CentOS/Fedora:**
```bash
sudo yum install gcc-c++ cmake glfw-devel mesa-libGL-devel pkgconfig git
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake glfw mesa pkgconf git
```

**macOS:**
```bash
brew install cmake glfw pkg-config git
```

## Building

1. **Clone and setup:**
   ```bash
   git clone <repository-url>
   cd ai-book-organizer
   ./setup.sh
   ```

2. **Build the project:**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

## Usage

Run the application with a directory path:

```bash
./BookOrganizer /path/to/your/books
```

Or run without arguments to be prompted for a directory:

```bash
./BookOrganizer
```

## Interface

### Left Panel - File List
- Shows all detected book files in the watched directory
- Files are sorted alphabetically by title
- Click on any file to select and view its metadata
- Hover over files to see the full file path

### Right Panel - Metadata Editor
- **Title**: Edit the book's title
- **Author**: Edit the book's author
- **Publish Year**: Edit the publication year
- **Preview**: Shows how the file will be renamed
- **Update Button**: Apply changes and rename the file
- **Reset Button**: Discard changes and reload original metadata

## File Naming Convention

The application uses this naming format:
```
[Title] [Author] (Year).extension
```

Examples:
- `The Great Gatsby F. Scott Fitzgerald (1925).pdf`
- `1984 George Orwell (1949).epub`
- `To Kill a Mockingbird Harper Lee (1960).mobi`

## Parsing Existing Files

The application can parse metadata from existing filenames that follow the naming convention. If a file doesn't match the expected format, the filename becomes the title, and author/year fields remain empty.

## Technical Details

### Architecture
- **EfswFileWatcher**: Monitors directory changes using efsw library (https://github.com/SpartanJ/efsw)
- **BookMetadata**: Handles metadata parsing and filename generation  
- **BookOrganizer**: Main application logic and ImGUI interface
- **Cross-platform**: Works on Linux, macOS, and Windows

### File Monitoring
- Uses efsw (Entropic File System Watcher) for efficient file monitoring
- Real-time detection of file changes using OS-native APIs (inotify on Linux, FSEvents on macOS, etc.)
- Detects new files, modifications, deletions, and moves
- Thread-safe file list updates
- Automatic refresh of the interface

## Troubleshooting

### Build Issues
- Ensure all dependencies are installed
- Check that ImGUI was downloaded correctly in `external/imgui/`
- Check that efsw was downloaded correctly in `external/efsw/`
- Verify CMake version is 3.16 or higher

### Runtime Issues
- Make sure the directory path exists and is readable
- Check file permissions for renaming operations
- Ensure the target filename doesn't already exist

### Performance
- For directories with thousands of files, initial scanning may take a few seconds
- The application is optimized for typical book collections (hundreds to low thousands of files)

## License

This project uses Dear ImGui (MIT License), GLFW (zlib/libpng License), and efsw (MIT License).
