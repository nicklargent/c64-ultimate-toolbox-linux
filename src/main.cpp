#include "app/Application.h"
#include "ui/ConnectDialog.h"
#include "ui/MainWindow.h"
#include "network/C64Connection.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    auto *connection = new C64Connection(&app);

    auto *dialog = new ConnectDialog(connection);

    QObject::connect(dialog, &ConnectDialog::connectedToolbox, [connection]() {
        auto *window = new MainWindow(connection);
        window->setMode(ConnectionMode::Toolbox);
        window->show();
    });

    QObject::connect(dialog, &ConnectDialog::connectedViewer, [connection]() {
        auto *window = new MainWindow(connection);
        window->setMode(ConnectionMode::Viewer);
        window->show();
    });

    dialog->show();

    return app.exec();
}
