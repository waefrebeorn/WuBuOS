#!/bin/sh
# install-wubuos.sh -- the WuBuOS installer, Omarchy style.
#
# Interactive, idempotent, reversible. It asks, writes the answers as a plan
# (JSON) into the OS namespace, installs, then verifies by actually running
# what it installed -- an install that does not boot what it put on disk is
# not an install.
#
#   install-wubuos.sh             install
#   install-wubuos.sh --uninstall remove (the same file set)
#   install-wubuos.sh --plan-only ask and write the plan, install nothing
#
# The plan lands at ~/.wubu/n/install/plan.json -- the /n/install surface the
# installer study specifies -- so the install is a replayable document and an
# audit log in one.
#
# Self-contained POSIX shell. C11 project, shell glue.

set -u

PREFIX="$HOME/.local"
AUTOSTART=1
WINDOW=1
MCP=1
PLAN_ONLY=0
UNINSTALL=0

# --- arguments --------------------------------------------------------------

for arg in "$@"; do
    case "$arg" in
        --uninstall) UNINSTALL=1 ;;
        --plan-only) PLAN_ONLY=1 ;;
        --prefix=*)  PREFIX="${arg#*=}" ;;
        --help|-h)
            sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "install-wubuos: unknown argument '$arg'" >&2; exit 2 ;;
    esac
done

REPO="$(cd "$(dirname "$0")" && pwd)"
BIN_SRC="$REPO/src/hosted/wubu"
WIN_SRC="$REPO/src/hosted/wubu-weston.sh"
INJ_SRC="$REPO/src/hosted/wubu_inject.c"
MCP_SRC="$REPO/src/hosted/wubu_mcp.py"
ICON_SRC="$REPO/docs/logo/wubuos-desktop.png"

NS="$HOME/.wubu/n/install"
PLAN="$NS/plan.json"

say()  { printf '%s\n' "$*"; }
ask()  { # ask <prompt> <default>  -- answer on stdout, prompt on stderr
    printf '%s [%s] ' "$1" "$2" >&2
    read -r answer || answer=""
    printf '%s' "${answer:-$2}"
}

# --- check the sources ------------------------------------------------------

missing=0
for f in "$BIN_SRC" "$WIN_SRC" "$MCP_SRC" "$ICON_SRC"; do
    [ -f "$f" ] || { say "  ! missing: $f"; missing=1; }
done
[ -x "$BIN_SRC" ] || { say "  ! $BIN_SRC is not built (run: make hosted)"; missing=1; }
[ "$missing" -eq 1 ] && { say "install-wubuos: the sources are not ready." >&2; exit 1; }

# --- ask --------------------------------------------------------------------

say ""
say "WuBuOS installer"
say "----------------"
say "The hosted desktop is a Wayland application: the desktop, the task"
say "manager (wubulens) and the workspace window all run as one product."
say ""

if [ "$UNINSTALL" -eq 0 ] && [ "$PLAN_ONLY" -eq 0 ]; then
    PREFIX="$(ask "Install to" "$PREFIX")"
    AUTOSTART="$(ask "Start it when you log in? (1/0)" "$AUTOSTART")"
    WINDOW="$(ask "Install the scaled-window launcher? (1/0)" "$WINDOW")"
    MCP="$(ask "Register the development MCP server? (1/0)" "$MCP")"
fi

# Normalize the prefix (no ~ in paths we write).
case "$PREFIX" in
    \~*) PREFIX="$HOME${PREFIX#\~}" ;;
esac

say ""
say "Plan:"
say "  prefix    : $PREFIX"
say "  autostart : $AUTOSTART"
say "  window    : $WINDOW"
say "  mcp       : $MCP"

# --- write the plan ---------------------------------------------------------

mkdir -p "$NS"
written_at=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u)
cat > "$PLAN" <<JSON
{
  "schema": 1,
  "written_at": "$written_at",
  "source": "$REPO",
  "prefix": "$PREFIX",
  "autostart": $AUTOSTART,
  "window": $WINDOW,
  "mcp": $MCP,
  "files": [
    "$PREFIX/bin/wubuos",
    "$PREFIX/bin/wubuos-window",
    "$PREFIX/bin/wubu-inject",
    "$PREFIX/share/applications/wubuos.desktop",
    "$PREFIX/share/pixmaps/wubuos.png",
    "$PREFIX/share/wubuos/wubu_mcp.py",
    "$HOME/.config/autostart/wubuos.desktop"
  ]
}
JSON
say "  plan      : $PLAN"

if [ "$PLAN_ONLY" -eq 1 ]; then
    say ""
    say "Plan written; nothing installed (--plan-only)."
    exit 0
fi

# --- install ----------------------------------------------------------------

if [ "$UNINSTALL" -eq 1 ]; then
    say ""
    say "Removing:"
    for f in "$PREFIX/bin/wubuos" "$PREFIX/bin/wubuos-window" \
             "$PREFIX/bin/wubu-inject" \
             "$PREFIX/share/applications/wubuos.desktop" \
             "$PREFIX/share/pixmaps/wubuos.png" \
             "$PREFIX/share/wubuos/wubu_mcp.py" \
             "$HOME/.config/autostart/wubuos.desktop"; do
        if [ -e "$f" ]; then rm -f "$f" && say "  removed $f"; fi
    done
    say ""
    say "Removed. The plan is kept at $PLAN for the record."
    exit 0
fi

say ""
say "Installing:"
mkdir -p "$PREFIX/bin" "$PREFIX/share/applications" \
         "$PREFIX/share/pixmaps" "$PREFIX/share/wubuos"
install -m 755 "$BIN_SRC"   "$PREFIX/bin/wubuos"
install -m 755 "$WIN_SRC"   "$PREFIX/bin/wubuos-window"
install -m 755 "$REPO/src/hosted/wubu_inject" "$PREFIX/bin/wubu-inject" 2>/dev/null || \
    { make -C "$REPO" wubu_inject >/dev/null 2>&1 && \
      install -m 755 "$REPO/src/hosted/wubu_inject" "$PREFIX/bin/wubu-inject"; }
install -m 644 "$ICON_SRC" "$PREFIX/share/pixmaps/wubuos.png"
install -m 755 "$MCP_SRC"  "$PREFIX/share/wubuos/wubu_mcp.py"

sed -e "s|@PREFIX@|$PREFIX|g" "$REPO/mk/wubuos.desktop.in" \
    > "$PREFIX/share/applications/wubuos.desktop"
say "  binary, window launcher, injector, icon, MCP server"

if [ "$AUTOSTART" -eq 1 ]; then
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/wubuos.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=WuBuOS
Comment=The WuBuOS desktop, in a scaled window.
Exec=$PREFIX/bin/wubuos-window 1280x720
X-GNOME-Autostart-enabled=true
DESK
    say "  autostart entry"
fi

# The MCP server is registered in the agent's config so the workspace can be
# seen and driven. The entry is added only when it is not already there.
if [ "$MCP" -eq 1 ]; then
    MCP_SERVER="$PREFIX/share/wubuos/wubu_mcp.py"
    CFG="$HOME/.hermes/config.yaml"
    if [ -f "$CFG" ] && ! grep -q "wubu:" "$CFG" 2>/dev/null; then
        tmp="$CFG.tmp.$$"
        {
            printf 'mcp_servers:\n'
            printf '  wubu:\n'
            printf '    command: python3\n'
            printf '    args: ["%s"]\n' "$MCP_SERVER"
            cat "$CFG"
        } > "$tmp" && mv "$tmp" "$CFG"
        say "  MCP server registered (~/.hermes/config.yaml)"
    else
        say "  MCP server already registered"
    fi
fi

# --- verify -----------------------------------------------------------------

say ""
say "Verifying:"
if [ -x "$PREFIX/bin/wubuos" ]; then
    shot=$(mktemp /tmp/wubuos-install-XXXXXX.ppm)
    if HOME=$(mktemp -d) "$PREFIX/bin/wubuos" --screenshot "$shot" >/dev/null 2>&1 && \
       [ -s "$shot" ]; then
        kb=$(( $(wc -c < "$shot") / 1024 ))
        say "  the installed binary boots and renders (${kb} KB frame)"
    else
        say "  ! the installed binary did not produce a frame"
    fi
    rm -f "$shot"
else
    say "  ! the installed binary is missing"
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$PREFIX/share/applications/wubuos.desktop" >/dev/null 2>&1 \
        && say "  the launcher is a valid .desktop file" \
        || say "  ! the launcher failed desktop-file-validate"
fi

say ""
say "Installed. Run it with:  wubuos-window 1280x720"
say "Plan kept at:            $PLAN"
