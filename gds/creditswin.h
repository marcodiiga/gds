#ifndef CREDITSWIN_H
#define CREDITSWIN_H

#include <QDialog>
#include <QtGui>
#include <QTimer>

namespace Ui
{
    class CreditsWin;
}

class CreditsWin : public QDialog
{
    Q_OBJECT
    
public:
    explicit CreditsWin(QWidget *parent = 0);
    ~CreditsWin();
    
private:
    Ui::CreditsWin *ui;
    QTimer m_animTmr;

private slots:
    void LinkedInClicked();
    void TwitterClicked();
};

#endif // CREDITSWIN_H
