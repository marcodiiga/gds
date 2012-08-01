#include "mainwindowviewmode.h"
#include "ui_mainwindowviewmode.h"

MainWindowViewMode::MainWindowViewMode(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindowViewMode)
{
    ui->setupUi(this);

    m_currentGraphElements.clear();
    m_currentGraphElementsAlreadyVisited.clear();
    m_selectedElement = NULL;
    m_graphWasClicked = true;   // This helps distinguish graph clicks (and clear the visited nodes history)
                                // by "Next Block" button clicks (that don't clear the visited nodes history)
    m_animationOnGoing = false; // If the animation is ongoing, zooming is disabled and next element navigation too
    // Still no element selected, and we're in level one
    m_currentLevelOneID = -1;
    m_currentLevelTwoID = -1;

    // Add the textEditor widget to the right part
    txtEditorWidget = new QTextEdit();
    ui->rightArea->addWidget(txtEditorWidget);

    // Create the code editor in the left pane and the line counter
    codeEditorWidget = new CodeEditorWidget(*(ui->lineCounter));
    // Create the code window on the left pane
    txtHighlighter = new CppHighlighter(codeEditorWidget->document());
    // Insert it into the window
    ui->codeLayout->addWidget(codeEditorWidget);
    codeEditorWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    codeEditorWidget->setMinimumWidth(200);
    codeEditorWidget->m_editMode = false; // Doesn't allow mouse highlight

    // Enable multisampling (anti-aliasing) if supported
    // for the following widgets
    QGLFormat glf = QGLFormat::defaultFormat();
    glf.setSampleBuffers(true);
    glf.setSamples(4);
    QGLFormat::setDefaultFormat(glf);

    // SET FIRST LEVEL ACTIVE
    //
    // Notice: after the constructor, no one else is allowed to change this variable's value,
    // use the proper method instead (that updates the code window too if needed)
    m_currentActiveLevel = LEVEL_ONE;

    // First time the edit mode is launched, we're in the first level (everyone can understand it), so
    // we don't need any code file
    this->ui->containerWidget->hide();
    this->ui->goToPreviousLevel->setEnabled(false);

    // Create the diagram widget
    GLDiagramWidget = new QGLDiagramWidget((QMainWindow*)this);
    GLDiagramWidget->m_gdsEditMode = false; // Animation while selecting
    this->ui->graphLayout->addWidget(GLDiagramWidget);
    // Assign the widget to this window
    GLDiagramWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    //
    // DATABASE OPERATIONS ONGOING
    //

    // Try to load the level-1 documentation if present, otherwise create the db directory
    // and set the "new graph" variable
    tryToLoadLevelDb(LEVEL_ONE, false);
}

MainWindowViewMode::~MainWindowViewMode()
{
    delete ui;
    delete txtEditorWidget;
    delete GLDiagramWidget;
    delete txtHighlighter;
    delete codeEditorWidget;
}
void MainWindowViewMode::closeEvent(QCloseEvent *)
{
    exit(1);
}


// This method is called by the openGL widget every time the selection is changed on the graph
void MainWindowViewMode::GLWidgetNotifySelectionChanged(void *m_newSelection)
{
    // Select our new element
    int index;
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        if(m_currentGraphElements[i]->glPointer == m_newSelection)
        {
            m_selectedElement = m_currentGraphElements[i];
            index = i;
            break;
        }
    }

    // Add it to the visited vector if this was initiated by the "Next" button
    if(!m_graphWasClicked)
    {
        m_currentGraphElementsAlreadyVisited.append(index);
        m_graphWasClicked = true;
    }
    else
    {
        // Otherwise clear the vector THEN add it
        m_currentGraphElementsAlreadyVisited.clear();
        m_currentGraphElementsAlreadyVisited.append(index);
    }

    // Load the selected element data in the panes, but first clear them
    clearAllPanes();
    loadSelectedElementDataInPanes();
    m_animationOnGoing = false;
}




void MainWindowViewMode::on_goToNextLevel_clicked()
{
    // Can't zoom while the animation is ongoing
    if(m_animationOnGoing)
        return;

    // If there's no root, just deny it
    if(m_currentGraphElements.size() == 0 || m_selectedElement == NULL)
    {
        QMessageBox::warning(this, "Zoom Error", "Cannot raise the zoom level if there's no element selected in the current graph");
        return;
    }

    // We can't go further if we're on level three
    if(m_currentActiveLevel == LEVEL_THREE)
    {
        QMessageBox::warning(this, "Zoom Error", "The maximum zoom level has already been reached");
        return;
    }

    //////////////////////////////////////
    //  Go to next level
    //////////////////////////////////////

    // 1) NO NEED TO SAVE DATA, view mode doesn't save anything, but we need to check if the file exists
    QString m_nextDbFile;
    switch(m_currentActiveLevel)
    {
        case LEVEL_ONE:
        {
            // We just have to save this ID
            m_currentLevelOneID = m_selectedElement->uniqueID;
            m_nextDbFile = QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";

        }break;
        case LEVEL_TWO:
        {
            // We already have one ID saved, the m_currentLevelOneID, save the other one
            m_currentLevelTwoID = m_selectedElement->uniqueID;
            m_nextDbFile = QString(GDS_DIR) + "/level3_" + QString("%1").arg(m_currentLevelOneID) + "_" +
                    QString("%1").arg(m_currentLevelTwoID) + ".gds";
        }break;
    }
    if(!QFile(m_nextDbFile).exists())
    {
        // No file detected, new graph needed at this level
        qWarning() << m_nextDbFile << " not detected";
        QMessageBox::warning(this, "Zoom not available", "This block hasn't an additional zoom level");
        return;
    }

    // 2) The file exists, step ahead with the level
    switch(m_currentActiveLevel)
    {
        case LEVEL_ONE:
        {
            // Step ahead in the level
            m_currentActiveLevel = LEVEL_TWO;
        }break;
        case LEVEL_TWO:
        {
            // Step ahead in the level
            m_currentActiveLevel = LEVEL_THREE;
        }break;
    }

    // 3) Either if we were on level 1 or level 2 before, now we have to show the left pane, and clear the right pane too
    this->ui->containerWidget->show();
    txtEditorWidget->clear();
    codeEditorWidget->clearAllCodeHighlights();

    // 4) Load the data

    qWarning() << m_nextDbFile << " DETECTED, loading data..";

    // File detected, load its data and display it
    GLDiagramWidget->clearGraphData();
    freeCurrentGraphElements();
    tryToLoadLevelDb(m_currentActiveLevel, false);


    // 5) Update navigation buttons and label
    switch(m_currentActiveLevel)
    {
        // Warning: no level one since the "nextlevel" button can only take us to two or three..
        case LEVEL_TWO:
        {
           ui->goToPreviousLevel->setEnabled(true);
           ui->goToNextLevel->setEnabled(true);
           ui->currentLevelLabel->setText("Currently in level 2 - A user with some technical skills should be able to understand this");

        }break;

        case LEVEL_THREE:
        {
           ui->goToPreviousLevel->setEnabled(true);
           ui->goToNextLevel->setEnabled(false);
           ui->currentLevelLabel->setText("Currently in level 3 - Programmers should be able to understand this");

        }break;
    }
}
void MainWindowViewMode::on_goToPreviousLevel_clicked()
{
    // Can't zoom while the animation is ongoing
    if(m_animationOnGoing)
        return;

    // We can't go back if we're on level one
    if(m_currentActiveLevel == LEVEL_ONE)
    {
        QMessageBox::warning(this, "Zoom Error", "The minimum zoom level has already been reached");
        return;
    }

    //////////////////////////////////////
    //  Go to previous level
    //////////////////////////////////////

    // 1) NO NEED TO SAVE ANYTHING - View mode doesn't save

    // 2) Show the left pane if we have to, and clear the right/left pane too
    if(m_currentActiveLevel == LEVEL_TWO) // if we're returning to level one hide the left pane
        this->ui->containerWidget->hide();
    txtEditorWidget->clear();
    codeEditorWidget->clearAllCodeHighlights();

    // 3) Check if the new appropriate file exists, otherwise CRITICAL ERROR - BROKEN DOCUMENTATION - try to recreate another one
    QString m_previousDbFile;
    switch(m_currentActiveLevel)
    {
        case LEVEL_THREE:
        {

            m_previousDbFile = QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";
            m_currentActiveLevel = LEVEL_TWO;

        }break;
        case LEVEL_TWO:
        {
            m_previousDbFile = QString(GDS_DIR) + "/level1_general.gds";
            m_currentActiveLevel = LEVEL_ONE;
        }break;
    }

    // 4) If the file doesn't exist: new graph, otherwise: load the data
    if(!QFile(m_previousDbFile).exists())
    {
        // No file detected, new graph needed at this level
        qWarning() << m_previousDbFile << "BROKEN DOCUMENTATION - FILE not detected";

        // Clear all graph data and free memory
        GLDiagramWidget->clearGraphData();
        freeCurrentGraphElements();

        QMessageBox::warning(this, "Error loading documentation", "Cannot find the previous level documentation file, documentation might be broken");
        return;
    }
    else
    {
        qWarning() << m_previousDbFile << "previous file DETECTED, loading data..";

        // File detected, load its data and display it
        GLDiagramWidget->clearGraphData();
        freeCurrentGraphElements();
        tryToLoadLevelDb(m_currentActiveLevel, true);
    }

    // 6) Update navigation buttons and label
    switch(m_currentActiveLevel)
    {
        // Warning: no level three since the "previouslevel" button can only take us to two or one..
        case LEVEL_TWO:
        {
           ui->goToPreviousLevel->setEnabled(true);
           ui->goToNextLevel->setEnabled(true);
           ui->currentLevelLabel->setText("Currently in level 2 - A user with some technical skills should be able to understand this");

        }break;

        case LEVEL_ONE:
        {
           ui->goToPreviousLevel->setEnabled(false);
           ui->goToNextLevel->setEnabled(true);
           ui->currentLevelLabel->setText("Currently in level 1 - Everyone should be able to understand this");

        }break;
    }

}


//// This method is called by the openGL widget when it's ready to draw and if there's a scheduled painting pending
void MainWindowViewMode::deferredPaintNow()
{
    // Sets the swapping value to prevent screen flickering (it disables repaint events)
    bool oldValue = GLDiagramWidget->m_swapInProgress;
    GLDiagramWidget->m_swapInProgress = true;

    GLDiagramWidget->clearGraphData();
    updateGLGraph();
    // Data insertion ended, calculate elements displacement and start drawing data
    GLDiagramWidget->calculateDisplacement();

    // Restore the swapping value to its previous
    GLDiagramWidget->m_swapInProgress = oldValue;
    GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);

    // We selected an element for the first time (the graph has been loaded), we need to recharge this item's data
    // Load the selected element data in the panes
    loadSelectedElementDataInPanes();
}


// Next step button has been clicked, follow the user index (doesn't matter if it's inconsistent or "weird", just follow it)
// with selections

struct elementIndexAndUserIndex
{
    quint32 currentGraphElementsIndex;
    quint32 uniqueID;
    quint32 userIndex;
};

bool userIndexLessThan(const elementIndexAndUserIndex &s1, const elementIndexAndUserIndex &s2)
{
    // If user indices are different, no problem
    if(s1.userIndex != s2.userIndex)
        return s1.userIndex < s2.userIndex;
    else
        // Otherwise order by uniqueID (and that's unique)
        return s1.uniqueID < s2.uniqueID;
}

void MainWindowViewMode::on_nextStepBtn_clicked()
{
    // Can't change element while the animation is ongoing
    if(m_animationOnGoing)
        return;

    // Select next element on the graph (if there's any)
    if(m_currentGraphElements.size() == 0 || m_selectedElement == NULL)
        return;

    // Search for the next element based on the userID
    QVector<elementIndexAndUserIndex> elementAndUserIndex;
    elementIndexAndUserIndex m_temp;
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        m_temp.currentGraphElementsIndex = i;
        m_temp.uniqueID = m_currentGraphElements[i]->uniqueID;
        m_temp.userIndex = m_currentGraphElements[i]->userIndex;
        elementAndUserIndex.append(m_temp);
    }
    // Now order the vector by userIndices
    qSort(elementAndUserIndex.begin(), elementAndUserIndex.end(), userIndexLessThan);

    // Choose a next element (not equal to the current one)
    for(int i=0; i<elementAndUserIndex.size(); i++)
    {
        // The next userIndex should be greater, the uniqueID different from the selected one
        // and we should not have visited it earlier
        if(m_selectedElement->userIndex <= elementAndUserIndex[i].userIndex
                && m_selectedElement->uniqueID != elementAndUserIndex[i].uniqueID
                && !m_currentGraphElementsAlreadyVisited.contains(elementAndUserIndex[i].currentGraphElementsIndex))
        {
            // We have a winner
            changeSelectedElement(elementAndUserIndex[i].currentGraphElementsIndex);
            break;
        }
    }
}
// Changes the selected element -> send the command to the openGL graph
void MainWindowViewMode::changeSelectedElement(quint32 newSelectedElementIndex)
{
    m_graphWasClicked = false;
    GLDiagramWidget->changeSelectedElement(m_currentGraphElements[newSelectedElementIndex]->glPointer);
}








////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////                      SYSTEM SERVICE ROUTINES - LOAD/GENERIC FUNCTIONS                   //
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////


QString MainWindowViewMode::convertToRelativePath(QString fileAbsolutePath)
{
    // Get current app absolute path
    QString myAbsolutePath = QApplication::applicationFilePath();

    // Convert into relative file path from this directory
    QStringList myPaths = myAbsolutePath.split(QRegExp("/"), QString::SkipEmptyParts);
    QStringList filePaths = fileAbsolutePath.split(QRegExp("/"), QString::SkipEmptyParts);

    // Delete the application name
    myPaths.removeLast();

    // if the file is on another drive, just take the entire path
    if(filePaths[0] != myPaths[0])
    {
        // Different drive
        return fileAbsolutePath;
    }
    else
    {
        // For each common path, delete this item
        do
        {
            if(myPaths.size() < 1 || filePaths.size() < 1)
                break;
            if(myPaths[0] == filePaths[0])
            {
                myPaths.removeFirst();
                filePaths.removeFirst();
            }
            else
                break;
        }while(1);
        // Substitute every not-common part of myPaths with ..
        if(myPaths.size() > 0)
        {
            for(int i=0; i<myPaths.size(); i++)
                myPaths.replace(i, "..");
        }
        // Add every non-common part of filePaths and we're done
        for(int i=0; i<filePaths.size(); i++)
            myPaths.append(filePaths[i]);

        // Restore the "/" separators
        return myPaths.join("/");
    }
}

QString MainWindowViewMode::convertToAbsolutePath(QString relativePath)
{
    // Split the file relative path
    QStringList filePaths = relativePath.split(QRegExp("/"), QString::SkipEmptyParts);

    // Get current app absolute path and split it
    QString myAbsolutePath = QApplication::applicationFilePath();
    QStringList myPaths = myAbsolutePath.split(QRegExp("/"), QString::SkipEmptyParts);

    // Delete the application name
    myPaths.removeLast();

    do
    {
        // Sanity check
        if(myPaths.size() < filePaths.count(".."))
            return "";

        if(filePaths[0] == "..")
        {
            myPaths.removeLast();
            filePaths.removeFirst();
        }
        else
            break;
    }while(1);

    // Append the rest and rejoin
    for(int i=0; i<filePaths.size(); i++)
        myPaths.append(filePaths[i]);

    // Restore the "/" separators
    return myPaths.join("/");
}


// This function takes care of converting and marshalling all memory pointers of
// QVector<dbDataStructure*> m_currentGraphElements into QVector<quint32> nextItemsIndices indices
// to store on disk
void MainWindowViewMode::convertDbDataToStorableData(bool m_towardsDiskFile)
{
    if(m_towardsDiskFile)
    {
        for(int i=0; i<m_currentGraphElements.size(); i++)
        {
            // Convert all children
            m_currentGraphElements[i]->nextItemsIndices.clear();
            for(int j=0; j<m_currentGraphElements[i]->nextItems.size(); j++)
            {
                m_currentGraphElements[i]->nextItemsIndices.append(
                            m_currentGraphElements.indexOf(m_currentGraphElements[i]->nextItems[j]) );
            }
            // Convert the father
            if(m_currentGraphElements[i]->father == NULL)
            {
                // Set this element as root - no father
                m_currentGraphElements[i]->fatherIndex = 0; // Just to put a placeholder value
                m_currentGraphElements[i]->noFatherRoot = true;
            }
            else
            {
                m_currentGraphElements[i]->noFatherRoot = false;
                m_currentGraphElements[i]->fatherIndex = m_currentGraphElements.indexOf(m_currentGraphElements[i]->father);
            }
        }
    }
    else
    {
        for(int i=0; i<m_currentGraphElements.size(); i++)
        {
            // De-convert all children
            m_currentGraphElements[i]->nextItems.clear();
            for(int j=0; j<m_currentGraphElements[i]->nextItemsIndices.size(); j++)
            {
                m_currentGraphElements[i]->nextItems.append(
                            m_currentGraphElements[m_currentGraphElements[i]->nextItemsIndices[j]] );
            }
            // Convert the father (root hasn't one)
            if(m_currentGraphElements[i]->fatherIndex == 0 && m_currentGraphElements[i]->noFatherRoot == true)
                m_currentGraphElements[i]->father = NULL;
            else
                m_currentGraphElements[i]->father = m_currentGraphElements[m_currentGraphElements[i]->fatherIndex];
        }
    }

    // Data is ready to be stored (NOT TO BE DRAWN) or drawn
}

// Restore all panes to their default values
void MainWindowViewMode::clearAllPanes()
{
    codeEditorWidget->clearAllCodeHighlights();
    txtEditorWidget->clear();
}

// Load everything from the selected element on the panes
void MainWindowViewMode::loadSelectedElementDataInPanes()
{
    qWarning() << "Loading selected element to panes ->";

    //
    // Load the right pane with the new values for the new selected element
    //
    txtEditorWidget->setHtml(QString(m_selectedElement->data));

    //
    // Load the file label
    //
    ui->fileLabel->setText(m_selectedElement->fileName);

    //
    // Load the code file and add it to the combobox IF WE'RE ON LEVEL 2/3, notice that there might not be a file associated
    //
    if(m_currentActiveLevel == LEVEL_ONE || m_selectedElement == NULL)
        return;
    // Maybe the filename is empty
    if(m_selectedElement->fileName.isEmpty())
    {
        // if so, clear the code pane
        codeEditorWidget->document()->setPlainText("");
        codeEditorWidget->m_selectedLines.clear();
        return;
    }
    QString convertedAbsoluteFileName = convertToAbsolutePath(m_selectedElement->fileName);
    if(!QFile::exists(convertedAbsoluteFileName)) // This is a relative path, convert to absolute path
    {
        QMessageBox::warning(this, "Error loading associated code file", "The code file associated with this element hasn't been found, the documentation might be corrupted");
        return;
    }
    QFile file(convertedAbsoluteFileName);
    if (!file.open(QFile::ReadOnly))
        return;
    // Read the entire file and display it into the code window
    QByteArray data = file.readAll();
    codeEditorWidget->setPlainText(QString(data));
    file.close();

    // Highlight the lines in the file we're associated to (if we have any)
    if(m_selectedElement->linesNumbers.size() == 0)
        return;
    // Check for corruption (source file changed)
    QStringList allLines = QString(data).split(QRegExp("\r\n"),QString::KeepEmptyParts);
    if((quint32)allLines.size() < m_selectedElement->linesNumbers[0])
    {
        // Corrupted
        QMessageBox::warning(this, "Error loading associated code file", "The code lines associated with this block have wrong line number values, the documentation might be corrupted");
        return;
    }
    // Find textually the first line
    if(allLines[m_selectedElement->linesNumbers[0]] != QString(m_selectedElement->firstLineData))
    {
        // Search near it for matchings
        int i_offset = 0;
        // First forward
        for(int i=1;((quint32)allLines.size() > m_selectedElement->linesNumbers[0]+i); i++)
        {
            if(allLines[m_selectedElement->linesNumbers[0]+i] == QString(m_selectedElement->firstLineData))
            {
                // Found, update with +i offset
                i_offset = i;
                break;
            }
        }
        // Then backward
        for(int i=1;(m_selectedElement->linesNumbers[0]-1 >= 0); i++)
        {
            if(allLines[m_selectedElement->linesNumbers[0]-i] == QString(m_selectedElement->firstLineData))
            {
                // Found, update with -i offset
                i_offset = -i;
                break;
            }
        }
        // Update if the offset was found
        if(i_offset == 0)
        {
            // Corrupted
            QMessageBox::warning(this, "Error loading associated code file", "The code lines associated with this block cannot be found, the documentation might be corrupted");
            return;
        }
        // Next items don't need to be updated, they're relative to the first
        m_selectedElement->linesNumbers[0] = m_selectedElement->linesNumbers[0] + i_offset;
    }

    // If we get here, we've found the line OR corrected the vector, draw the lines highlighted now
    codeEditorWidget->highlightLines(m_selectedElement->linesNumbers);
}

void MainWindowViewMode::freeCurrentGraphElements()
{
    // Free memory and clear elements' buffer
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        delete m_currentGraphElements[i];
    }
    m_currentGraphElements.clear();

    m_selectedElement = NULL;
}

// Update the openGL graph widget (the graph has changed)
void MainWindowViewMode::updateGLGraph()
{
    void *temp;
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        if(m_currentGraphElements[i]->father == NULL)
        {
            // Root
            temp = GLDiagramWidget->insertTreeData(m_currentGraphElements[i]->label, NULL);
            m_currentGraphElements[i]->glPointer = temp;
        }
        else
        {
            temp = GLDiagramWidget->insertTreeData(m_currentGraphElements[i]->label, m_currentGraphElements[i]->father->glPointer);
            m_currentGraphElements[i]->glPointer = temp;
        }
    }
}

// Try to load a level database or fail if there isn't any
void MainWindowViewMode::tryToLoadLevelDb(level lvl, bool returnToElement)
{
    // Check if the db directory exists in the current directory
    if(!QDir(GDS_DIR).exists())
    {
        // There's nothing, not even the directory.. view mode stops here
        QMessageBox::warning(this, "Error loading documentation", "The documentation directory cannot be found");
        exit(1);
        return;
    }
    else
    {
        switch(lvl)
        {
            case LEVEL_ONE:
            {
                // "level1_general.gds" file

                // If the file doesn't exist, exit
                if(!QFile(QString(GDS_DIR) + "/level1_general.gds").exists())
                {
                    QMessageBox::warning(this, "Error loading documentation", "The level one documentation file cannot be found");
                    exit(1);
                    return;
                }

                freeCurrentGraphElements();

                // De-Serialize our current data
                QFile file(QString(GDS_DIR) + "/level1_general.gds");
                file.open(QFile::ReadOnly);

                // Thanks to our << and >> overloads, this will serialize just what we need
                QDataStream in(&file);

                // Read the number of elements stored
                int m_numElements;
                in >> m_numElements;

                dbDataStructure *m_tempPointer;
                for(int i=0; i<m_numElements; i++)
                {
                    // Read one structure and allocate it into memory
                    m_tempPointer = new dbDataStructure();
                    in >> *m_tempPointer;
                    m_currentGraphElements.append(m_tempPointer);
                }

                file.close();

                // Data is unusable yet, we need to re-convert each index-pointer to a proper memory pointer first
                convertDbDataToStorableData(false);

                // Draw loaded data and set root selected
                m_selectedElement = m_currentGraphElements[0];

                // We can't draw yet if the shaders haven't been compiled so check for them first and set a callback if they're
                // not ready
                if(!GLDiagramWidget->m_readyToDraw)
                {
                    // Signal that data is ready to be painted and exit
                    GLDiagramWidget->m_associatedWindowRepaintScheduled = true;
                }
                else
                {
                    // Widget is ready to draw, probably we have been taken here by the "previous level" button
                    deferredPaintNow();
                    // If zooming back, restore previous element
                    if(returnToElement)
                    {
                        // Retrieve the uniqueID
                        m_selectedElement = NULL;
                        for(int i=0; i<m_currentGraphElements.size(); i++)
                        {
                            if(m_currentLevelOneID == m_currentGraphElements[i]->uniqueID)
                            {
                                m_selectedElement = m_currentGraphElements[i];
                                break;
                            }
                        }
                        GLDiagramWidget->firstTimeDrawing = false;
                        GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);
                        loadSelectedElementDataInPanes();
                    }
                }
            }break;

        case LEVEL_TWO:
        {
            // "level2_X1.gds" file
            QString m_dbFile = QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";

            // If the file doesn't exist or has been deleted, first time mode
            if(!QFile(m_dbFile).exists())
            {
                QMessageBox::warning(this, "Error loading documentation", "The level two requested documentation file cannot be found");
                return;
            }

            freeCurrentGraphElements();

            // De-Serialize our current data
            QFile file(m_dbFile);
            file.open(QFile::ReadOnly);

            // Thanks to our << and >> overloads, this will serialize just what we need
            QDataStream in(&file);

            // Read the number of elements stored
            int m_numElements;
            in >> m_numElements;

            dbDataStructure *m_tempPointer;
            for(int i=0; i<m_numElements; i++)
            {
                // Read one structure and allocate it into memory
                m_tempPointer = new dbDataStructure();
                in >> *m_tempPointer;
                m_currentGraphElements.append(m_tempPointer);
            }

            file.close();

            // Data is unusable yet, we need to re-convert each index-pointer to a proper memory pointer first
            convertDbDataToStorableData(false);

            // Draw loaded data and set root selected
            m_selectedElement = m_currentGraphElements[0];

            // This editor always starts in level one, so openGL widget is already loaded, redraw data!
            deferredPaintNow();
            // If zooming back, restore previous element
            if(returnToElement)
            {
                // Retrieve the uniqueID
                m_selectedElement = NULL;
                for(int i=0; i<m_currentGraphElements.size(); i++)
                {
                    if(m_currentLevelTwoID == m_currentGraphElements[i]->uniqueID)
                    {
                        m_selectedElement = m_currentGraphElements[i];
                        break;
                    }
                }
                GLDiagramWidget->firstTimeDrawing = false;
                GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);
                loadSelectedElementDataInPanes();
            }
        }break;

        case LEVEL_THREE:
        {
            // "level3_X1_X2.gds" file
            QString m_dbFile = QString(GDS_DIR) + "/level3_" + QString("%1").arg(m_currentLevelOneID) + "_" +
                    QString("%1").arg(m_currentLevelTwoID) + ".gds";

            // If the file doesn't exist or has been deleted, first time mode
            if(!QFile(m_dbFile).exists())
            {
                QMessageBox::warning(this, "Error loading documentation", "The level three requested documentation file cannot be found");
                return;
            }

            freeCurrentGraphElements();

            // De-Serialize our current data
            QFile file(m_dbFile);
            file.open(QFile::ReadOnly);

            // Thanks to our << and >> overloads, this will serialize just what we need
            QDataStream in(&file);

            // Read the number of elements stored
            int m_numElements;
            in >> m_numElements;

            dbDataStructure *m_tempPointer;
            for(int i=0; i<m_numElements; i++)
            {
                // Read one structure and allocate it into memory
                m_tempPointer = new dbDataStructure();
                in >> *m_tempPointer;
                m_currentGraphElements.append(m_tempPointer);
            }

            file.close();

            // Data is unusable yet, we need to re-convert each index-pointer to a proper memory pointer first
            convertDbDataToStorableData(false);

            // Draw loaded data and set root selected
            m_selectedElement = m_currentGraphElements[0];

            // This editor always starts in level one, so openGL widget is already loaded, redraw data!
            deferredPaintNow();
        }break;

        }

    }
}


