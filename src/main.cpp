#include <QApplication>
#include <QFile>
#include <QCoreApplication>

#include "launcher.h"      // SAMPLINUX_VERSION
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // These three values determine where QSettings and QStandardPaths store
    // application data (~/.local/share/samp-linux/, etc.).
    QCoreApplication::setOrganizationName("SA:MP Linux Launcher");
    QCoreApplication::setApplicationName("samp-linux");
    QCoreApplication::setApplicationVersion(SAMPLINUX_VERSION);

    QFile styleFile(":/styles/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    MainWindow window;
    window.show();

    return app.exec();
}
