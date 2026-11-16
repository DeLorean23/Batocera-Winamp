#!/bin/bash
# Winamp Installer for Batocera
# Installs Winamp 5.8 as its own system "Winamp" (Milkdrop, skins, wallpaper,
# mouse pointer and controller support) and the DeLorean Logo-Fix, which shows
# the system logo with every theme. Can also uninstall everything again.
# A fun project for nostalgia - Winamp like in the Windows XP days.
# Created by DeLorean23
# https://github.com/DeLorean23/Batocera-Winamp
# License: MIT (the installer and DeLorean23 files only)

set -uo pipefail

INSTALLER_VERSION="2.0"

SELF="$(readlink -f "$0")"
SELF_DIR="$(dirname "$SELF")"
SELF_NAME="$(basename "$SELF")"
SELF_STEM="${SELF_NAME%.*}"
DATA="$SELF_DIR/.winamp-installer"
WORK=/userdata/system/.winamp-install-tmp

ROM_DIR=/userdata/roms/winamp
APP_DIR="$ROM_DIR/Winamp.pc"
WALL_DIR="$ROM_DIR/wallpaper"
MUSIC_DIR=/userdata/roms/musik
SETTINGS_DIR=/userdata/system/configs/winamp
CFG=/userdata/system/configs/emulationstation/es_systems_winamp.cfg
BOTTLES=/userdata/system/wine-bottles/windows
LOGO_DIR=/userdata/system/delorean-logos
SERVICE_DIR=/userdata/system/services
SERVICE="$SERVICE_DIR/delorean_logos"
LOG_DIR=/userdata/system/logs
LOG="$LOG_DIR/winamp-installer.log"

# Downloads from the original sources (only when the installer has no
# bundled files). Checksums make sure the right files arrive.
URL_7Z="https://github.com/ip7z/7zip/releases/download/24.09/7z2409-linux-x64.tar.xz"
SHA_7Z="914c7e20ad5ef8e4d3cf08620ff8894b28fe11b7eb99809d6930870fbe48a281"
URL_WINAMP="https://download.nullsoft.com/winamp/client/winamp58_3660_beta_full_en-us.exe"
SHA_WINAMP="115ad6a4be58320c98f992b1dae26e162976158c3cced251584a2b78f059b822"
URL_DX="https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe"
SHA_DX="053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b"
URL_SKINS="https://raw.githubusercontent.com/captbaritone/webamp/7e73a9b602774a5648a5ff32261efec75307e654/packages"
SKINS="webamp-demo/skins/AmigaPPC-dark.wsz webamp-demo/skins/Green-Dimension-V2.wsz webamp-demo/skins/Internet-Archive.wsz
webamp-demo/skins/MacOSXAqua1-5.wsz webamp-demo/skins/Skinner_Atlas.wsz webamp-demo/skins/TopazAmp1-2.wsz
webamp-demo/skins/Vizor1-01.wsz webamp-demo/skins/XMMS-Turquoise.wsz webamp-demo/skins/ZaxonRemake1-0.wsz
webamp-demo/skins/base-2.91.wsz webamp-modern/assets/skins/Paper-Amp.wsz webamp-modern/assets/skins/CornerAmp_Redux.wal
webamp-modern/assets/skins/MMD3.wal"
URL_PRESETS="https://codeload.github.com/projectM-visualizer/presets-cream-of-the-crop/zip/0180df21f5e0bd39b9060cc5de420ed2f1f9e509"

# ---------------------------------------------------------------------------
# Open a visible terminal window (Ports scripts run without one by default)
# ---------------------------------------------------------------------------
if [[ "${1:-}" != "--in-terminal" ]]; then
    mkdir -p "$LOG_DIR"
    export DISPLAY="${DISPLAY:-:0.0}"
    if command -v xterm >/dev/null 2>&1; then
        exec xterm -fullscreen -fa "Monospace" -fs 11 -bg black -fg white \
            -T "Winamp Installer" -e /bin/bash "$SELF" --in-terminal
    fi
    echo "xterm not found - cannot open a terminal window." >> "$LOG"
    exit 1
fi

mkdir -p "$LOG_DIR"
{ exec 3>/dev/tty; } 2>/dev/null || exec 3>&1     # fd 3: screen only (progress bars)
exec > >(tee -a "$LOG") 2>&1
echo "===== $(date) - Winamp Installer $INSTALLER_VERSION ====="

# ---------------------------------------------------------------------------
# Screen
# ---------------------------------------------------------------------------
show_banner() {
cat <<'BANNER'
@@BANNER@@
BANNER
printf '\n'
}

log()  { echo "[winamp] $*"; }
warn() { echo "[winamp] WARNING: $*"; }

STEP=0; STEPS=1
step() {   # step "<title>"   -> "[3/8] title"
    STEP=$((STEP + 1))
    echo
    echo "[$STEP/$STEPS] $*"
}

bar() {    # bar <percent> [text]   (drawn on the screen only, not in the log)
    local p="${1:-0}" text="${2:-}" w=40 n
    (( p < 0 )) && p=0; (( p > 100 )) && p=100
    n=$((p * w / 100))
    printf '\r  [%s%s] %3d %%  %-34.34s' "$(printf '%*s' "$n" '' | tr ' ' '#')" \
        "$(printf '%*s' "$((w - n))" '' | tr ' ' '-')" "$p" "$text" >&3
    (( p >= 100 )) && printf '\n' >&3
    return 0
}

# ---------------------------------------------------------------------------
# Controller / keyboard input
# ---------------------------------------------------------------------------
write_pad_helper() {   # prints one of: up down a b   (then exits)
cat > "$1" <<'PY'
import sys, select
try:
    from evdev import InputDevice, list_devices, ecodes as e
except Exception:
    sys.exit(0)
devs = []
for p in list_devices():
    try:
        d = InputDevice(p)
        keys = set(d.capabilities().get(e.EV_KEY, []))
        if e.BTN_SOUTH in keys and e.BTN_EAST in keys:
            devs.append(d)
        else:
            d.close()
    except Exception:
        pass
if not devs:
    sys.exit(0)
fds = {d.fd: d for d in devs}
def out(s):
    print(s, flush=True); sys.exit(0)
while True:
    r, _, _ = select.select(list(fds), [], [], 1.0)
    for fd in r:
        try:
            for ev in fds[fd].read():
                if ev.type == e.EV_KEY and ev.value == 1:
                    if ev.code == e.BTN_SOUTH: out("a")
                    if ev.code == e.BTN_EAST: out("b")
                    if ev.code == getattr(e, "BTN_DPAD_UP", -1): out("up")
                    if ev.code == getattr(e, "BTN_DPAD_DOWN", -1): out("down")
                elif ev.type == e.EV_ABS:
                    if ev.code == e.ABS_HAT0Y and ev.value != 0:
                        out("up" if ev.value < 0 else "down")
                    if ev.code == e.ABS_Y:
                        info = fds[fd].absinfo(e.ABS_Y)
                        mid = (info.max + info.min) / 2.0
                        rng = (info.max - info.min) / 2.0 or 1
                        v = (ev.value - mid) / rng
                        if v < -0.7: out("up")
                        if v > 0.7: out("down")
        except OSError:
            fds.pop(fd, None)
    if not fds:
        sys.exit(0)
PY
}

read_input() {   # waits for one input: up / down / a / b
    local key rest pad_pid helper out res=""
    helper="$(mktemp /tmp/winamp-pad-XXXX.py)"
    out="$(mktemp /tmp/winamp-pad-XXXX.txt)"
    write_pad_helper "$helper"
    python3 "$helper" > "$out" 2>/dev/null &
    pad_pid=$!
    while [[ -z "$res" ]]; do
        if IFS= read -rsn1 -t 0.2 key; then
            case "$key" in
                $'\e') read -rsn2 -t 0.05 rest
                       case "$rest" in "[A") res="up" ;; "[B") res="down" ;; esac ;;
                "") res="a" ;;
                y|Y) res="a" ;;
                n|N|q|Q) res="b" ;;
                w|W|k) res="up" ;;
                s|S|j) res="down" ;;
            esac
        fi
        if [[ -z "$res" && -s "$out" ]]; then res="$(head -n1 "$out")"; fi
    done
    kill "$pad_pid" 2>/dev/null; wait "$pad_pid" 2>/dev/null
    rm -f "$helper" "$out"
    sleep 0.15
    echo "$res"
}

ask_yes_no() {  # returns 0 for yes, 1 for no
    local prompt="$1" in
    printf '%s [y/n] (Controller: A = yes, B = no): ' "$prompt"
    while true; do
        in="$(read_input)"
        case "$in" in
            a) echo "y"; return 0 ;;
            b) echo "n"; return 1 ;;
        esac
    done
}

wait_key() {
    local in
    printf 'Press A / Enter to exit '
    while true; do
        in="$(read_input)"
        [[ "$in" == "a" || "$in" == "b" ]] && break
    done
    echo
}

MENU_CHOICE=0
menu() {   # menu "<title>" option1 option2 ...  -> MENU_CHOICE (0-based)
    local title="$1"; shift
    local opts=("$@") sel=0 i in
    while true; do
        clear >&3
        show_banner >&3
        printf '%s\n\n' "$title" >&3
        for i in "${!opts[@]}"; do
            if (( i == sel )); then printf '   > %s\n' "${opts[$i]}" >&3
            else printf '     %s\n' "${opts[$i]}" >&3; fi
        done
        printf '\n  D-pad / arrow keys = choose,  A / Enter = OK\n' >&3
        in="$(read_input)"
        case "$in" in
            up)   sel=$(( (sel - 1 + ${#opts[@]}) % ${#opts[@]} )) ;;
            down) sel=$(( (sel + 1) % ${#opts[@]} )) ;;
            a)    MENU_CHOICE=$sel; echo "menu: ${opts[$sel]}"; return 0 ;;
            b)    MENU_CHOICE=$(( ${#opts[@]} - 1 )); return 0 ;;   # B = last entry (Exit)
        esac
    done
}

fail() {
    echo
    echo "[ERROR] $*"
    echo "Log: $LOG"
    rm -rf "$WORK"
    echo
    wait_key
    exit 1
}

# ---------------------------------------------------------------------------
# Python helpers with progress bars (downloads, copying, unpacking)
# ---------------------------------------------------------------------------
PYTOOLS=/tmp/winamp-tools.py
cat > "$PYTOOLS" <<'PY'
import hashlib, os, re, shutil, subprocess, sys, time, urllib.request, zipfile

try:
    TTY = open("/dev/tty", "w")
except OSError:
    TTY = sys.stdout

def bar(p, text=""):
    p = max(0, min(100, int(p)))
    n = p * 40 // 100
    TTY.write("\r  [%s%s] %3d %%  %-34.34s" % ("#" * n, "-" * (40 - n), p, text))
    if p >= 100:
        TTY.write("\n")
    TTY.flush()

def mb(n):
    return "%.1f MB" % (n / 1048576.0)

def download(url, out, sha=""):
    tmp = out + ".part"
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 Batocera-Winamp-Installer"})
    ok = False
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=60) as r, open(tmp, "wb") as f:
                total = int(r.headers.get("Content-Length") or 0)
                done = 0
                last = 0
                while True:
                    chunk = r.read(256 * 1024)
                    if not chunk:
                        break
                    f.write(chunk)
                    done += len(chunk)
                    if time.time() - last > 0.2:
                        last = time.time()
                        if total:
                            bar(done * 100 // total, "%s / %s" % (mb(done), mb(total)))
                        else:
                            bar(0, mb(done))
            ok = True
            break
        except Exception as ex:
            print("download error (%s): %s" % (url, ex))
            time.sleep(2)
    if not ok:
        # last try: curl
        if subprocess.call(["curl", "-fsSL", "--retry", "3", "-o", tmp, url]) != 0:
            return 1
    if sha:
        h = hashlib.sha256()
        with open(tmp, "rb") as f:
            for b in iter(lambda: f.read(1 << 20), b""):
                h.update(b)
        if h.hexdigest() != sha:
            print("checksum mismatch for %s" % url)
            os.remove(tmp)
            return 2
    os.replace(tmp, out)
    bar(100, mb(os.path.getsize(out)))
    return 0

def copytree(src, dst):
    files = []
    for root, dirs, fs in os.walk(src):
        for f in fs:
            files.append(os.path.join(root, f))
    total = len(files) or 1
    last = 0
    for i, s in enumerate(files, 1):
        d = os.path.join(dst, os.path.relpath(s, src))
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy2(s, d)
        if time.time() - last > 0.1 or i == total:
            last = time.time()
            bar(i * 100 // total, "%d / %d files" % (i, total))
    bar(100, "%d files" % total)
    return 0

def unzip_milk(zf, dst):
    os.makedirs(dst, exist_ok=True)
    try:
        z = zipfile.ZipFile(zf)
    except zipfile.BadZipFile:
        return 1
    with z:
        names = [n for n in z.namelist() if n.lower().endswith(".milk") and "/! transition/" not in n.lower()]
        total = len(names) or 1
        last = 0
        for i, n in enumerate(names, 1):
            target = os.path.join(dst, os.path.basename(n))
            if not os.path.exists(target):
                with z.open(n) as s, open(target, "wb") as d:
                    shutil.copyfileobj(s, d)
            if time.time() - last > 0.1 or i == total:
                last = time.time()
                bar(i * 100 // total, "%d / %d presets" % (i, total))
    bar(100, "%d presets" % total)
    return 0 if names else 1

def run_7z(sevenzip, args):
    # runs 7-Zip and shows its percentage as a bar
    p = subprocess.Popen([sevenzip] + args + ["-bsp1", "-bso0", "-y"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    buf = b""
    while True:
        c = p.stdout.read(64)
        if not c:
            break
        buf = (buf + c)[-200:]
        m = re.findall(rb"(\d+)%", buf)
        if m:
            bar(int(m[-1]), "unpacking")
    rc = p.wait()
    if rc == 0:
        bar(100, "done")
    return rc

def rmtree_progress(paths):
    items = []
    for p in paths:
        if os.path.isdir(p) and not os.path.islink(p):
            for root, dirs, fs in os.walk(p, topdown=False):
                items += [os.path.join(root, f) for f in fs] + [os.path.join(root, d) for d in dirs]
            items.append(p)
        elif os.path.lexists(p):
            items.append(p)
    total = len(items) or 1
    last = 0
    for i, it in enumerate(items, 1):
        try:
            if os.path.isdir(it) and not os.path.islink(it):
                os.rmdir(it)
            else:
                os.remove(it)
        except OSError:
            pass
        if time.time() - last > 0.1 or i == total:
            last = time.time()
            bar(i * 100 // total, "%d / %d" % (i, total))
    bar(100, "removed")
    return 0

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "download":
        sys.exit(download(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else ""))
    if cmd == "copytree":
        sys.exit(copytree(sys.argv[2], sys.argv[3]))
    if cmd == "unzip_milk":
        sys.exit(unzip_milk(sys.argv[2], sys.argv[3]))
    if cmd == "7z":
        sys.exit(run_7z(sys.argv[2], sys.argv[3:]))
    if cmd == "rm":
        sys.exit(rmtree_progress(sys.argv[2:]))
PY
pytool() { python3 "$PYTOOLS" "$@"; }

# ---------------------------------------------------------------------------
# Game lists
# ---------------------------------------------------------------------------
set_gamelist() {   # set_gamelist <gamelist.xml> <path> <name> <image> [desc]
python3 - "$@" <<'PY'
import os, sys
import xml.etree.ElementTree as ET
gl, path, name, image = sys.argv[1:5]
desc = sys.argv[5] if len(sys.argv) > 5 else None
root = None
if os.path.exists(gl):
    try:
        tree = ET.parse(gl); root = tree.getroot()
    except ET.ParseError:
        root = None
if root is None:
    root = ET.Element("gameList"); tree = ET.ElementTree(root)
game = None
for g in root.findall("game"):
    if os.path.basename(g.findtext("path") or "") == os.path.basename(path):
        game = g; break
if game is None:
    game = ET.SubElement(root, "game")
    ET.SubElement(game, "path").text = path
vals = [("name", name), ("image", image), ("thumbnail", image)]
if desc:
    vals.append(("desc", desc))
for tag, val in vals:
    el = game.find(tag)
    if el is None:
        el = ET.SubElement(game, tag)
    el.text = val
tree.write(gl, encoding="utf-8", xml_declaration=True)
PY
}

remove_from_gamelist() {  # remove_from_gamelist <gamelist.xml> <file name>
python3 - "$1" "$2" <<'PY'
import os, sys
import xml.etree.ElementTree as ET
gl, name = sys.argv[1:3]
if not os.path.exists(gl):
    sys.exit(0)
tree = ET.parse(gl); root = tree.getroot()
for g in list(root.findall("game")):
    if os.path.basename(g.findtext("path") or "") == name:
        root.remove(g)
tree.write(gl, encoding="utf-8", xml_declaration=True)
PY
}

# ---------------------------------------------------------------------------
# Wine: find it exactly like Batocera's batocera-wine does
# ---------------------------------------------------------------------------
WINE=""; WINESERVER=""; WINE_RUNNER_NAME=""
find_wine() {
    local runner dir lib32 lib64
    runner="$(batocera-settings-get 'windows["Winamp.pc"].wine-runner' 2>/dev/null)"
    [[ -z "$runner" ]] && runner="$(batocera-settings-get windows.wine-runner 2>/dev/null)"
    [[ -z "$runner" ]] && runner="$(batocera-settings-get windows.core 2>/dev/null)"
    [[ -z "$runner" ]] && runner="wine-tkg"
    [[ "$runner" == "lutris" ]] && runner="wine-tkg"
    [[ "$runner" == "proton" ]] && runner="wine-proton"
    if [[ ! "$runner" =~ ^wine-(tkg|proton)$ ]]; then
        if [[ -e "/userdata/system/wine/custom/$runner" ]]; then dir=/userdata/system/wine/custom
        else runner=wine-tkg; dir=/usr/wine; fi
    else
        dir=/usr/wine
    fi
    [[ -e "$dir/$runner" ]] || return 1
    if [[ -e "$dir/$runner/lib/wine/i386-unix/wine" ]]; then
        WINE="$dir/$runner/lib/wine/i386-unix/wine"
    else
        WINE="$dir/$runner/bin/wine"
    fi
    WINESERVER="$dir/$runner/bin/wineserver"
    lib64="$dir/$runner/lib/wine"; [[ -e "$dir/$runner/lib64/wine" ]] && lib64="$dir/$runner/lib64/wine"
    lib32="$dir/$runner/lib/wine"; [[ -e "$dir/$runner/lib32/wine" ]] && lib32="$dir/$runner/lib32/wine"
    export LD_LIBRARY_PATH="/lib32:${lib32}/i386-unix:/lib:/usr/lib:${lib64}/x86_64-unix"
    export WINEDLLPATH="${lib32}/i386-windows:${lib64}/x86_64-windows"
    export LIBGL_DRIVERS_PATH="/lib32/dri:/usr/lib/dri"
    export PATH="$PATH:$dir/$runner/bin"
    WINE_RUNNER_NAME="$runner"
    [[ -e "$WINE" && -e "$WINESERVER" ]]
}

# Prepare Winamp's Wine folder now, without the Wine Mono / Gecko download
# windows ("Install / Cancel") that would otherwise pop up on the first start.
prepare_wine() {
    local pfx pid t est=90 p
    find_wine || { warn "Wine runner not found"; return 1; }
    pfx="$BOTTLES/$WINE_RUNNER_NAME/Winamp.pc.wine"
    log "Wine runner: $WINE_RUNNER_NAME ($WINE)"
    log "Wine folder: $pfx"
    export WINEDEBUG=-all
    if [[ ! -e "$pfx/system.reg" ]]; then
        rm -rf "$pfx"; mkdir -p "$pfx" || return 1
        ( WINEPREFIX="$pfx" WINEDLLOVERRIDES="mscoree,mshtml=;winegstreamer=" timeout 600 "$WINE" wineboot -u >> "$LOG" 2>&1
          WINEPREFIX="$pfx" timeout 300 "$WINESERVER" -w ) &
        pid=$!
        t=0
        while kill -0 "$pid" 2>/dev/null; do
            p=$(( t * 95 / est )); (( p > 95 )) && p=95
            bar "$p" "setting up Wine ... ${t}s"
            sleep 1; t=$((t + 1))
        done
        wait "$pid"
        if [[ ! -e "$pfx/system.reg" ]]; then
            printf '\n' >&3
            warn "Wine folder could not be created"
            rm -rf "$pfx"
            return 1
        fi
    else
        log "Wine folder already exists."
    fi
    # Mono / Gecko are not needed by Winamp
    WINEPREFIX="$pfx" "$WINE" reg add 'HKCU\Software\Wine\DllOverrides' /v '*mscoree' /t REG_SZ /d native /f >> "$LOG" 2>&1
    WINEPREFIX="$pfx" "$WINE" reg add 'HKCU\Software\Wine\DllOverrides' /v '*mshtml' /t REG_SZ /d native /f >> "$LOG" 2>&1
    WINEPREFIX="$pfx" "$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /t REG_SZ /d "0 0 0" /f >> "$LOG" 2>&1
    WINEPREFIX="$pfx" timeout 120 "$WINESERVER" -w
    bar 100 "Wine is ready"
    return 0
}

# ---------------------------------------------------------------------------
# Get the Winamp files: bundled (offline package) or from the original sources
# ---------------------------------------------------------------------------
SRC_APP=""   # folder with the finished Winamp.pc (third-party files)
get_winamp_files() {
    if [[ -f "$DATA/payload/Winamp.pc/winamp.exe" ]]; then
        SRC_APP="$DATA/payload/Winamp.pc"
        return 0
    fi
    local sz w="$WORK" f n i ok
    rm -rf "$w"; mkdir -p "$w/dl" "$w/Winamp.pc" || fail "Cannot create $w"

    step "Downloading 7-Zip (to unpack the Winamp installer)"
    pytool download "$URL_7Z" "$w/dl/7z.tar.xz" "$SHA_7Z" || fail "Download of 7-Zip failed. Check the internet connection."
    mkdir -p "$w/7z" && tar -xJf "$w/dl/7z.tar.xz" -C "$w/7z" || fail "Could not unpack 7-Zip"
    sz="$w/7z/7zz"; chmod +x "$sz"

    step "Downloading Winamp 5.8 (from Nullsoft)"
    pytool download "$URL_WINAMP" "$w/dl/winamp58.exe" "$SHA_WINAMP" || fail "Download of Winamp failed. Check the internet connection."

    step "Downloading DirectX files for Milkdrop (from Microsoft, ~95 MB)"
    pytool download "$URL_DX" "$w/dl/directx.exe" "$SHA_DX" || fail "Download of the DirectX files failed. Check the internet connection."

    step "Downloading extra skins"
    mkdir -p "$w/skins"
    n=0; ok=0; i=0
    for f in $SKINS; do n=$((n + 1)); done
    for f in $SKINS; do
        i=$((i + 1))
        bar $(( (i - 1) * 100 / n )) "$(basename "$f")"
        if curl -fsSL --retry 2 -o "$w/skins/$(basename "$f")" "$URL_SKINS/$f" >> "$LOG" 2>&1; then
            ok=$((ok + 1))
        else
            rm -f "$w/skins/$(basename "$f")"
        fi
    done
    bar 100 "$ok of $n skins"
    (( ok < n )) && warn "$((n - ok)) skin(s) could not be downloaded"

    step "Downloading Milkdrop presets (Cream of the Crop, ~10,000 presets)"
    if ! pytool download "$URL_PRESETS" "$w/dl/presets.zip"; then
        warn "Preset pack could not be downloaded - only the Winamp presets are installed"
        rm -f "$w/dl/presets.zip"
    fi

    step "Unpacking"
    echo "  Winamp:"
    pytool 7z "$sz" x "$w/dl/winamp58.exe" "-o$w/nsis" || fail "Could not unpack the Winamp installer"
    echo "  DirectX:"
    pytool 7z "$sz" e "$w/dl/directx.exe" "*d3dx9_31_x86*" "*d3dx9_36_x86*" "-o$w/dx" || fail "Could not unpack the DirectX files"
    for f in "$w/dx/"*.cab; do "$sz" e -y -bso0 -bsp0 "$f" "-o$w/dx" >> "$LOG" 2>&1; done
    [[ -f "$w/dx/d3dx9_31.dll" && -f "$w/dx/d3dx9_36.dll" ]] || fail "DirectX files missing after unpacking"

    # put the Winamp folder together (same layout as a normal installation)
    local a="$w/Winamp.pc" s="$w/nsis"
    for f in winamp.exe winampa.exe elevator.exe whatsnew.txt Plugins System Shared Components Microsoft.VC90.CRT; do
        [[ -e "$s/$f" ]] && cp -a "$s/$f" "$a/"
    done
    [[ -f "$a/winamp.exe" ]] || fail "winamp.exe not found in the Winamp installer"
    rm -f "$a/Plugins/\$0" "$a/Plugins/\$R4" "$a/Plugins/gen_crasher.dll" "$a/reporter.exe"
    mkdir -p "$a/Skins" "$a/Plugins/Milkdrop2"
    cp -a "$s/\$R0/." "$a/Skins/" 2>/dev/null
    cp -a "$s/\$_12_/Plugins/Milkdrop2/." "$a/Plugins/Milkdrop2/" 2>/dev/null
    cp -f "$s/\$_12_/links.xml" "$a/" 2>/dev/null
    cp -f "$w/dx/d3dx9_31.dll" "$w/dx/d3dx9_36.dll" "$a/"
    cp -f "$w/skins/"* "$a/Skins/" 2>/dev/null
    if [[ -f "$w/dl/presets.zip" ]]; then
        echo "  Milkdrop presets:"
        pytool unzip_milk "$w/dl/presets.zip" "$a/Plugins/Milkdrop2/presets" || warn "Preset pack could not be unpacked"
    fi
    SRC_APP="$a"
}

# ---------------------------------------------------------------------------
# Install / Repair
# ---------------------------------------------------------------------------
do_install() {
    local repair=0 keep_ini="" online=1 f
    [[ -f "$APP_DIR/winamp.exe" ]] && repair=1
    [[ -f "$DATA/payload/Winamp.pc/winamp.exe" ]] && online=0
    if (( online )); then STEPS=10; else STEPS=4; fi
    STEP=0

    clear
    show_banner
    echo "Winamp 5.8 is installed as its own system \"Winamp\":"
    echo "  - Milkdrop visualizations, skins, wallpaper and a mouse pointer"
    echo "  - music from $MUSIC_DIR"
    echo "  - the DeLorean Logo-Fix: system logos are shown with every theme"
    echo
    if (( online )); then
        echo "The installer downloads Winamp, the DirectX files for Milkdrop, skins and"
        echo "presets from their original sources (about 140 MB). This needs an internet"
        echo "connection and takes a while."
        echo
    fi
    if (( repair )); then
        echo "Winamp is already installed. Your settings, music, own skins and"
        echo "wallpapers are kept."
        echo
        ask_yes_no "Reinstall / repair Winamp?" || return 1
    else
        ask_yes_no "Install Winamp?" || return 1
    fi

    get_winamp_files

    step "Installing Winamp"
    mkdir -p "$ROM_DIR" "$MUSIC_DIR" || fail "Cannot create $ROM_DIR"
    if [[ -f "$APP_DIR/Plugins/gen_delorean.ini" ]]; then
        keep_ini="$(mktemp /tmp/winamp-ini-XXXX)"; cp "$APP_DIR/Plugins/gen_delorean.ini" "$keep_ini"
    fi
    pytool copytree "$SRC_APP" "$APP_DIR" || fail "Copying Winamp failed (disk full?)"
    cp -a "$DATA/files/." "$APP_DIR/" || fail "Copying the DeLorean23 files failed"
    if [[ -n "$keep_ini" ]]; then
        cp "$keep_ini" "$APP_DIR/Plugins/gen_delorean.ini"; rm -f "$keep_ini"
    fi
    log "Winamp installed to $APP_DIR"

    step "Setting up the system \"Winamp\""
    bar 10 "game list and cover"
    mkdir -p "$ROM_DIR/images"
    cp -f "$DATA/Winamp.jpg" "$ROM_DIR/images/Winamp.jpg"
    set_gamelist "$ROM_DIR/gamelist.xml" "./Winamp.pc" "Winamp" "./images/Winamp.jpg" \
        "Winamp 5.8 with Milkdrop. Music from roms/musik. Created by DeLorean23" || warn "gamelist.xml not updated"
    bar 40 "wallpaper"
    mkdir -p "$WALL_DIR"
    for f in "$DATA/wallpaper/"*; do
        [ -e "$WALL_DIR/$(basename "$f")" ] || cp "$f" "$WALL_DIR/"
    done
    bar 70 "system"
    mkdir -p "$(dirname "$CFG")"
    cp -f "$DATA/es_systems_winamp.cfg" "$CFG" || fail "Cannot write $CFG"
    bar 100 "done"
    log "System added: $CFG"

    step "DeLorean Logo-Fix"
    bar 20 "logo"
    mkdir -p "$LOGO_DIR" "$SERVICE_DIR"
    cp -f "$DATA/winamp.png" "$LOGO_DIR/winamp.png"
    cp -f "$DATA/delorean_logos" "$SERVICE"
    chmod +x "$SERVICE"
    command -v batocera-services >/dev/null 2>&1 && batocera-services enable delorean_logos >/dev/null 2>&1
    bar 50 "themes"
    "$SERVICE" apply
    bar 100 "done"
    if [[ " $(batocera-settings-get system.services 2>/dev/null) " == *" delorean_logos "* ]]; then
        log "Logo-Fix service enabled."
    else
        warn "Logo-Fix service could not be enabled - the logo is only set up until the next theme change."
    fi

    step "Preparing Wine for Winamp (first time takes a minute)"
    prepare_wine || warn "Wine could not be prepared now. Winamp still works, the first start just takes longer."

    rm -rf "$WORK"
    echo
    echo "======================================================================"
    echo " Winamp is installed."
    echo
    echo " Music:      $MUSIC_DIR"
    echo " Wallpaper:  $WALL_DIR  (settings in config.ini)"
    echo " Skins:      $APP_DIR/Skins"
    echo "======================================================================"
    echo
    return 0
}

# ---------------------------------------------------------------------------
# Uninstall
# ---------------------------------------------------------------------------
do_uninstall() {
    local keep_wall=0 paths=() other lf="$SERVICE"
    # use the Logo-Fix that came with this installer (an older installed one may lack "remove")
    [[ -f "$DATA/delorean_logos" ]] && lf="$DATA/delorean_logos"
    clear
    show_banner
    if [[ ! -e "$APP_DIR" && ! -e "$CFG" && ! -e "$LOGO_DIR/winamp.png" ]]; then
        echo "Winamp is not installed."
        return 1
    fi
    echo "This removes Winamp completely: the system \"Winamp\", the program,"
    echo "its settings, its Wine folder and its logo."
    echo "Your music in $MUSIC_DIR stays where it is."
    echo
    ask_yes_no "Really remove Winamp completely?" || return 1
    if [[ -d "$WALL_DIR" ]]; then
        echo
        ask_yes_no "Delete your wallpapers too ($WALL_DIR)?" || keep_wall=1
    fi
    STEPS=3; STEP=0

    step "Removing Winamp"
    if (( keep_wall )); then
        paths+=("$APP_DIR" "$ROM_DIR/images" "$ROM_DIR/gamelist.xml")
    else
        paths+=("$ROM_DIR")
    fi
    paths+=("$SETTINGS_DIR" "$CFG")
    for other in "$BOTTLES"/*/Winamp.pc.wine "$BOTTLES"/Winamp.pc.wine; do
        [[ -e "$other" ]] && paths+=("$other")
    done
    pytool rm "${paths[@]}"
    (( keep_wall )) && log "Wallpapers kept in $WALL_DIR"

    step "Removing the logo"
    bar 30 "themes"
    [[ -f "$lf" ]] && bash "$lf" remove winamp
    rm -f "$LOGO_DIR/winamp.png"
    if ! ls "$LOGO_DIR"/*.png >/dev/null 2>&1; then
        # no other DeLorean23 logos left: remove the Logo-Fix too
        bar 70 "Logo-Fix service"
        command -v batocera-services >/dev/null 2>&1 && batocera-services disable delorean_logos >/dev/null 2>&1
        [[ -f "$lf" ]] && bash "$lf" stop
        rm -f "$SERVICE"
        rmdir "$LOGO_DIR" 2>/dev/null
        log "Logo-Fix removed (no other logos left)."
    else
        log "Logo-Fix kept (other DeLorean23 logos are still installed)."
    fi
    bar 100 "done"

    step "Cleaning up"
    rm -rf "$WORK"
    bar 100 "done"
    echo
    echo "======================================================================"
    echo " Winamp has been removed."
    echo "======================================================================"
    echo
    return 0
}

# ---------------------------------------------------------------------------
# End: remove the installer from Ports? reboot?
# ---------------------------------------------------------------------------
finish() {   # finish <reboot question>
    local REMOVE=0
    if ask_yes_no "Remove installer from Ports?"; then REMOVE=1; fi
    echo
    if ! ask_yes_no "$1"; then
        if (( REMOVE )); then
            rm -rf "$DATA" "$SELF_DIR/images/$SELF_STEM.jpg"
            echo "Installer removed."
        else
            set_gamelist "$SELF_DIR/gamelist.xml" "./$SELF_NAME" "$SELF_STEM" "./images/$SELF_STEM.jpg" >> "$LOG" 2>&1
        fi
        echo
        echo "No reboot. Reboot Batocera later to finish."
        echo
        wait_key
        (( REMOVE )) && rm -f "$SELF"
        exit 0
    fi
    # Reboot: stop EmulationStation first so it can't overwrite the game lists.
    # Runs detached, because stopping EmulationStation also closes this window.
    echo "Rebooting..."
    declare -f remove_from_gamelist set_gamelist > /tmp/winamp-finish-funcs.sh
    cat > /tmp/winamp-finish.sh <<SH
#!/bin/bash
LOG="$LOG"
sleep 1
if [[ -x /etc/init.d/S31emulationstation ]]; then
    /etc/init.d/S31emulationstation stop >> "\$LOG" 2>&1
else
    killall -TERM emulationstation >> "\$LOG" 2>&1
fi
for i in \$(seq 1 30); do pgrep -x emulationstation >/dev/null || break; sleep 1; done
source /tmp/winamp-finish-funcs.sh
if [[ "$REMOVE" == "1" ]]; then
    remove_from_gamelist "$SELF_DIR/gamelist.xml" "$SELF_NAME" >> "\$LOG" 2>&1
    rm -rf "$DATA" "$SELF_DIR/images/$SELF_STEM.jpg" "$SELF"
else
    set_gamelist "$SELF_DIR/gamelist.xml" "./$SELF_NAME" "$SELF_STEM" "./images/$SELF_STEM.jpg" >> "\$LOG" 2>&1
fi
sync
reboot
SH
    chmod +x /tmp/winamp-finish.sh
    setsid nohup /bin/bash /tmp/winamp-finish.sh >/dev/null 2>&1 < /dev/null &
    sleep 1
    exit 0
}

# ---------------------------------------------------------------------------
# Start
# ---------------------------------------------------------------------------
clear
show_banner
[ "$(id -u)" = "0" ]          || fail "This installer must run as root."
[ -d /userdata ]              || fail "/userdata not found. Is this Batocera?"
[ "$(uname -m)" = "x86_64" ]  || fail "Winamp needs Wine, which Batocera only has on x86_64 PCs. This machine is $(uname -m)."
command -v python3 >/dev/null || fail "python3 not found."
[ -d "$DATA/files" ]          || fail "Installer files are missing: $DATA"

rm -rf "$DATA/Winamp.pc"   # left over from installer v1 (Winamp is in payload/ or downloaded now)
if [[ -f "$APP_DIR/winamp.exe" ]]; then FIRST="Repair / reinstall Winamp"; else FIRST="Install Winamp"; fi
menu "Winamp Installer $INSTALLER_VERSION - what do you want to do?" "$FIRST" "Uninstall Winamp" "Exit"
case "$MENU_CHOICE" in
    0) if do_install; then finish "Reboot now? (needed for the new system)"; fi ;;
    1) if do_uninstall; then finish "Reboot now? (needed to remove the system)"; fi ;;
    *) exit 0 ;;
esac
echo
echo "Nothing changed."
echo
wait_key
exit 0
