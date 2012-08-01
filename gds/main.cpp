#include "qtsingleapplication/singleapplication.h"
#include "startupmodewin.h"
#include "mainwindoweditmode.h"
#include "mainwindowviewmode.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    SingleApplication app(argc, argv, "gds#uids#");

    if (app.isRunning())
    {
        // Another instance is already running
        qWarning() << "Another instance is already running";
        return EXIT_FAILURE;
    }

    //app.setQuitOnLastWindowClosed(true);

    // First show the startup window and let the user choose between
    // view mode and edit mode

    startupModeWin *startWin = new startupModeWin();
    if(startWin->exec())
    {
        MainWindowViewMode *mainViewWin;
        MainWindowEditMode *mainEditWin;
        if(startWin->m_viewMode)
        {
            qWarning() << "View mode";

            mainViewWin = new MainWindowViewMode();
            mainViewWin->show();
        }
        else
        {
            qWarning() << "Edit mode";

            mainEditWin = new MainWindowEditMode();
            mainEditWin->show();
        }
        return app.exec();
    }

    return -1;
}
