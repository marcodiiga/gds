#include "creditswin.h"
#include "ui_creditswin.h"

CreditsWin::CreditsWin(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CreditsWin)
{
    ui->setupUi(this);

    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setAttribute(Qt::WA_QuitOnClose);
    this->setFixedSize(466, 156);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->graphicsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // This timer will activate the transition
    m_animTmr.setInterval(500);
    m_animTmr.setSingleShot(true);

    QGraphicsScene *scene = new QGraphicsScene(ui->graphicsView);
    scene->setBackgroundBrush(scene->palette().window());

    // Notice: we can't set geometry here because these widgets have no parent, we'll set after adding them to the scene
    QLabel *lblImg = new QLabel();
    lblImg->setPixmap(QPixmap(":/genericResources/gds_icon/gds_logo.png"));
    QLabel *lblTitle = new QLabel("<font size=\"4\"><b>G</b>raphical <b>D</b>ocumentation <b>S</b>ystem</font>");
    QLabel *lblDescr = new QLabel("A simple and intuitive documentation system for C/C++ code projects.\n\nIf you want to contact the author of this application you can use the following information");
    lblDescr->setWordWrap(true);
    QLabel *lblWrittenBy = new QLabel("Written by Alesiani Marco");
    QToolButton *btnLinkedIn = new QToolButton();
    btnLinkedIn->setText("LinkedIn profile");
    btnLinkedIn->setIcon(QIcon(":/genericResources/gds_icon/linkedin_logo.png"));
    btnLinkedIn->setIconSize(QSize(200,16));
    btnLinkedIn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QToolButton *btnTwitter = new QToolButton();
    btnTwitter->setText("Twitter profile");
    btnTwitter->setIcon(QIcon(":/genericResources/gds_icon/twitter_logo.png"));
    btnTwitter->setIconSize(QSize(200,16));
    btnTwitter->setToolButtonStyle(Qt::ToolButtonIconOnly);


    scene->addWidget(lblImg);
    scene->addWidget(lblTitle);
    scene->addWidget(lblDescr);
    scene->addWidget(lblWrittenBy);
    scene->addWidget(btnLinkedIn);
    scene->addWidget(btnTwitter);


    QStateMachine *machine;
    QState *s1 = new QState();
    QState *s2 = new QState();

    // Setto le robe sugli stati

    s2->assignProperty(lblImg, "geometry", QRect(0,0,150,150));
    s2->assignProperty(lblTitle, "geometry", QRect(160, 5, 191, 16));
    s2->assignProperty(lblDescr, "geometry", QRect(160, 25, 281, 71));
    s2->assignProperty(lblWrittenBy, "geometry", QRect(160, 105, 121, 16));
    s2->assignProperty(btnLinkedIn, "geometry", QRect(160, 125, 101, 19));
    s2->assignProperty(btnTwitter, "geometry", QRect(270, 125, 101, 19));


    s1->assignProperty(lblImg, "geometry", QRect(0,0,1,1));
    s1->assignProperty(lblTitle, "geometry", QRect(660, 5, 191, 16));
    s1->assignProperty(lblDescr, "geometry", QRect(222, 668, 281, 71));
    s1->assignProperty(lblWrittenBy, "geometry", QRect(760, 505, 121, 16));
    s1->assignProperty(btnLinkedIn, "geometry", QRect(160, 525, 101, 19));
    s1->assignProperty(btnTwitter, "geometry", QRect(270, 525, 101, 19));


    // Add transitions
    QAbstractTransition *t1 = s1->addTransition(&m_animTmr, SIGNAL(timeout()), s2);

    // Add smooth animations to transitions
    QSequentialAnimationGroup *animation1SubGroup = new QSequentialAnimationGroup;
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(lblImg, "geometry"));
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(lblTitle, "geometry"));
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(lblDescr, "geometry"));
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(lblWrittenBy, "geometry"));
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(btnLinkedIn, "geometry"));
    animation1SubGroup->addPause(222);
    animation1SubGroup->addAnimation(new QPropertyAnimation(btnTwitter, "geometry"));

    t1->addAnimation(animation1SubGroup);


    machine = new QStateMachine(this);
    machine->addState(s1);
    machine->addState(s2);
    machine->setInitialState(s1);

    machine->start();

    ui->graphicsView->setScene(scene);

    // Set the external links
    connect(btnLinkedIn, SIGNAL(clicked()), this, SLOT(LinkedInClicked()));
    connect(btnTwitter, SIGNAL(clicked()), this, SLOT(TwitterClicked()));

    m_animTmr.start();
}

CreditsWin::~CreditsWin()
{
    delete ui;
}

void CreditsWin::LinkedInClicked()
{
    QDesktopServices::openUrl(QUrl("http://www.linkedin.com/pub/marco-alesiani/47/4b6/252"));
}

void CreditsWin::TwitterClicked()
{
    QDesktopServices::openUrl(QUrl("https://twitter.com/marcodiiga"));
}
