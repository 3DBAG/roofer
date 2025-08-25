#!/bin/sh

set -e  # Exit on any error

echo "[*] Detecting platform..."

ARCH=$(uname -m)
OS=$(uname -s)

# Normalize architecture
case "$ARCH" in
    # x86_64)
    #     ARCH="x86_64"
    #     ;;
    arm64|aarch64)
        ARCH="arm64"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Normalize OS
case "$OS" in
    # Linux)
    #     PLATFORM="linux"
    #     ;;
    Darwin)
        PLATFORM="macOS"
        ;;
    *)
        echo "Unsupported OS: $OS"
        exit 1
        ;;
esac

echo "[*] Detected platform: $PLATFORM-$ARCH"

# Set the binary URL (replace with your actual binary URLs)
# BINARY_URL="https://example.com/downloads/mytool-${PLATFORM}-${ARCH}"
VERSION="1.0.0-beta.4"
BINARY_URL="https://github.com/3DBAG/roofer/releases/download/v${VERSION}/roofer-${PLATFORM}-${ARCH}-v${VERSION}.zip"
INSTALL_DIR="$HOME/.roofer"
INSTALL_BIN="$INSTALL_DIR/bin/roofer"

if [ -d "$INSTALL_DIR" ]; then
    echo "⚠️  $INSTALL_DIR already exists."
    printf "Do you want to remove it and reinstall? [y/N]: "
    read choice < /dev/tty
    case "$choice" in
        y|Y ) rm -rf "$INSTALL_DIR";;
        * ) echo "Aborted."; exit 1;;
    esac
fi

mkdir -p "$INSTALL_DIR"

echo "[*] Downloading binary..."
curl -L --progress-bar "$BINARY_URL" -o "roofer.zip"
# unzip contents
unzip -qo "roofer.zip" -d "$INSTALL_DIR"
rm "roofer.zip"
chmod +x "$INSTALL_BIN"

echo "[*] Installed to $INSTALL_BIN"

# Suggest adding to PATH
case ":$PATH:" in
  *:"$INSTALL_DIR":*)
    echo "[*] $INSTALL_DIR already in PATH"
    ;;
  *)
    echo
    echo "➤ Add the following to your shell profile (e.g., ~/.bashrc or ~/.zshrc):"
    echo "  export PATH=\"$INSTALL_DIR/bin\":\$PATH"
    echo
    echo "➤ You can uninstall roofer using:"
    echo "  rm -r ~/.roofer"
    echo
    ;;
esac

echo "✅ Installation complete!"
