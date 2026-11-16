# Winamp for Batocera

*Created by DeLorean23* · [Deutsche Anleitung: LIESMICH.md](LIESMICH.md)

**A fun project for nostalgia:** Winamp 5.8 just like back in the Windows XP days, with Milkdrop visualizations, skins, a mouse pointer and a desktop wallpaper, but on your TV and with a controller, as its own system "Winamp" in Batocera.

This is an unofficial fan project. It is not affiliated with Winamp or Batocera.

## What you get

- **Own system "Winamp"** with its own logo
- **Winamp 5.8** with Media Library, playlist and equalizer
- **Milkdrop 2** with about 10,000 visualizations, started with a button press, also in fullscreen
- **17 skins**: Big Bento (default), Bento, Winamp Modern, Winamp Classic and 13 classic skins
- **Wallpaper** behind the Winamp windows, as a fixed picture or a slideshow
- **XP mouse pointer** to use everything with the controller. A real mouse works too.
- **Progress bars** while installing and uninstalling, and a complete **uninstall**

## Download and installation

1. Download **`Batocera-Winamp.zip`** from the **[Releases](https://github.com/DeLorean23/Batocera-Winamp/releases/latest)** page.
2. Extract the contents of the ZIP to `/userdata`. Everything goes to `roms/ports`.
3. In Batocera, refresh the game list and launch **Winamp Installer** from **Ports**.
4. Choose **Install Winamp** and confirm with **A**.
5. At the end, the installer asks whether to remove itself from Ports and whether to reboot. The reboot is needed for the new system.

> **Important: internet required, the installation takes a while.**
> Winamp, the DirectX files for Milkdrop, the skins and the presets are downloaded from their original sources during installation: Nullsoft, Microsoft and GitHub, about 140 MB in total. This project does not distribute these files itself. The progress bar shows how far it is.

Requirements: Batocera v43 on an x86_64 PC, and an internet connection during installation.

## Music

Put your music into **`/userdata/roms/musik`**. Subfolders are fine. Over the network, the folder is at `\\BATOCERA\share\roms\musik`.

Every time Winamp starts, it loads everything from there into the playlist and starts playing. New tracks are added to the Media Library automatically. Supported formats include MP3, FLAC, OGG, WAV, M4A/AAC, WMA, Opus, MIDI and tracker modules (MOD, XM, IT, S3M).

## Wallpaper

Folder: **`/userdata/roms/winamp/wallpaper`**

Put pictures there (`.jpg`, `.png`, `.bmp`). They fill the screen without being stretched. If the aspect ratio doesn't match, the edges are cut off a little. Two DeLorean wallpapers are included.

The settings are in **`config.ini`** in the same folder:

```ini
[Wallpaper]
wallpaper=1      ; 1 = wallpaper on, 0 = off (black)
slideshow=0      ; 1 = slideshow on, 0 = off
time=10          ; slideshow: change every x seconds
random=0         ; slideshow: 1 = random, 0 = by file name
image=DeLorean23 Synthwave.jpg   ; fixed picture when slideshow=0
```

Changes take effect **immediately**, even while Winamp is running. For example, you can pick a new picture from your PC over the network share.

## Controls

**Mouse mode** (on at start, **L3** switches it):

| Button | Function |
|---|---|
| Left stick | Move the pointer (D-pad: pixel by pixel) |
| A | Left click, hold = drag (sliders, windows) |
| B | Right click (context menu) |
| X | Double click |
| Right stick | Scroll |

**Always:**

| Button | Function |
|---|---|
| Y | Milkdrop on/off |
| Start | Milkdrop fullscreen on/off |
| LB / RB | Previous / next track |
| LT / RT | 5 seconds back / forward |
| R3 | Repeat on/off |
| Select | Next skin |
| Select + Start | Exit Winamp (Hotkey + Start works too) |

**Mouse mode off:** A = play/pause, B = stop, X = shuffle, D-pad up/down = volume, left/right = previous/next visualization, right stick up = switch visualization right away.

The pointer hides after 10 seconds without movement and comes back with the next move.

## Skins

Put your own skins (`.wsz` or `.wal`) into `/userdata/roms/winamp/Winamp.pc/Skins`. **Select** cycles through all skins. Thousands to choose from: [Winamp Skin Museum](https://skins.webamp.org).

## Settings

| What | Where |
|---|---|
| Winamp settings | `/userdata/system/configs/winamp` (delete the folder = default settings) |
| Music folder, autoplay, pointer size and speed | `/userdata/roms/winamp/Winamp.pc/Plugins/gen_delorean.ini` |
| Wallpaper | `/userdata/roms/winamp/wallpaper/config.ini` |

## Uninstall

Launch **Winamp Installer** from Ports and choose **Uninstall Winamp**. It removes the system, the program, its settings, its Wine folder and the logo. It asks before deleting your wallpapers. **Your music is always kept.**

## How it works

Winamp runs under Wine in its own screen-sized Wine desktop. A small Winamp plugin by DeLorean23 (`gen_delorean.dll`, source in `src/`) handles the controller, the mouse pointer, the wallpaper and the music folder.

## Legal

- Winamp is a trademark of its owner. This is an unofficial project, not affiliated with Winamp, Nullsoft, Llama Group or Batocera.
- Winamp, the DirectX files, the skins and the Milkdrop presets are not included. They are downloaded from their original sources during installation, under their own licenses.
- Skins: [Webamp](https://github.com/captbaritone/webamp) collection. Presets: [Cream of the Crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop) by ISOSCELES / projectM.
- The installer, plugin, Logo-Fix, covers and wallpapers by DeLorean23 are released under the [MIT License](LICENSE).
