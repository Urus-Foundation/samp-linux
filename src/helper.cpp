#include "helper.h"

// This function must called on the end of window, because window size can be changed
// after all widgets are added to window. So, call this function after all widgets are
// added to window.
void disableWindowMaximizeButton(QWidget *window)
{
    if (!window)
        return;

    window->setWindowFlags(window->windowFlags() & ~Qt::WindowMaximizeButtonHint);
    window->adjustSize();
    window->setFixedSize(window->size());
}

namespace {
QPixmap& atlasPixmap()
{
    static QPixmap atlas;
    if (atlas.isNull())
        atlas = QPixmap(":/icons/ICON_ATLAS.png");
    return atlas;
}
} // namespace

QIcon getIcon(IconIndex idx) {
    int col = idx % 8, row = idx / 8;
    const QRect src(2 + col * 34, 2 + row * 34, 32, 32);
    return QIcon(atlasPixmap().copy(src));
}
