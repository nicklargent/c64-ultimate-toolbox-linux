# C64 Ultimate Toolbox — Linux/Qt/C++ Port Specification

**Source project:** `/home/nsl/src/c64-ultimate-toolbox`
**Target:** Native Linux desktop application using Qt 6 and C++20
**Original:** ~10,253 lines of Swift across 43 files (macOS/AppKit/Metal)

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Framework Mapping](#2-framework-mapping)
3. [Project Structure](#3-project-structure)
4. [Phase 1: Project Skeleton & Build System](#phase-1-project-skeleton--build-system)
5. [Phase 2: Network & Protocol Layer](#phase-2-network--protocol-layer)
6. [Phase 3: Video Pipeline](#phase-3-video-pipeline)
7. [Phase 4: Audio Pipeline](#phase-4-audio-pipeline)
8. [Phase 5: Main Window & Video Display](#phase-5-main-window--video-display)
9. [Phase 6: Connection Dialog & Device Scanner](#phase-6-connection-dialog--device-scanner)
10. [Phase 7: File Manager](#phase-7-file-manager)
11. [Phase 8: System Inspector Panel](#phase-8-system-inspector-panel)
12. [Phase 9: Display & Audio Controls Panel](#phase-9-display--audio-controls-panel)
13. [Phase 10: Debug Tools](#phase-10-debug-tools)
14. [Phase 11: BASIC Editor](#phase-11-basic-editor)
15. [Phase 12: Keyboard Forwarding](#phase-12-keyboard-forwarding)
16. [Phase 13: Recording & Screenshots](#phase-13-recording--screenshots)
17. [Phase 14: Settings & Persistence](#phase-14-settings--persistence)
18. [Phase 15: Polish & Testing](#phase-15-polish--testing)
19. [Appendix A: REST API Reference](#appendix-a-rest-api-reference)
20. [Appendix B: FTP Protocol Details](#appendix-b-ftp-protocol-details)
21. [Appendix C: UDP Video Protocol](#appendix-c-udp-video-protocol)
22. [Appendix D: UDP Audio Protocol](#appendix-d-udp-audio-protocol)
23. [Appendix E: CRT Shader Specification](#appendix-e-crt-shader-specification)
24. [Appendix F: BASIC Tokenizer Specification](#appendix-f-basic-tokenizer-specification)
25. [Appendix G: Keyboard Mapping](#appendix-g-keyboard-mapping)
26. [Appendix H: C64 Color Palette](#appendix-h-c64-color-palette)

---

## 1. Architecture Overview

The application is a companion tool for Commodore 64 Ultimate devices. It has two operating modes:

### Viewer Mode (Passive)
- Listens for UDP video (port 11000) and audio (port 11001) streams
- No device API connection required
- Displays real-time video with CRT post-processing
- Plays real-time audio

### Toolbox Mode (Full Control)
- Connects to device via REST API (HTTP) and FTP (port 21)
- All Viewer features plus:
  - File manager (FTP-based upload/download/delete/rename/mkdir)
  - Device configuration browser and editor
  - Drive management (mount/unmount disk images)
  - Machine control (reset/reboot/poweroff/pause/resume)
  - Memory browser and debug monitor
  - BASIC program editor with tokenizer
  - Keyboard forwarding to device
  - File runners (SID/MOD/PRG/CRT upload and execute)
  - Disk image creation (D64/D71/D81/DNP)

### Key Design Principles for Port
- Keep the networking/protocol layer separate from UI code
- Use signals/slots for async event delivery (replaces Swift async/await + callbacks)
- Use QOpenGLWidget for the CRT shader (GLSL port of Metal shader)
- Use Qt Multimedia or raw ALSA/PulseAudio for audio playback
- Use QSettings for persistence (replaces UserDefaults)
- Use JSON files for preset storage (same format as original)

---

## 2. Framework Mapping

| macOS Framework | Qt/Linux Equivalent | Notes |
|----------------|---------------------|-------|
| AppKit (NSWindow, NSView, etc.) | Qt Widgets (QMainWindow, QWidget) | Direct mapping for most widgets |
| Metal | OpenGL 3.3+ via QOpenGLWidget | GLSL shader replaces Metal shader |
| MetalKit (MTKView) | QOpenGLWidget | Subclass for rendering |
| AVFoundation (AVAudioEngine) | Qt Multimedia (QAudioSink) or PulseAudio | Push-model audio playback |
| Network (NWConnection, NWListener) | QUdpSocket, QTcpSocket | Standard Qt networking |
| URLSession | QNetworkAccessManager | HTTP client |
| CoreWLAN | Not needed | Can use standard socket APIs for local IP |
| IOKit (power mgmt) | D-Bus inhibit or xdg-screensaver | Prevent screen blank during streaming |
| AVAssetWriter | FFmpeg (libavcodec/libavformat) | Video recording |
| NSOutlineView | QTreeView + QFileSystemModel or custom | File browser |
| NSTableView | QTableView / QTableWidget | Lists and grids |
| NSSplitViewController | QSplitter | Panel layout |
| NSToolbar | QToolBar | Toolbar |
| NSSegmentedControl | QButtonGroup or custom | Segmented controls |
| UserDefaults | QSettings | Key-value persistence |
| Metal shader (.metal) | GLSL shader (.glsl) | Nearly 1:1 translation |

---

## 3. Project Structure

```
c64-ultimate-toolbox-linux/
├── CMakeLists.txt
├── PORTING_SPEC.md                    (this file)
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.h/cpp          (QApplication subclass, menu bar)
│   │   └── Log.h/cpp                  (Logging utility)
│   ├── network/
│   │   ├── C64ApiClient.h/cpp         (REST API client)
│   │   ├── FtpClient.h/cpp            (FTP protocol implementation)
│   │   ├── UdpVideoReceiver.h/cpp     (UDP video listener)
│   │   ├── UdpAudioReceiver.h/cpp     (UDP audio listener)
│   │   ├── C64Connection.h/cpp        (Connection state manager)
│   │   ├── DeviceScanner.h/cpp        (Network device discovery)
│   │   └── KeyboardForwarder.h/cpp    (PETSCII keyboard injection)
│   ├── video/
│   │   ├── FrameAssembler.h/cpp       (UDP packet → frame)
│   │   ├── CrtRenderer.h/cpp          (QOpenGLWidget + CRT shader)
│   │   └── ColorPalette.h/cpp         (C64 16-color palette + LUT)
│   ├── audio/
│   │   └── AudioPlayer.h/cpp          (PCM audio playback)
│   ├── recording/
│   │   └── MediaCapture.h/cpp         (Screenshot + video recording)
│   ├── models/
│   │   ├── DeviceInfo.h               (Device info struct)
│   │   ├── CrtSettings.h/cpp          (CRT parameter struct)
│   │   ├── CrtPreset.h/cpp            (Built-in + custom presets)
│   │   ├── FtpFileEntry.h             (File entry struct)
│   │   ├── ConnectionMode.h/cpp       (Viewer/Toolbox modes)
│   │   ├── BasicTokenizer.h/cpp       (BASIC V2 tokenizer)
│   │   └── BasicSamples.h/cpp         (Built-in BASIC templates)
│   ├── ui/
│   │   ├── MainWindow.h/cpp           (Main window, splitters, toolbar)
│   │   ├── ConnectDialog.h/cpp        (Connection/listener dialog)
│   │   ├── VideoWidget.h/cpp          (Video display + status bar)
│   │   ├── FileManagerWidget.h/cpp    (FTP file browser)
│   │   ├── SystemPanel.h/cpp          (Device info + config + drives)
│   │   ├── DisplayAudioPanel.h/cpp    (CRT presets + sliders + audio)
│   │   ├── DebugPanel.h/cpp           (Memory browser + monitor)
│   │   ├── MemoryBrowserWidget.h/cpp  (Hex/ASCII memory view)
│   │   ├── DebugMonitorWidget.h/cpp   (Registers + memory R/W)
│   │   ├── BasicEditorWindow.h/cpp    (BASIC scratchpad window)
│   │   ├── BasicEditorWidget.h/cpp    (Syntax-highlighted editor)
│   │   ├── KeyStripWidget.h/cpp       (On-screen C64 keyboard)
│   │   └── StatusBarWidget.h/cpp      (Recording/keyboard indicators)
│   ├── settings/
│   │   └── PresetManager.h/cpp        (JSON preset persistence)
│   └── shaders/
│       ├── crt.vert                   (Vertex shader — fullscreen quad)
│       └── crt.frag                   (Fragment shader — CRT effects)
└── resources/
    ├── icons/                         (Toolbar icons)
    └── c64-ultimate-toolbox.qrc       (Qt resource file)
```

---

## Phase 1: Project Skeleton & Build System

### Goal
CMake project that builds an empty Qt 6 window.

### Tasks
1. Create `CMakeLists.txt` requiring Qt 6 (Widgets, Network, Multimedia, OpenGL)
2. Create `src/main.cpp` with QApplication + empty QMainWindow
3. Create `src/app/Application.h/cpp` with basic app setup
4. Verify build with `cmake -B build && cmake --build build`

### Dependencies (system packages)
```
qt6-base-dev qt6-multimedia-dev qt6-opengl-dev
libgl-dev
cmake g++
```

Optional for recording:
```
libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

---

## Phase 2: Network & Protocol Layer

### Goal
All device communication working independently of UI. Testable via command line.

### Reference files
- `Network/C64APIClient.swift` → `src/network/C64ApiClient.h/cpp`
- `Network/FTPClient.swift` → `src/network/FtpClient.h/cpp`
- `Network/UDPVideoReceiver.swift` → `src/network/UdpVideoReceiver.h/cpp`
- `Network/UDPAudioReceiver.swift` → `src/network/UdpAudioReceiver.h/cpp`
- `Network/C64Connection.swift` → `src/network/C64Connection.h/cpp`
- `Network/DeviceScanner.swift` → `src/network/DeviceScanner.h/cpp`
- `Network/LocalIPAddress.swift` → use `QNetworkInterface`

### 2a: REST API Client (`C64ApiClient`)

Use `QNetworkAccessManager` for HTTP. All methods return results via signals.

**Class interface:**
```cpp
class C64ApiClient : public QObject {
    Q_OBJECT
public:
    explicit C64ApiClient(const QString& host, const QString& password = "", QObject* parent = nullptr);

    // Device info
    void fetchDeviceInfo();

    // Stream control
    void startStream(const QString& name, const QString& clientIp, int port);
    void stopStream(const QString& name);

    // File runners (upload binary)
    void runSidFile(const QByteArray& data);
    void runModFile(const QByteArray& data);
    void runPrgFile(const QByteArray& data);
    void runCrtFile(const QByteArray& data);

    // File runners (by device path)
    void runPrgFromPath(const QString& path);
    void loadPrgFromPath(const QString& path);
    void playSidFromPath(const QString& path);
    void playModFromPath(const QString& path);
    void runCrtFromPath(const QString& path);

    // Drives
    void fetchDrives();
    void mountDrive(const QString& drive, const QString& imagePath);
    void removeDrive(const QString& drive);
    void resetDrive(const QString& drive);

    // Disk image creation
    void createD64(const QString& path, int tracks = 35);
    void createD71(const QString& path);
    void createD81(const QString& path);
    void createDnp(const QString& path, int tracks);

    // Configuration
    void fetchConfigCategories();
    void fetchConfig(const QString& category);
    void fetchConfigDetail(const QString& category, const QString& item);
    void setConfig(const QString& category, const QString& item, const QString& value);
    void saveConfigToFlash();
    void loadConfigFromFlash();
    void resetConfigToDefault();

    // Machine control
    void machineReset();
    void machineReboot();
    void machinePowerOff();
    void machineMenuButton();
    void machinePause();
    void machineResume();

    // Memory access
    void readMemory(uint16_t address, int length);
    void writeMemory(uint16_t address, const QByteArray& data);
    void writeMemoryHex(uint16_t address, const QString& hexData);

signals:
    void deviceInfoReceived(const DeviceInfo& info);
    void streamStarted(const QString& name);
    void streamStopped(const QString& name);
    void drivesReceived(const QJsonArray& drives);
    void configCategoriesReceived(const QStringList& categories);
    void configReceived(const QString& category, const QJsonObject& config);
    void configDetailReceived(const QString& category, const QString& item, const QJsonObject& detail);
    void memoryDataReceived(uint16_t address, const QByteArray& data);
    void requestSucceeded(const QString& operation);
    void requestFailed(const QString& operation, const QString& error);
    void authenticationRequired();
};
```

**URL encoding note:** Device paths in query parameters must encode `/:&=+` characters. Use `QUrl::toPercentEncoding()` with appropriate exclusions.

**Authentication:** Add `X-Password` header to all requests when password is set. Emit `authenticationRequired()` on 403 responses.

### 2b: FTP Client (`FtpClient`)

Use `QTcpSocket` for control and data connections. Run FTP operations in a worker thread (QThread) to avoid blocking UI.

**Key implementation details:**
- Passive mode only (PASV)
- When parsing PASV response `(h1,h2,h3,h4,p1,p2)`, override the host with the original connection host (the device may report an internal IP)
- Transfer chunk size: 65536 bytes
- Directory listing format: Unix `ls -l` style
- Support recursive directory upload

**Struct:**
```cpp
struct FtpFileEntry {
    QString name;
    QString path;
    uint64_t size;
    bool isDirectory;
    QDateTime modificationDate;
};
```

**Signals for progress:**
```cpp
signals:
    void transferProgress(qint64 bytesTransferred, qint64 totalBytes);
    void operationCompleted(const QString& operation);
    void operationFailed(const QString& operation, const QString& error);
    void directoryListed(const QString& path, const QList<FtpFileEntry>& entries);
```

### 2c: UDP Video Receiver

Use `QUdpSocket` bound to the configured port (default 11000).

**Packet format (780 bytes, little-endian):**
```
Offset  Size  Field
0       2     sequenceNum (uint16)
2       2     frameNum (uint16)
4       2     lineNum: bits 0-14 = line number, bit 15 = isLastPacket
6       6     (reserved/unused)
12      768   payload: 4 lines × 192 bytes/line, 2 pixels per byte (4-bit indexed)
```

**Pixel unpacking:** Each payload byte contains 2 pixels:
- Left pixel = low nibble (bits 0-3)
- Right pixel = high nibble (bits 4-7)

**Frame dimensions:**
- Width: 384 pixels (192 bytes × 2 pixels/byte)
- Height: 272 (PAL) or 240 (NTSC), auto-detected from max line number

**Signals:**
```cpp
signals:
    void frameReady(const QByteArray& rgbaData, int width, int height);
```

### 2d: UDP Audio Receiver

Use `QUdpSocket` bound to configured port (default 11001).

**Packet format (770 bytes):**
```
Offset  Size  Field
0       2     sequenceNum (uint16, little-endian)
2       768   pcmData: 192 stereo samples, 16-bit signed LE interleaved (L,R,L,R,...)
```

**Signals:**
```cpp
signals:
    void audioDataReceived(const QByteArray& pcmData, uint16_t sequenceNum);
```

### 2e: Frame Assembler

Assembles UDP packets into complete frames. Port of `Video/FrameAssembler.swift`.

**Logic:**
1. Receive packet from UdpVideoReceiver
2. If `frameNum` changed from current, finalize previous frame and start new one
3. Store payload at `lineNum` in line buffer
4. If `isLastPacket` set or 100ms timeout, assemble frame:
   - Fill missing lines by duplicating nearest valid line above
   - Convert 4-bit indexed pixels to RGBA using color palette LUT
5. Emit `frameReady` signal with RGBA buffer

**Color LUT:** Pre-compute a 256-entry table. Each byte maps to two RGBA pixel values (for the two 4-bit indices packed in one byte). See Appendix H for the palette.

### 2f: Connection State Manager (`C64Connection`)

Owns all sub-components and coordinates connection lifecycle.

```cpp
class C64Connection : public QObject {
    Q_OBJECT
public:
    enum class Mode { Viewer, Toolbox };

    // Toolbox mode
    void connectToDevice(const QString& host, const QString& password = "");
    // Viewer mode
    void startListening(int videoPort = 11000, int audioPort = 11001);

    void disconnect();

    C64ApiClient* apiClient() const;
    FtpClient* ftpClient() const;
    FrameAssembler* frameAssembler() const;
    AudioPlayer* audioPlayer() const;

    // Stream control
    void startVideoStream();
    void stopVideoStream();
    void startAudioStream();
    void stopAudioStream();
};
```

---

## Phase 3: Video Pipeline

### Goal
Render assembled frames with CRT post-processing effects using OpenGL.

### Reference files
- `Video/MetalRenderer.swift` → `src/video/CrtRenderer.h/cpp`
- `Video/ColorPalette.swift` → `src/video/ColorPalette.h/cpp`
- `Shaders/CRTShader.metal` → `src/shaders/crt.vert` + `src/shaders/crt.frag`

### CRT Renderer (`CrtRenderer`)

Subclass `QOpenGLWidget`. Implement `initializeGL()`, `paintGL()`, `resizeGL()`.

**OpenGL setup:**
- Require OpenGL 3.3 Core Profile
- Create fullscreen quad (2 triangles)
- Create source texture (RGBA8, 384×272 or 384×240)
- Create 2 accumulation textures (RGBA8, same size as widget) for double-buffering
- Create FBO for off-screen CRT pass
- Load and compile GLSL shaders

**Render loop (called from `paintGL()`):**
1. Upload new frame data to source texture (if available)
2. **Pass 1:** Render fullscreen quad to accumulation FBO with CRT fragment shader
   - Inputs: source texture + previous accumulation texture (for afterglow)
   - Output: current accumulation texture
3. **Pass 2:** Blit accumulation texture to screen (default FBO)
4. Swap accumulation texture index (0↔1)
5. Call `update()` to schedule next frame

**Aspect ratio:** Maintain 384:272 (PAL) or 384:240 (NTSC) aspect ratio with black bars.

**Frame timing:** Use a QTimer at ~50Hz (PAL) or ~60Hz (NTSC) to call `update()`, or simply `update()` on each new frame from the assembler.

### GLSL Shader

Port the Metal shader to GLSL. The shader language is very similar — main differences:
- Metal `float2/float3/float4` → GLSL `vec2/vec3/vec4`
- Metal `texture2d.sample()` → GLSL `texture(sampler2D, uv)`
- Metal `[[stage_in]]` → GLSL `in/out` varyings
- Metal struct uniforms → GLSL uniform block or individual uniforms

**Uniform block:**
```glsl
uniform float scanlineIntensity;    // 0.0-1.0
uniform float scanlineWidth;        // 0.5-2.0
uniform float blurRadius;           // 0.0-2.0
uniform float bloomIntensity;       // 0.0-1.0
uniform float bloomRadius;          // 0.5-3.0
uniform float afterglowStrength;    // 0.0-1.0
uniform float afterglowDecaySpeed;  // 1.0-10.0
uniform int tintMode;               // 0=off, 1=amber, 2=green, 3=mono
uniform float tintStrength;         // 0.0-1.0
uniform int maskType;               // 0=off, 1=aperture, 2=shadow, 3=slot
uniform float maskIntensity;        // 0.0-1.0
uniform float curvatureAmount;      // 0.0-1.0
uniform float vignetteStrength;     // 0.0-1.0
uniform float dtMs;                 // delta time in ms
uniform vec2 outputSize;            // widget dimensions
uniform vec2 sourceSize;            // source texture dimensions
```

See Appendix E for full shader algorithm details.

---

## Phase 4: Audio Pipeline

### Goal
Play 48kHz 16-bit stereo PCM audio from UDP packets with volume/balance control.

### Reference files
- `Audio/AudioPlayer.swift` → `src/audio/AudioPlayer.h/cpp`

### AudioPlayer

Use `QAudioSink` (Qt 6 Multimedia) with a push-model buffer.

**Format:**
- Sample rate: 48000 Hz
- Sample format: 16-bit signed integer
- Channels: 2 (stereo)
- Byte order: little-endian

**Interface:**
```cpp
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    void start();
    void stop();
    void appendData(const QByteArray& pcmData);
    void setVolume(float volume);  // 0.0-1.0
    void setBalance(float balance); // -1.0 (left) to 1.0 (right)
    void setMuted(bool muted);
};
```

**Volume/balance:** Apply in software before writing to audio sink. For each stereo sample pair:
- Left amplitude = volume × min(1.0, 1.0 - balance)
- Right amplitude = volume × min(1.0, 1.0 + balance)

**Buffer management:** Write PCM data to QIODevice obtained from `QAudioSink::start()`. Monitor buffer underruns via `QAudioSink::state()`.

---

## Phase 5: Main Window & Video Display

### Goal
Main application window with split-panel layout, toolbar, and live video display.

### Reference files
- `DeviceWindowController.swift` → `src/ui/MainWindow.h/cpp`
- `ViewControllers/VideoViewController.swift` → `src/ui/VideoWidget.h/cpp`
- `Views/StatusBarView.swift` → `src/ui/StatusBarWidget.h/cpp`

### MainWindow

```
┌─────────────────────────────────────────────────────────────┐
│ Toolbar                                                      │
├──────────┬──────────────────────────────┬───────────────────┤
│ Sidebar  │  Video Display               │  Inspector Panel  │
│ (File    │  ┌─────────────────────────┐  │  ┌─────────────┐ │
│ Manager) │  │                         │  │  │ System /    │ │
│          │  │   CRT Renderer          │  │  │ Display tab │ │
│          │  │   (QOpenGLWidget)        │  │  │             │ │
│          │  │                         │  │  │  ...        │ │
│          │  └─────────────────────────┘  │  │             │ │
│          │  [Status Bar]                │  │             │ │
│          │  ┌─────────────────────────┐  │  │             │ │
│          │  │ Debug Panel (collapsible)│  │  │             │ │
│          │  └─────────────────────────┘  │  └─────────────┘ │
├──────────┴──────────────────────────────┴───────────────────┤
```

- Use `QSplitter` for the 3-column layout (sidebar | center | inspector)
- Center column: vertical `QSplitter` (video+status on top, debug panel on bottom)
- Sidebar hidden in Viewer mode
- Inspector shows only Display/Audio tab in Viewer mode

### Toolbar Items (QToolBar)

**Toolbox mode:**
- Toggle Sidebar | Spacer | Pause, Reset, Reboot, Power Off | Spacer | New Scratchpad, Run File | Spacer | Send Keyboard (toggle) | Spacer | Screenshot, Record | Spacer | Debug Toggle | Spacer | Inspector Toggle

**Viewer mode:**
- Screenshot, Record | Spacer | Inspector Toggle

### Window Properties
- Minimum size: 700×450
- Default size: 1200×750
- Title: `"<Product> — <Hostname>"` in Toolbox mode, `"C64 Ultimate Viewer"` in Viewer mode
- Save/restore geometry via `QSettings`

---

## Phase 6: Connection Dialog & Device Scanner

### Goal
Dialog for connecting to devices (Toolbox) or starting a listener (Viewer).

### Reference files
- `OpenDeviceWindowController.swift` → `src/ui/ConnectDialog.h/cpp`
- `Network/DeviceScanner.swift` → `src/network/DeviceScanner.h/cpp`

### Connect Dialog (QDialog)

**Two tabs (QTabWidget):**

**Tab 1 — Toolbox:**
- Help text with brief instructions
- "Discovered Devices" list (QListWidget), auto-populated by scanner
- Manual IP address field (QLineEdit)
- Password field (QLineEdit, echo mode: password)
- Connect button
- Error/status label

**Tab 2 — Viewer:**
- Recent sessions list (QListWidget)
- Video port field (QSpinBox, default 11000)
- Audio port field (QSpinBox, default 11001)
- Listen button

### Device Scanner

**Algorithm:**
1. Get local IP via `QNetworkInterface::allInterfaces()`
2. Extract subnet (e.g., `192.168.1`)
3. Scan .1-.254 concurrently using `QNetworkAccessManager`
4. For each IP, GET `http://<ip>/v1/info` with 1.5s timeout
5. 200-299 → unprotected device found, parse DeviceInfo JSON
6. 403 → password-protected device found
7. Also check FTP port 21 availability via `QTcpSocket::connectToHost()` with 1.5s timeout
8. Emit signal per discovered device

**Concurrency:** Use `QNetworkAccessManager` which handles concurrent requests internally. For FTP port checks, use a pool of `QTcpSocket` objects.

---

## Phase 7: File Manager

### Goal
FTP-based file browser with upload, download, delete, rename, mkdir, and disk image creation.

### Reference files
- `ViewControllers/FileManagerViewController.swift` → `src/ui/FileManagerWidget.h/cpp`

### FileManagerWidget

**Layout:**
- Toolbar: Upload, Download, Delete, Rename, New Folder, Create Disk Image
- Tree view (QTreeView with custom model) showing directory hierarchy
- Context menu on right-click with same operations
- Progress bar/overlay for FTP transfers

**Tree model:** Custom `QAbstractItemModel` backed by `FtpFileEntry` data. Lazy-load directories on expand.

**Disk image creation dialog (QDialog):**
- Format selector: D64, D71, D81, DNP
- Track count (for D64 and DNP): QSpinBox
- Filename: QLineEdit
- Create button → calls API endpoint

**Drag and drop:** Accept file drops from system file manager (`dragEnterEvent`/`dropEvent`). Upload dropped files via FTP.

---

## Phase 8: System Inspector Panel

### Goal
Device information display, configuration browser/editor, and drive management.

### Reference files
- `ViewControllers/SystemViewController.swift` → `src/ui/SystemPanel.h/cpp`

### SystemPanel (QWidget, placed in inspector)

**Sections (vertical layout with group boxes):**

1. **Device Info** (QFormLayout)
   - Product, Hostname, Firmware, FPGA Version, Core Version, Unique ID
   - Populated from `DeviceInfo` struct

2. **Configuration Browser**
   - Category dropdown (QComboBox)
   - Item list (QTableWidget: name, current value)
   - Value editor (QLineEdit + Apply button)
   - Save to Flash / Load from Flash / Reset to Default buttons

3. **Drives**
   - Drive list (QListWidget showing drive letter + mounted image)
   - Mount button → file browser filtered to disk images
   - Remove / Reset buttons

---

## Phase 9: Display & Audio Controls Panel

### Goal
CRT preset selection, shader parameter sliders, and audio volume/balance controls.

### Reference files
- `ViewControllers/DisplayAudioViewController.swift` → `src/ui/DisplayAudioPanel.h/cpp`
- `Settings/CRTPreset.swift` → `src/models/CrtPreset.h/cpp`

### DisplayAudioPanel (QWidget)

**Sections:**

1. **CRT Presets**
   - Dropdown (QComboBox) with 8 built-in + custom presets
   - Save As, Delete, Rename buttons for custom presets

2. **CRT Settings** (QFormLayout with QSliders)
   - Scanline Intensity (0-100 → 0.0-1.0)
   - Scanline Width (50-200 → 0.5-2.0)
   - Blur Radius (0-200 → 0.0-2.0)
   - Bloom Intensity (0-100 → 0.0-1.0)
   - Bloom Radius (50-300 → 0.5-3.0)
   - Afterglow Strength (0-100 → 0.0-1.0)
   - Afterglow Decay Speed (100-1000 → 1.0-10.0)
   - Tint Mode (QComboBox: Off/Amber/Green/Mono)
   - Tint Strength (0-100 → 0.0-1.0, hidden when tint off)
   - Mask Type (QComboBox: Off/Aperture Grille/Shadow Mask/Slot Mask)
   - Mask Intensity (0-100 → 0.0-1.0, hidden when mask off)
   - Curvature (0-100 → 0.0-1.0)
   - Vignette (0-100 → 0.0-1.0)

3. **Audio Controls**
   - Volume slider (0-100)
   - Balance slider (-100 to 100)
   - Mute checkbox

All sliders emit `valueChanged` → update `CrtSettings` → push uniforms to renderer.

---

## Phase 10: Debug Tools

### Goal
Memory browser (hex/ASCII view) and debug monitor (CPU registers, memory R/W).

### Reference files
- `ViewControllers/MemoryBrowserViewController.swift` → `src/ui/MemoryBrowserWidget.h/cpp`
- `ViewControllers/DebugMonitorViewController.swift` → `src/ui/DebugMonitorWidget.h/cpp`

### DebugPanel

Horizontal `QSplitter` containing MemoryBrowserWidget (left, 540px min) and DebugMonitorWidget (right, 200px min). Collapsible from the main window.

### MemoryBrowserWidget
- Address navigation (QLineEdit for hex address + Go button)
- Hex dump display (QTextEdit or custom widget, monospaced font)
- Format: `ADDR: XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX  |ASCII...........|`
- 16 bytes per row
- Current view indicator / memory map

### DebugMonitorWidget
- **Registers** (QFormLayout): A, X, Y, PC, SP, Flags (N/V/B/D/I/Z/C)
- Read address (QLineEdit) + length (QSpinBox) + Read button
- Write address (QLineEdit) + hex data (QLineEdit) + Write button
- Result display (hex dump)

---

## Phase 11: BASIC Editor

### Goal
BASIC program editor with syntax highlighting, tokenization, and device upload.

### Reference files
- `ViewControllers/BASICScratchpadViewController.swift` → `src/ui/BasicEditorWindow.h/cpp`
- `Views/BASICEditorView.swift` → `src/ui/BasicEditorWidget.h/cpp`
- `Models/BASICTokenizer.swift` → `src/models/BasicTokenizer.h/cpp`
- `Models/BASICSamples.swift` → `src/models/BasicSamples.h/cpp`

### BasicEditorWindow (QMainWindow or QDialog)

Separate floating window (like the original).

**Layout:**
- Menu: File (New, Open, Save, Save As, Templates submenu), Device (Upload, Load From Device, device selector)
- Central widget: BasicEditorWidget (QPlainTextEdit with syntax highlighter)

### BasicEditorWidget

Use `QSyntaxHighlighter` subclass for:
- BASIC keywords (bold or colored)
- String literals (different color)
- REM statements (comment color)
- Line numbers (different color)

### BASIC Tokenizer

See Appendix F for the full token table and algorithm. Key output:
```cpp
struct TokenizeResult {
    QByteArray programData;  // Binary BASIC program
    uint16_t endAddress;     // Value for BASIC variable pointer ($002D)
};
```

Upload flow:
1. Tokenize editor text → `TokenizeResult`
2. POST `/v1/runners:run_prg` with `programData`
3. Or: write to memory at $0801 and set $002D-$002E

---

## Phase 12: Keyboard Forwarding

### Goal
Forward keyboard input to the C64 via KERNAL keyboard buffer injection.

### Reference files
- `Network/C64KeyboardForwarder.swift` → `src/network/KeyboardForwarder.h/cpp`

### Mechanism
1. Toggle enabled via toolbar button
2. When enabled, MainWindow captures key events (override `keyPressEvent`)
3. Map Qt key codes to PETSCII (see Appendix G)
4. Queue PETSCII codes
5. Poll at 50ms intervals:
   - Read byte at $00C6 (keyboard buffer counter)
   - If counter < 10 and queue non-empty:
     - Write PETSCII byte to $0277 + counter
     - Write counter+1 to $00C6

### Key mapping (Qt → PETSCII)
- `Qt::Key_Return` → 0x0D
- `Qt::Key_Backspace` → 0x14 (DEL)
- `Qt::Key_Escape` → 0x03 (RUN/STOP)
- `Qt::Key_Home` → 0x13 (HOME)
- Arrow keys → cursor PETSCII codes
- F1-F8 → C64 function key codes
- See Appendix G for full table

---

## Phase 13: Recording & Screenshots

### Goal
Screenshot capture and video recording with synchronized audio.

### Reference files
- `Recording/MediaCapture.swift` → `src/recording/MediaCapture.h/cpp`

### Screenshots
1. Read pixels from OpenGL framebuffer (`glReadPixels`)
2. Convert to QImage (RGBA → QImage::Format_RGBA8888)
3. Show QFileDialog for save location
4. Save as PNG

### Video Recording (optional — requires FFmpeg libraries)

Use libavcodec/libavformat for encoding:
- Container: MP4 or MKV
- Video: H.264 (libx264)
- Audio: AAC
- Sample rate: 48kHz stereo

**Pipeline:**
1. Start: create output file, initialize encoder contexts
2. Per frame: read OpenGL framebuffer → encode video frame with timestamp
3. Per audio packet: encode audio samples with timestamp
4. Stop: flush encoders, write trailer, close file, prompt save dialog

**Timing:** Both video and audio timestamps based on elapsed time from recording start (using `QElapsedTimer`).

If FFmpeg is not available at build time, disable recording but keep screenshot support.

---

## Phase 14: Settings & Persistence

### Goal
Save/restore all user preferences and CRT presets.

### Reference files
- `Settings/PresetManager.swift` → `src/settings/PresetManager.h/cpp`

### QSettings keys
- `audio/volume` (float, 0.0-1.0)
- `audio/balance` (float, -1.0 to 1.0)
- `recent/viewer` (JSON string array of recent viewer sessions)
- `recent/toolbox` (JSON string array of recent toolbox sessions)
- `window/geometry` (QByteArray)
- `window/splitterState` (QByteArray)

### Preset Storage

**Location:** `~/.config/c64-ultimate-toolbox/presets.json`

**JSON format (same as original):**
```json
{
  "customPresets": [
    {
      "id": "<UUID>",
      "name": "My Preset",
      "settings": {
        "scanlineIntensity": 0.5,
        "scanlineWidth": 1.0,
        "blurRadius": 0.0,
        "bloomIntensity": 0.3,
        "bloomRadius": 1.5,
        "afterglowStrength": 0.0,
        "afterglowDecaySpeed": 5.0,
        "tintMode": 0,
        "tintStrength": 0.5,
        "maskType": 0,
        "maskIntensity": 0.5,
        "curvatureAmount": 0.0,
        "vignetteStrength": 0.0
      }
    }
  ],
  "builtInOverrides": {},
  "selectedIdentifier": { "builtIn": "Clean" }
}
```

Save on change with 500ms debounce (QTimer::singleShot).

### 8 Built-In Presets

1. **Clean** — all zeros (no effects)
2. **Home CRT** — scanlines 0.35, width 1.0, blur 0.5, bloom 0.25/1.5, shadow mask 0.4, curvature 0.15, vignette 0.2
3. **P3 Amber** — scanlines 0.5, width 0.8, afterglow 0.4/4.0, amber tint 0.8, aperture grille 0.3
4. **P1 Green** — scanlines 0.5, width 0.8, afterglow 0.5/3.5, green tint 0.85, aperture grille 0.35
5. **Crisp** — scanlines 0.15 only
6. **Warm Glow** — bloom 0.3/1.5, afterglow 0.3/5.0, shadow mask 0.25, vignette 0.15
7. **Old TV** — scanlines 0.45, bloom 0.35/2.0, shadow mask 0.5, curvature 0.25, vignette 0.3
8. **Arcade** — bloom 0.4/2.0, aperture grille 0.45, curvature 0.2, vignette 0.25

(Exact values should be verified against `Settings/CRTPreset.swift` during implementation.)

---

## Phase 15: Polish & Testing

### Tasks
1. Test with actual C64 Ultimate device (Toolbox + Viewer modes)
2. Verify all REST API endpoints work correctly
3. Test FTP operations (upload, download, recursive directory upload)
4. Verify CRT shader matches original visual quality
5. Test audio playback (latency, underruns, volume/balance)
6. Test keyboard forwarding
7. Test recording (if FFmpeg available)
8. Verify preset save/load
9. Test device scanner on different network configurations
10. Handle edge cases: device disconnect, network errors, timeout recovery
11. Desktop integration: .desktop file, icon, app metadata
12. Package: AppImage, Flatpak, or deb/rpm

---

## Appendix A: REST API Reference

**Base URL:** `http://<host>/`

### Endpoints

| Method | URL | Body | Response | Notes |
|--------|-----|------|----------|-------|
| GET | `/v1/info` | — | JSON: product, firmware, fpga, core, hostname, uniqueId | |
| PUT | `/v1/streams/<name>:start?ip=<IP>:<port>` | — | — | name: "video" or "audio" |
| PUT | `/v1/streams/<name>:stop` | — | — | |
| POST | `/v1/runners:sidplay` | binary | — | Content-Type: application/octet-stream |
| POST | `/v1/runners:modplay` | binary | — | |
| POST | `/v1/runners:run_prg` | binary | — | |
| POST | `/v1/runners:run_crt` | binary | — | |
| PUT | `/v1/runners:run_prg?file=<path>` | — | — | path percent-encoded |
| PUT | `/v1/runners:load_prg?file=<path>` | — | — | |
| PUT | `/v1/runners:sidplay?file=<path>` | — | — | |
| PUT | `/v1/runners:modplay?file=<path>` | — | — | |
| PUT | `/v1/runners:run_crt?file=<path>` | — | — | |
| GET | `/v1/drives` | — | JSON: {drives: [...]} | |
| PUT | `/v1/drives/<d>:mount?image=<path>` | — | — | |
| PUT | `/v1/drives/<d>:remove` | — | — | |
| PUT | `/v1/drives/<d>:reset` | — | — | |
| PUT | `/v1/files/<path>:create_d64?tracks=<n>` | — | — | default 35 tracks |
| PUT | `/v1/files/<path>:create_d71` | — | — | |
| PUT | `/v1/files/<path>:create_d81` | — | — | |
| PUT | `/v1/files/<path>:create_dnp?tracks=<n>` | — | — | |
| GET | `/v1/configs` | — | JSON: {categories: [...]} | |
| GET | `/v1/configs/<cat>` | — | JSON: {cat: {item: val, ...}} | |
| GET | `/v1/configs/<cat>/<item>` | — | JSON: {cat: {item: {detail}}} | |
| PUT | `/v1/configs/<cat>/<item>?value=<v>` | — | — | |
| PUT | `/v1/configs:save_to_flash` | — | — | |
| PUT | `/v1/configs:load_from_flash` | — | — | |
| PUT | `/v1/configs:reset_to_default` | — | — | May return 502 |
| PUT | `/v1/machine:reset` | — | — | |
| PUT | `/v1/machine:reboot` | — | — | May timeout |
| PUT | `/v1/machine:poweroff` | — | — | |
| PUT | `/v1/machine:menu_button` | — | — | |
| PUT | `/v1/machine:pause` | — | — | |
| PUT | `/v1/machine:resume` | — | — | |
| GET | `/v1/machine:readmem?address=<HEX>&length=<n>` | — | binary | 4-digit hex addr |
| POST | `/v1/machine:writemem?address=<HEX>` | binary | — | |
| PUT | `/v1/machine:writemem?address=<HEX>&data=<HEX>` | — | — | hex string data |

**Authentication:** `X-Password: <password>` header. 403 = auth required.

**Path encoding:** Encode `/:&=+` in query values.

---

## Appendix B: FTP Protocol Details

**Port:** 21, passive mode only.

### Commands Used
USER, PASS, TYPE I, PASV, CWD, PWD, LIST, MKD, DELE, RMD, RNFR, RNTO, STOR, RETR, SIZE

### PASV Response Parsing
Format: `227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)`
- Port = p1 × 256 + p2
- **Important:** Ignore h1-h4 and use the original host IP instead (device may report internal IP)

### Directory Listing Format
Unix `ls -l` style:
```
drwxr-xr-x  1 user group     0 Jan 01 12:00 dirname
-rw-r--r--  1 user group 12345 Jan 01 12:00 filename.prg
```
Parse: permissions (d prefix = directory), size, date, name (last field, may contain spaces — take everything after the date).

### Transfer
- 65536-byte chunks
- Binary mode (TYPE I)
- Close data connection when transfer complete

---

## Appendix C: UDP Video Protocol

**Port:** 11000 (default)

### Packet Layout (780 bytes total, little-endian)
```
Bytes 0-1:   sequenceNum (uint16_t)
Bytes 2-3:   frameNum (uint16_t)
Bytes 4-5:   lineInfo (uint16_t)
               bits 0-14: lineNum (actual raster line number)
               bit 15:    isLastPacket (1 = final packet of frame)
Bytes 6-11:  reserved
Bytes 12-779: payload (768 bytes = 4 lines × 192 bytes/line)
```

### Pixel Format
Each payload byte = 2 pixels (4-bit indexed color):
- Low nibble (bits 0-3) = left pixel color index
- High nibble (bits 4-7) = right pixel color index

### Frame Dimensions
- Width: 384 pixels
- Height: 272 (PAL) or 240 (NTSC)
- Auto-detect: if max line ≤ 240, assume NTSC

### Frame Assembly
1. Track current frameNum; new frameNum → finalize previous frame
2. Store each packet's payload at its lineNum offset
3. On last packet (bit 15 set) or 100ms timeout → assemble:
   - Fill missing lines by duplicating nearest received line above
   - Convert indexed pixels → RGBA via lookup table

---

## Appendix D: UDP Audio Protocol

**Port:** 11001 (default)

### Packet Layout (770 bytes, little-endian)
```
Bytes 0-1:   sequenceNum (uint16_t)
Bytes 2-769: pcmData (768 bytes = 192 stereo samples)
```

### PCM Format
- 192 samples per packet
- 16-bit signed integer, little-endian
- Stereo interleaved: L0, R0, L1, R1, ...
- 4 bytes per sample pair
- Sample rate: ~48000 Hz (47940.34 NTSC / 47982.89 PAL)

---

## Appendix E: CRT Shader Specification

### Processing Order (fragment shader)

1. **Curvature (barrel distortion)**
   - Center UV at (0.5, 0.5)
   - Apply: `uv = uv + uv * dot(uv, uv) * curvatureAmount * 0.25`
   - Shift back; clamp to [0,1]; return black if out of bounds

2. **Sampling**
   - If blurRadius > 0: 9-tap Gaussian blur (center + 4 cardinal + 4 diagonal)
   - Else: snap UV to pixel center for sharp sampling

3. **Bloom (13-tap)**
   - Weights: center 0.16, 4 near 0.10, 4 diagonal 0.06, 4 far 0.05
   - Scale by luminance: `smoothstep(0.1, 0.8, luma)`
   - Shoulder compression: `min(b, 0.8) + clamp(b-0.8)/(1+2.5*clamp(b-0.8))`
   - Blend: `color + bloom * bloomIntensity`

4. **Scanlines**
   - Source line = uv.y × sourceHeight
   - Fractional position mapped through: `pow(sin(π × frac), exponent)`
   - Exponent = lerp(1.0, 8.0, scanlineWidth)
   - Bright pixels (luma > 0.5) resist darkening
   - Apply: `color *= mix(1.0, scanline, scanlineIntensity)`

5. **Phosphor Mask**
   - **Aperture grille (type 1):** Vertical RGB stripes, pixel.x % 3
   - **Shadow mask (type 2):** Row-offset RGB triads, pixel.x % 3 with row.y % 2 offset
   - **Slot mask (type 3):** Triads with horizontal gap every 3rd row
   - Apply: `color *= mix(vec3(1), maskColor, maskIntensity)`

6. **Tint**
   - Amber (mode 1): `vec3(1.0, 0.75, 0.2)` weighted by luminance
   - Green (mode 2): `vec3(0.15, 1.0, 0.15)` weighted by luminance
   - Mono (mode 3): `vec3(luminance)`
   - Apply: `color = mix(color, tinted, tintStrength)`

7. **Vignette**
   - Distance from center: `length((uv - 0.5) * 2.0)`
   - Factor: `smoothstep(1.3, 0.5, dist)`
   - Apply: `color *= mix(1.0, vignette, vignetteStrength)`

8. **Afterglow (phosphor persistence)**
   - Read previous frame from accumulation texture
   - Per-channel decay: `exp(-(1.0, 1.25, 1.5) * decaySpeed * dt)`
   - Persisted = previous × decay rates
   - Apply: `color = max(color, persisted * afterglowStrength)`

### Vertex Shader
Fullscreen quad: 2 triangles covering NDC [-1,1]. Pass UV coordinates [0,1] to fragment.

---

## Appendix F: BASIC Tokenizer Specification

### Token Table (BASIC V2, values 128-203)

```
128: end       129: for       130: next      131: data
132: input#    133: input     134: dim       135: read
136: let       137: goto      138: run       139: if
140: restore   141: gosub     142: return    143: rem
144: stop      145: on        146: wait      147: load
148: save      149: verify    150: def       151: poke
152: print#    153: print     154: cont      155: list
156: clr       157: cmd       158: sys       159: open
160: close     161: get       162: new       163: tab(
164: to        165: fn        166: spc(      167: then
168: not       169: step      170: +         171: -
172: *         173: /         174: ^         175: and
176: or        177: >         178: =         179: <
180: sgn       181: int       182: abs       183: usr
184: fre       185: pos       186: sqr       187: rnd
188: log       189: exp       190: cos       191: sin
192: tan       193: atn       194: peek      195: len
196: str$      197: val       198: asc       199: chr$
200: left$     201: right$    202: mid$      203: go
```

### Tokenization Algorithm
1. Split input by newlines, trim whitespace
2. For each line:
   a. Parse line number from leading digits (error if none)
   b. Convert remaining text to lowercase for matching
   c. Iterate character by character:
      - If inside quotes or after REM: emit raw PETSCII, no tokenization
      - If `{...}` escape: look up special code (see below), emit byte
      - Try longest-match against token table (greedy, longest keywords first)
      - If match: emit token byte
      - Else: convert character to PETSCII and emit
   d. Append 0x00 line terminator
3. Build program binary:
   - Base address: $0801
   - Each line: [2-byte next-line pointer LE] [2-byte line number LE] [tokenized bytes] [0x00]
   - End marker: [0x0000]
4. Return program data and end address (for $002D/$002E)

### Special Escape Codes
```
{rvs on}  → 0x12    {rvs off} → 0x92
{clr}     → 0x93    {home}    → 0x13
{up}      → 0x91    {down}    → 0x11
{left}    → 0x9D    {right}   → 0x1D
{del}     → 0x14    {inst}    → 0x94
{blk}     → 0x90    {wht}     → 0x05
{red}     → 0x1C    {cyn}     → 0x9F
{pur}     → 0x9C    {grn}     → 0x1E
{blu}     → 0x1F    {yel}     → 0x9E
{org}     → 0x81    {brn}     → 0x95
{lred}    → 0x96    {dgry}    → 0x97
{mgry}    → 0x98    {lgrn}    → 0x99
{lblu}    → 0x9A    {lgry}    → 0x9B
```

### PETSCII Mapping
- ASCII 0x20 (space): pass-through
- ASCII 0x30-0x40 (digits, symbols): pass-through
- ASCII 0x41-0x5A (uppercase): → 0xC1-0xDA
- ASCII 0x61-0x7A (lowercase): → 0x41-0x5A
- `[`, `]`, `@`: pass-through

---

## Appendix G: Keyboard Mapping

### Qt Key → PETSCII

| Qt Key | PETSCII | C64 Key |
|--------|---------|---------|
| Key_Return | 0x0D | RETURN |
| Key_Backspace | 0x14 | DEL |
| Key_Escape | 0x03 | RUN/STOP |
| Key_Home | 0x13 | HOME |
| Key_Up | 0x91 | CRSR UP |
| Key_Down | 0x11 | CRSR DOWN |
| Key_Left | 0x9D | CRSR LEFT |
| Key_Right | 0x1D | CRSR RIGHT |
| Key_F1 | 0x85 | F1 |
| Key_F2 | 0x89 | F2 |
| Key_F3 | 0x86 | F3 |
| Key_F4 | 0x8A | F4 |
| Key_F5 | 0x87 | F5 |
| Key_F6 | 0x8B | F6 |
| Key_F7 | 0x88 | F7 |
| Key_F8 | 0x8C | F8 |
| Key_Insert | 0x94 | INST |
| Key_Delete | 0x14 | DEL |

### Character Mapping
- Lowercase a-z → PETSCII 0x41-0x5A (displayed as uppercase on C64)
- Uppercase A-Z → PETSCII 0xC1-0xDA (displayed as shifted/graphics chars)
- Digits 0-9 → pass-through
- Space → 0x20
- Common symbols → pass-through where PETSCII matches ASCII

### Special Keys (on-screen KeyStrip)
| Label | PETSCII |
|-------|---------|
| RUN/STOP | 0x03 |
| HOME | 0x13 |
| CLR | 0x93 |
| DEL | 0x14 |
| INST | 0x94 |
| £ | 0x1C |
| ↑ | 0x5E |
| ← | 0x5F |
| Shift+RETURN | 0x8D |

### Injection Mechanism
- Buffer: $0277-$0280 (10 bytes max)
- Counter: $00C6
- Poll every 50ms:
  1. Read $00C6
  2. If < 10 and keys queued: write PETSCII to $0277+counter, write counter+1 to $00C6

---

## Appendix H: C64 Color Palette

### 16 Colors (RGBA, 0xRRGGBBAA format)

| Index | Name | R | G | B | Hex |
|-------|------|---|---|---|-----|
| 0 | Black | 0 | 0 | 0 | #000000 |
| 1 | White | 247 | 247 | 247 | #F7F7F7 |
| 2 | Red | 141 | 47 | 52 | #8D2F34 |
| 3 | Cyan | 106 | 212 | 205 | #6AD4CD |
| 4 | Purple | 152 | 53 | 164 | #9835A4 |
| 5 | Green | 76 | 180 | 66 | #4CB442 |
| 6 | Blue | 44 | 41 | 177 | #2C29B1 |
| 7 | Yellow | 239 | 239 | 93 | #EFEF5D |
| 8 | Orange | 152 | 78 | 32 | #984E20 |
| 9 | Brown | 91 | 56 | 0 | #5B3800 |
| 10 | Pink | 209 | 103 | 109 | #D1676D |
| 11 | Dark Grey | 74 | 74 | 74 | #4A4A4A |
| 12 | Medium Grey | 123 | 123 | 123 | #7B7B7B |
| 13 | Light Green | 159 | 239 | 147 | #9FEF93 |
| 14 | Light Blue | 109 | 106 | 239 | #6D6AEF |
| 15 | Light Grey | 178 | 178 | 178 | #B2B2B2 |

### Lookup Table Construction

Pre-compute a 256-entry table where each entry maps one payload byte to two RGBA pixel values:

```cpp
struct PixelPair {
    uint32_t left;   // RGBA for low nibble
    uint32_t right;  // RGBA for high nibble
};

PixelPair lut[256];

for (int i = 0; i < 256; i++) {
    lut[i].left  = palette[i & 0x0F];
    lut[i].right = palette[(i >> 4) & 0x0F];
}
```

This allows converting the entire frame without per-pixel branching.
