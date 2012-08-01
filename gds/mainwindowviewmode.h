#ifndef MAINWINDOWVIEWMODE_H
#define MAINWINDOWVIEWMODE_H

#include <QMainWindow>
#include <QMessageBox>
#include "diagramwidget/qgldiagramwidget.h"
#include "texteditorwin.h"
#include "gdsdbreader.h"
#include "cpphighlighter.h"
#include "codeeditorwid.h"

namespace Ui
{
    class MainWindowViewMode;
}

class MainWindowViewMode : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowViewMode(QWidget *parent = 0);
    ~MainWindowViewMode();

    friend class QGLDiagramWidget;

private slots:
    void on_goToNextLevel_clicked();
    void on_goToPreviousLevel_clicked();
    void on_nextStepBtn_clicked();

protected:
    void closeEvent(QCloseEvent *);

private:
    // Window components
    Ui::MainWindowViewMode *ui;
    QGLDiagramWidget *GLDiagramWidget;
    QTextEdit *txtEditorWidget;
    CppHighlighter *txtHighlighter;
    CodeEditorWidget *codeEditorWidget;

    // Generic functions and variables

    bool m_graphWasClicked;
    bool m_animationOnGoing;
    QVector<quint32> m_currentGraphElementsAlreadyVisited;
    void tryToLoadLevelDb(level lvl, bool returnToElement);
    void freeCurrentGraphElements();
    void convertDbDataToStorableData(bool m_towardsDiskFile);
    void deferredPaintNow();
    void updateGLGraph();
    void loadSelectedElementDataInPanes();
    QString convertToRelativePath(QString fileAbsolutePath);
    QString convertToAbsolutePath(QString relativePath);
    void clearAllPanes();
    void GLWidgetNotifySelectionChanged(void *m_newSelection);
    void changeSelectedElement(quint32 newSelectedElementIndex);

    // -- System structures

    // The current active level, this is a fundamental variable
    level m_currentActiveLevel;

    // All elements for the current active graph (and relative GL pointers)
    QVector<dbDataStructure*> m_currentGraphElements;
    // The selected element index for the current active graph (this is updated by the openGL widget through a function)
    dbDataStructure* m_selectedElement;
    // These pointers help in finding/creating the next database file while browsing zoom levels
    quint64 m_currentLevelOneID;
    quint64 m_currentLevelTwoID;
    // This function gets the next free unique ID based on the elements on the graph
    quint64 getThisGraphNextFreeID();
};

#endif // MAINWINDOWVIEWMODE_H
