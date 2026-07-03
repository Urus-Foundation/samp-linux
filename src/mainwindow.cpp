#include "mainwindow.h"
#include "qutils.h"

#include <QTabWidget>
#include <QTableView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QInputDialog>
#include <QFormLayout>
#include <QMenu>
#include <QStatusBar>
#include <QSettings>
#include <QTimer>
#include <QProcess>
#include <QApplication>
#include <QClipboard>

#include "serverlistmodel.h"
#include "sampquery.h"
#include "favoritesmanager.h"
#include "launcher.h"
#include "directconnectdialog.h"
#include "serverpropertiesdialog.h"
#include "settingsdialog.h"

namespace {
constexpr char kMasterListUrl[] = "https://api.open.mp/servers";
} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_query(new SampQuery(this))
    , m_favoritesManager(new FavoritesManager(this))
    , m_launcher(new Launcher(this))
    , m_netManager(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
{
    setWindowTitle(tr("SA:MP Linux Launcher"));
    setWindowIcon(QIcon(":/icons/samp-linux.png"));
    resize(980, 640);

    buildUi();

    connect(m_query,      &SampQuery::resultReady,
            this,          &MainWindow::onQueryResult);
    connect(m_netManager, &QNetworkAccessManager::finished,
            this,          &MainWindow::onMasterListReply);
    connect(m_pingTimer,  &QTimer::timeout,
            this,          &MainWindow::onPingTimerTick);
    m_pingTimer->start(3000);

    reloadFavoritesModel();

    QSettings settings;
    if (settings.value("paths/gtaDir").toString().isEmpty()) {
        statusBar()->showMessage(
            tr("Tip: open Settings and set your GTA San Andreas folder before connecting."), 8000);
    }

    refreshInternetList();
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root    = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildHeaderBar());

    m_tabs = new QTabWidget(central);
    m_tabs->addTab(buildInternetTab(),   tr("Internet"));
    m_tabs->addTab(buildFavoritesTab(),  tr("Favorites"));
    root->addWidget(m_tabs, 1);

    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    m_statusBarLabel = new QLabel(this);
    m_statusBarLabel->setObjectName("MutedLabel");
    m_statusLabel->setObjectName("MutedLabel");
    // Set padding so its more good to read
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet("padding-left: 20px; padding-right: 20px;");

    root->addWidget(m_statusLabel);
    statusBar()->addWidget(m_statusBarLabel);
}

QWidget *MainWindow::buildHeaderBar()
{
    auto *bar    = new QWidget(this);
    bar->setObjectName("HeaderBar");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(20, 14, 20, 14);

    auto *titleBox = new QVBoxLayout();
    auto *title    = new QLabel(tr("SA:MP Linux Launcher"), bar);
    title->setObjectName("AppTitle");
    auto *subtitle = new QLabel(tr("Play GTA San Andreas Multiplayer on Linux"), bar);
    subtitle->setObjectName("AppSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    layout->addLayout(titleBox);
    layout->addStretch(1);

    auto *directBtn   = new QPushButton(tr("Direct Connect"), bar);
    auto *settingsBtn = new QPushButton(tr("Settings"), bar);
    connect(directBtn,   &QPushButton::clicked, this, &MainWindow::openDirectConnect);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    layout->addWidget(directBtn);
    layout->addWidget(settingsBtn);

    return bar;
}

// Shared view configuration applied to both Internet and Favorites tables.
void MainWindow::setupTableView(QTableView *view, QSortFilterProxyModel *proxy)
{
    view->setModel(proxy);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setAlternatingRowColors(true);
    view->setSortingEnabled(true);
    view->setWordWrap(false);
    view->setTextElideMode(Qt::ElideRight);
    view->verticalHeader()->setVisible(false);

    auto *hdr = view->horizontalHeader();
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(ServerListModel::ColName,    QHeaderView::Stretch);
    hdr->setSectionResizeMode(ServerListModel::ColLock,    QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(ServerListModel::ColAddress, QHeaderView::ResizeToContents);

    view->setColumnWidth(ServerListModel::ColMode,    160);
    view->setColumnWidth(ServerListModel::ColPlayers,  90);
    view->setColumnWidth(ServerListModel::ColPing,     80);
    view->setColumnWidth(ServerListModel::ColAddress, 150);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

// Shared proxy configuration.
void MainWindow::setupProxy(QSortFilterProxyModel *proxy, ServerListModel *model)
{
    proxy->setSourceModel(model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);            // search all columns
    proxy->setSortRole(Qt::UserRole + 1);     // numeric sort keys
    proxy->setDynamicSortFilter(true);
}

QWidget *MainWindow::buildInternetTab()
{
    auto *tab    = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 12, 16, 12);

    // Toolbar
    auto *toolbar = new QHBoxLayout();

    m_internet.filter = new QLineEdit(tab);
    m_internet.filter->setPlaceholderText(tr("Filter by name, gamemode or address..."));
    connect(m_internet.filter, &QLineEdit::textChanged,
            this, &MainWindow::onInternetFilterChanged);
    toolbar->addWidget(m_internet.filter, 1);

    m_internetRefreshBtn = new QPushButton(tr("Refresh List"), tab);
    connect(m_internetRefreshBtn, &QPushButton::clicked,
            this, &MainWindow::refreshInternetList);
    toolbar->addWidget(m_internetRefreshBtn);

    auto *pingBtn = new QPushButton(tr("Ping All"), tab);
    connect(pingBtn, &QPushButton::clicked, this, &MainWindow::requeryInternetTab);
    toolbar->addWidget(pingBtn);

    auto *favBtn = new QPushButton(tr("Add to Favorites"), tab);
    connect(favBtn, &QPushButton::clicked, this, &MainWindow::addSelectedInternetToFavorites);
    toolbar->addWidget(favBtn);

    auto *connectBtn = new QPushButton(tr("Connect"), tab);
    connectBtn->setObjectName("PrimaryButton");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToInternetSelection);
    toolbar->addWidget(connectBtn);

    layout->addLayout(toolbar);

    // Model / proxy / view
    m_internet.model = new ServerListModel(this);
    m_internet.proxy = new QSortFilterProxyModel(this);
    m_internet.view  = new QTableView(tab);
    setupProxy(m_internet.proxy, m_internet.model);
    setupTableView(m_internet.view, m_internet.proxy);

    connect(m_internet.view, &QTableView::doubleClicked,
            this, [this](const QModelIndex &) { connectToInternetSelection(); });
    connect(m_internet.view, &QTableView::customContextMenuRequested,
            this, &MainWindow::onInternetContextMenu);

    layout->addWidget(m_internet.view, 1);
    return tab;
}

QWidget *MainWindow::buildFavoritesTab()
{
    auto *tab    = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 12, 16, 12);

    // Toolbar
    auto *toolbar = new QHBoxLayout();

    m_favorites.filter = new QLineEdit(tab);
    m_favorites.filter->setPlaceholderText(tr("Filter favorites..."));
    connect(m_favorites.filter, &QLineEdit::textChanged,
            this, &MainWindow::onFavoritesFilterChanged);
    toolbar->addWidget(m_favorites.filter, 1);

    auto *addBtn = new QPushButton(tr("Add by IP..."), tab);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addFavoriteManually);
    toolbar->addWidget(addBtn);

    auto *pingBtn = new QPushButton(tr("Ping All"), tab);
    connect(pingBtn, &QPushButton::clicked, this, &MainWindow::requeryFavoritesTab);
    toolbar->addWidget(pingBtn);

    auto *removeBtn = new QPushButton(tr("Remove"), tab);
    removeBtn->setObjectName("DangerButton");
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedFavorite);
    toolbar->addWidget(removeBtn);

    auto *connectBtn = new QPushButton(tr("Connect"), tab);
    connectBtn->setObjectName("PrimaryButton");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToFavoriteSelection);
    toolbar->addWidget(connectBtn);

    layout->addLayout(toolbar);

    // Model / proxy / view
    m_favorites.model = new ServerListModel(this);
    m_favorites.proxy = new QSortFilterProxyModel(this);
    m_favorites.view  = new QTableView(tab);
    setupProxy(m_favorites.proxy, m_favorites.model);
    setupTableView(m_favorites.view, m_favorites.proxy);

    connect(m_favorites.view, &QTableView::doubleClicked,
            this, [this](const QModelIndex &) { connectToFavoriteSelection(); });
    connect(m_favorites.view, &QTableView::customContextMenuRequested,
            this, &MainWindow::onFavoritesContextMenu);

    layout->addWidget(m_favorites.view, 1);
    return tab;
}

// ---------------------------------------------------------------------------
// Internet list fetch
// ---------------------------------------------------------------------------

void MainWindow::refreshInternetList()
{
    m_internetRefreshBtn->setEnabled(false);
    m_statusLabel->setText(tr("Fetching server list..."));

    QNetworkRequest req{QUrl(QString::fromLatin1(kMasterListUrl))};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("samp-linux/" SAMPLINUX_VERSION));
    m_netManager->get(req);
}

void MainWindow::onMasterListReply(QNetworkReply *reply)
{
    reply->deleteLater();
    m_internetRefreshBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(
            tr("Could not fetch server list: %1").arg(reply->errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        m_statusLabel->setText(tr("Server list response was not understood."));
        return;
    }

    QVector<ServerInfo> servers;
    servers.reserve(doc.array().size());

    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject())
            continue;

        // The open.mp API sometimes nests fields under a "core" object.
        QJsonObject obj = v.toObject();
        if (obj.contains("core") && obj.value("core").isObject())
            obj = obj.value("core").toObject();

        const QString ip = obj.value("ip").toString();
        if (ip.isEmpty())
            continue;

        // "ip" field encodes "host:port"
        QString  address = ip;
        quint16  port    = 7777;
        const int colon  = ip.lastIndexOf(':');
        if (colon > 0) {
            address = ip.left(colon);
            port    = static_cast<quint16>(ip.mid(colon + 1).toInt());
        }

        ServerInfo s;
        s.address    = address;
        s.port       = port;
        // Support both short-form keys (hn, gm, la, pc, pm, pa) used by the
        // open.mp API and the long-form names as a fallback.
        s.hostname   = obj.value("hn").toString(obj.value("hostname").toString());
        s.gamemode   = obj.value("gm").toString(obj.value("gamemode").toString());
        s.language   = obj.value("la").toString(obj.value("language").toString());
        s.players    = static_cast<quint16>(obj.value("pc").toInt(obj.value("players").toInt()));
        s.maxPlayers = static_cast<quint16>(obj.value("pm").toInt(obj.value("maxplayers").toInt()));
        s.passworded = obj.value("pa").toBool(obj.value("password").toBool());
        s.online     = true;
        s.queried    = true;   // master list gives us metadata; live ping is still TBD
        // TODO: implement a proper “live ping” state machine/caching so UI can
        //       distinguish between “measured once”, “currently pinging”, and
        //       “timed out” per-server (instead of using pingMs < 0 heuristics).
        // TODO: validate port parsing from open.mp response (ensure 1..65535)
        //       and handle malformed entries without silently using default 7777.

        servers.append(s);
    }

    if (servers.isEmpty()) {
        m_statusLabel->setText(tr("Server list is empty or could not be parsed."));
        return;
    }

    for (const ServerInfo &s : std::as_const(servers))
        m_serverCache.insert(s.key(), s);

    applyServerCache(&servers);
    m_internet.model->setServers(servers);
    m_statusLabel->setText(
        tr("%1 servers loaded. Select one and click \"Ping All\" for live ping.")
            .arg(servers.size()));

    // Kick off a ping sweep right away so the list shows real latency.
    requeryInternetTab();
}

// ---------------------------------------------------------------------------
// Ping timer / re-query
// ---------------------------------------------------------------------------

// Max number of times to re-query a server if it is offline.
// This is to avoid ping timeout bug or the server being offline at the time of the query.
// We want to give it a few chances to respond.
#define PING_MAX_RETRIES 2

void MainWindow::onPingTimerTick()
{
    // Pause ping sweeps while the game is running to reduce noise.
    if (m_gamePid > 0) {
        if (isProcessRunning(m_gamePid))
            return;
        m_gamePid = 0;
    }

    requeryTab(m_internet);
    requeryTab(m_favorites);
}

void MainWindow::requeryTab(const ServerTabWidgets &tab)
{
    for (const ServerInfo &s : tab.model->all()) {
        int retry = 0;
        while (!s.online && retry < PING_MAX_RETRIES) {
            // If the server was previously offline, we want to re-query it
            // even if it was already queried before. This is because the
            // server may have come back online since the last query.
            // or just timed out ping. The master list may not have the latest status,
            // so we always re-query.
            m_query->queryInfo(s.address, s.port);
            retry++;
        }
    }
}

void MainWindow::requeryInternetTab()
{
    m_statusLabel->setText(tr("Pinging servers..."));
    requeryTab(m_internet);
    m_statusLabel->clear();
}

void MainWindow::requeryFavoritesTab()
{
    m_statusLabel->setText(tr("Pinging servers..."));
    requeryTab(m_favorites);
    m_statusLabel->clear();
}

// ---------------------------------------------------------------------------
// Query results
// ---------------------------------------------------------------------------

void MainWindow::onQueryResult(ServerInfo info)
{
    mergeIntoCache(info);
    info = mergedWithCache(info);

    if (m_internet.model->indexOfKey(info.key()) >= 0)
        updateModelEntry(m_internet.model, info);
    if (m_favorites.model->indexOfKey(info.key()) >= 0)
        updateModelEntry(m_favorites.model, info);
}

// ---------------------------------------------------------------------------
// Context menus
// ---------------------------------------------------------------------------

void MainWindow::onInternetContextMenu(const QPoint &pos)
{
    showServerContextMenu(m_internet, pos, false);
}

void MainWindow::onFavoritesContextMenu(const QPoint &pos)
{
    showServerContextMenu(m_favorites, pos, true);
}

ServerInfo MainWindow::serverAt(const ServerTabWidgets &tab, const QPoint &pos) const
{
    const QModelIndex proxyIdx = tab.view->indexAt(pos);
    if (!proxyIdx.isValid())
        return {};
    const QModelIndex srcIdx = tab.proxy->mapToSource(proxyIdx);
    return tab.model->at(srcIdx.row());
}

void MainWindow::showServerContextMenu(const ServerTabWidgets &tab,
                                       const QPoint &pos,
                                       bool isFavorite)
{
    const ServerInfo info = serverAt(tab, pos);
    if (info.address.isEmpty())
        return;

    QMenu menu(this);
    menu.addAction(tr("Connect"),        [this, info] { connectServer(info); });
    menu.addAction(tr("Add to Favorites"), [this, info] { addServerToFavorites(info); });
    menu.addAction(tr("Server Details"),  [this, info] { showServerDetails(info); });
    menu.addAction(tr("Copy Server Info"), [this, info] { copyServerInfo(info); });
    if (isFavorite) {
        menu.addSeparator();
        menu.addAction(tr("Remove from Favorites"),
                       [this, info] { removeServerFromFavorites(info); });
    }
    menu.exec(tab.view->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Connecting to a server
// ---------------------------------------------------------------------------

ServerInfo MainWindow::selectedServer(const ServerTabWidgets &tab) const
{
    const QModelIndex proxyIdx = tab.view->currentIndex();
    if (!proxyIdx.isValid())
        return {};
    const QModelIndex srcIdx = tab.proxy->mapToSource(proxyIdx);
    return tab.model->at(srcIdx.row());
}

void MainWindow::connectServer(const ServerInfo &info)
{
    if (info.address.isEmpty()) {
        QMessageBox::information(this, tr("No server selected"),
                                 tr("Please select a server first."));
        return;
    }

    QSettings settings;
    QString nickname = settings.value("identity/nickname").toString().trimmed();
    if (nickname.isEmpty()) {
        bool ok = false;
        nickname = QInputDialog::getText(this,
                                         tr("Nickname required"),
                                         tr("Enter the nickname you want to play with:"),
                                         QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || nickname.isEmpty())
            return;
        settings.setValue("identity/nickname", nickname);
    }

    // Pre-fill saved passwords if this server is in Favorites.
    ServerInfo dialogInfo = info;
    for (const ServerInfo &fav : m_favoritesManager->favorites()) {
        if (fav.address == info.address && fav.port == info.port) {
            if (dialogInfo.savedPassword.isEmpty())
                dialogInfo.savedPassword = fav.savedPassword;
            if (dialogInfo.rconPassword.isEmpty())
                dialogInfo.rconPassword = fav.rconPassword;
            break;
        }
    }

    ServerPropertiesDialog dlg(dialogInfo, this);
    if (dlg.exec() != QDialog::Accepted || dlg.action() == ServerPropertiesDialog::Action::Cancelled)
        return;

    if (dlg.action() == ServerPropertiesDialog::Action::Save) {
        ServerInfo toSave      = info;
        toSave.savedPassword   = dlg.serverPassword();
        toSave.rconPassword    = dlg.rconPassword();
        m_favoritesManager->removeFavorite(toSave.address, toSave.port);
        m_favoritesManager->addFavorite(toSave);
        reloadFavoritesModel();
        statusBar()->showMessage(
            tr("Saved %1 to Favorites.")
                .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
        return;
    }

    // Action::Connect
    const Launcher::LaunchResult result =
        m_launcher->launch(info.address, info.port, nickname, dlg.serverPassword());

    if (!result.started) {
        QMessageBox::warning(this, tr("Could not launch SA:MP"), result.error);
        return;
    }

    m_favoritesManager->pushRecent(info);
    if (result.pid > 0)
        m_gamePid = result.pid;
    statusBar()->showMessage(
        tr("Launching %1:%2 via Wine...").arg(info.address).arg(info.port), 5000);
}

void MainWindow::connectToSelection(const ServerTabWidgets &tab)
{
    connectServer(selectedServer(tab));
}

void MainWindow::connectToInternetSelection()  { connectToSelection(m_internet);  }
void MainWindow::connectToFavoriteSelection()  { connectToSelection(m_favorites); }

// ---------------------------------------------------------------------------
// Favorites management
// ---------------------------------------------------------------------------

void MainWindow::reloadFavoritesModel()
{
    QVector<ServerInfo> favs = m_favoritesManager->favorites();
    for (const ServerInfo &s : std::as_const(favs))
        m_serverCache.insert(s.key(), s);
    applyServerCache(&favs);
    m_favorites.model->setServers(favs);
    requeryFavoritesTab();
}

void MainWindow::addSelectedInternetToFavorites()
{
    const ServerInfo info = selectedServer(m_internet);
    if (info.address.isEmpty()) {
        QMessageBox::information(this, tr("No server selected"),
                                 tr("Please select a server first."));
        return;
    }
    addServerToFavorites(info);
}

void MainWindow::addFavoriteManually()
{
    DirectConnectDialog dlg(this);
    dlg.setWindowTitle(tr("Add Favorite"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    ServerInfo info;
    info.address       = dlg.host();
    info.port          = dlg.port();
    info.savedPassword = dlg.password();
    m_favoritesManager->addFavorite(info);
    reloadFavoritesModel();
    m_query->queryInfo(info.address, info.port);
}

void MainWindow::removeSelectedFavorite()
{
    const ServerInfo info = selectedServer(m_favorites);
    if (info.address.isEmpty())
        return;
    removeServerFromFavorites(info);
}

void MainWindow::addServerToFavorites(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;
    m_favoritesManager->addFavorite(info);
    reloadFavoritesModel();
    statusBar()->showMessage(
        tr("Added %1 to Favorites.")
            .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
}

void MainWindow::removeServerFromFavorites(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;
    m_favoritesManager->removeFavorite(info.address, info.port);
    reloadFavoritesModel();
    statusBar()->showMessage(
        tr("Removed %1 from Favorites.")
            .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
}

// ---------------------------------------------------------------------------
// Server details / clipboard
// ---------------------------------------------------------------------------

void MainWindow::showServerDetails(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(
        tr("Server Details - %1")
            .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname));
    dlg.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form   = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);

    auto addRow = [&](const QString &label, const QString &value) {
        auto *lbl = new QLabel(label, &dlg);
        lbl->setStyleSheet("font-weight: 600;");
        form->addRow(lbl, new QLabel(value, &dlg));
    };

    const bool live = info.queried && info.online;
    addRow(tr("Address:"),     info.displayAddress());
    addRow(tr("Server Name:"), info.hostname.isEmpty()  ? tr("unknown") : info.hostname);
    addRow(tr("Gamemode:"),    info.gamemode.isEmpty()   ? tr("unknown") : info.gamemode);
    addRow(tr("Language:"),    info.language.isEmpty()   ? tr("unknown") : info.language);
    addRow(tr("Players:"),     live ? QStringLiteral("%1 / %2").arg(info.players).arg(info.maxPlayers)
                                    : tr("unknown"));
    addRow(tr("Ping:"),        (live && info.pingMs >= 0) ? QStringLiteral("%1 ms").arg(info.pingMs)
                                                           : tr("unknown"));
    addRow(tr("Online:"),      info.online    ? tr("Yes") : tr("No"));
    addRow(tr("Passworded:"),  info.passworded ? tr("Yes") : tr("No"));

    layout->addLayout(form);

    auto *buttonRow = new QHBoxLayout();
    auto *copyBtn   = new QPushButton(tr("Copy Server Info"), &dlg);
    auto *closeBtn  = new QPushButton(tr("Close"), &dlg);
    buttonRow->addWidget(copyBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeBtn);
    layout->addLayout(buttonRow);

    connect(copyBtn,  &QPushButton::clicked, this, [this, info] { copyServerInfo(info); });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    disableWindowMaximizeButton(&dlg);
    dlg.exec();
}

void MainWindow::copyServerInfo(const ServerInfo &info) const
{
    if (info.address.isEmpty())
        return;

    const QString text =
        tr("Name: %1\nAddress: %2\nGamemode: %3\nLanguage: %4\nPlayers: %5/%6\nPing: %7 ms\nPassworded: %8")
            .arg(info.hostname.isEmpty() ? tr("unknown") : info.hostname)
            .arg(info.displayAddress())
            .arg(info.gamemode.isEmpty()  ? tr("unknown") : info.gamemode)
            .arg(info.language.isEmpty()  ? tr("unknown") : info.language)
            .arg(info.players)
            .arg(info.maxPlayers)
            .arg(info.pingMs >= 0 ? QString::number(info.pingMs) : tr("unknown"))
            .arg(info.passworded ? tr("Yes") : tr("No"));

    QApplication::clipboard()->setText(text);
    statusBar()->showMessage(tr("Server info copied."), 3000);
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void MainWindow::openDirectConnect()
{
    DirectConnectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSettings settings;
    QString nickname = settings.value("identity/nickname").toString().trimmed();
    if (nickname.isEmpty()) {
        bool ok = false;
        nickname = QInputDialog::getText(this,
                                         tr("Nickname required"),
                                         tr("Enter the nickname you want to play with:"),
                                         QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || nickname.isEmpty())
            return;
        settings.setValue("identity/nickname", nickname);
    }

    ServerInfo info;
    info.address = dlg.host();
    info.port    = dlg.port();

    const Launcher::LaunchResult result =
        m_launcher->launch(info.address, info.port, nickname, dlg.password());

    if (!result.started) {
        QMessageBox::warning(this, tr("Could not launch SA:MP"), result.error);
        return;
    }

    m_favoritesManager->pushRecent(info);
    if (result.pid > 0)
        m_gamePid = result.pid;
    statusBar()->showMessage(
        tr("Launching %1:%2 via Wine...").arg(info.address).arg(info.port), 5000);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Filter slots
// ---------------------------------------------------------------------------

void MainWindow::onInternetFilterChanged(const QString &text)
{
    m_internet.proxy->setFilterFixedString(text);
}

void MainWindow::onFavoritesFilterChanged(const QString &text)
{
    m_favorites.proxy->setFilterFixedString(text);
}

// ---------------------------------------------------------------------------
// Cache helpers
// ---------------------------------------------------------------------------

// Apply any cached data to a freshly loaded server list (e.g. ping from a
// previous query cycle, so rows don't flicker back to "..." on reload).
void MainWindow::applyServerCache(QVector<ServerInfo> *servers) const
{
    if (!servers)
        return;
    for (ServerInfo &s : *servers) {
        const auto it = m_serverCache.find(s.key());
        if (it == m_serverCache.end())
            continue;
        const ServerInfo &cached = it.value();
        if (s.hostname.isEmpty())   s.hostname   = cached.hostname;
        if (s.gamemode.isEmpty())   s.gamemode   = cached.gamemode;
        if (s.language.isEmpty())   s.language   = cached.language;
        if (s.pingMs < 0)           s.pingMs     = cached.pingMs;
        if (!s.online && cached.online) {
            s.online     = true;
            s.passworded = cached.passworded;
        }
        s.queried = s.queried || cached.queried;
    }
}

// Merge a fresh UDP result into the cache, keeping the best known values.
void MainWindow::mergeIntoCache(const ServerInfo &incoming)
{
    auto it = m_serverCache.find(incoming.key());
    if (it == m_serverCache.end()) {
        m_serverCache.insert(incoming.key(), incoming);
        return;
    }
    ServerInfo &cached = it.value();
    if (!incoming.hostname.isEmpty())  cached.hostname   = incoming.hostname;
    if (!incoming.gamemode.isEmpty())  cached.gamemode   = incoming.gamemode;
    if (!incoming.language.isEmpty())  cached.language   = incoming.language;
    if (incoming.pingMs >= 0)          cached.pingMs     = incoming.pingMs;
    if (incoming.players > 0)          cached.players    = incoming.players;
    if (incoming.maxPlayers > 0)       cached.maxPlayers = incoming.maxPlayers;
    if (incoming.online)               cached.online     = true;
    cached.queried    = true;
    cached.passworded = incoming.passworded;
}

// Return a copy of info with any missing fields filled from the cache.
ServerInfo MainWindow::mergedWithCache(const ServerInfo &info) const
{
    const auto it = m_serverCache.constFind(info.key());
    if (it == m_serverCache.constEnd())
        return info;

    ServerInfo merged  = info;
    const ServerInfo &c = it.value();
    if (merged.hostname.isEmpty())   merged.hostname   = c.hostname;
    if (merged.gamemode.isEmpty())   merged.gamemode   = c.gamemode;
    if (merged.language.isEmpty())   merged.language   = c.language;
    if (merged.pingMs < 0)           merged.pingMs     = c.pingMs;
    if (merged.players == 0)         merged.players    = c.players;
    if (merged.maxPlayers == 0)      merged.maxPlayers = c.maxPlayers;
    merged.queried = true;
    return merged;
}

void MainWindow::updateModelEntry(ServerListModel *model, const ServerInfo &info) const
{
    const int row = model->indexOfKey(info.key());
    if (row >= 0)
        model->upsertServer(info);
}

// ---------------------------------------------------------------------------
// Process monitoring
// ---------------------------------------------------------------------------

bool MainWindow::isProcessRunning(qint64 pid) const
{
    if (pid <= 0)
        return false;
    return QProcess::execute("kill", {"-0", QString::number(pid)}) == 0;
}
