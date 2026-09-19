#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EnduranceTestGUI");
    app.setOrganizationName("asobi");

    MainWindow window;
    window.show();

    return app.exec();
}
