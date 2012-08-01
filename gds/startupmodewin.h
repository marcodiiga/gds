#ifndef STARTUPMODEWIN_H
#define STARTUPMODEWIN_H

#include <QDialog>
#include "creditswin.h"

namespace Ui
{
    class startupModeWin;
}

class startupModeWin : public QDialog
{
    Q_OBJECT
    
public:
    explicit startupModeWin(QWidget *parent = 0);
    ~startupModeWin();

    bool m_viewMode;

protected:
    void closeEvent(QCloseEvent *);
    
private slots:
    void on_viewModeBtn_clicked();

    void on_editModeBtn_clicked();

    void on_creditsBtn_clicked();

private:
    Ui::startupModeWin *ui;
};

#endif // STARTUPMODEWIN_H
