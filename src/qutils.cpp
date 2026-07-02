#include "qutils.h"

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