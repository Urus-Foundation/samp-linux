#pragma once

#include <QDialog>

class QLineEdit;

// "Connect by IP" dialog — used both for direct connect and for manually
// adding a server to Favorites.
class DirectConnectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DirectConnectDialog(QWidget *parent = nullptr);

    QString host()     const;
    quint16 port()     const;
    QString password() const;

private slots:
    void validateAndAccept();

private:
    QLineEdit *m_addressEdit;   // accepts "host:port" or just "host"
    QLineEdit *m_passwordEdit;
};
