#ifndef CODEEDITORWIDGET_H
#define CODEEDITORWIDGET_H

#include <QTextEdit>
#include <QDebug>
#include <QTextBlock>
#include <QMouseEvent>
#include <QPainter>
#include <QLayout>
#include <QScrollBar>
#include <QApplication>

class CodeEditorWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditorWidget(QTextEdit &lineCounter, QWidget *parent = 0);

    // This vector stores the line numbers currently selected
    QVector<quint32> m_selectedLines;
    void highlightLines(QVector<quint32> linesNumbers);
    QString getLineData(quint32 blockNum);
    void clearAllCodeHighlights();

    bool m_editMode; // Need to be set if in view mode

protected:
    void mouseReleaseEvent(QMouseEvent *e);

private:
    // A friend textedit to count lines
    QTextEdit *m_lineCounter;

signals:
     void updateScrollBarValueChanged(int newValue);

private slots:
    void updateFriendLineCounter();
};

#endif // CODEEDITORWIDGET_H
