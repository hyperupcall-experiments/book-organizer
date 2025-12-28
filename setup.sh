#!/bin/bash

set -e

echo "Setting up Book Organizer dependencies..."

# Create external directory if it doesn't exist
mkdir -p external
cd external

# Download Dear ImGui if not already present
if [ ! -d "imgui" ]; then
    echo "Downloading Dear ImGui..."
    git clone --depth 1 --branch v1.90.1 https://github.com/ocornut/imgui.git
    echo "Dear ImGui downloaded successfully"
else
    echo "Dear ImGui already exists, skipping download"
fi

# Download efsw if not already present
if [ ! -d "efsw" ]; then
    echo "Downloading efsw (file watcher)..."
    git clone --depth 1 https://github.com/SpartanJ/efsw.git
    echo "efsw downloaded successfully"
else
    echo "efsw already exists, skipping download"
fi

cd ..

# Install system dependencies
echo "Installing system dependencies..."

if command -v apt-get &> /dev/null; then
    # Ubuntu/Debian
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        libglfw3-dev \
        libgl1-mesa-dev \
        pkg-config \
        python3
elif command -v yum &> /dev/null; then
    # RHEL/CentOS/Fedora
    sudo yum install -y \
        gcc-c++ \
        cmake \
        glfw-devel \
        mesa-libGL-devel \
        pkgconfig \
        python3 \
        mesa-libGL-devel
elif command -v pacman &> /dev/null; then
    # Arch Linux
    sudo pacman -S --needed \
        base-devel \
        cmake \
        glfw \
        mesa \
        pkgconf \
        python \
        mesa
elif command -v brew &> /dev/null; then
    # macOS with Homebrew
    brew install \
        cmake \
        glfw \
        pkg-config \
        python3 \
        libgl-dev
else
    echo "Unknown package manager. Please install the following dependencies manually:"
    echo "- build-essential/gcc-c++/base-devel"
    echo "- cmake"
    echo "- glfw3-dev/glfw-devel/glfw"
    echo "- mesa-libGL-dev/mesa"
    echo "- pkg-config/pkgconfig/pkgconf"
    echo "- python3 (optional)"
fi



echo "Setup completed successfully!"
echo ""
echo "To build the project:"
echo "1. mkdir build && cd build"
echo "2. cmake .."
echo "3. make"
echo ""
echo "To run:"
echo "./BookOrganizer /path/to/your/books/directory"
