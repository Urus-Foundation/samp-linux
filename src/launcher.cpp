#include "launcher.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLauncher, "samplinux.launcher")

Launcher::Launcher(QObject *parent) : QObject(parent) {}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool Launcher::validatePaths(const QString &wineBin, const QString &gtaDir, QString *error)
{
    if (wineBin.trimmed().isEmpty()) {
        if (error)
            *error = tr("Wine binary is not configured. Open Settings and set it "
                        "(e.g. \"wine\" or a Proton \"run\" script).");
        return false;
    }

    if (gtaDir.trimmed().isEmpty() || !QDir(gtaDir).exists()) {
        if (error)
            *error = tr("GTA San Andreas folder is not set or does not exist. "
                        "Open Settings and point it at your GTA SA installation.");
        return false;
    }

    if (!QFileInfo(QDir(gtaDir).filePath("samp.exe")).exists()) {
        if (error)
            *error = tr("samp.exe was not found in the configured GTA San Andreas folder. "
                        "Make sure SA:MP is installed there.");
        return false;
    }

    return true;
}

// Write the player nickname to HKCU\Software\SAMP\PlayerName via wine reg.
// This is a blocking call (up to 5 s) that must complete before samp.exe
// starts — otherwise the game reads a stale or empty value.
void Launcher::writeNicknameToRegistry(const QString &wineBin, const QString &nickname)
{
    const QString name = nickname.trimmed();
    if (name.isEmpty())
        return;

    QProcess reg;
    reg.setProgram(wineBin);
    reg.setArguments({
        "reg", "add",
        "HKEY_CURRENT_USER\\Software\\SAMP",
        "/v", "PlayerName",
        "/d", name,
        "/f"
    });
    reg.start();
    if (!reg.waitForFinished(5000)) {
        qCWarning(lcLauncher) << "wine reg timed out while writing PlayerName";
        reg.kill();
    } else if (reg.exitCode() != 0) {
        qCWarning(lcLauncher) << "wine reg exited with code" << reg.exitCode()
                              << "— nickname may not have been saved";
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Launcher::LaunchResult Launcher::launch(const QString &ip,
                                        quint16        port,
                                        const QString &nickname,
                                        const QString &password) const
{
    QSettings settings;
    const QString wineBin    = settings.value("paths/wineBin",  "wine").toString();
    const QString gtaDir     = settings.value("paths/gtaDir").toString();
    const QString winePrefix = settings.value("paths/winePrefix").toString();

    QString error;
    if (!validatePaths(wineBin, gtaDir, &error))
        return { false, 0, error };

    // Set WINEPREFIX before launching so the child process inherits it.
    if (!winePrefix.trimmed().isEmpty())
        qputenv("WINEPREFIX", winePrefix.trimmed().toLocal8Bit());

    writeNicknameToRegistry(wineBin, nickname);

    // SA:MP command line:  samp.exe host:port [password]
    // The nickname is intentionally NOT passed here — see header comment.
    QStringList args;
    args << QDir(gtaDir).filePath("samp.exe");
    args << QString("%1:%2").arg(ip).arg(port);
    if (!password.trimmed().isEmpty())
        args << password.trimmed();

    qint64 pid  = 0;
    const bool ok = QProcess::startDetached(wineBin, args, gtaDir, &pid);

    if (!ok) {
        return { false, 0,
                 tr("Failed to start Wine. Check that \"%1\" is a valid executable.").arg(wineBin) };
    }

    qCInfo(lcLauncher) << "Launched" << wineBin << "PID" << pid
                       << "→" << ip << ":" << port;
    return { true, pid, {} };
}
