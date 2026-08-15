#!/bin/bash
# wubu_proton_ge_bundle.sh -- Proton-GE Bundle Manager for WuBuOS
#
# Downloads, verifies, and installs Proton-GE (GloriousEggroll's Proton build)
# for optimal Windows gaming compatibility on Linux.
#
# Usage:
#   ./wubu_proton_ge_bundle.sh install [version]   # Install specific or latest Proton-GE
#   ./wubu_proton_ge_bundle.sh list                # List available versions
#   ./wubu_proton_ge_bundle.sh remove [version]    # Remove installed version
#   ./wubu_proton_ge_bundle.sh current             # Show current version

set -euo pipefail

# Configuration
PROTON_BASE="${XDG_DATA_HOME:-$HOME/.local/share}/wuubu/proton"
PROTON_GE_URL="https://github.com/GloriousEggroll/proton-ge-custom/releases"
PROTON_GE_API="https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases"
CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/wuubu/proton-ge"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_err()   { echo -e "${RED}[ERR]${NC} $*"; }

# Ensure directories exist
mkdir -p "$PROTON_BASE/versions"
mkdir -p "$CACHE_DIR"

# Get latest release info from GitHub API
get_latest_release() {
    curl -s "$PROTON_GE_API/latest" | grep -o '"tag_name": "[^"]*"' | cut -d'"' -f4
}

# List available versions
list_versions() {
    log_info "Fetching available Proton-GE versions..."
    curl -s "$PROTON_GE_API" | grep -o '"tag_name": "[^"]*"' | cut -d'"' -f4 | head -20
}

# Get download URL for a specific version
get_download_url() {
    local version="$1"
    curl -s "$PROTON_GE_API/tags/$version" | grep -o '"browser_download_url": "[^"]*\.tar\.gz"' | cut -d'"' -f4 | head -1
}

# Verify checksum
verify_checksum() {
    local file="$1"
    local expected_sha256="$2"
    
    if [[ -z "$expected_sha256" ]]; then
        log_warn "No checksum provided, skipping verification"
        return 0
    fi
    
    local actual_sha256
    actual_sha256=$(sha256sum "$file" | cut -d' ' -f1)
    
    if [[ "$actual_sha256" == "$expected_sha256" ]]; then
        log_ok "Checksum verified"
        return 0
    else
        log_err "Checksum mismatch!"
        log_err "Expected: $expected_sha256"
        log_err "Got:      $actual_sha256"
        return 1
    fi
}

# Install Proton-GE version
install_proton_ge() {
    local version="${1:-$(get_latest_release)}"
    
    if [[ -z "$version" ]]; then
        log_err "Could not determine version to install"
        return 1
    fi
    
    log_info "Installing Proton-GE $version..."
    
    local target_dir="$PROTON_BASE/versions/$version"
    if [[ -d "$target_dir" ]]; then
        log_warn "Version $version already installed at $target_dir"
        return 0
    fi
    
    local download_url
    download_url=$(get_download_url "$version")
    
    if [[ -z "$download_url" ]]; then
        log_err "Could not find download URL for $version"
        return 1
    fi
    
    log_info "Downloading from $download_url..."
    
    local archive="$CACHE_DIR/${version}.tar.gz"
    if ! curl -L --fail --progress-bar -o "$archive" "$download_url"; then
        log_err "Download failed"
        return 1
    fi
    
    # Try to verify checksum if available
    local sha256_url="${download_url}.sha256"
    local expected_sha256=""
    if curl -sL --fail -o "/tmp/${version}.sha256" "$sha256_url" 2>/dev/null; then
        expected_sha256=$(cat "/tmp/${version}.sha256" | cut -d' ' -f1)
        log_info "Found checksum: $expected_sha256"
    fi
    
    if ! verify_checksum "$archive" "$expected_sha256"; then
        return 1
    fi
    
    log_info "Extracting..."
    mkdir -p "$target_dir"
    if ! tar -xzf "$archive" -C "$target_dir" --strip-components=1; then
        log_err "Extraction failed"
        rm -rf "$target_dir"
        return 1
    fi
    
    # Verify Proton structure
    if [[ ! -f "$target_dir/proton" ]]; then
        log_err "Invalid Proton structure (missing proton script)"
        rm -rf "$target_dir"
        return 1
    fi
    
    chmod +x "$target_dir/proton"
    
    # Create/update current symlink
    ln -sfn "$target_dir" "$PROTON_BASE/current"
    
    log_ok "Proton-GE $version installed successfully"
    log_info "Location: $target_dir"
    log_info "Current symlink: $PROTON_BASE/current -> $target_dir"
    
    return 0
}

# Remove installed version
remove_proton_ge() {
    local version="$1"
    
    if [[ -z "$version" ]]; then
        log_err "Version required"
        return 1
    fi
    
    local target_dir="$PROTON_BASE/versions/$version"
    if [[ ! -d "$target_dir" ]]; then
        log_err "Version $version not installed"
        return 1
    fi
    
    # Check if it's the current version
    local current_target
    current_target=$(readlink -f "$PROTON_BASE/current" 2>/dev/null || true)
    if [[ "$current_target" == "$target_dir" ]]; then
        log_warn "Removing current version, symlink will be updated"
        rm -f "$PROTON_BASE/current"
        # Point to another version if available
        local another
        another=$(ls -1 "$PROTON_BASE/versions" 2>/dev/null | head -1 || true)
        if [[ -n "$another" ]]; then
            ln -sfn "$PROTON_BASE/versions/$another" "$PROTON_BASE/current"
            log_info "Switched current to $another"
        fi
    fi
    
    rm -rf "$target_dir"
    log_ok "Removed Proton-GE $version"
    
    return 0
}

# Show current version
show_current() {
    local current_link="$PROTON_BASE/current"
    if [[ -L "$current_link" ]]; then
        local target
        target=$(readlink -f "$current_link")
        local version
        version=$(basename "$target")
        log_ok "Current Proton-GE: $version"
        echo "$version"
    else
        log_warn "No current Proton-GE version set"
        return 1
    fi
}

# Setup Proton environment
setup_environment() {
    local version="${1:-current}"
    local proton_dir
    
    if [[ "$version" == "current" ]]; then
        proton_dir="$PROTON_BASE/current"
        if [[ ! -L "$proton_dir" ]]; then
            log_err "No current Proton version set"
            return 1
        fi
    else
        proton_dir="$PROTON_BASE/versions/$version"
        if [[ ! -d "$proton_dir" ]]; then
            log_err "Version $version not installed"
            return 1
        fi
    fi
    
    export PROTON_PATH="$proton_dir"
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$proton_dir"
    export WINEPREFIX="${WINEPREFIX:-$HOME/.local/share/wuubu/proton/prefixes/default}"
    
    log_ok "Proton environment configured"
    log_info "PROTON_PATH=$PROTON_PATH"
    log_info "WINEPREFIX=$WINEPREFIX"
    
    # Ensure prefix exists
    mkdir -p "$WINEPREFIX"
    
    # Initialize prefix if needed
    if [[ ! -f "$WINEPREFIX/system.reg" ]]; then
        log_info "Initializing Wine prefix..."
        "$proton_dir/proton" run wineboot -u
    fi
}

# Run a Windows executable with Proton
run_windows_app() {
    setup_environment "$@" || return 1
    
    local proton_dir="$PROTON_BASE/current"
    local app_args=("${@:2}")
    
    if [[ $# -lt 2 ]]; then
        log_err "Usage: $0 run [version] <windows_exe> [args...]"
        return 1
    fi
    
    log_info "Running: ${app_args[*]}"
    exec "$proton_dir/proton" run "${app_args[@]}"
}

# Main command dispatcher
main() {
    local cmd="${1:-help}"
    shift || true
    
    case "$cmd" in
        install)
            install_proton_ge "$@"
            ;;
        list)
            list_versions
            ;;
        remove)
            remove_proton_ge "$@"
            ;;
        current)
            show_current
            ;;
        env)
            setup_environment "$@"
            ;;
        run)
            run_windows_app "$@"
            ;;
        help|--help|-h)
            cat <<EOF
WuBuOS Proton-GE Bundle Manager

Usage: $0 <command> [args]

Commands:
  install [version]     Install Proton-GE (latest if version omitted)
  list                  List available Proton-GE versions
  remove <version>      Remove installed Proton-GE version
  current               Show currently active version
  env [version]         Setup Proton environment variables
  run [version] <exe>   Run Windows application with Proton
  help                  Show this help

Environment:
  PROTON_BASE    Base directory for Proton versions (default: ~/.local/share/wuubu/proton)
  XDG_DATA_HOME  XDG data directory
  XDG_CACHE_HOME XDG cache directory

Examples:
  $0 install                    # Install latest Proton-GE
  $0 install GE-Proton9-20      # Install specific version
  $0 list                       # List available versions
  $0 current                    # Show active version
  $0 env                        # Setup environment for current
  $0 env GE-Proton9-20          # Setup environment for specific version
  $0 run game.exe               # Run game with current Proton
  $0 run GE-Proton9-20 game.exe # Run game with specific version

EOF
            ;;
        *)
            log_err "Unknown command: $cmd"
            main help
            exit 1
            ;;
    esac
}

main "$@"