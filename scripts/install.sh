#!/bin/bash

# Resolve absolute paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$BASE_DIR/build"

# Help message
show_help() {
    echo "Usage: ./install.sh"
    echo
    echo "Installs the project using 'make install'."
    echo "Note: This may require sudo depending on your install prefix."
    echo "Make sure to run './build.sh' beforehand."
}

# Parse --help
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    show_help
    exit 0
fi

# Ensure build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "[install.sh, ERROR] Build directory not found at: $BUILD_DIR"
    echo "[install.sh, INFO] Please run './build.sh' before installing."
    exit 1
fi

# Run make install with sudo
cd "$BUILD_DIR" || exit 1
echo "[install.sh] Running 'sudo make install' in: $BUILD_DIR"
sudo make install

echo "[install.sh] Install complete."
