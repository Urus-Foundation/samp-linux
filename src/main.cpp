#include <QApplication>
#include <QFile>
#include <QCoreApplication>

#include "launcher.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("SA:MP Linux Launcher");
    QCoreApplication::setApplicationName("SA:MP Linux");
    QCoreApplication::setApplicationVersion(LAUNCHER_VERSION);

    QFile styleFile(":/styles/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
