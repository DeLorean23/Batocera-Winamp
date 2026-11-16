#!/bin/bash
# Builds out/Batocera-Winamp.zip (the installer that downloads Winamp etc. from the original sources)
# Created by DeLorean23
set -euo pipefail
W="$(cd "$(dirname "$0")" && pwd)"
OUT="$W/out"
rm -rf "$OUT"; mkdir -p "$OUT"
bash "$W/src/build.sh"

make_data() {   # make_data <ports dir>
    local PORTS="$1" DATA="$1/.winamp-installer" F
    F="$DATA/files"
    mkdir -p "$PORTS/images" "$DATA/wallpaper" "$F/Plugins/delorean_src"
    cp "$W/src/gen_delorean.dll" "$F/Plugins/"
    cp "$W/src/gen_delorean.c" "$W/src/mouse_wallpaper.inc" "$W/src/delorean_start.c" "$F/Plugins/delorean_src/"
    cp "$W/src/delorean_start.exe" "$F/"
    cp -r "$W/seed" "$F/Seed"
    printf '[Winamp]\r\ninidir=Z:\\userdata\\system\\configs\\winamp\r\n' > "$F/paths.ini"
    printf '%s\r\n' \
      '[DeLorean]' \
      '; Music folder (Z: = Batocera root)' \
      'musicdir=Z:\userdata\roms\musik' \
      '; 1 = start playing right away' \
      'autoplay=1' \
      '; 1 = add new music to the Media Library' \
      'library=1' \
      '; Mouse mode at start: 1 = on, 0 = off (L3 switches)' \
      'mouse=1' \
      '; Size of the arrow: 1 = original, 2 = double (TV), up to 4' \
      'cursorsize=2' \
      '; Pointer speed (pixels per second at full stick)' \
      'mousespeed=1400' \
      '; Wallpaper folder (config.ini there)' \
      'wallpaperdir=Z:\userdata\roms\winamp\wallpaper' > "$F/Plugins/gen_delorean.ini"
    printf '@echo off\r\nrem Winamp for Batocera - Created by DeLorean23\r\nrem First start: copy the default settings\r\nif not exist "Z:\\userdata\\system\\configs\\winamp\\Winamp.ini" xcopy "Seed" "Z:\\userdata\\system\\configs\\winamp\\" /E /I /Y /Q\r\ndelorean_start.exe\r\n' > "$F/start.bat"
    local OV='${WINEDLLOVERRIDES};mscoree,mshtml=;d3dx9_37,d3dx9_38,d3dx9_39,d3dx9_40,d3dx9_41,d3dx9_42,d3dx9_43=d;d3dx9_36,d3dx9_31=n'
    printf 'CMD=start.bat\r\nENV=WINEDLLOVERRIDES="%s"\r\n' "$OV" > "$F/autorun.cmd"

    cp "$W/assets/Winamp.jpg" "$DATA/Winamp.jpg"
    cp "$W/assets/winamp.png" "$DATA/winamp.png"
    cp "$W/logofix/delorean_logos" "$DATA/delorean_logos"
    cp "$W/assets/wallpaper/"* "$DATA/wallpaper/"
    cat > "$DATA/es_systems_winamp.cfg" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<systemList>
  <system>
    <fullname>Winamp</fullname>
    <name>winamp</name>
    <manufacturer>Nullsoft</manufacturer>
    <release>1997</release>
    <hardware>system</hardware>
    <path>/userdata/roms/winamp</path>
    <extension>.pc</extension>
    <command>emulatorlauncher %CONTROLLERSCONFIG% -system windows -rom %ROM% -gameinfoxml %GAMEINFOXML% -systemname %SYSTEMNAME%</command>
    <platform>pc</platform>
    <theme>winamp</theme>
    <emulators><emulator name="wine"><cores><core name="wine-tkg" default="true" /></cores></emulator></emulators>
  </system>
</systemList>
EOF
    python3 - "$W/installer/Winamp Installer.template.sh" "$PORTS/Winamp Installer.sh" "$W/installer/banner_car.txt" <<'PY'
import sys
src, out, car = sys.argv[1:4]
banner = open(car).read().rstrip("\n") + "\n\n" + \
    "                         Winamp Installer\n" + \
    "                       Created by DeLorean23"
t = open(src).read().replace("@@BANNER@@", banner)
assert "@@" not in t
open(out, "w", newline="\n").write(t)
PY
    chmod +x "$PORTS/Winamp Installer.sh"
    bash -n "$PORTS/Winamp Installer.sh"
    cp "$W/assets/Winamp Installer.jpg" "$PORTS/images/Winamp Installer.jpg"
}

S="$OUT/pkg"; mkdir -p "$S/roms/ports"
make_data "$S/roms/ports"
cp "$W/README.md" "$S/README.txt"; cp "$W/LIESMICH.md" "$S/LIESMICH.txt"
(cd "$S" && zip -qr -9 "$OUT/Batocera-Winamp.zip" README.txt LIESMICH.txt roms)
rm -rf "$S"
ls -la "$OUT"
