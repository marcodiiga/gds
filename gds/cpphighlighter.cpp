#include "cpphighlighter.h"

CppHighlighter::CppHighlighter(QTextDocument *parent) :
    QSyntaxHighlighter(parent)
{
    // Defines a set of syntax rules structures with the couple
    // QRegExp pattern; -> the matching regex on the text
    // QTextCharFormat format; -> the format to apply to the matching text
    //
    // This is pretty much the same method used in the richtext-syntaxhighlighter demo
    HighlightingRule rule;

    // Define C/C++ keywords and their color
    keywordFormat.setForeground(Qt::blue);
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns;
    keywordPatterns << "\\bchar\\b" << "\\bclass\\b" << "\\bconst\\b"
                    << "\\bdouble\\b" << "\\benum\\b" << "\\bexplicit\\b"
                    << "\\bfriend\\b" << "\\binline\\b" << "\\bint\\b"
                    << "\\blong\\b" << "\\bnamespace\\b" << "\\boperator\\b"
                    << "\\bprivate\\b" << "\\bprotected\\b" << "\\bpublic\\b"
                    << "\\bshort\\b" << "\\bsignals\\b" << "\\bsigned\\b"
                    << "\\bslots\\b" << "\\bstatic\\b" << "\\bstruct\\b"
                    << "\\btemplate\\b" << "\\btypedef\\b" << "\\btypename\\b"
                    << "\\bunion\\b" << "\\bunsigned\\b" << "\\bvirtual\\b"
                    << "\\bvoid\\b" << "\\bvolatile\\b";
    // Insert each of the above keywords in the vector that contains all the text rules
    foreach (const QString &pattern, keywordPatterns)
    {
        rule.pattern = QRegExp(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Rule: Qt class name or keyword
    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegExp("\\bQ[A-Za-z]+\\b");
    rule.format = classFormat;
    highlightingRules.append(rule);

    // Rule: single line text comment
    singleLineCommentFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegExp("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    // Multiline comments cannot be applied with a single regex but are handled with "blocks" of text in the
    // highlightBlock function
    multiLineCommentFormat.setForeground(Qt::darkGreen);

    // Rule: text between quotation marks
    quotationFormat.setForeground(Qt::darkYellow);
    rule.pattern = QRegExp("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // Rule: function name()
    //functionFormat.setFontItalic(true);
    //functionFormat.setFontWeight(QFont::Bold);
    functionFormat.setForeground(Qt::darkRed);
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // Multiline comments start and end regex, used in the "highlighBlock" function
    commentStartExpression = QRegExp("/\\*");
    commentEndExpression = QRegExp("\\*/");
}

// This function is called whenever a text block changes or whenever the
// entire document needs to. The "text" variable is each line that has been changed, this
// function is called multiple times
void CppHighlighter::highlightBlock(const QString &text)
{
    // Try to apply each rule to this changed block of text
    foreach (const HighlightingRule &rule, highlightingRules)
    {
        // If this rule matches the text, store the initial index
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        // Index is negative if there was no match with the regex
        while (index >= 0)
        {
            // Apply the rule format to this portion of text
            int length = expression.matchedLength();
            setFormat(index, length, rule.format);
            // Continue searching the text block for other matches
            index = expression.indexIn(text, index + length);
        }
    }

    // Now handle the /**/ multiline comment

    // Set this block to 0 - not in comment
    setCurrentBlockState(0);

    // Try to find a /* occurrence
    int startIndex = 0;
    // If the previous block wasn't in comment, try to find a /*
    if (previousBlockState() != 1)
        startIndex = commentStartExpression.indexIn(text);

    while (startIndex >= 0)
    {
        // There's a /* occurrence in this block or there was one in one of the previous ones
        int endIndex = commentEndExpression.indexIn(text, startIndex);
        int commentLength;

        if (endIndex == -1)
        {
            // No */ occurrence, just get the total length of this block and comment everything
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else
        {
            // If there's a /* in this line, comment just what's before it
            commentLength = endIndex - startIndex
                    + commentEndExpression.matchedLength(); // plus the "/*" size, alias 2
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        // Search for other occurrences on this line (if we're totally commented, the commentLength will take us to the end)
        startIndex = commentStartExpression.indexIn(text, startIndex + commentLength);
    }
}
