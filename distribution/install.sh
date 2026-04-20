#!/bin/sh

set -e  # Exit on any error

confirm_overwrite() {
    if [ "${ROOFER_INSTALL_FORCE:-}" = "1" ]; then
        return 0
    fi

    if [ ! -r /dev/tty ]; then
        echo "An existing roofer installation was found."
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

shell_quote() {
    printf "'"
    printf "%s" "$1" | sed "s/'/'\\\\''/g"
    printf "'"
}

write_launcher() {
    {
        echo "#!/bin/sh"
        echo
        printf "ROOFER_PREFIX=%s\n" "$(shell_quote "$INSTALL_DIR")"
        echo
        echo 'if [ -d "$ROOFER_PREFIX/share/gdal" ]; then'
        echo '    export GDAL_DATA="$ROOFER_PREFIX/share/gdal"'
        echo "fi"
        echo
        echo 'if [ -d "$ROOFER_PREFIX/share/proj" ]; then'
        echo '    export PROJ_DATA="$ROOFER_PREFIX/share/proj"'
        echo "fi"
        echo
        echo 'exec "$ROOFER_PREFIX/bin/roofer" "$@"'
    } > "$INSTALL_BIN"
    chmod +x "$INSTALL_BIN"
}

echo "[*] Detecting platform..."

require_command curl
require_command mktemp
require_command sed
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
INSTALL_DIR="${ROOFER_INSTALL_DIR:-${ROOFER_SHARE_DIR:-$HOME/.local/share/roofer}}"
INSTALL_BIN="$BIN_DIR/roofer"
TMP_DIR=$(mktemp -d)

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if [ -e "$INSTALL_BIN" ] || [ -e "$INSTALL_DIR" ]; then
    echo "⚠️  Existing roofer installation found."
    if [ -e "$INSTALL_BIN" ]; then
        echo "    Launcher: $INSTALL_BIN"
    fi
    if [ -e "$INSTALL_DIR" ]; then
        echo "    Bundle:   $INSTALL_DIR"
    fi
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

rm -rf "$TMP_DIR/prefix"
mkdir -p "$TMP_DIR/prefix"
cp -R "$TMP_DIR/bin" "$TMP_DIR/prefix/bin"
if [ -d "$TMP_DIR/lib" ]; then
    cp -R "$TMP_DIR/lib" "$TMP_DIR/prefix/lib"
fi
if [ -d "$TMP_DIR/share" ]; then
    cp -R "$TMP_DIR/share" "$TMP_DIR/prefix/share"
fi
chmod +x "$TMP_DIR/prefix/bin/roofer"

mkdir -p "$(dirname "$INSTALL_DIR")"
rm -rf "$INSTALL_DIR"
mv "$TMP_DIR/prefix" "$INSTALL_DIR"

write_launcher

echo "[*] Installed bundle to $INSTALL_DIR"
echo "[*] Installed launcher to $INSTALL_BIN"

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
    echo "  rm -rf \"$INSTALL_DIR\""
    echo
    ;;
esac

echo "✅ Installation complete!"
