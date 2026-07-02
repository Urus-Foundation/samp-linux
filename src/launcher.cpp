#include "launcher.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

Launcher::Launcher(QObject *parent) : QObject(parent)
{
}

bool Launcher::validatePaths(const QString &wineBin, const QString &gtaDir, QString *error)
{
    if (wineBin.trimmed().isEmpty()) {
        if (error) *error = tr("Wine binary is not configured. Open Settings and set it "
                                "(e.g. \"wine\" or a Proton \"run\" script).");
        return false;
    }
    if (gtaDir.trimmed().isEmpty() || !QDir(gtaDir).exists()) {
        if (error) *error = tr("GTA San Andreas folder is not set or does not exist. "
                                "Open Settings and point it at your GTA SA installation.");
        return false;
    }
    const QFileInfo sampExe(QDir(gtaDir).filePath("samp.exe"));
    if (!sampExe.exists()) {
        if (error) *error = tr("samp.exe was not found in the configured GTA San Andreas "
                                "folder. Make sure SA:MP is installed there.");
        return false;
    }
    return true;
}

Launcher::LaunchResult Launcher::launch(const QString &ip,
                                         quint16 port,
                                         const QString &nickname,
                                         const QString &password) const
{
    LaunchResult result;

    QSettings settings;
    const QString wineBin = settings.value("paths/wineBin", "wine").toString();
    const QString gtaDir = settings.value("paths/gtaDir").toString();

    QString error;
    if (!validatePaths(wineBin, gtaDir, &error)) {
        result.started = false;
        result.error = error;
        return result;
    }

    // SA:MP's samp.exe command line is:  samp.exe host:port [password]
    // The nickname is NOT a command-line argument - it lives in the Windows
    // registry (HKCU\Software\SAMP\PlayerName), exactly like the official
    // Windows client stores it. Passing the nickname as a positional arg
    // (as older versions of this launcher did) shifts it into the password
    // slot and causes a bogus "Wrong server password" on connect.
    if (!nickname.trimmed().isEmpty()) {
        const QStringList regArgs = {
            "reg", "add", "HKEY_CURRENT_USER\\Software\\SAMP",
            "/v", "PlayerName",
            "/d", nickname.trimmed(),
            "/f"
        };
        QProcess regProcess;
        regProcess.setProgram(wineBin);
        regProcess.setArguments(regArgs);
        regProcess.start();
        regProcess.waitForFinished(5000); // quick, blocking: must finish before samp.exe reads it
    }

    QStringList args;
    args << QDir(gtaDir).filePath("samp.exe");
    args << QString("%1:%2").arg(ip).arg(port);
    if (!password.trimmed().isEmpty())
        args << password.trimmed();

    // Make sure Wine finds the right prefix if the user configured one.
    // We set it in this process's environment before launching detached,
    // which is inherited by the child, then leave it as-is afterwards
    // (harmless for the lifetime of the launcher process).
    const QString winePrefix = settings.value("paths/winePrefix").toString();
    if (!winePrefix.trimmed().isEmpty())
        qputenv("WINEPREFIX", winePrefix.trimmed().toLocal8Bit());

    qint64 pid = 0;
    const bool ok = QProcess::startDetached(wineBin, args, gtaDir, &pid);

    if (!ok) {
        result.started = false;
        result.error = tr("Failed to start Wine. Check that \"%1\" is a valid, executable binary.")
                            .arg(wineBin);
        return result;
    }

    result.started = true;
    result.pid = pid;
    return result;
}
