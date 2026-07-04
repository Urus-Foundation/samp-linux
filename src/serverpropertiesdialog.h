#pragma once

#include <QDialog>
#include "serverinfo.h"

class QTreeWidget;
class QLabel;
class QLineEdit;

/* Full "Server Properties" dialog with two sections:
 *   1. Server Information — fixed fields (hostname, gamemode, language, etc.)
 *   2. Server Rules       — dynamic key-value map from opcode 'r', scrollable
 *
 * Rules are populated lazily via setRules() once the UDP reply arrives.
 */
class ServerPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Action {
        Cancelled,
        Connect,
        Save
    };

    enum class Mode {
        Minimal,
        Full
    };

    explicit ServerPropertiesDialog(const ServerInfo &info,
                                    Mode mode = Mode::Full,
                                    QWidget *parent = nullptr);

    /* Called by MainWindow when the 'r' query result arrives. */
    void setRules(const QMap<QString, QString> &rules);

    Action  action()         const { return m_action; }
    QString serverPassword() const;
    QString rconPassword()   const;

signals:
    void copyRequested(const ServerInfo &info);

private slots:
    void onConnectClicked();
    void onSaveClicked();
    void onCopyClicked();

private:
    void buildInfoSection(const ServerInfo &info);
    void buildRulesSection();

    ServerInfo  m_info;
    Mode        m_mode   = Mode::Full;
    Action      m_action = Action::Cancelled;

    QTreeWidget *m_rulesTree         = nullptr;
    QLabel      *m_rulesStatus       = nullptr;
    QLineEdit   *m_serverPasswordEdit = nullptr;
    QLineEdit   *m_rconPasswordEdit   = nullptr;
};
