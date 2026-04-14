# C64 Ultimate Toolbox — Linux Port

A native Linux companion app for your [Commodore 64 Ultimate](https://www.commodore.net) device — browse and manage files, write BASIC programs, view a live CRT display, and control your device from your Linux desktop.

This is a port of [C64 Ultimate Toolbox](https://github.com/mist64/c64-ultimate-toolbox) (originally a macOS/Swift/Metal app) to Linux using Qt 6 and C++20 with OpenGL.

## Features

- **Two connection modes** — Viewer Mode passively listens for streams; Toolbox Mode connects via REST API and FTP for full device control
- **File manager** — Browse, upload, download, rename, and delete files on your device. Create D64, D71, D81, and DNP disk images. Mount and unmount drives.
- **BASIC editor** — Write Commodore BASIC programs with syntax highlighting and send them directly to your C64. Includes sample programs to get started.
- **Remote debug monitor** — Interactive monitor with commands for memory dump, read/write, fill, and hunt
- **Memory browser** — Hex/ASCII memory viewer with preset jumps (Zero Page, Screen, BASIC, VIC-II, SID, CIA) and auto-refresh
- **Live video** — Real-time UDP video streaming with frame assembly and PAL/NTSC auto-detection
- **Live audio** — 48kHz 16-bit stereo PCM playback with volume and balance control
- **CRT shader effects** — OpenGL post-processing with scanlines, bloom, phosphor afterglow, shadow masks, barrel distortion, tint, and vignette
- **8 built-in presets** — Clean, Home CRT, P3 Amber, P1 Green, Crisp, Warm Glow, Old TV, Arcade
- **Custom presets** — Save and manage your own CRT configurations (stored as JSON)
- **Device control** — Reset, reboot, power off, pause/resume, access the Ultimate menu
- **File runners** — Load and run SID, MOD, PRG, and CRT files directly from the file manager or toolbar
- **Keyboard forwarding** — Type on your keyboard and have it appear on the C64, with an on-screen strip for C64-specific keys (RUN/STOP, HOME, CLR, function keys, etc.)
- **Smart key routing** — Keyboard input goes to the C64 by default, but automatically yields to text fields, the BASIC editor, and other input widgets
- **Auto device discovery** — Scans your local subnet to find Ultimate devices
- **Screenshot capture** — Save the CRT-processed output as PNG
- **Device configuration** — Browse and edit all device settings with search, save to flash
- **Drive management** — View drive status, mount/unmount disk images

## Requirements

- Linux with X11 or Wayland
- Qt 6 (Widgets, Network, Multimedia, OpenGL, OpenGLWidgets)
- OpenGL 3.3+ capable GPU
- CMake 3.22+, C++20 compiler
- A Commodore 64 Ultimate device on the local network
- Toolbox Mode requires FTP File Service and Web Remote Control Service enabled on the device

### System packages (Debian/Ubuntu)

```bash
sudo apt install cmake g++ qt6-base-dev qt6-multimedia-dev libqt6opengl6-dev libgl-dev
```

## Building

```bash
cmake -B build
cmake --build build
./build/c64-ultimate-toolbox
```

## Network Setup

The app communicates with the C64 Ultimate over multiple channels:

- **HTTP REST API** — Device info, stream control, configuration, machine control
- **FTP (port 21)** — File browsing, upload, download, and management
- **UDP port 11000** — Video stream (4-bit indexed color, assembled into RGBA frames)
- **UDP port 11001** — Audio stream (16-bit stereo PCM at ~48kHz)

### Firewall

UDP ports 11000 and 11001 must be open for incoming traffic. On NixOS:

```nix
networking.firewall.allowedUDPPorts = [ 11000 11001 ];
```

### Streaming setup

Video and audio streams must be configured on the device:

1. On the C64 Ultimate, open the menu (press the cartridge button)
2. Navigate to **F1 > Streams**
3. Set **VIC Stream** to `<your-ip>:11000` and press Enter
4. Set **Audio Stream** to `<your-ip>:11001` and press Enter
5. Both should show a diamond icon indicating they are active

### Troubleshooting

- **"Cannot find MAC" error on the device** — The device's streaming stack may not be able to ARP-resolve your machine. This can happen if `rp_filter` is set to strict mode. Fix with: `sudo sysctl net.ipv4.conf.<interface>.rp_filter=0`. Sending a gratuitous ARP can also help: `arping -c 5 -A -I <interface> <your-ip>`.
- **No video despite streams enabled** — Check your firewall allows incoming UDP on ports 11000/11001.
- **Two network interfaces on the same subnet** — Can cause ARP confusion. Disable one or set `rp_filter=0`.

## Architecture

This is a faithful port of the macOS/Swift/Metal original to Linux/Qt/C++/OpenGL:

| macOS Original | Linux Port |
|---------------|------------|
| AppKit | Qt Widgets |
| Metal | OpenGL 3.3 (GLSL) |
| AVFoundation | Qt Multimedia (QAudioSink) |
| Network.framework | QUdpSocket, QTcpSocket |
| URLSession | QNetworkAccessManager |
| UserDefaults | QSettings |

The CRT shader is a direct GLSL port of the original Metal shader, implementing the same 8-stage pipeline: curvature, blur/sharp sampling, bloom, scanlines, phosphor mask, tint, vignette, and afterglow.

## Links

- [Original macOS app](https://github.com/mist64/c64-ultimate-toolbox)
- [Quick Start Guide](https://discuss.bradroot.me/t/c64-ultimate-toolbox-quick-start-guide/80)
- [C64 Ultimate](https://www.commodore.net)

## License

[Mozilla Public License 2.0](LICENSE)
