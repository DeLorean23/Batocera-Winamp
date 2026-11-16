# Winamp für Batocera

*Created by DeLorean23* · [English: README.md](README.md)

**Ein Spaßprojekt aus Nostalgie:** Winamp 5.8 so wie damals zu Windows-XP-Zeiten, mit Milkdrop-Visualisierung, Skins, Mauszeiger und Desktop-Hintergrund, aber auf dem Fernseher und mit dem Controller, als eigenes System „Winamp“ in Batocera.

Das ist ein inoffizielles Fanprojekt und hat nichts mit Winamp oder Batocera selbst zu tun.

## Was du bekommst

- **Eigenes System „Winamp“** mit Logo, das bei jedem Theme angezeigt wird, auch beim Batocera-Standard-Theme
- **Winamp 5.8** mit Medienbibliothek, Playlist und Equalizer
- **Milkdrop 2** mit rund 10.000 Visualisierungen, erst auf Knopfdruck, auch im Vollbild
- **17 Skins**: Big Bento (Standard), Bento, Winamp Modern, Winamp Classic und 13 klassische Skins
- **Wallpaper** hinter den Winamp-Fenstern, als festes Bild oder Diashow
- **XP-Mauszeiger** zum Bedienen mit dem Controller. Eine echte Maus geht auch.
- **Ladebalken** bei Installation und Deinstallation, und **Deinstallieren** ohne Rückstände

## Download und Installation

1. **`Batocera-Winamp.zip`** von den **[Releases](https://github.com/DeLorean23/Batocera-Winamp/releases/latest)** herunterladen.
2. Den Inhalt der ZIP nach `/userdata` entpacken. Alles landet in `roms/ports`.
3. In Batocera die Spieleliste aktualisieren und unter **Ports** den **Winamp Installer** starten.
4. **Install Winamp** wählen und mit **A** bestätigen.
5. Am Ende fragt der Installer, ob er sich aus Ports entfernen soll, und ob neu gestartet werden soll. Der Neustart ist für das neue System nötig.

> **Wichtig: Internet nötig, die Installation dauert eine Weile.**
> Winamp, die DirectX-Dateien für Milkdrop, die Skins und die Presets werden beim Installieren von ihren Originalquellen heruntergeladen, zusammen etwa 140 MB: Nullsoft, Microsoft und GitHub. Das Projekt verteilt diese Dateien nicht selbst. Der Ladebalken zeigt den Fortschritt.

Voraussetzungen: Batocera v43 auf einem x86_64-PC und eine Internetverbindung während der Installation.

## Musik

Lege deine Musik nach **`/userdata/roms/musik`**, Unterordner gehen auch. Über das Netzwerk findest du den Ordner unter `\\BATOCERA\share\roms\musik`.

Bei jedem Start lädt Winamp alles von dort in die Playlist und spielt los. Neue Titel landen automatisch in der Medienbibliothek. Unterstützt werden unter anderem MP3, FLAC, OGG, WAV, M4A/AAC, WMA, Opus, MIDI und Tracker-Module (MOD, XM, IT, S3M).

## Wallpaper

Ordner: **`/userdata/roms/winamp/wallpaper`**

Lege dort einfach Bilder ab (`.jpg`, `.png`, `.bmp`). Sie werden bildschirmfüllend angezeigt, ohne Verzerrung. Passt das Seitenverhältnis nicht, wird an den Rändern etwas abgeschnitten. Zwei DeLorean-Wallpaper sind schon dabei.

Eingestellt wird in der **`config.ini`** im selben Ordner:

```ini
[Wallpaper]
wallpaper=1      ; 1 = Hintergrundbild an, 0 = aus (schwarz)
slideshow=0      ; 1 = Diashow an, 0 = aus
time=10          ; Diashow: Wechsel alle x Sekunden
random=0         ; Diashow: 1 = zufällig, 0 = nach Dateiname
image=DeLorean23 Synthwave.jpg   ; festes Bild, wenn slideshow=0
```

Änderungen wirken **sofort**, auch während Winamp läuft. So kannst du zum Beispiel vom PC aus über die Netzwerkfreigabe ein neues Bild einstellen.

## Steuerung

**Maus-Modus** (beim Start an, **L3** schaltet um):

| Taste | Funktion |
|---|---|
| Linker Stick | Mauszeiger bewegen (Steuerkreuz: pixelgenau) |
| A | Linksklick, gedrückt halten = ziehen (Regler, Fenster) |
| B | Rechtsklick (Kontextmenü) |
| X | Doppelklick |
| Rechter Stick | Scrollen |

**Immer:**

| Taste | Funktion |
|---|---|
| Y | Milkdrop an/aus |
| Start | Milkdrop Vollbild an/aus |
| LB / RB | Vorheriger / nächster Titel |
| LT / RT | 5 Sekunden zurück / vor |
| R3 | Wiederholen an/aus |
| Select | Nächster Skin |
| Select + Start | Winamp beenden (Hotkey + Start geht auch) |

**Maus-Modus aus:** A = Play/Pause, B = Stop, X = Zufall, Steuerkreuz hoch/runter = Lautstärke, links/rechts = vorherige/nächste Visualisierung, rechter Stick hoch = sofort andere Visualisierung.

Der Mauszeiger verschwindet nach 10 Sekunden ohne Bewegung und kommt bei der nächsten Bewegung wieder.

## Skins

Eigene Skins (`.wsz` oder `.wal`) einfach nach `/userdata/roms/winamp/Winamp.pc/Skins` legen. Mit **Select** schaltest du durch alle Skins. Riesige Auswahl: [Winamp Skin Museum](https://skins.webamp.org).

## Einstellungen

| Was | Wo |
|---|---|
| Winamp-Einstellungen | `/userdata/system/configs/winamp` (Ordner löschen = Grundeinstellungen) |
| Musikordner, Autoplay, Mauszeiger (Größe, Tempo) | `/userdata/roms/winamp/Winamp.pc/Plugins/gen_delorean.ini` |
| Wallpaper | `/userdata/roms/winamp/wallpaper/config.ini` |

## Deinstallieren

Den **Winamp Installer** in Ports starten und **Uninstall Winamp** wählen. Entfernt werden das System, das Programm, die Einstellungen, die Wine-Umgebung und das Logo. Vorher fragt er, ob deine Wallpaper mit gelöscht werden sollen. **Deine Musik bleibt immer erhalten.**

## DeLorean Logo-Fix

Der Installer bringt einen kleinen Dienst mit (`delorean_logos`). Er legt die Systemlogos aus `/userdata/system/delorean-logos` bei jedem Start in alle installierten Themes, auch in das schreibgeschützte Standard-Theme. Dabei wird am System selbst nichts verändert. Den Dienst nutzen auch andere Installer von DeLorean23. Log: `/userdata/system/logs/delorean_logos.log`.

## Wie es funktioniert

Winamp läuft unter Wine in einem eigenen Wine-Desktop in Bildschirmgröße. Ein kleines Winamp-Plugin von DeLorean23 (`gen_delorean.dll`, Quellcode in `src/`) kümmert sich um Controller, Mauszeiger, Wallpaper und den Musikordner.

## Rechtliches

- Winamp ist ein Markenzeichen seines Inhabers. Dieses Projekt ist inoffiziell und nicht mit Winamp, Nullsoft, Llama Group oder Batocera verbunden.
- Winamp, die DirectX-Dateien, die Skins und die Milkdrop-Presets werden nicht mitgeliefert, sondern beim Installieren von ihren Originalquellen geladen. Es gelten deren Lizenzen.
- Skins: [Webamp](https://github.com/captbaritone/webamp)-Sammlung. Presets: [Cream of the Crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop) von ISOSCELES / projectM.
- Installer, Plugin, Logo-Fix, Cover und Wallpaper von DeLorean23 stehen unter der [MIT-Lizenz](LICENSE).
