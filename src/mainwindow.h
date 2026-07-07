#pragma once

#include <QMainWindow>
#include <QHash>

#include "serverinfo.h"

class QBoxLayout;
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

// Groups together the widgets and models for a single server-list tab
// (Internet or Favorites) so we don't repeat eight member variables twice.
struct ServerTabWidgets {
    QTableView            *view     = nullptr;
    ServerListModel       *model    = nullptr;
    QSortFilterProxyModel *proxy    = nullptr;
    QLineEdit             *filter   = nullptr;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Master-list fetch
    void refreshInternetList();
    void onMasterListReply(QNetworkReply *reply);

    // Periodic ping sweep
    void onPingTimerTick();

    // UDP query results
    void onQueryResult(ServerInfo info);

    // Context menus
    void onInternetContextMenu(const QPoint &pos);
    void onFavoritesContextMenu(const QPoint &pos);

    // Re-query buttons
    void requeryTab(const ServerTabWidgets &tab);
    void requeryInternetTab();
    void requeryFavoritesTab();

    // Connect actions
    void connectToSelection(const ServerTabWidgets &tab);
    void connectToInternetSelection();
    void connectToFavoriteSelection();

    // Favorites management
    void addSelectedInternetToFavorites();
    void addFavoriteManually();
    void removeSelectedFavorite();

    // Toolbar dialogs
    void openDirectConnect();
    void openSettings();

    // Filter boxes
    void onInternetFilterChanged(const QString &text);
    void onFavoritesFilterChanged(const QString &text);

private:
    // UI construction
    void     buildUi();
    QWidget *buildHeaderBar();
    QWidget *buildInternetTab();
    QWidget *buildFavoritesTab();

    // UI Component
    void buildFilterToolbar(QWidget *tab, QBoxLayout *toolbar);

    // Tab widget helpers (reduce duplication between Internet / Favorites)
    void    setupTableView(QTableView *view, QSortFilterProxyModel *proxy);
    void    setupProxy(QSortFilterProxyModel *proxy, ServerListModel *model);

    // Server interaction
    void connectServer(const ServerInfo &info);
    void reloadFavoritesModel();

    // Selection helpers
    ServerInfo selectedServer(const ServerTabWidgets &tab) const;
    ServerInfo serverAt(const ServerTabWidgets &tab, const QPoint &pos) const;

    // Context menu (shared between tabs)
    void showServerContextMenu(const ServerTabWidgets &tab, const QPoint &pos, bool isFavorite);

    // Favorites CRUD
    void addServerToFavorites(const ServerInfo &info);
    void removeServerFromFavorites(const ServerInfo &info);

    // Detail / clipboard
    void showServerProperties(const ServerInfo &info);
    void copyServerInfo(const ServerInfo &info) const;

    // Cache helpers
    void applyServerCache(QVector<ServerInfo> *servers) const;
    void mergeIntoCache(const ServerInfo &incoming);
    ServerInfo mergedWithCache(const ServerInfo &info) const;
    void updateModelEntry(ServerListModel *model, const ServerInfo &info) const;

    // Process monitoring
    bool isProcessRunning(qint64 pid) const;

    // Core objects
    SampQuery            *m_query;
    FavoritesManager     *m_favoritesManager;
    Launcher             *m_launcher;
    QNetworkAccessManager *m_netManager;
    QTimer               *m_pingTimer;

    // Server cache (shared between both tabs)
    QHash<QString, ServerInfo> m_serverCache;
    qint64                     m_gamePid = 0;

    // Tabs
    QTabWidget       *m_tabs;
    ServerTabWidgets  m_internet;
    ServerTabWidgets  m_favorites;

    // Extra widgets only on the Internet tab
    QPushButton  *m_internetRefreshBtn = nullptr;


    // Status bar
    QLabel *m_statusLabel = nullptr;  // Top
    QLabel *m_statusBarLabel = nullptr;  // Down
};
