#pragma once

#include <QObject>
#include <QString>

// Application version — also used in the HTTP User-Agent header.
#define SAMPLINUX_VERSION "0.1.1"

// Builds and runs the Wine command that starts GTA:SA + SA:MP.
//
// SA:MP (samp.dll injected into gta_sa.exe) is Windows-only, so on Linux it
// runs through Wine or a Proton "run" wrapper.  This class launches whatever
// the user has configured in Settings — it does not embed or download any
// game files.
//
// Nickname handling:
//   SA:MP reads the player name from the Windows registry key
//   HKCU\Software\SAMP\PlayerName, not from the command line.  Before
//   starting the game we write the value via "wine reg add …" so the
//   correct nickname is in place when samp.exe reads it.
//
// WINEPREFIX:
//   If the user configured a custom prefix we set WINEPREFIX in the current
//   process environment before calling QProcess::startDetached.  The value
//   is inherited by the child process.  It is not restored afterwards, which
//   is acceptable for the lifetime of the launcher process.
//
// Error reporting:
//   LaunchResult carries both a boolean and a human-readable error string.
//   The caller is responsible for surfacing errors to the user (e.g. via
//   QMessageBox).  Wine process exit codes and crash logs are NOT currently
//   captured — the game runs fully detached.
class Launcher : public QObject
{
    Q_OBJECT
public:
    explicit Launcher(QObject *parent = nullptr);

    struct LaunchResult {
        bool    started = false;
        qint64  pid     = 0;
        QString error;
    };

    // Attempt to launch the game connecting to the given server.
    // Returns immediately; the game process is detached.
    LaunchResult launch(const QString &ip,
                        quint16        port,
                        const QString &nickname,
                        const QString &password) const;

private:
    // Returns false and populates *error if required paths are missing or
    // obviously wrong (no samp.exe found, empty wine path, etc.).
    static bool validatePaths(const QString &wineBin,
                              const QString &gtaDir,
                              QString       *error);

    // Writes PlayerName to the Wine registry.  Non-fatal: if this step
    // fails the game will still launch with whatever name was last saved.
    static void writeNicknameToRegistry(const QString &wineBin, const QString &nickname);
};
