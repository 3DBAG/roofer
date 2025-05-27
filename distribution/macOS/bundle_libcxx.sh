#!/bin/bash

# Script to bundle dynamic libraries for roofer binary into ../lib and update paths

set -e

# Define paths
BINARY="roofer"
LIB_DIR="$(dirname "$BINARY")/../lib"
mkdir -p "$LIB_DIR"

# Function to check if a library is a system library
is_system_lib() {
    local lib_path="$1"
    if [[ "$lib_path" == /usr/lib/* || "$lib_path" == /System/Library/* ]]; then
        return 0 # True (is system library)
    else
        return 1 # False (not system library)
    fi
}

# Function to get dependencies of a binary or library
get_deps() {
    local file="$1"
    otool -L "$file" | tail -n +2 | awk '{print $1}' | while read -r dep; do
        echo "$dep"
    done
}

# Copy non-system libraries to LIB_DIR and update paths
copy_and_update_lib() {
    local lib_path="$1"
    local target_file="$2" # Binary or library to update
    local lib_name=$(basename "$lib_path")
    local new_path="@executable_path/../lib/$lib_name"

    if ! is_system_lib "$lib_path"; then
        # Copy library to LIB_DIR
        cp "$lib_path" "$LIB_DIR/$lib_name"
        chmod +w "$LIB_DIR/$lib_name"

        # Update the library's own ID
        install_name_tool -id "$new_path" "$LIB_DIR/$lib_name"

        # Update the path in the target file
        install_name_tool -change "$lib_path" "$new_path" "$target_file"

        # Process dependencies of the copied library
        for dep in $(get_deps "$lib_path"); do
            local dep_name=$(basename "$dep")
            local new_dep_path="@executable_path/../lib/$dep_name"
            if ! is_system_lib "$dep"; then
                cp "$dep" "$LIB_DIR/$dep_name"
                chmod +w "$LIB_DIR/$dep_name"
                install_name_tool -id "$new_dep_path" "$LIB_DIR/$dep_name"
                install_name_tool -change "$dep" "$new_dep_path" "$LIB_DIR/$lib_name"
            fi
        done
    fi
}

# Ensure the binary is writable
chmod +w "$BINARY"

# Process dependencies of the roofer binary
for dep in $(get_deps "$BINARY"); do
    copy_and_update_lib "$dep" "$BINARY"
done

echo "Bundling complete. Libraries copied to $LIB_DIR and paths updated."
