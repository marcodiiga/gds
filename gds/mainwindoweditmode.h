#ifndef MAINWINDOWEDITMODE_H
#define MAINWINDOWEDITMODE_H

#include <QMainWindow>
#include <QMessageBox>
#include "diagramwidget/qgldiagramwidget.h"
#include "texteditorwin.h"
#include "gdsdbreader.h"
#include "cpphighlighter.h"
#include "codeeditorwid.h"

namespace Ui
{
    class MainWindowEditMode;
}

class MainWindowEditMode : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindowEditMode(QWidget *parent = 0);
    ~MainWindowEditMode();

    friend class QGLDiagramWidget;
    
private slots:

    void on_goToNextLevel_clicked();
    void on_addChildBlockBtn_clicked();
    void enterPressedOnLabelEditBox();
    void on_deleteSelectedElementBtn_clicked();
    void on_swapBtn_clicked();
    void on_spinBox_editingFinished();
    void closeEvent(QCloseEvent *);
    void on_goToPreviousLevel_clicked();
    void on_browseCodeFiles_clicked();
    void on_fileComboBox_activated(const QString &arg1);
    void on_clearCodeFileBtn_clicked();

private:
    // Window components
    Ui::MainWindowEditMode *ui;
    QGLDiagramWidget *GLDiagramWidget;
    textEditorWin *txtEditorWidget;
    CppHighlighter *txtHighlighter;
    CodeEditorWidget *codeEditorWidget;

    // Generic functions and variables

    void tryToLoadLevelDb(level lvl, bool returnToElement);
    void saveCurrentLevelDb();
    void convertDbDataToStorableData(bool m_towardsDiskFile);
    void freeCurrentGraphElements();
    void updateGLGraph();

    void *GLWidgetNotifySelectionChanged(void *m_newSelection);
    void deferredPaintNow();

    void recursiveDelete(dbDataStructure* element);
    bool m_swapRunning;
    bool m_lastSelectedHasBeenDeleted;
    void loadSelectedElementDataInPanes();
    QString convertToRelativePath(QString fileAbsolutePath);
    QString convertToAbsolutePath(QString relativePath);
    void saveEverythingOnThePanesToMemory();
    void clearAllPanes();

    QVector<QString> m_recentFilePaths; // Used by the combo box to display a maximum of 15 files

    // -- System structures

    // If this variable is set, the graph is empty and there's no root element yet in our current level
    bool m_firstTimeGraphInCurrentLevel;
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

#endif // MAINWINDOWEDITMODE_H
