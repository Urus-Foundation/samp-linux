#include "serverpropertiesdialog.h"
#include "qutils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QFont>

ServerPropertiesDialog::ServerPropertiesDialog(const ServerInfo &info, QWidget *parent)
    : QDialog(parent)
    , m_info(info)
{
    setWindowTitle(tr("Server Properties - (%1)")
                       .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname));
    setMinimumWidth(380);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *nameLabel = new QLabel(info.hostname.isEmpty() ? tr("(unnamed server)") : info.hostname, this);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignHCenter);
    mainLayout->addWidget(nameLabel);

    auto *infoForm = new QFormLayout();
    infoForm->setLabelAlignment(Qt::AlignLeft);

    auto addRow = [infoForm, this](const QString &labelText, const QString &value) {
        auto *label = new QLabel(labelText, this);
        label->setStyleSheet("color: #5b8cff; font-weight: 600;");
        auto *valueLabel = new QLabel(value, this);
        infoForm->addRow(label, valueLabel);
    };

    addRow(tr("Address:"), info.displayAddress());
    addRow(tr("Players:"), info.queried && info.online
                                ? QString("%1 / %2").arg(info.players).arg(info.maxPlayers)
                                : tr("unknown"));
    addRow(tr("Ping:"), (info.queried && info.online && info.pingMs >= 0)
                             ? QString("%1 ms").arg(info.pingMs)
                             : tr("unknown"));
    addRow(tr("Mode:"), info.gamemode.isEmpty() ? tr("unknown") : info.gamemode);
    addRow(tr("Language:"), info.language.isEmpty() ? tr("unknown") : info.language);

    mainLayout->addLayout(infoForm);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #23262d;");
    mainLayout->addWidget(sep);

    auto *pwForm = new QFormLayout();
    m_serverPasswordEdit = new QLineEdit(this);
    m_serverPasswordEdit->setEchoMode(QLineEdit::Password);
    m_serverPasswordEdit->setText(info.savedPassword);
    m_serverPasswordEdit->setPlaceholderText(tr("Leave empty if the server has no password"));
    pwForm->addRow(tr("Server Password:"), m_serverPasswordEdit);

    m_rconPasswordEdit = new QLineEdit(this);
    m_rconPasswordEdit->setEchoMode(QLineEdit::Password);
    m_rconPasswordEdit->setText(info.rconPassword);
    m_rconPasswordEdit->setPlaceholderText(tr("(optional, saved for your reference)"));
    pwForm->addRow(tr("RCON Password:"), m_rconPasswordEdit);

    mainLayout->addLayout(pwForm);

    auto *buttonRow = new QHBoxLayout();
    auto *connectBtn = new QPushButton(tr("Connect"), this);
    connectBtn->setObjectName("PrimaryButton");
    auto *saveBtn = new QPushButton(tr("Save"), this);
    auto *cancelBtn = new QPushButton(tr("Cancel"), this);

    buttonRow->addWidget(connectBtn);
    buttonRow->addWidget(saveBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelBtn);

    mainLayout->addLayout(buttonRow);

    connect(connectBtn, &QPushButton::clicked, this, &ServerPropertiesDialog::onConnectClicked);
    connect(saveBtn, &QPushButton::clicked, this, &ServerPropertiesDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_serverPasswordEdit->setFocus();

    disableWindowMaximizeButton(this);
}

void ServerPropertiesDialog::onConnectClicked()
{
    m_action = Action::Connect;
    accept();
}

void ServerPropertiesDialog::onSaveClicked()
{
    m_action = Action::Save;
    accept();
}

QString ServerPropertiesDialog::serverPassword() const
{
    return m_serverPasswordEdit->text();
}

QString ServerPropertiesDialog::rconPassword() const
{
    return m_rconPasswordEdit->text();
}
