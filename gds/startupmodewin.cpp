#include "startupmodewin.h"
#include "ui_startupmodewin.h"

startupModeWin::startupModeWin(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::startupModeWin)
{
    ui->setupUi(this);
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setAttribute(Qt::WA_QuitOnClose);
}

startupModeWin::~startupModeWin()
{
    delete ui;
}

void startupModeWin::on_viewModeBtn_clicked()
{
    m_viewMode = true;
    accept();
}

void startupModeWin::on_editModeBtn_clicked()
{
    m_viewMode = false;
    accept();
}

void startupModeWin::closeEvent(QCloseEvent *)
{
    //exit(1);
}

void startupModeWin::on_creditsBtn_clicked()
{
    CreditsWin m_creditsWin;
    QWidget::setVisible(false); // Call the base class to prevent this QDialog's exit
    m_creditsWin.exec();
    setVisible(true);
}
