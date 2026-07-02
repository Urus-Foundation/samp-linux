#pragma once

#include <QDialog>

class QLineEdit;
class QDialogButtonBox;

// Global settings: nickname + Wine/GTA paths, persisted via QSettings.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString nickname() const;

private slots:
    void browseGtaDir();
    void browseWineBin();
    void browseWinePrefix();
    void onAccept();

private:
    QLineEdit *m_nicknameEdit;
    QLineEdit *m_gtaDirEdit;
    QLineEdit *m_wineBinEdit;
    QLineEdit *m_winePrefixEdit;

    void loadSettings();
    void saveSettings();
};
