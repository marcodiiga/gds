#include "codeeditorwid.h"


CodeEditorWidget::CodeEditorWidget(QTextEdit &lineCounter, QWidget *parent) :
    QTextEdit(parent)
{
    m_lineCounter = &lineCounter;
    m_editMode = true;          // By default mouse lines highlighting is enabled, disable this to
                                // enter view mode

    // Initialization settings
    setReadOnly(true);
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::NoWrap);

    // Signal to redraw the line counter when our text has changed
    connect(this, SIGNAL(textChanged()), this, SLOT(updateFriendLineCounter()));
    // And to scroll too
    QScrollBar *scroll1 = this->verticalScrollBar();
    QScrollBar *scroll2 = m_lineCounter->verticalScrollBar();
    connect((const QObject*)scroll1, SIGNAL(valueChanged(int)), (const QObject*)scroll2, SLOT(setValue(int)));
    connect(this, SIGNAL(updateScrollBarValueChanged(int)), (const QObject*)scroll2, SLOT(setValue(int)));
}


void CodeEditorWidget::updateFriendLineCounter()
{
    QString lineNumbers = "";
    QString myText = toPlainText();
    // Count the number of lines of my file
    int numberOfLines = myText.count(QRegExp("\n")) + 1;

    // Adjust the line counter width
    QString numberOfLinesStr(QString("%1").arg(numberOfLines));
    int maximumNumberOfDigits = numberOfLinesStr.size();
    if(myText.isEmpty())
    {
        m_lineCounter->setMinimumWidth(20);
        m_lineCounter->setMaximumWidth(20);
    }
    else
    {
        m_lineCounter->setMinimumWidth(maximumNumberOfDigits * 10);
        m_lineCounter->setMaximumWidth(maximumNumberOfDigits * 10);
    }

    // Now fill the text box with the line numbers
    for(int i = 1; i <= numberOfLines; i++)
    {
        lineNumbers.append(QString::number(i)+"\n");
    }
    m_lineCounter->clear();
    m_lineCounter->setPlainText(lineNumbers);
    m_lineCounter->toPlainText();

    // Make sure that both the line counter and this code box have the same font
    QFont font("Verdana");
    //font.setPixelSize(12);
    this->setFont(font);
    m_lineCounter->setFont(font);

    // If the scrollbar isn't in the initial position, we need to update ours
    QScrollBar *main = this->verticalScrollBar();
    emit updateScrollBarValueChanged(main->value());
}


// Called to highlight lines of code
void CodeEditorWidget::highlightLines(QVector<quint32> linesNumbers)
{
    // FIXME: Qt QTextEdit controls need time to "process setText events", but there's nothing in the
    // documentation about it, these lines fix the problem but it's just a workaround
    QApplication::processEvents();
    QApplication::processEvents();

    // Take back to normal all positions (the elements next to the first are relative to the first)
    QVector<quint32> linesNumbersNormalized;
    linesNumbersNormalized.append(linesNumbers[0]);
    for(int i=1; i<linesNumbers.size(); i++)
    {
        linesNumbersNormalized.append(linesNumbers[0] + linesNumbers[i]);
    }
    for(int i=1; i<linesNumbersNormalized.size(); i++)
    {
        // Highlight EACH line we've saved (except the first, we'll highlight it at the end so the cursor will be there)

        // To go to a specific line, go to 0 and then scroll down to the specific line
        this->setFocus();
        QTextCursor cursor = this->textCursor();
        cursor.setPosition(0);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, linesNumbersNormalized[i]);
        this->setTextCursor(cursor);
        QTextBlock block = document()->findBlockByNumber(linesNumbersNormalized[i]);
        QTextBlockFormat blkfmt = block.blockFormat();
        // Select it
        blkfmt.setBackground(Qt::yellow);
        this->textCursor().mergeBlockFormat(blkfmt);
    }
    // The first line now
    this->setFocus();
    QTextCursor cursor = this->textCursor();
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, linesNumbersNormalized[0]);
    this->setTextCursor(cursor);
    QTextBlock block = document()->findBlockByNumber(linesNumbersNormalized[0]);
    QTextBlockFormat blkfmt = block.blockFormat();
    // Select it
    blkfmt.setBackground(Qt::yellow);
    this->textCursor().mergeBlockFormat(blkfmt);

    // Synchronize this edit box vector with the one read from the db
    m_selectedLines.clear();
    m_selectedLines = linesNumbersNormalized;

    qWarning() << endl;
    qWarning() << "IT WAS TOLD ME TO HIGHLIGHT THESE LINES (SHOULD BE ABSOLUTE)";
    for(int j=0;j<m_selectedLines.size();j++)
        qWarning() << m_selectedLines[j] << " ";
    qWarning() << endl;
}

void CodeEditorWidget::clearAllCodeHighlights()
{
    // Clear ALL the highlighted lines WITHOUT reloading the document (reloading is faster though)

    QTextCursor cursor = this->textCursor();
    for(int i=0; i<m_selectedLines.size(); i++)
    {
        // To go to a specific line, go to 0 and then scroll down to the specific line
        this->setFocus();
        cursor.setPosition(0);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, m_selectedLines[i]);
        this->setTextCursor(cursor);
        QTextBlock block = document()->findBlockByNumber(m_selectedLines[i]);
        QTextBlockFormat blkfmt = block.blockFormat();
        // Select it
        blkfmt.setBackground(Qt::NoBrush);
        this->textCursor().mergeBlockFormat(blkfmt);
    }
    m_selectedLines.clear();
}

QString CodeEditorWidget::getLineData(quint32 blockNum)
{
    return document()->findBlockByNumber(blockNum).text();
}

void CodeEditorWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if(e->button() != Qt::LeftButton || m_editMode == false)
        return; // Not our business

    // Selection could be ready, if it's ready draw the background of these selected lines and store their numbers
    bool m_newSelection = false;

    // Absolute values in the document (number of characters), if there isn't a selection these values are the caret position
    int start = this->textCursor().selectionStart();
    int end = this->textCursor().selectionEnd();

    // Select the line under the cursor or clear the line under the cursor
    this->setFocus();
    this->textCursor().select(QTextCursor::LineUnderCursor);
    QTextBlockFormat blkfmt = this->textCursor().blockFormat();
    if(blkfmt.background() == Qt::yellow)
    {
        // Clear it
        blkfmt.setBackground(Qt::NoBrush);

        m_newSelection = false;
    }
    else
    {
        // Select it
        blkfmt.setBackground(Qt::yellow);

        m_newSelection = true;
    }
    this->textCursor().mergeBlockFormat(blkfmt);


    if(this->textCursor().hasSelection())
    {
        // Try to find line numbers of the selection (start-end)
        QTextCursor c = textCursor();
        c.setPosition(start);
        setTextCursor(c);
        int firstBlock = this->textCursor().blockNumber(); // First block where the selection started
        c.setPosition(end);
        setTextCursor(c);
        int lastBlock = this->textCursor().blockNumber(); // Last block where the selection ended

        if(m_newSelection) // Add lines
        {
            // Add multiple lines to the vector
            for(int i=firstBlock; i<=lastBlock; i++)
            {
                if(!m_selectedLines.contains(i))
                    m_selectedLines.append(i);
            }
        }
        else
        {
            // Clear multiple selection
            for(int i=firstBlock; i<=lastBlock; i++)
            {
                if(m_selectedLines.contains(i))
                {
                    int index = m_selectedLines.indexOf(i);
                    m_selectedLines.remove(index);
                }
            }
        }

        //qWarning() << "Selection from start: " << firstBlock << " to end: " << lastBlock << endl;
    }
    else
    {
        // No selection, just store one line

        //QTextCursor c = textCursor();
        //c.setPosition(start);
        //setTextCursor(c);
        int singleBlock = this->textCursor().blockNumber(); // Single block

        if(m_newSelection) // Add lines
        {
            // Add one line to the vector (if there isn't yet)
            if(!m_selectedLines.contains(singleBlock))
                m_selectedLines.append(singleBlock);
        }
        else
        {
            // Clear selection
            if(m_selectedLines.contains(singleBlock))
            {
                int index = m_selectedLines.indexOf(singleBlock);
                m_selectedLines.remove(index);
            }
        }
        //qWarning() << "No selection, line:" << singleBlock << endl;
    }
    this->update();
}
