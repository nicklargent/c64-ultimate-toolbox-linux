cmake_minimum_required(VERSION 3.22)
project(c64-ultimate-toolbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia OpenGL OpenGLWidgets)
qt_standard_project_setup()

add_executable(c64-ultimate-toolbox
    src/main.cpp
    src/app/Application.cpp

    # Models
    src/models/ConnectionMode.cpp

    # Video
    src/video/ColorPalette.cpp
    src/video/FrameAssembler.cpp
    src/video/CrtRenderer.cpp

    # Audio
    src/audio/AudioPlayer.cpp

    # Network
    src/network/C64ApiClient.cpp
    src/network/FtpClient.cpp
    src/network/UdpVideoReceiver.cpp
    src/network/UdpAudioReceiver.cpp
    src/network/C64Connection.cpp
    src/network/DeviceScanner.cpp
    src/network/KeyboardForwarder.cpp

    # UI
    src/ui/MainWindow.cpp
    src/ui/VideoWidget.cpp
    src/ui/StatusBarWidget.cpp
    src/ui/KeyStripWidget.cpp

    # Resources
    resources/shaders.qrc
)

target_include_directories(c64-ultimate-toolbox PRIVATE src)

target_link_libraries(c64-ultimate-toolbox PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
