#ifndef CPPHIGHLIGHTER_H
#define CPPHIGHLIGHTER_H

#include <QSyntaxHighlighter>

#include <QVector>
#include <QTextCharFormat>

class CppHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CppHighlighter(QTextDocument *parent = 0);

protected:
    void highlightBlock(const QString &text);
    
signals:
    
public slots:

private:
    struct HighlightingRule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };

    // The vector that contains all the rules for the text
    QVector<HighlightingRule> highlightingRules;

    QRegExp commentStartExpression;
    QRegExp commentEndExpression;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    
};

#endif // CPPHIGHLIGHTER_H
