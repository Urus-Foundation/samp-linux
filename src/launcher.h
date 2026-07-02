#pragma once

#define LAUNCHER_VERSION "0.1.0"

#include <QObject>
#include <QString>

// Builds and runs the command used to start GTA:SA + SA:MP through Wine
// (or Proton, if the user points Settings at a proton "run" wrapper).
//
// SA:MP itself is a Windows-only mod (samp.dll injected into gta_sa.exe),
// so on Linux it is normally run through Wine. This class does not embed
// any game files - it only launches what the user already has installed,
// using paths configured in Settings.
class Launcher : public QObject
{
    Q_OBJECT
public:
    explicit Launcher(QObject *parent = nullptr);

    struct LaunchResult {
        bool started = false;
        QString error;
    };

    // ip/port/nickname/password describe the server to connect to.
    // Returns immediately; actual process runs detached so the launcher
    // window is not blocked while the game is running.
    LaunchResult launch(const QString &ip,
                         quint16 port,
                         const QString &nickname,
                         const QString &password) const;

private:
    static bool validatePaths(const QString &wineBin, const QString &gtaDir, QString *error);
};
