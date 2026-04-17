#!/bin/sh

set -e  # Exit on any error

confirm_overwrite() {
    if [ "${ROOFER_INSTALL_FORCE:-}" = "1" ]; then
        return 0
    fi

    if [ ! -r /dev/tty ]; then
        echo "$INSTALL_BIN already exists."
        echo "Set ROOFER_INSTALL_FORCE=1 to overwrite it in non-interactive mode."
        return 1
    fi

    printf "Do you want to overwrite it? [y/N]: " > /dev/tty
    read choice < /dev/tty
    case "$choice" in
        y|Y ) return 0 ;;
        * ) return 1 ;;
    esac
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1"
        exit 1
    fi
}

echo "[*] Detecting platform..."

require_command curl
require_command mktemp
require_command tar
require_command uname

ARCH=$(uname -m)
OS=$(uname -s)

# Normalize architecture
case "$ARCH" in
    x86_64)
        ARCH="x86_64"
        ;;
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
    Linux)
        PLATFORM="linux"
        ;;
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
VERSION="${ROOFER_VERSION:-1.0.0-beta.6}"
BINARY_URL="https://github.com/3DBAG/roofer/releases/download/v${VERSION}/roofer-${PLATFORM}-${ARCH}-v${VERSION}.tar.gz"
BIN_DIR="${ROOFER_BIN_DIR:-$HOME/.local/bin}"
SHARE_DIR="${ROOFER_SHARE_DIR:-$HOME/.local/share/roofer}"
INSTALL_BIN="$BIN_DIR/roofer"
TMP_DIR=$(mktemp -d)

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if [ -e "$INSTALL_BIN" ]; then
    echo "⚠️  $INSTALL_BIN already exists."
    if ! confirm_overwrite; then
        echo "Aborted."
        exit 1
    fi
fi

mkdir -p "$BIN_DIR"

echo "[*] Downloading binary..."
curl -fL --progress-bar "$BINARY_URL" -o "$TMP_DIR/roofer.tar.gz"
tar -xzf "$TMP_DIR/roofer.tar.gz" -C "$TMP_DIR"

if [ ! -f "$TMP_DIR/bin/roofer" ]; then
    echo "Downloaded archive does not contain bin/roofer"
    exit 1
fi

cp "$TMP_DIR/bin/roofer" "$TMP_DIR/roofer"
chmod +x "$TMP_DIR/roofer"
mv "$TMP_DIR/roofer" "$INSTALL_BIN"
chmod +x "$INSTALL_BIN"

if [ -d "$TMP_DIR/share" ]; then
    rm -rf "$TMP_DIR/share-install"
    mkdir -p "$(dirname "$SHARE_DIR")"
    cp -R "$TMP_DIR/share" "$TMP_DIR/share-install"
    rm -rf "$SHARE_DIR"
    mv "$TMP_DIR/share-install" "$SHARE_DIR"
fi

echo "[*] Installed to $INSTALL_BIN"

if "$INSTALL_BIN" --version >/dev/null 2>&1; then
    echo "[*] Verified installed binary"
else
    echo "⚠️  Installed binary did not pass a --version check."
    echo "    You may need to install missing runtime libraries or report this release."
fi

# Suggest adding to PATH
case ":$PATH:" in
  *:"$BIN_DIR":*)
    echo "[*] $BIN_DIR already in PATH"
    ;;
  *)
    echo
    echo "➤ Add the following to your shell profile (e.g., ~/.bashrc or ~/.zshrc):"
    echo "  export PATH=\"$BIN_DIR\":\$PATH"
    echo
    echo "➤ You can uninstall roofer using:"
    echo "  rm -f \"$INSTALL_BIN\""
    echo "  rm -rf \"$SHARE_DIR\""
    echo
    ;;
esac

echo "✅ Installation complete!"
