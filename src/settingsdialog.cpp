#include "settingsdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    resize(520, 320);

    auto *mainLayout = new QVBoxLayout(this);

    // ---- Identity group ----
    auto *identityGroup = new QGroupBox(tr("Identity"), this);
    auto *identityForm = new QFormLayout(identityGroup);
    m_nicknameEdit = new QLineEdit(identityGroup);
    m_nicknameEdit->setPlaceholderText(tr("Your in-game nickname"));
    identityForm->addRow(tr("Nickname:"), m_nicknameEdit);
    mainLayout->addWidget(identityGroup);

    // ---- Paths group ----
    auto *pathsGroup = new QGroupBox(tr("Game && Wine paths"), this);
    auto *pathsForm = new QFormLayout(pathsGroup);

    auto makePathRow = [this, pathsGroup](const QString &placeholder, QLineEdit *&edit, const char *slot) -> QWidget* {
        auto *row = new QWidget(pathsGroup);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        edit->setPlaceholderText(placeholder);
        auto *browseBtn = new QPushButton(tr("Browse..."), row);
        rowLayout->addWidget(edit);
        rowLayout->addWidget(browseBtn);
        connect(browseBtn, SIGNAL(clicked()), this, slot);
        return row;
    };

    pathsForm->addRow(tr("GTA San Andreas folder:"),
                       makePathRow(tr("/home/you/Games/GTA-SanAndreas"), m_gtaDirEdit, SLOT(browseGtaDir())));
    pathsForm->addRow(tr("Wine binary:"),
                       makePathRow(tr("wine  (or a Proton \"run\" script)"), m_wineBinEdit, SLOT(browseWineBin())));
    pathsForm->addRow(tr("WINEPREFIX (optional):"),
                       makePathRow(tr("/home/you/.wine"), m_winePrefixEdit, SLOT(browseWinePrefix())));

    auto *hint = new QLabel(tr("The GTA San Andreas folder must contain gta_sa.exe and samp.exe "
                                "(installed via the SA:MP client setup)."), pathsGroup);
    hint->setObjectName("MutedLabel");
    hint->setWordWrap(true);
    pathsForm->addRow(QString(), hint);

    mainLayout->addWidget(pathsGroup);
    mainLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    loadSettings();
}

void SettingsDialog::browseGtaDir()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select GTA San Andreas folder"),
                                                            m_gtaDirEdit->text());
    if (!dir.isEmpty())
        m_gtaDirEdit->setText(dir);
}

void SettingsDialog::browseWineBin()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select Wine binary / launch script"),
                                                        m_wineBinEdit->text());
    if (!file.isEmpty())
        m_wineBinEdit->setText(file);
}

void SettingsDialog::browseWinePrefix()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select WINEPREFIX folder"),
                                                            m_winePrefixEdit->text());
    if (!dir.isEmpty())
        m_winePrefixEdit->setText(dir);
}

void SettingsDialog::loadSettings()
{
    QSettings settings;
    m_nicknameEdit->setText(settings.value("identity/nickname").toString());
    m_gtaDirEdit->setText(settings.value("paths/gtaDir").toString());
    m_wineBinEdit->setText(settings.value("paths/wineBin", "wine").toString());
    m_winePrefixEdit->setText(settings.value("paths/winePrefix").toString());
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("identity/nickname", m_nicknameEdit->text().trimmed());
    settings.setValue("paths/gtaDir", m_gtaDirEdit->text().trimmed());
    settings.setValue("paths/wineBin", m_wineBinEdit->text().trimmed());
    settings.setValue("paths/winePrefix", m_winePrefixEdit->text().trimmed());
}

void SettingsDialog::onAccept()
{
    saveSettings();
    accept();
}

QString SettingsDialog::nickname() const
{
    return m_nicknameEdit->text().trimmed();
}
