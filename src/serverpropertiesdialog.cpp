#include "serverpropertiesdialog.h"
#include "helper.h"
#include "mainwindow.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QScrollArea>
#include <QTreeWidget>
#include <QHeaderView>

/* ---------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------*/

namespace {

/* Creates a section header label matching the app style. */
QLabel *makeSectionHeader(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    QFont f = lbl->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 1);
    lbl->setFont(f);
    lbl->setStyleSheet(QStringLiteral("color: #5b8cff; border-bottom: 1px solid #23262d; "
                                      "padding-bottom: 3px; margin-bottom: 2px;"));
    return lbl;
}

/* Creates a horizontal rule separator. */
QFrame *makeSeparator(QWidget *parent)
{
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: #23262d;"));
    return sep;
}

} // namespace

/* ---------------------------------------------------------------------------
 * Construction
 * ---------------------------------------------------------------------------*/

// In case this ever change
#define SERVER_PROPERTIES_MINIMAL_HEIGHT 320
#define SERVER_PROPERTIES_FULL_HEIGHT 320

ServerPropertiesDialog::ServerPropertiesDialog(const ServerInfo &info, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_info(info)
    , m_mode(mode)
{
    setWindowTitle(tr("Server Properties - %1")
                       .arg(info.hostname.isEmpty() ?
                         info.displayAddress() : info.hostname));

    setMinimumWidth(320);
    setMaximumWidth(mode == Mode::Full ? 380 : 320);

    setMinimumHeight(mode == Mode::Minimal ?
        SERVER_PROPERTIES_MINIMAL_HEIGHT
        :
        SERVER_PROPERTIES_FULL_HEIGHT);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    /* Server Information */
    buildInfoSection(info);

    auto *infoForm = new QFormLayout();
    infoForm->setLabelAlignment(Qt::AlignLeft);
    infoForm->setHorizontalSpacing(16);
    infoForm->setVerticalSpacing(6);

    auto addRow = [&](const QString &labelText, const QString &value) {
        auto *label = new QLabel(labelText, this);
        label->setStyleSheet(QStringLiteral("font-weight: 600;"));
        auto *valueLabel = new QLabel(value, this);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        infoForm->addRow(label, valueLabel);
    };

    const bool live = info.queried && info.online;

    addRow(tr("Hostname:"),    info.hostname.isEmpty()  ? tr("unknown") : info.hostname);
    addRow(tr("Gamemode:"),    info.gamemode.isEmpty()  ? tr("unknown") : info.gamemode);
    addRow(tr("Language:"),    info.language.isEmpty()  ? tr("unknown") : info.language);
    addRow(tr("Players:"),     live ? QStringLiteral("%1 / %2").arg(info.players).arg(info.maxPlayers)
                                    : tr("unknown"));
    addRow(tr("Max Players:"), live ? QString::number(info.maxPlayers) : tr("unknown"));
    addRow(tr("Password:"),    info.passworded ? tr("Yes") : tr("No"));
    addRow(tr("Ping:"),        (live && info.pingMs >= 0)
                                   ? QStringLiteral("%1 ms").arg(info.pingMs)
                                   : tr("unknown"));

    mainLayout->addLayout(infoForm);

    // Full mode shows the rules section, minimal mode shows password fields and buttons.
    if (mode == Mode::Full)
    {
        mainLayout->addWidget(makeSeparator(this));
        buildRulesSection();
        mainLayout->addWidget(m_rulesStatus);

        m_rulesTree->setMinimumHeight(160);
        m_rulesTree->setMaximumHeight(220);
        mainLayout->addWidget(m_rulesTree);

        if (info.rulesQueried && !info.rules.isEmpty())
            setRules(info.rules);
        
        // Copy button & close button
        auto *buttonRow = new QHBoxLayout();
        auto *copyBtn = new QPushButton(tr("Copy Info"), this);
        auto *closeBtn = new QPushButton(tr("Close"),this);

        closeBtn->setObjectName(QStringLiteral("PrimaryButton"));

        buttonRow->addWidget(copyBtn);
        buttonRow->addStretch(1);
        buttonRow->addWidget(closeBtn);

        mainLayout->addLayout(buttonRow);

        connect(copyBtn, &QPushButton::clicked, this, &ServerPropertiesDialog::onCopyClicked);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    else
    {
        mainLayout->addWidget(makeSeparator(this));

        /* Password fields */
        auto *pwForm = new QFormLayout();
        pwForm->setHorizontalSpacing(16);

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

        /* Buttons */
        auto *buttonRow = new QHBoxLayout();
        auto *connectBtn = new QPushButton(tr("Connect"), this);
        connectBtn->setObjectName(QStringLiteral("PrimaryButton"));
        auto *saveBtn   = new QPushButton(tr("Save"),    this);
        auto *cancelBtn = new QPushButton(tr("Cancel"),  this);

        buttonRow->addWidget(connectBtn);
        buttonRow->addWidget(saveBtn);
        buttonRow->addStretch(1);
        buttonRow->addWidget(cancelBtn);
        mainLayout->addLayout(buttonRow);

        connect(connectBtn, &QPushButton::clicked, this, &ServerPropertiesDialog::onConnectClicked);
        connect(saveBtn,    &QPushButton::clicked, this, &ServerPropertiesDialog::onSaveClicked);
        connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

        m_serverPasswordEdit->setFocus();
    }

    disableWindowMaximizeButton(this);
}

void ServerPropertiesDialog::buildInfoSection(const ServerInfo &)
{
    /* Nothing to do here — form rows are added directly in the constructor
     * after this method stub. Kept for clarity / future expansion. */
}

void ServerPropertiesDialog::buildRulesSection()
{
    m_rulesStatus = new QLabel(tr("Fetching rules…"), this);
    m_rulesStatus->setStyleSheet(QStringLiteral("font-style: italic;"));

    m_rulesTree = new QTreeWidget(this);
    m_rulesTree->setColumnCount(2);
    m_rulesTree->setHeaderLabels({tr("Rule"), tr("Value")});
    m_rulesTree->sortByColumn(0, Qt::AscendingOrder);
    m_rulesTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rulesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTree->setFocusPolicy(Qt::NoFocus);
    m_rulesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rulesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_rulesTree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rulesTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

/* ---------------------------------------------------------------------------
 * Public: populate rules from UDP reply or cache
 * ---------------------------------------------------------------------------*/

void ServerPropertiesDialog::setRules(const QMap<QString, QString> &rules)
{
    m_rulesStatus->setVisible(false);

    if (rules.isEmpty()) {
        m_rulesStatus->setText(tr("Server did not report any rules."));
        m_rulesStatus->setVisible(true);
        return;
    }

    m_rulesTree->clear();

    /* Iterate over the full map — no hardcoded keys, future-proof. */
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        auto *item = new QTreeWidgetItem(m_rulesTree);
        item->setText(0, it.key());
        item->setText(1, it.value());
        item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
        qDebug().noquote() << "ServerPropertiesDialog: rule" << it.key() << "=" << it.value();
    }
}

/* ---------------------------------------------------------------------------
 * Slots
 * ---------------------------------------------------------------------------*/

void ServerPropertiesDialog::onCopyClicked()
{
    emit copyRequested(m_info);
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

/* ---------------------------------------------------------------------------
 * Accessors
 * ---------------------------------------------------------------------------*/

QString ServerPropertiesDialog::serverPassword() const
{
    return m_serverPasswordEdit->text();
}

QString ServerPropertiesDialog::rconPassword() const
{
    return m_rconPasswordEdit->text();
}