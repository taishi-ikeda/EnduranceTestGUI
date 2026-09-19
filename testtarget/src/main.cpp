#include <QApplication>

#include "TestTargetWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TestTarget");
    app.setOrganizationName("asobi");

    TestTargetWindow window;
    window.show();

    return app.exec();
}
