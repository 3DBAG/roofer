#!/bin/bash
set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
    echo "Usage: $0 <executable_path>"
    exit 1
fi

echo "Bundling libraries for: $EXECUTABLE"

# Get the directory where the executable is located
EXEC_DIR=$(dirname "$EXECUTABLE")
EXEC_NAME=$(basename "$EXECUTABLE")

# Find the libc++ library path
LIBCXX_PATH=$(otool -L "$EXECUTABLE" | grep 'libc++.*\.dylib' | awk '{print $1}' | head -1)

if [ -z "$LIBCXX_PATH" ]; then
    echo "No libc++ dependency found"
    exit 0
fi

echo "Found libc++ at: $LIBCXX_PATH"

# Copy the library to the executable directory
LIBCXX_NAME=$(basename "$LIBCXX_PATH")
cp "$LIBCXX_PATH" "$EXEC_DIR/$LIBCXX_NAME"

# Update the executable to look for the bundled library
install_name_tool -change "$LIBCXX_PATH" "@executable_path/$LIBCXX_NAME" "$EXECUTABLE"

echo "Updated $EXEC_NAME to use bundled $LIBCXX_NAME"
echo "Verification:"
otool -L "$EXECUTABLE"
