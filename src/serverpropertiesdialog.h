#pragma once

#include <QDialog>
#include "serverinfo.h"

class QLineEdit;
class QLabel;

// Recreates the classic SA:MP launcher's "Server Properties" popup that
// appears when you hit Connect on a server: shows a read-only summary
// (address, players, ping, mode, language) plus editable Server Password
// / RCON Password fields, and Connect / Save / Cancel actions.
class ServerPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Action {
        Cancelled,
        Connect,
        Save
    };

    explicit ServerPropertiesDialog(const ServerInfo &info, QWidget *parent = nullptr);

    Action action() const { return m_action; }
    QString serverPassword() const;
    QString rconPassword() const;

private slots:
    void onConnectClicked();
    void onSaveClicked();

private:
    ServerInfo m_info;
    Action m_action = Action::Cancelled;

    QLineEdit *m_serverPasswordEdit;
    QLineEdit *m_rconPasswordEdit;
};
