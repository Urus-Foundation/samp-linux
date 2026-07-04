#pragma once

#include <QWidget>

// 32x32 icon atlas for the app.
typedef enum {
    ICON_SAMP_LOGO = 0,

    // Main window icons
    ICON_CONNECT,
    ICON_DISCONNECT,
    ICON_REFRESH,

    // Context menu icons
    ICON_ADD_FAV,
    ICON_DELETE_SVR,
    ICON_COPY_INFO,
    ICON_SVR_PROPS,

    ICON_FILTER,
    // Tab icons
    ICON_TAB_FAV,
    ICON_TAB_INTERNET,
    ICON_TAB_HOSTED,
    
    ICON_PING_GRAPH,
    ICON_FIND,

    // Server status icons
    ICON_PASSWORD,
    ICON_UNPASSWORD,

    // Toolbar icons
    ICON_TB_CONNECT, ICON_TB_ADD, ICON_TB_DELETE,
    ICON_TB_REFRESH, ICON_TB_INFO, ICON_TB_CHAT,
    ICON_TB_SETTINGS, ICON_TB_WEB,

    // Server status icons
    ICON_SVR_ONLINE,
    ICON_SVR_OFFLINE,
    ICON_SVR_FULL
} IconIndex;

void disableWindowMaximizeButton(QWidget *window);
QIcon getIcon(IconIndex idx);
