#include "app/Application.h"
#include "ui/MainWindow.h"
#include "network/C64Connection.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    auto *connection = new C64Connection(&app);
    auto *window = new MainWindow(connection);
    window->show();

    return app.exec();
}
