#include "app/Application.h"
#include "app/Log.h"

Q_LOGGING_CATEGORY(logApp, "c64.app")
Q_LOGGING_CATEGORY(logNetwork, "c64.network")
Q_LOGGING_CATEGORY(logVideo, "c64.video")
Q_LOGGING_CATEGORY(logAudio, "c64.audio")
Q_LOGGING_CATEGORY(logFtp, "c64.ftp")

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("C64 Ultimate Toolbox");
    setOrganizationName("c64-ultimate-toolbox");
    setOrganizationDomain("c64-ultimate-toolbox.local");
}
