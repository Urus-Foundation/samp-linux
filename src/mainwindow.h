#pragma once

#include <QMainWindow>
#include <QVector>
#include <QHash>
#include "serverinfo.h"

class QTabWidget;
class QTableView;
class QLineEdit;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class ServerListModel;
class SampQuery;
class FavoritesManager;
class Launcher;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshInternetList();
    void onMasterListReply(QNetworkReply *reply);
    void onPingTimerTick();

    void onQueryResult(ServerInfo info);
    void onInternetContextMenu(const QPoint &pos);
    void onFavoritesContextMenu(const QPoint &pos);

    void requeryVisibleServers(QTableView *view, ServerListModel *model);
    void requeryInternet();
    void requeryFavorites();

    void connectToSelected(QTableView *view, ServerListModel *model);
    void connectToInternetSelection();
    void connectToFavoriteSelection();

    void addSelectedInternetToFavorites();
    void addFavoriteManually();
    void removeSelectedFavorite();

    void openDirectConnect();
    void openSettings();

    void onInternetFilterChanged(const QString &text);
    void onFavoritesFilterChanged(const QString &text);

private:
    QTabWidget *m_tabs;

    // Internet tab
    QTableView *m_internetView;
    ServerListModel *m_internetModel;
    QSortFilterProxyModel *m_internetProxy;
    QLineEdit *m_internetFilter;
    QLabel *m_internetStatusLabel;
    QPushButton *m_internetRefreshBtn;

    // Favorites tab
    QTableView *m_favoritesView;
    ServerListModel *m_favoritesModel;
    QSortFilterProxyModel *m_favoritesProxy;
    QLineEdit *m_favoritesFilter;

    QLabel *m_statusBarLabel;

    SampQuery *m_query;
    FavoritesManager *m_favoritesManager;
    Launcher *m_launcher;
    QNetworkAccessManager *m_netManager;
    QTimer *m_pingTimer;
    qint64 m_gamePid = 0;
    QHash<QString, ServerInfo> m_serverCache;

    void buildUi();
    QWidget *buildInternetTab();
    QWidget *buildFavoritesTab();
    QWidget *buildHeaderBar();

    void connectServer(const ServerInfo &info);
    void reloadFavoritesModel();

    ServerInfo selectedServer(QTableView *view, ServerListModel *model) const;
    ServerInfo serverAt(QTableView *view, ServerListModel *model, const QPoint &pos) const;
    void showServerContextMenu(QTableView *view, ServerListModel *model, const QPoint &pos, bool isFavorite);
    void addServerToFavorites(const ServerInfo &info);
    void removeServerFromFavorites(const ServerInfo &info);
    void showServerDetails(const ServerInfo &info);
    void copyServerInfo(const ServerInfo &info) const;
    bool isProcessRunning(qint64 pid) const;
    void applyServerCache(QVector<ServerInfo> *servers) const;
    void updateServerEntry(ServerListModel *model, const ServerInfo &info) const;
};
