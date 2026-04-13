#include "app/Application.h"

#include <QMainWindow>

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("C64 Ultimate Toolbox");
    window.setMinimumSize(700, 450);
    window.resize(1200, 750);
    window.show();

    return app.exec();
}
