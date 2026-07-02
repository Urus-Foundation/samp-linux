#include "mainwindow.h"

#include <QTabWidget>
#include <QTableView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
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
#include <QMenu>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include <QProcess>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>

#include "serverlistmodel.h"
#include "sampquery.h"
#include "favoritesmanager.h"
#include "launcher.h"
#include "directconnectdialog.h"
#include "serverpropertiesdialog.h"
#include "settingsdialog.h"

namespace {
constexpr char kMasterListUrl[] = "https://api.open.mp/servers";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_query(new SampQuery(this))
    , m_favoritesManager(new FavoritesManager(this))
    , m_launcher(new Launcher(this))
    , m_netManager(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
{
    // Set the platform to XCB to avoid issues with Wayland on some systems
    qputenv("QT_QPA_PLATFORM", "xcb"); 

    setWindowTitle(tr("SA:MP Launcher"));
    setWindowIcon(QIcon(":/icons/samp-linux.png"));

    resize(980, 640);

    buildUi();

    connect(m_query, &SampQuery::resultReady, this, &MainWindow::onQueryResult);
    connect(m_netManager, &QNetworkAccessManager::finished, this, &MainWindow::onMasterListReply);
    connect(m_pingTimer, &QTimer::timeout, this, &MainWindow::onPingTimerTick);
    m_pingTimer->start(3000);

    reloadFavoritesModel();

    QSettings settings;
    if (settings.value("paths/gtaDir").toString().isEmpty()) {
        statusBar()->showMessage(tr("Tip: open Settings and set your GTA San Andreas folder before connecting."), 8000);
    }

    refreshInternetList();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(buildHeaderBar());

    m_tabs = new QTabWidget(central);
    m_tabs->addTab(buildInternetTab(), tr("Internet"));
    m_tabs->addTab(buildFavoritesTab(), tr("Favorites"));
    layout->addWidget(m_tabs, 1);

    setCentralWidget(central);

    m_statusBarLabel = new QLabel(this);
    statusBar()->addWidget(m_statusBarLabel);
}

QWidget *MainWindow::buildHeaderBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName("HeaderBar");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(20, 14, 20, 14);

    auto *titleBox = new QVBoxLayout();
    auto *title = new QLabel(tr("SA:MP Launcher"), bar);
    title->setObjectName("AppTitle");
    auto *subtitle = new QLabel(tr("Play GTA San Andreas Multiplayer on Linux"), bar);
    subtitle->setObjectName("AppSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    layout->addLayout(titleBox);

    layout->addStretch(1);

    auto *directBtn = new QPushButton(tr("Direct Connect"), bar);
    connect(directBtn, &QPushButton::clicked, this, &MainWindow::openDirectConnect);
    layout->addWidget(directBtn);

    auto *settingsBtn = new QPushButton(tr("Settings"), bar);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    layout->addWidget(settingsBtn);

    return bar;
}

QWidget *MainWindow::buildInternetTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 12, 16, 12);

    auto *toolbar = new QHBoxLayout();
    m_internetFilter = new QLineEdit(tab);
    m_internetFilter->setPlaceholderText(tr("Filter by name, gamemode or address..."));
    connect(m_internetFilter, &QLineEdit::textChanged, this, &MainWindow::onInternetFilterChanged);
    toolbar->addWidget(m_internetFilter, 1);

    m_internetRefreshBtn = new QPushButton(tr("Refresh List"), tab);
    connect(m_internetRefreshBtn, &QPushButton::clicked, this, &MainWindow::refreshInternetList);
    toolbar->addWidget(m_internetRefreshBtn);

    auto *requeryBtn = new QPushButton(tr("Ping All"), tab);
    connect(requeryBtn, &QPushButton::clicked, this, &MainWindow::requeryInternet);
    toolbar->addWidget(requeryBtn);

    auto *favBtn = new QPushButton(tr("Add to Favorites"), tab);
    connect(favBtn, &QPushButton::clicked, this, &MainWindow::addSelectedInternetToFavorites);
    toolbar->addWidget(favBtn);

    auto *connectBtn = new QPushButton(tr("Connect"), tab);
    connectBtn->setObjectName("PrimaryButton");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToInternetSelection);
    toolbar->addWidget(connectBtn);

    layout->addLayout(toolbar);

    m_internetModel = new ServerListModel(this);
    m_internetProxy = new QSortFilterProxyModel(this);
    m_internetProxy->setSourceModel(m_internetModel);
    m_internetProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_internetProxy->setFilterKeyColumn(-1); // filter across all columns
    m_internetProxy->setSortRole(Qt::UserRole + 1);
    m_internetProxy->setDynamicSortFilter(true);

    m_internetView = new QTableView(tab);
    m_internetView->setModel(m_internetProxy);
    m_internetView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_internetView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_internetView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_internetView->setAlternatingRowColors(true);
    m_internetView->setSortingEnabled(true);
    m_internetView->setWordWrap(false);
    m_internetView->setTextElideMode(Qt::ElideRight);
    m_internetView->horizontalHeader()->setStretchLastSection(false);
    m_internetView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColName, QHeaderView::Stretch);
    m_internetView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColLock, QHeaderView::ResizeToContents);
    m_internetView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColAddress, QHeaderView::ResizeToContents);
    m_internetView->verticalHeader()->setVisible(false);
    m_internetView->setColumnWidth(ServerListModel::ColMode, 160);
    m_internetView->setColumnWidth(ServerListModel::ColPlayers, 90);
    m_internetView->setColumnWidth(ServerListModel::ColPing, 80);
    m_internetView->setColumnWidth(ServerListModel::ColAddress, 150);
    connect(m_internetView, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        connectToInternetSelection();
    });
    m_internetView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_internetView, &QTableView::customContextMenuRequested, this, &MainWindow::onInternetContextMenu);

    layout->addWidget(m_internetView, 1);

    m_internetStatusLabel = new QLabel(tab);
    m_internetStatusLabel->setObjectName("MutedLabel");
    layout->addWidget(m_internetStatusLabel);

    return tab;
}

QWidget *MainWindow::buildFavoritesTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 12, 16, 12);

    auto *toolbar = new QHBoxLayout();
    m_favoritesFilter = new QLineEdit(tab);
    m_favoritesFilter->setPlaceholderText(tr("Filter favorites..."));
    connect(m_favoritesFilter, &QLineEdit::textChanged, this, &MainWindow::onFavoritesFilterChanged);
    toolbar->addWidget(m_favoritesFilter, 1);

    auto *addBtn = new QPushButton(tr("Add by IP..."), tab);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addFavoriteManually);
    toolbar->addWidget(addBtn);

    auto *refreshBtn = new QPushButton(tr("Ping All"), tab);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::requeryFavorites);
    toolbar->addWidget(refreshBtn);

    auto *removeBtn = new QPushButton(tr("Remove"), tab);
    removeBtn->setObjectName("DangerButton");
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedFavorite);
    toolbar->addWidget(removeBtn);

    auto *connectBtn = new QPushButton(tr("Connect"), tab);
    connectBtn->setObjectName("PrimaryButton");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToFavoriteSelection);
    toolbar->addWidget(connectBtn);

    layout->addLayout(toolbar);

    m_favoritesModel = new ServerListModel(this);
    m_favoritesProxy = new QSortFilterProxyModel(this);
    m_favoritesProxy->setSourceModel(m_favoritesModel);
    m_favoritesProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_favoritesProxy->setFilterKeyColumn(-1);
    m_favoritesProxy->setSortRole(Qt::UserRole + 1);
    m_favoritesProxy->setDynamicSortFilter(true);

    m_favoritesView = new QTableView(tab);
    m_favoritesView->setModel(m_favoritesProxy);
    m_favoritesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_favoritesView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_favoritesView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_favoritesView->setAlternatingRowColors(true);
    m_favoritesView->setSortingEnabled(true);
    m_favoritesView->setWordWrap(false);
    m_favoritesView->setTextElideMode(Qt::ElideRight);
    m_favoritesView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColName, QHeaderView::Stretch);
    m_favoritesView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColLock, QHeaderView::ResizeToContents);
    m_favoritesView->horizontalHeader()->setSectionResizeMode(ServerListModel::ColAddress, QHeaderView::ResizeToContents);
    m_favoritesView->verticalHeader()->setVisible(false);
    connect(m_favoritesView, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        connectToFavoriteSelection();
    });
    m_favoritesView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_favoritesView, &QTableView::customContextMenuRequested, this, &MainWindow::onFavoritesContextMenu);

    layout->addWidget(m_favoritesView, 1);

    return tab;
}

// ---------------------------------------------------------------------
// Internet list
// ---------------------------------------------------------------------

void MainWindow::refreshInternetList()
{
    m_internetRefreshBtn->setEnabled(false);
    m_internetStatusLabel->setText(tr("Fetching server list..."));
    QNetworkRequest req{QUrl(QString::fromLatin1(kMasterListUrl))};
    // XXX - Add a accessbility to set a custom user agent,
    // bcz linux is customable but this is not a good idea to hardcode it,
    // but for now this is fine.
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("samp-linux/" LAUNCHER_VERSION));
    m_netManager->get(req);
}

void MainWindow::onMasterListReply(QNetworkReply *reply)
{
    reply->deleteLater();
    m_internetRefreshBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        m_internetStatusLabel->setText(tr("Could not fetch server list: %1").arg(reply->errorString()));
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        m_internetStatusLabel->setText(tr("Server list response was not understood."));
        return;
    }

    QVector<ServerInfo> servers;
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        // Some list APIs nest fields under "core"; support both shapes.
        if (obj.contains("core") && obj.value("core").isObject())
            obj = obj.value("core").toObject();

        const QString ip = obj.value("ip").toString();
        if (ip.isEmpty())
            continue;

        QString address = ip;
        quint16 port = 7777;
        const int colon = ip.lastIndexOf(':');
        if (colon > 0) {
            address = ip.left(colon);
            port = static_cast<quint16>(ip.mid(colon + 1).toInt());
        }

        ServerInfo s;
        s.address = address;
        s.port = port;
        s.hostname = obj.value("hn").toString(obj.value("hostname").toString());
        s.gamemode = obj.value("gm").toString(obj.value("gamemode").toString());
        s.language = obj.value("la").toString(obj.value("language").toString());
        s.players = static_cast<quint16>(obj.value("pc").toInt(obj.value("players").toInt()));
        s.maxPlayers = static_cast<quint16>(obj.value("pm").toInt(obj.value("maxplayers").toInt()));
        s.passworded = obj.value("pa").toBool(obj.value("password").toBool());
        s.online = true;
        s.queried = true; // list already gives us basic info; ping still needs a live query
        servers.append(s);
    }

    if (servers.isEmpty()) {
        m_internetStatusLabel->setText(tr("Server list is empty or could not be parsed."));
        return;
    }

    for (const ServerInfo &server : servers)
        m_serverCache.insert(server.key(), server);

    applyServerCache(&servers);
    m_internetModel->setServers(servers);
    m_internetStatusLabel->setText(tr("%1 servers loaded. Select one and click \"Ping All\" for live ping.")
                                        .arg(servers.size()));

    // Kick off a light ping sweep so the list isn't just static numbers
    // from the master list.
    requeryInternet();
}

// ---------------------------------------------------------------------
// Live querying
// ---------------------------------------------------------------------

void MainWindow::onPingTimerTick()
{
    if (m_gamePid > 0) {
        if (isProcessRunning(m_gamePid))
            return;
        m_gamePid = 0;
    }

    requeryVisibleServers(m_internetView, m_internetModel);
    requeryVisibleServers(m_favoritesView, m_favoritesModel);
}

void MainWindow::requeryVisibleServers(QTableView *view, ServerListModel *model)
{
    Q_UNUSED(view);
    const QVector<ServerInfo> servers = model->all();
    for (const ServerInfo &s : servers)
        m_query->queryInfo(s.address, s.port);
}

void MainWindow::requeryInternet()
{
    requeryVisibleServers(m_internetView, m_internetModel);
    m_internetStatusLabel->setText(tr("Pinging servers..."));
}

void MainWindow::requeryFavorites()
{
    requeryVisibleServers(m_favoritesView, m_favoritesModel);
}

void MainWindow::onInternetContextMenu(const QPoint &pos)
{
    showServerContextMenu(m_internetView, m_internetModel, pos, false);
}

void MainWindow::onFavoritesContextMenu(const QPoint &pos)
{
    showServerContextMenu(m_favoritesView, m_favoritesModel, pos, true);
}

ServerInfo MainWindow::serverAt(QTableView *view, ServerListModel *model, const QPoint &pos) const
{
    const QModelIndex proxyIndex = view->indexAt(pos);
    if (!proxyIndex.isValid())
        return ServerInfo();

    auto *proxy = qobject_cast<QSortFilterProxyModel *>(view->model());
    const QModelIndex sourceIndex = proxy ? proxy->mapToSource(proxyIndex) : proxyIndex;
    return model->at(sourceIndex.row());
}

void MainWindow::showServerContextMenu(QTableView *view, ServerListModel *model, const QPoint &pos, bool isFavorite)
{
    const ServerInfo info = serverAt(view, model, pos);
    if (info.address.isEmpty())
        return;

    QMenu menu(this);
    menu.addAction(tr("Connect"), [this, info] {
        connectServer(info);
    });
    menu.addAction(tr("Add to Favorites"), [this, info] {
        addServerToFavorites(info);
    });
    menu.addAction(tr("Server Details"), [this, info] {
        showServerDetails(info);
    });
    menu.addAction(tr("Copy Server Info"), [this, info] {
        copyServerInfo(info);
    });
    if (isFavorite) {
        menu.addSeparator();
        menu.addAction(tr("Remove from Favorites"), [this, info] {
            removeServerFromFavorites(info);
        });
    }
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::onQueryResult(ServerInfo info)
{
    if (m_serverCache.contains(info.key())) {
        const ServerInfo cached = m_serverCache.value(info.key());
        if (info.address.isEmpty())
            info.address = cached.address;
        if (info.port == 0)
            info.port = cached.port;
        if (info.hostname.isEmpty())
            info.hostname = cached.hostname;
        if (info.gamemode.isEmpty())
            info.gamemode = cached.gamemode;
        if (info.language.isEmpty())
            info.language = cached.language;
        if (info.players == 0 && info.maxPlayers == 0 && cached.players > 0)
            info.players = cached.players;
        if (info.maxPlayers == 0 && cached.maxPlayers > 0)
            info.maxPlayers = cached.maxPlayers;
        if (!info.online && cached.online) {
            info.online = true;
            info.passworded = cached.passworded;
            if (info.pingMs < 0 && cached.pingMs >= 0)
                info.pingMs = cached.pingMs;
        }
    }

    m_serverCache.insert(info.key(), info);

    if (m_internetModel->indexOfKey(info.key()) >= 0)
        updateServerEntry(m_internetModel, info);
    if (m_favoritesModel->indexOfKey(info.key()) >= 0)
        updateServerEntry(m_favoritesModel, info);
}

// ---------------------------------------------------------------------
// Connecting
// ---------------------------------------------------------------------

ServerInfo MainWindow::selectedServer(QTableView *view, ServerListModel *model) const
{
    const QModelIndex proxyIdx = view->currentIndex();
    if (!proxyIdx.isValid())
        return ServerInfo();

    auto *proxy = qobject_cast<QSortFilterProxyModel *>(view->model());
    const QModelIndex sourceIdx = proxy ? proxy->mapToSource(proxyIdx) : proxyIdx;
    return model->at(sourceIdx.row());
}

void MainWindow::connectServer(const ServerInfo &info)
{
    if (info.address.isEmpty()) {
        QMessageBox::information(this, tr("No server selected"), tr("Please select a server first."));
        return;
    }

    QSettings settings;
    QString nickname = settings.value("identity/nickname").toString();
    if (nickname.trimmed().isEmpty()) {
        bool ok = false;
        nickname = QInputDialog::getText(this, tr("Nickname required"),
                                          tr("Enter the nickname you want to play with:"),
                                          QLineEdit::Normal, QString(), &ok);
        if (!ok || nickname.trimmed().isEmpty())
            return;
        settings.setValue("identity/nickname", nickname.trimmed());
    }

    // Pre-fill the saved password if this server is already a favorite.
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
        ServerInfo toSave = info;
        toSave.savedPassword = dlg.serverPassword();
        toSave.rconPassword = dlg.rconPassword();
        m_favoritesManager->removeFavorite(toSave.address, toSave.port);
        m_favoritesManager->addFavorite(toSave);
        reloadFavoritesModel();
        statusBar()->showMessage(tr("Saved %1 to Favorites.")
                                      .arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
        return;
    }

    // Action::Connect
    const Launcher::LaunchResult result = m_launcher->launch(info.address, info.port, nickname, dlg.serverPassword());
    if (!result.started) {
        QMessageBox::warning(this, tr("Could not launch SA:MP"), result.error);
        return;
    }

    m_favoritesManager->pushRecent(info);
    if (result.pid > 0)
        m_gamePid = result.pid;
    statusBar()->showMessage(tr("Launching %1:%2 via Wine...").arg(info.address).arg(info.port), 5000);
}

void MainWindow::connectToSelected(QTableView *view, ServerListModel *model)
{
    connectServer(selectedServer(view, model));
}

void MainWindow::connectToInternetSelection()
{
    connectToSelected(m_internetView, m_internetModel);
}

void MainWindow::connectToFavoriteSelection()
{
    connectToSelected(m_favoritesView, m_favoritesModel);
}

// ---------------------------------------------------------------------
// Favorites management
// ---------------------------------------------------------------------

void MainWindow::reloadFavoritesModel()
{
    QVector<ServerInfo> favorites = m_favoritesManager->favorites();
    for (const ServerInfo &server : favorites)
        m_serverCache.insert(server.key(), server);
    applyServerCache(&favorites);
    m_favoritesModel->setServers(favorites);
    requeryFavorites();
}

void MainWindow::addSelectedInternetToFavorites()
{
    const ServerInfo info = selectedServer(m_internetView, m_internetModel);
    if (info.address.isEmpty()) {
        QMessageBox::information(this, tr("No server selected"), tr("Please select a server first."));
        return;
    }
    m_favoritesManager->addFavorite(info);
    reloadFavoritesModel();
    statusBar()->showMessage(tr("Added %1 to Favorites.").arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
}

void MainWindow::addFavoriteManually()
{
    DirectConnectDialog dlg(this);
    dlg.setWindowTitle(tr("Add Favorite"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    ServerInfo info;
    info.address = dlg.host();
    info.port = dlg.port();
    info.savedPassword = dlg.password();
    m_favoritesManager->addFavorite(info);
    reloadFavoritesModel();
    m_query->queryInfo(info.address, info.port);
}

void MainWindow::removeSelectedFavorite()
{
    const ServerInfo info = selectedServer(m_favoritesView, m_favoritesModel);
    if (info.address.isEmpty())
        return;

    m_favoritesManager->removeFavorite(info.address, info.port);
    reloadFavoritesModel();
}

void MainWindow::addServerToFavorites(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;
    m_favoritesManager->addFavorite(info);
    reloadFavoritesModel();
    statusBar()->showMessage(tr("Added %1 to Favorites.").arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
}

void MainWindow::removeServerFromFavorites(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;
    m_favoritesManager->removeFavorite(info.address, info.port);
    reloadFavoritesModel();
    statusBar()->showMessage(tr("Removed %1 from Favorites.").arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname), 4000);
}

void MainWindow::showServerDetails(const ServerInfo &info)
{
    if (info.address.isEmpty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Server Details - %1").arg(info.hostname.isEmpty() ? info.displayAddress() : info.hostname));
    dlg.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);

    auto addRow = [form, this](const QString &labelText, const QString &value) {
        auto *label = new QLabel(labelText, this);
        label->setStyleSheet("font-weight: 600;");
        form->addRow(label, new QLabel(value, this));
    };

    addRow(tr("Address:"), info.displayAddress());
    addRow(tr("Server Name:"), info.hostname.isEmpty() ? tr("unknown") : info.hostname);
    addRow(tr("Gamemode:"), info.gamemode.isEmpty() ? tr("unknown") : info.gamemode);
    addRow(tr("Language:"), info.language.isEmpty() ? tr("unknown") : info.language);
    addRow(tr("Players:"), info.queried && info.online ? QString("%1 / %2").arg(info.players).arg(info.maxPlayers) : tr("unknown"));
    addRow(tr("Ping:"), (info.queried && info.online && info.pingMs >= 0) ? QString("%1 ms").arg(info.pingMs) : tr("unknown"));
    addRow(tr("Online:"), info.online ? tr("Yes") : tr("No"));
    addRow(tr("Passworded:"), info.passworded ? tr("Yes") : tr("No"));
    addRow(tr("Queried:"), info.queried ? tr("Yes") : tr("No"));

    layout->addLayout(form);

    auto *buttonRow = new QHBoxLayout();
    auto *copyBtn = new QPushButton(tr("Copy Server Info"), &dlg);
    auto *closeBtn = new QPushButton(tr("Close"), &dlg);
    buttonRow->addWidget(copyBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeBtn);
    layout->addLayout(buttonRow);

    connect(copyBtn, &QPushButton::clicked, this, [this, info] {
        copyServerInfo(info);
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}

void MainWindow::copyServerInfo(const ServerInfo &info) const
{
    if (info.address.isEmpty())
        return;

    const QString text = tr("Name: %1\nAddress: %2\nGamemode: %3\nLanguage: %4\nPlayers: %5/%6\nPing: %7 ms\nPassworded: %8")
        .arg(info.hostname.isEmpty() ? tr("unknown") : info.hostname)
        .arg(info.displayAddress())
        .arg(info.gamemode.isEmpty() ? tr("unknown") : info.gamemode)
        .arg(info.language.isEmpty() ? tr("unknown") : info.language)
        .arg(info.players)
        .arg(info.maxPlayers)
        .arg(info.pingMs >= 0 ? QString::number(info.pingMs) : tr("unknown"))
        .arg(info.passworded ? tr("Yes") : tr("No"));

    QApplication::clipboard()->setText(text);
    statusBar()->showMessage(tr("Server info copied."), 3000);
}

// ---------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------

void MainWindow::openDirectConnect()
{
    DirectConnectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ServerInfo info;
    info.address = dlg.host();
    info.port = dlg.port();

    QSettings settings;
    QString nickname = settings.value("identity/nickname").toString();
    if (nickname.trimmed().isEmpty()) {
        bool ok = false;
        nickname = QInputDialog::getText(this, tr("Nickname required"),
                                          tr("Enter the nickname you want to play with:"),
                                          QLineEdit::Normal, QString(), &ok);
        if (!ok || nickname.trimmed().isEmpty())
            return;
        settings.setValue("identity/nickname", nickname.trimmed());
    }

    const Launcher::LaunchResult result = m_launcher->launch(info.address, info.port, nickname, dlg.password());
    if (!result.started) {
        QMessageBox::warning(this, tr("Could not launch SA:MP"), result.error);
        return;
    }
    m_favoritesManager->pushRecent(info);
    if (result.pid > 0)
        m_gamePid = result.pid;
    statusBar()->showMessage(tr("Launching %1:%2 via Wine...").arg(info.address).arg(info.port), 5000);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------

void MainWindow::onInternetFilterChanged(const QString &text)
{
    m_internetProxy->setFilterFixedString(text);
}

void MainWindow::onFavoritesFilterChanged(const QString &text)
{
    m_favoritesProxy->setFilterFixedString(text);
}

void MainWindow::applyServerCache(QVector<ServerInfo> *servers) const
{
    if (!servers)
        return;

    for (ServerInfo &server : *servers) {
        if (!m_serverCache.contains(server.key()))
            continue;

        const ServerInfo cached = m_serverCache.value(server.key());
        if (server.hostname.isEmpty())
            server.hostname = cached.hostname;
        if (server.gamemode.isEmpty())
            server.gamemode = cached.gamemode;
        if (server.language.isEmpty())
            server.language = cached.language;
        if (server.address.isEmpty())
            server.address = cached.address;
        if (server.port == 0)
            server.port = cached.port;
        if (server.players == 0 && cached.players > 0)
            server.players = cached.players;
        if (server.maxPlayers == 0 && cached.maxPlayers > 0)
            server.maxPlayers = cached.maxPlayers;
        if (server.pingMs < 0 && cached.pingMs >= 0)
            server.pingMs = cached.pingMs;
        if (cached.online)
            server.online = true;
        server.queried = server.queried || cached.queried;
        server.passworded = server.passworded || cached.passworded;
    }
}

void MainWindow::updateServerEntry(ServerListModel *model, const ServerInfo &info) const
{
    if (!model)
        return;

    const int row = model->indexOfKey(info.key());
    if (row < 0)
        return;

    ServerInfo merged = info;
    if (m_serverCache.contains(info.key())) {
        const ServerInfo cached = m_serverCache.value(info.key());
        if (merged.hostname.isEmpty())
            merged.hostname = cached.hostname;
        if (merged.gamemode.isEmpty())
            merged.gamemode = cached.gamemode;
        if (merged.language.isEmpty())
            merged.language = cached.language;
        if (merged.address.isEmpty())
            merged.address = cached.address;
        if (merged.port == 0)
            merged.port = cached.port;
        if (merged.players == 0 && cached.players > 0)
            merged.players = cached.players;
        if (merged.maxPlayers == 0 && cached.maxPlayers > 0)
            merged.maxPlayers = cached.maxPlayers;
        if (merged.pingMs < 0 && cached.pingMs >= 0)
            merged.pingMs = cached.pingMs;
        if (merged.online && !cached.online)
            merged.online = true;
        merged.queried = true;
    }

    model->upsertServer(merged);
}

bool MainWindow::isProcessRunning(qint64 pid) const
{
    if (pid <= 0)
        return false;
    const int exitCode = QProcess::execute("kill", {"-0", QString::number(pid)});
    return exitCode == 0;
}
