#include "directconnectdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

DirectConnectDialog::DirectConnectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Direct Connect"));
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);
    auto *form   = new QFormLayout();

    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText(tr("127.0.0.1:7777"));
    form->addRow(tr("Server address:"), m_addressEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("(optional)"));
    form->addRow(tr("Password:"), m_passwordEdit);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Connect"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName("PrimaryButton");
    connect(buttons, &QDialogButtonBox::accepted, this, &DirectConnectDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_addressEdit->setFocus();
}

void DirectConnectDialog::validateAndAccept()
{
    if (host().isEmpty()) {
        QMessageBox::warning(this, tr("Invalid address"),
                             tr("Please enter a server address, e.g. 127.0.0.1:7777"));
        return;
    }
    accept();
}

QString DirectConnectDialog::host() const
{
    const QString text = m_addressEdit->text().trimmed();
    const int idx = text.lastIndexOf(':');
    return idx <= 0 ? text : text.left(idx);
}

quint16 DirectConnectDialog::port() const
{
    const QString text = m_addressEdit->text().trimmed();
    const int idx = text.lastIndexOf(':');
    if (idx <= 0 || idx == text.size() - 1)
        return 7777;
    bool ok = false;
    const int p = text.mid(idx + 1).toInt(&ok);
    if (!ok || p <= 0 || p > 65535)
        return 7777;
    return static_cast<quint16>(p);
}

QString DirectConnectDialog::password() const
{
    return m_passwordEdit->text();
}
