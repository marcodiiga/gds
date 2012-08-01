#include "texteditorwin.h"


const QString rsrcPath = ":/editorResources/editorimages";


textEditorWin::~textEditorWin()
{
    // Free up all allocated memory

    delete m_textEditorWin;
}

textEditorWin::textEditorWin(QWidget *parent)
    : QMainWindow(parent)
{
    setToolButtonStyle(Qt::ToolButtonFollowStyle);

    // Set up edit actions like copy,paste,cut,etc..
    setupEditActions();
    // Set up text actions like italic, bold, etc..
    setupTextActions();

    // Create the QTextEdit with rich text and connect the signals when the cursor moves on
    // different text formats
    m_textEditorWin = new QTextEdit();
    connect(m_textEditorWin, SIGNAL(currentCharFormatChanged(QTextCharFormat)),
            this, SLOT(currentCharFormatChanged(QTextCharFormat)));
    connect(m_textEditorWin, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChanged()));

    setCentralWidget(m_textEditorWin);
    m_textEditorWin->setFocus();

    // Set everything with default font/text/alignment
    fontChanged(m_textEditorWin->font());
    colorChanged(m_textEditorWin->textColor());
    alignmentChanged(m_textEditorWin->alignment());

    // Let do and undo be available
    connect(m_textEditorWin->document(), SIGNAL(undoAvailable(bool)),
            actionUndo, SLOT(setEnabled(bool)));
    connect(m_textEditorWin->document(), SIGNAL(redoAvailable(bool)),
            actionRedo, SLOT(setEnabled(bool)));

    actionUndo->setEnabled(m_textEditorWin->document()->isUndoAvailable());
    actionRedo->setEnabled(m_textEditorWin->document()->isRedoAvailable());

    // Set undo and redo operative
    connect(actionUndo, SIGNAL(triggered()), m_textEditorWin, SLOT(undo()));
    connect(actionRedo, SIGNAL(triggered()), m_textEditorWin, SLOT(redo()));

    // First we need to select or copy something to use these
    actionCut->setEnabled(false);
    actionCopy->setEnabled(false);

    connect(actionCut, SIGNAL(triggered()), m_textEditorWin, SLOT(cut()));
    connect(actionCopy, SIGNAL(triggered()), m_textEditorWin, SLOT(copy()));
    connect(actionPaste, SIGNAL(triggered()), m_textEditorWin, SLOT(paste()));

    connect(m_textEditorWin, SIGNAL(copyAvailable(bool)), actionCut, SLOT(setEnabled(bool)));
    connect(m_textEditorWin, SIGNAL(copyAvailable(bool)), actionCopy, SLOT(setEnabled(bool)));
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardDataChanged()));
}

void textEditorWin::closeEvent(QCloseEvent *)
{
    qWarning() << "closing down editor..";
}

void textEditorWin::setupEditActions()
{
    QToolBar *tb = new QToolBar(this);
    tb->setWindowTitle(tr("Edit Actions"));
    addToolBar(Qt::LeftToolBarArea, tb);

    QAction *a;
    a = actionUndo = new QAction(QIcon::fromTheme("edit-undo", QIcon(rsrcPath + "/editundo.png")),
                                              tr("&Undo"), this);
    a->setShortcut(QKeySequence::Undo);
    tb->addAction(a);
    a = actionRedo = new QAction(QIcon::fromTheme("edit-redo", QIcon(rsrcPath + "/editredo.png")),
                                              tr("&Redo"), this);
    a->setPriority(QAction::LowPriority);
    a->setShortcut(QKeySequence::Redo);
    tb->addAction(a);
    a = actionCut = new QAction(QIcon::fromTheme("edit-cut", QIcon(rsrcPath + "/editcut.png")),
                                             tr("Cu&t"), this);
    a->setPriority(QAction::LowPriority);
    a->setShortcut(QKeySequence::Cut);
    tb->addAction(a);
    a = actionCopy = new QAction(QIcon::fromTheme("edit-copy", QIcon(rsrcPath + "/editcopy.png")),
                                 tr("&Copy"), this);
    a->setPriority(QAction::LowPriority);
    a->setShortcut(QKeySequence::Copy);
    tb->addAction(a);
    a = actionPaste = new QAction(QIcon::fromTheme("edit-paste", QIcon(rsrcPath + "/editpaste.png")),
                                  tr("&Paste"), this);
    a->setPriority(QAction::LowPriority);
    a->setShortcut(QKeySequence::Paste);
    tb->addAction(a);
    if (const QMimeData *md = QApplication::clipboard()->mimeData())
        actionPaste->setEnabled(md->hasText());
}

void textEditorWin::setupTextActions()
{
    QToolBar *tb = new QToolBar(this);
    tb->setWindowTitle(tr("Format Actions"));
    addToolBar(Qt::LeftToolBarArea, tb);

    actionTextBold = new QAction(QIcon::fromTheme("format-text-bold", QIcon(rsrcPath + "/textbold.png")),
                                 tr("&Bold"), this);
    actionTextBold->setShortcut(Qt::CTRL + Qt::Key_B);
    actionTextBold->setPriority(QAction::LowPriority);
    QFont bold;
    bold.setBold(true);
    actionTextBold->setFont(bold);
    connect(actionTextBold, SIGNAL(triggered()), this, SLOT(textBold()));
    tb->addAction(actionTextBold);
    actionTextBold->setCheckable(true);

    actionTextItalic = new QAction(QIcon::fromTheme("format-text-italic", QIcon(rsrcPath + "/textitalic.png")),
                                   tr("&Italic"), this);
    actionTextItalic->setPriority(QAction::LowPriority);
    actionTextItalic->setShortcut(Qt::CTRL + Qt::Key_I);
    QFont italic;
    italic.setItalic(true);
    actionTextItalic->setFont(italic);
    connect(actionTextItalic, SIGNAL(triggered()), this, SLOT(textItalic()));
    tb->addAction(actionTextItalic);
    actionTextItalic->setCheckable(true);

    actionTextUnderline = new QAction(QIcon::fromTheme("format-text-underline", QIcon(rsrcPath + "/textunder.png")),
                                      tr("&Underline"), this);
    actionTextUnderline->setShortcut(Qt::CTRL + Qt::Key_U);
    actionTextUnderline->setPriority(QAction::LowPriority);
    QFont underline;
    underline.setUnderline(true);
    actionTextUnderline->setFont(underline);
    connect(actionTextUnderline, SIGNAL(triggered()), this, SLOT(textUnderline()));
    tb->addAction(actionTextUnderline);
    actionTextUnderline->setCheckable(true);

    actionInsertImage = new QAction(QIcon::fromTheme("format-image-insert", QIcon(rsrcPath + "/insertimage.png")),
                                    tr("&Insert Image"), this);
    actionInsertImage->setCheckable(false);
    actionInsertImage->setPriority(QAction::LowPriority);
    connect(actionInsertImage, SIGNAL(triggered()), this, SLOT(addImageToText()));
    tb->addAction(actionInsertImage);

    QActionGroup *grp = new QActionGroup(this);
    connect(grp, SIGNAL(triggered(QAction*)), this, SLOT(textAlign(QAction*)));

    // Make sure the alignLeft is always left of the alignRight
    if (QApplication::isLeftToRight())
    {
        actionAlignLeft = new QAction(QIcon::fromTheme("format-justify-left", QIcon(rsrcPath + "/textleft.png")),
                                      tr("&Left"), grp);
        actionAlignCenter = new QAction(QIcon::fromTheme("format-justify-center", QIcon(rsrcPath + "/textcenter.png")), tr("C&enter"), grp);
        actionAlignRight = new QAction(QIcon::fromTheme("format-justify-right", QIcon(rsrcPath + "/textright.png")), tr("&Right"), grp);
    }
    else
    {
        actionAlignRight = new QAction(QIcon::fromTheme("format-justify-right", QIcon(rsrcPath + "/textright.png")), tr("&Right"), grp);
        actionAlignCenter = new QAction(QIcon::fromTheme("format-justify-center", QIcon(rsrcPath + "/textcenter.png")), tr("C&enter"), grp);
        actionAlignLeft = new QAction(QIcon::fromTheme("format-justify-left", QIcon(rsrcPath + "/textleft.png")), tr("&Left"), grp);
    }
    actionAlignJustify = new QAction(QIcon::fromTheme("format-justify-fill", QIcon(rsrcPath + "/textjustify.png")), tr("&Justify"), grp);

    actionAlignLeft->setShortcut(Qt::CTRL + Qt::Key_L);
    actionAlignLeft->setCheckable(true);
    actionAlignLeft->setPriority(QAction::LowPriority);
    actionAlignCenter->setShortcut(Qt::CTRL + Qt::Key_E);
    actionAlignCenter->setCheckable(true);
    actionAlignCenter->setPriority(QAction::LowPriority);
    actionAlignRight->setShortcut(Qt::CTRL + Qt::Key_R);
    actionAlignRight->setCheckable(true);
    actionAlignRight->setPriority(QAction::LowPriority);
    actionAlignJustify->setShortcut(Qt::CTRL + Qt::Key_J);
    actionAlignJustify->setCheckable(true);
    actionAlignJustify->setPriority(QAction::LowPriority);

    tb->addActions(grp->actions());


    // Now create the list, font and size toolbar

    QPixmap pix(16, 16);
    pix.fill(Qt::black);
    actionTextColor = new QAction(pix, tr("&Color..."), this);
    connect(actionTextColor, SIGNAL(triggered()), this, SLOT(textColor()));
    tb->addAction(actionTextColor);

    tb = new QToolBar(this);
    tb->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    tb->setWindowTitle(tr("Format Actions"));
    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(tb);

    comboStyle = new QComboBox(tb);
    tb->addWidget(comboStyle);
    comboStyle->addItem("Standard");
    comboStyle->addItem("Bullet List (Disc)");
    comboStyle->addItem("Bullet List (Circle)");
    comboStyle->addItem("Bullet List (Square)");
    comboStyle->addItem("Ordered List (Decimal)");
    comboStyle->addItem("Ordered List (Alpha lower)");
    comboStyle->addItem("Ordered List (Alpha upper)");
    comboStyle->addItem("Ordered List (Roman lower)");
    comboStyle->addItem("Ordered List (Roman upper)");
    connect(comboStyle, SIGNAL(activated(int)),
            this, SLOT(textStyle(int)));

    comboFont = new QFontComboBox(tb);
    tb->addWidget(comboFont);
    connect(comboFont, SIGNAL(activated(QString)),
            this, SLOT(textFamily(QString)));

    comboSize = new QComboBox(tb);
    comboSize->setObjectName("comboSize");
    tb->addWidget(comboSize);
    comboSize->setEditable(true);

    QFontDatabase db;
    foreach(int size, db.standardSizes())
        comboSize->addItem(QString::number(size));

    connect(comboSize, SIGNAL(activated(QString)),
            this, SLOT(textSize(QString)));
    comboSize->setCurrentIndex(comboSize->findText(QString::number(QApplication::font()
                                                                   .pointSize())));
}

// Adds an image to the text embedding it with base64 into html
void textEditorWin::addImageToText()
{
    // Prompt the user to retrieve the image
    QString fn = QFileDialog::getOpenFileName(this, tr("Open Image File..."),
                                              QString(), tr("Image Files (*.png *.jpg *.bmp)"));
    if (fn.isEmpty() || !QFile::exists(fn))
        // Nothing to load
        return;

    QFile file(fn);
    if (!file.open(QFile::ReadOnly))
        return;

    // This returns (if recognized): "jpeg", "bmp" or "png"
    QByteArray imageFormat = QImageReader::imageFormat(fn);
    if(imageFormat != QByteArray("jpeg") && imageFormat != QByteArray("bmp") && imageFormat != QByteArray("png"))
    {
        QMessageBox::warning(this, "Error loading image", "Format not recognized, supported formats are jpeg,png,bmp");
        file.close();
        return;
    }

    QByteArray data = file.readAll();

    m_textEditorWin->insertHtml("<img alt=\"\" src=\"data:image//"+QString(imageFormat)+";base64,"+data.toBase64()+" //>");

    file.close();
}

void textEditorWin::textBold()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(actionTextBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(fmt);
}

void textEditorWin::textUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(actionTextUnderline->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void textEditorWin::textItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(actionTextItalic->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void textEditorWin::textFamily(const QString &f)
{
    QTextCharFormat fmt;
    fmt.setFontFamily(f);
    mergeFormatOnWordOrSelection(fmt);
}

void textEditorWin::textSize(const QString &p)
{
    qreal pointSize = p.toFloat();
    if (p.toFloat() > 0)
    {
        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        mergeFormatOnWordOrSelection(fmt);
    }
}

// Set a text style from the current cursor position
void textEditorWin::textStyle(int styleIndex)
{
    QTextCursor cursor = m_textEditorWin->textCursor();

    if (styleIndex != 0)
    {
        QTextListFormat::Style style = QTextListFormat::ListDisc;

        switch (styleIndex)
        {
            default:
            case 1:
                style = QTextListFormat::ListDisc;
                break;
            case 2:
                style = QTextListFormat::ListCircle;
                break;
            case 3:
                style = QTextListFormat::ListSquare;
                break;
            case 4:
                style = QTextListFormat::ListDecimal;
                break;
            case 5:
                style = QTextListFormat::ListLowerAlpha;
                break;
            case 6:
                style = QTextListFormat::ListUpperAlpha;
                break;
            case 7:
                style = QTextListFormat::ListLowerRoman;
                break;
            case 8:
                style = QTextListFormat::ListUpperRoman;
                break;
        }

        cursor.beginEditBlock();

        QTextBlockFormat blockFmt = cursor.blockFormat();

        QTextListFormat listFmt;

        // If we already are in a list, set the style to it,
        // otherwise indent text as a list
        if (cursor.currentList())
        {
            listFmt = cursor.currentList()->format();
        }
        else
        {
            listFmt.setIndent(blockFmt.indent() + 1);
            blockFmt.setIndent(0);
            cursor.setBlockFormat(blockFmt);
        }

        listFmt.setStyle(style);

        cursor.createList(listFmt);

        cursor.endEditBlock();
    }
    else
    {
        // ####
        QTextBlockFormat bfmt;
        bfmt.setObjectIndex(-1); // Standard style
        cursor.mergeBlockFormat(bfmt);
    }
}

// Change text color
void textEditorWin::textColor()
{
    QColor col = QColorDialog::getColor(m_textEditorWin->textColor(), this);
    if (!col.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setForeground(col);
    mergeFormatOnWordOrSelection(fmt);
    colorChanged(col);
}

// Change text alignment
void textEditorWin::textAlign(QAction *a)
{
    if (a == actionAlignLeft)
        m_textEditorWin->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    else if (a == actionAlignCenter)
        m_textEditorWin->setAlignment(Qt::AlignHCenter);
    else if (a == actionAlignRight)
        m_textEditorWin->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    else if (a == actionAlignJustify)
        m_textEditorWin->setAlignment(Qt::AlignJustify);
}

// Reflect the changes to the font/color
void textEditorWin::currentCharFormatChanged(const QTextCharFormat &format)
{
    fontChanged(format.font());
    colorChanged(format.foreground().color());
}

void textEditorWin::cursorPositionChanged()
{
    alignmentChanged(m_textEditorWin->alignment());
}

void textEditorWin::clipboardDataChanged()
{
    if (const QMimeData *md = QApplication::clipboard()->mimeData())
        actionPaste->setEnabled(md->hasText());
}

void textEditorWin::mergeFormatOnWordOrSelection(const QTextCharFormat &format)
{
    QTextCursor cursor = m_textEditorWin->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    m_textEditorWin->mergeCurrentCharFormat(format);
}

void textEditorWin::fontChanged(const QFont &f)
{
    comboFont->setCurrentIndex(comboFont->findText(QFontInfo(f).family()));
    comboSize->setCurrentIndex(comboSize->findText(QString::number(f.pointSize())));
    actionTextBold->setChecked(f.bold());
    actionTextItalic->setChecked(f.italic());
    actionTextUnderline->setChecked(f.underline());
}

void textEditorWin::colorChanged(const QColor &c)
{
    QPixmap pix(16, 16);
    pix.fill(c);
    actionTextColor->setIcon(pix);
}

void textEditorWin::alignmentChanged(Qt::Alignment a)
{
    if (a & Qt::AlignLeft)
    {
        actionAlignLeft->setChecked(true);
    } else if (a & Qt::AlignHCenter)
    {
        actionAlignCenter->setChecked(true);
    } else if (a & Qt::AlignRight)
    {
        actionAlignRight->setChecked(true);
    } else if (a & Qt::AlignJustify)
    {
        actionAlignJustify->setChecked(true);
    }
}

