#!/bin/bash

# Script to bundle dynamic libraries for roofer binary into ../lib and update rpath

set -e

# Define paths
BINARY="roofer"
LIB_DIR="$(dirname "$BINARY")/../lib"
mkdir -p "$LIB_DIR"

# Function to check if a library is a system library (glibc or kernel)
is_system_lib() {
    local lib_path="$1"
    local lib_name=$(basename "$lib_path")
    if [[ "$lib_path" == linux-vdso.so* ]] || \
       [[ "$lib_name" == libc.so* ]] || \
       [[ "$lib_name" == libm.so* ]] || \
       [[ "$lib_name" == libdl.so* ]] || \
       [[ "$lib_name" == libpthread.so* ]] || \
       [[ "$lib_name" == librt.so* ]] || \
       [[ "$lib_name" == ld-linux*.so* ]]; then
        return 0 # True (is system library)
    else
        return 1 # False (not system library)
    fi
}

# Function to get dependencies of a binary or library
get_deps() {
    local file="$1"
    ldd "$file" 2>/dev/null | grep "=>" | awk '{print $3}' | while read -r dep; do
        if [[ "$dep" != "not" ]]; then
            echo "$dep"
        fi
    done
}

# Copy non-system libraries to LIB_DIR and update rpath
copy_and_update_lib() {
    local lib_path="$1"
    local target_file="$2" # Binary or library to update
    local lib_name=$(basename "$lib_path")
    local new_path="$LIB_DIR/$lib_name"

    if ! is_system_lib "$lib_path"; then
        if [[ -r "$lib_path" ]]; then
            # Copy library to LIB_DIR
            cp "$lib_path" "$new_path"
            chmod +w "$new_path"

            # Update rpath of the copied library
            patchelf --set-rpath "\$ORIGIN" "$new_path" || {
                echo "Error: Failed to set rpath for $new_path"
                exit 1
            }

            # If processing the binary, update its rpath to include LIB_DIR
            if [[ "$target_file" == "$BINARY" ]]; then
                patchelf --set-rpath "\$ORIGIN/../lib" "$target_file" || {
                    echo "Error: Failed to set rpath for $target_file"
                    exit 1
                }
            fi

            # Process dependencies of the copied library
            for dep in $(get_deps "$lib_path"); do
                local dep_name=$(basename "$dep")
                if ! is_system_lib "$dep"; then
                    if [[ -r "$dep" ]]; then
                        cp "$dep" "$LIB_DIR/$dep_name"
                        chmod +w "$LIB_DIR/$dep_name"
                        patchelf --set-rpath "\$ORIGIN" "$LIB_DIR/$dep_name" || {
                            echo "Error: Failed to set rpath for $LIB_DIR/$dep_name"
                            exit 1
                        }
                    else
                        echo "Warning: Cannot read $dep, skipping"
                    fi
                fi
            done
        else
            echo "Warning: Cannot read $lib_path, skipping"
        fi
    else
        echo "Skipping system library: $lib_path"
    fi
}

# Ensure the binary is writable
chmod +w "$BINARY"

# Process dependencies of the roofer binary
for dep in $(get_deps "$BINARY"); do
    copy_and_update_lib "$dep" "$BINARY"
done

echo "Bundling complete. Libraries copied to $LIB_DIR and rpath updated."
