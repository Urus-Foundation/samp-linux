#pragma once

#include <QMainWindow>
#include <QVector>
#include "serverinfo.h"

class QTabWidget;
class QTableView;
class QLineEdit;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QNetworkAccessManager;
class QNetworkReply;

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

    void onQueryResult(ServerInfo info);

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

    void buildUi();
    QWidget *buildInternetTab();
    QWidget *buildFavoritesTab();
    QWidget *buildHeaderBar();

    void connectServer(const ServerInfo &info);
    void reloadFavoritesModel();

    ServerInfo selectedServer(QTableView *view, ServerListModel *model) const;
};
