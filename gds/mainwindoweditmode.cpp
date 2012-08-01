#include "mainwindoweditmode.h"
#include "ui_mainwindoweditmode.h"

MainWindowEditMode::MainWindowEditMode(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindowEditMode)
{
    ui->setupUi(this);

    m_swapRunning = false;
    m_lastSelectedHasBeenDeleted = false;
    m_currentGraphElements.clear();
    m_selectedElement = NULL;
    ui->spinBox->setEnabled(true);
    // Still no element selected, and we're in level one
    m_currentLevelOneID = -1;
    m_currentLevelTwoID = -1;

    // Add the textEditor widget to the right part
    txtEditorWidget = new textEditorWin();
    ui->rightArea->addWidget(txtEditorWidget);

    // Create the code editor in the left pane and the line counter
    codeEditorWidget = new CodeEditorWidget(*(ui->lineCounter));
    // Create the code window on the left pane
    txtHighlighter = new CppHighlighter(codeEditorWidget->document());
    // Insert it into the window
    ui->codeLayout->addWidget(codeEditorWidget);
    codeEditorWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    codeEditorWidget->setMinimumWidth(200);

    // Set the connection for the label box
    connect(ui->txtLabel, SIGNAL(returnPressed()), this, SLOT(enterPressedOnLabelEditBox()));

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
    GLDiagramWidget->m_gdsEditMode = true; // No animation while selecting
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
MainWindowEditMode::~MainWindowEditMode()
{
    delete ui;
    delete txtEditorWidget;
    delete GLDiagramWidget;
    delete txtHighlighter;
    delete codeEditorWidget;
}

// This method is called by the openGL widget every time the selection is changed on the graph
void *MainWindowEditMode::GLWidgetNotifySelectionChanged(void *m_newSelection)
{
    // First save the right/left pane data for the old selected element
    if(!m_lastSelectedHasBeenDeleted)
    {
        qWarning() << "GLWidgetNotifySelectionChanged.saveEverythingOnThePanesToMemory()";
        saveEverythingOnThePanesToMemory();
    }

    if(m_swapRunning)
    {
        // There's a swap running, swap the selected element with the new selected element
        GLDiagramWidget->m_swapInProgress = true;

        dbDataStructure *m_newSelectedElement;
        // Select our new element
        long m_newSelectionIndex = -1;
        long m_selectedElementIndex = -1;
        int foundBoth = 0;
        for(int i=0; i<m_currentGraphElements.size(); i++)
        {
            if(foundBoth == 2)
                break;

            if(m_currentGraphElements[i]->glPointer == m_newSelection)
            {
                m_newSelectedElement = m_currentGraphElements[i];
                m_newSelectionIndex = i;
                foundBoth++;
            }
            if(m_currentGraphElements[i]->glPointer == m_selectedElement->glPointer)
            {
                m_selectedElementIndex = i;
                foundBoth++;
            }
        }

        // Swap these two structure's data
        QByteArray m_temp = m_newSelectedElement->data;
        m_newSelectedElement->data = m_selectedElement->data;
        m_selectedElement->data = m_temp;

        QString m_temp2 = m_newSelectedElement->fileName;
        m_newSelectedElement->fileName = m_selectedElement->fileName;
        m_selectedElement->fileName = m_temp2;

        m_temp2 = m_newSelectedElement->label;
        m_newSelectedElement->label = m_selectedElement->label;
        m_selectedElement->label = m_temp2;

        long m_temp3 = m_newSelectedElement->userIndex;
        m_newSelectedElement->userIndex = m_selectedElement->userIndex;
        m_selectedElement->userIndex = m_temp3;

        m_temp = m_newSelectedElement->firstLineData;
        m_newSelectedElement->firstLineData = m_selectedElement->firstLineData;
        m_selectedElement->firstLineData = m_temp;

        QVector<quint32> m_temp4 = m_newSelectedElement->linesNumbers;
        m_newSelectedElement->linesNumbers = m_selectedElement->linesNumbers;
        m_selectedElement->linesNumbers = m_temp4;


        // Avoid a recursive this-method recalling when selected element changes: set swapInProgress and avoid repainting

        GLDiagramWidget->clearGraphData();
        updateGLGraph();
        // Data insertion ended, calculate elements displacement and start drawing data
        GLDiagramWidget->calculateDisplacement();

        GLDiagramWidget->m_swapInProgress = false;
    }
    else
    {
        // Select our new element
        for(int i=0; i<m_currentGraphElements.size(); i++)
        {
            if(m_currentGraphElements[i]->glPointer == m_newSelection)
            {
                m_selectedElement = m_currentGraphElements[i];
                break;
            }
        }
    }

    qWarning() << "New element selected: " + m_selectedElement->label;

    // Load the selected element data in the panes, but first clear them
    qWarning() << "GLWidgetNotifySelectionChanged.clearAllPanes() and loadSelectedElementDataInPanes()";
    clearAllPanes();
    loadSelectedElementDataInPanes();

    if(m_swapRunning)
    {
        m_swapRunning = false;
        ui->swapBtn->toggle();

        // Return to the painting widget the new element to be selected (data has been modified)
        return m_selectedElement->glPointer;
    }
    else
        return NULL;
}




void MainWindowEditMode::on_goToNextLevel_clicked()
{
    // If there's no root, just deny it
    if(m_firstTimeGraphInCurrentLevel || m_selectedElement == NULL)
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

    // 1) Save all panes and data to disk
    saveEverythingOnThePanesToMemory();
    saveCurrentLevelDb();

    // 2) Save our current selected element's level and unique index
    switch(m_currentActiveLevel)
    {
        case LEVEL_ONE:
        {
            // We just have to save this ID
            m_currentLevelOneID = m_selectedElement->uniqueID;

        }break;
        case LEVEL_TWO:
        {
            // We already have one ID saved, the m_currentLevelOneID, save the other one
            m_currentLevelTwoID = m_selectedElement->uniqueID;

        }break;
    }

    // 3) Either if we were on level 1 or level 2 before, now we have to show the left pane, and clear the right pane too
    this->ui->containerWidget->show();
    txtEditorWidget->m_textEditorWin->clear();
    codeEditorWidget->clearAllCodeHighlights();
    this->ui->spinBox->setValue(0);
    this->ui->txtLabel->setText("Block");

    // 4) Check if the new appropriate file exists
    QString m_nextDbFile;
    switch(m_currentActiveLevel)
    {
        case LEVEL_ONE:
        {

            m_nextDbFile = QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";
            m_currentActiveLevel = LEVEL_TWO;

        }break;
        case LEVEL_TWO:
        {
            m_nextDbFile = QString(GDS_DIR) + "/level3_" + QString("%1").arg(m_currentLevelOneID) + "_" +
                    QString("%1").arg(m_currentLevelTwoID) + ".gds";
            m_currentActiveLevel = LEVEL_THREE;

        }break;
    }

    // 5) If the file doesn't exist: new graph, otherwise: load the data
    if(!QFile(m_nextDbFile).exists())
    {
        // No file detected, new graph needed at this level
        qWarning() << m_nextDbFile << " not detected, creating a new graph..";

        // Clear all graph data and free memory
        GLDiagramWidget->clearGraphData();
        freeCurrentGraphElements();
        // Also de-load the code file we've been using
        codeEditorWidget->clear();
        m_firstTimeGraphInCurrentLevel = true;
    }
    else
    {
        qWarning() << m_nextDbFile << " DETECTED, loading data..";

        // File detected, load its data and display it
        GLDiagramWidget->clearGraphData();
        freeCurrentGraphElements();
        tryToLoadLevelDb(m_currentActiveLevel, false);
    }

    // 6) Update navigation buttons and label
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
void MainWindowEditMode::on_goToPreviousLevel_clicked()
{
    // We can't go back if we're on level one
    if(m_currentActiveLevel == LEVEL_ONE)
    {
        QMessageBox::warning(this, "Zoom Error", "The minimum zoom level has already been reached");
        return;
    }

    //////////////////////////////////////
    //  Go to previous level
    //////////////////////////////////////

    // 1) Save all panes and data to disk if this graph isn't empty
    saveEverythingOnThePanesToMemory();
    saveCurrentLevelDb();

    // 2) Show the left pane if we have to, and clear the right/left pane too
    if(m_currentActiveLevel == LEVEL_TWO) // if we're returning to level one hide the left pane
        this->ui->containerWidget->hide();
    txtEditorWidget->m_textEditorWin->clear();
    codeEditorWidget->clearAllCodeHighlights();
    this->ui->spinBox->setValue(0);
    this->ui->txtLabel->setText("Block");

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
        qWarning() << m_previousDbFile << "BROKEN DOCUMENTATION - FILE not detected, creating a new graph..";

        // Clear all graph data and free memory
        GLDiagramWidget->clearGraphData();
        freeCurrentGraphElements();
        m_firstTimeGraphInCurrentLevel = true;
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


// This method is called by the openGL widget when it's ready to draw and if there's a scheduled painting pending
void MainWindowEditMode::deferredPaintNow()
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


void MainWindowEditMode::enterPressedOnLabelEditBox()
{
    // Update this node's label and redraw it
    if(m_selectedElement == NULL || m_currentGraphElements.size() == 0)
        return;

    m_selectedElement->label = ui->txtLabel->text();

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
}

// This happens when the spinbox loses focus or enter is pressed
void MainWindowEditMode::on_spinBox_editingFinished()
{
    // Just update the selected element's data
    if(m_selectedElement == NULL || m_currentGraphElements.size() == 0 || QString(ui->spinBox->value()) == "")
        return;

    m_selectedElement->userIndex = ui->spinBox->value();
}


// Update the openGL graph widget (the graph has changed)
void MainWindowEditMode::updateGLGraph()
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


// Delete current selected element and ask for total elimination (all children too) or partial elimination
void MainWindowEditMode::on_deleteSelectedElementBtn_clicked()
{
    // If there are no elements, exit
    if(m_currentGraphElements.size() == 0)
        return;

    // Does the user want to delete the root?
    if(m_selectedElement->father == NULL)
    {
        // Damn, he wants to delete the root... we can't allow that without destroying the entire graph
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Deletion confirmation"),
                                      "Deleting the root element will cause the entire graph to be lost, do you want to continue?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return;
        qWarning() << "ROOT DESTROYING AND EVERYTHING RELATED";
        // Destroy EVERYTHING
        for(int i=0; i<m_currentGraphElements.size(); i++)
        {
            delete m_currentGraphElements[i];
        }
        m_currentGraphElements.clear();

        m_firstTimeGraphInCurrentLevel = true;
        m_selectedElement = NULL;
    }
    else
    {

        // Check if the selected element has children
        if(m_selectedElement->nextItems.size() == 0)
        {
            // No children, total elimination
            qWarning() << "no children, total elimination";
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, tr("Deletion confirmation"),
                                          "Do you really want to delete this element?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return;

            // Save its father (we'll select this after the deletion)
            dbDataStructure *m_father = m_selectedElement->father;
            //qWarning() << "father has data: " << QString(m_father->data);

            // Delete the node from the global vector and from the father's children (if not NULL, maybe this selected is the root)
            int index = -1;
            for(int i=0; i<m_currentGraphElements.size(); i++)
            {
                if(m_currentGraphElements[i] == m_selectedElement)
                {
                    index = i;
                    break;
                }
            }
            m_currentGraphElements.remove(index);

            if(m_father != NULL)
            {
                for(int i=0; i<m_father->nextItems.size(); i++)
                {
                    if(m_father->nextItems[i] == m_selectedElement)
                    {
                        index = i;
                        break;
                    }
                }
                m_father->nextItems.remove(index);
            }
            delete m_selectedElement; // Free memory

            // This prevents messing with the data of the precedent selection
            m_lastSelectedHasBeenDeleted = true;

            // Select the father
            m_selectedElement = m_father;
        }
        else
        {
            // It has children, ask for total or partial elimination

            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, tr("Deletion confirmation"),
                                          "Do you want to delete this element's children as well?\r\nChoose Cancel to abort the deletion",
                                          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

            if (reply == QMessageBox::Cancel)
                return;

            if (reply == QMessageBox::Yes)
            {
                // Total deletion (children included)
                qWarning() << "Deletion WITH children";
                // Save its father (we'll select this after the deletion)
                dbDataStructure *m_father = m_selectedElement->father;
                //qWarning() << "father has data: " << QString(m_father->data);

                // Delete this child from its father's children
                int index = -1;
                for(int i=0; i<m_father->nextItems.size(); i++)
                {
                    if(m_father->nextItems[i] == m_selectedElement)
                    {
                        index = i;
                        break;
                    }
                }
                m_father->nextItems.remove(index);

                // Delete the node and all its children
                recursiveDelete(m_selectedElement);

                // This prevents messing with the data of the precedent selection
                m_lastSelectedHasBeenDeleted = true;

                // Select the father
                m_selectedElement = m_father;
            }
            else
            {
                // Delete the element but preserve the children

                // Save its father (we'll select this after the deletion)
                dbDataStructure *m_father = m_selectedElement->father;
                qWarning() << "Deletion WITHOUT children";
                //qWarning() << "father has data: " << QString(m_father->data);
                // Delete this child from its father's children
                int index = -1;
                for(int i=0; i<m_father->nextItems.size(); i++)
                {
                    if(m_father->nextItems[i] == m_selectedElement)
                    {
                        index = i;
                        break;
                    }
                }
                m_father->nextItems.remove(index);

                // Set the new children's father
                for(int i=0; i<m_selectedElement->nextItems.size(); i++)
                {
                    m_selectedElement->nextItems[i]->father = m_father;
                }
                // Set the new father's children
                for(int i=0; i<m_selectedElement->nextItems.size(); i++)
                {
                    m_father->nextItems.append(m_selectedElement->nextItems[i]);
                }

                for(int i=0; i<m_currentGraphElements.size(); i++)
                {
                    if(m_currentGraphElements[i] == m_selectedElement)
                    {
                        index = i;
                        break;
                    }
                }
                m_currentGraphElements.remove(index);
                delete m_selectedElement; // Free memory

                // This prevents messing with the data of the precedent selection
                m_lastSelectedHasBeenDeleted = true;

                // Select the father
                m_selectedElement = m_father;
            }

        }
    }


    // Redraw
    GLDiagramWidget->clearGraphData();
    updateGLGraph();

    // Nothing to calculate if there are no elements
    if(m_currentGraphElements.size() > 0)
    {
        // Data insertion ended, calculate elements displacement and start drawing data
        GLDiagramWidget->calculateDisplacement();
        // Select our selected element (if not NULL)
        if(m_selectedElement != NULL)
        {
            GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);

            // Load its data
            loadSelectedElementDataInPanes();
        }
    }
}
void MainWindowEditMode::recursiveDelete(dbDataStructure* element)
{
    for(int i=0; i<element->nextItems.size(); i++)
    {
        recursiveDelete(element->nextItems[i]);
    }

    int index = -1;
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        if(m_currentGraphElements[i] == element)
        {
            index = i;
            break;
        }
    }
    m_currentGraphElements.remove(index);
    delete element; // Free memory
}

void MainWindowEditMode::on_addChildBlockBtn_clicked()
{
    // If first time, create the root element and store it into the db
    if(m_firstTimeGraphInCurrentLevel)
    {
        // Create a root for the current graph

        // Level one and no graph, create the root
        dbDataStructure *rootElement = new dbDataStructure();
        rootElement->depth = 0;
        rootElement->userIndex = ui->spinBox->text().toLong();

        if(ui->txtLabel->text().isEmpty())
        {
            rootElement->label = "Block";
            ui->txtLabel->setText("Block");
        }
        else
            rootElement->label = ui->txtLabel->text();

        rootElement->father = NULL; // root has no father

        rootElement->uniqueID = getThisGraphNextFreeID();

        // If there's data on the right pane, store it with us
        if(!txtEditorWidget->m_textEditorWin->document()->isEmpty())
        {
            rootElement->data.clear();
            rootElement->data.append(txtEditorWidget->m_textEditorWin->toHtml());
        }
        // If there's code on the left pane, store it with us
        if(!codeEditorWidget->document()->isEmpty())
        {
            rootElement->fileName.clear();
            rootElement->fileName.append(ui->fileComboBox->currentText());
        }

        // -----------------------------------------------------

        // Add it to the element list
        m_currentGraphElements.append(rootElement);

        // Select this
        m_selectedElement = rootElement;

        GLDiagramWidget->clearGraphData();
        updateGLGraph();
        // Data insertion ended, calculate elements displacement and start drawing data
        GLDiagramWidget->calculateDisplacement();
        // Select our selected element
        GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);

        m_firstTimeGraphInCurrentLevel = false; // We added an element

        saveEverythingOnThePanesToMemory(); // This saves the code lines highlighted
    }
    else
    {
        // Create another element every level

        // Level one, create another element child of the one selected
        dbDataStructure *newElement = new dbDataStructure();
        newElement->depth = m_selectedElement->depth + 1;
        // Increment the spinbox value
        ui->spinBox->setValue(ui->spinBox->value()+1);
        newElement->userIndex = ui->spinBox->text().toLong();

        // Set new element's father and add it to the selected element's children
        newElement->father = m_selectedElement;
        m_selectedElement->nextItems.append(newElement);

        // Assign a new unique ID for this graph
        newElement->uniqueID = getThisGraphNextFreeID();

        // Save everything for the old element
        saveEverythingOnThePanesToMemory();

        // Set the label text to "Block" as default
        newElement->label = "Block";
        ui->txtLabel->setText("Block");
        // And clear the text of the new block
        newElement->data.clear();
        txtEditorWidget->m_textEditorWin->clear();

        // And then clear the highlights on the code file (the code file might remain the same, just to speed things up)
        if(m_currentActiveLevel != LEVEL_ONE)
        {
            codeEditorWidget->clearAllCodeHighlights();
            // Obviously if the code remains the same, we need to set it on our child :)
            newElement->fileName.clear();
            newElement->fileName.append(m_selectedElement->fileName);
        }


        // -----------------------------------------------------

        // Add it to the element list
        m_currentGraphElements.append(newElement);

        // Select this
        m_selectedElement = newElement;

        GLDiagramWidget->clearGraphData();
        updateGLGraph();
        // Data insertion ended, calculate elements displacement and start drawing data
        GLDiagramWidget->calculateDisplacement();
        // Select our selected element
        GLDiagramWidget->changeSelectedElement(m_selectedElement->glPointer);
    }
}

// Swap an element with another one, this handles just the button
void MainWindowEditMode::on_swapBtn_clicked()
{
    if(m_selectedElement == NULL)
    {
        // Nothing to swap
        if(ui->swapBtn->isChecked())
            ui->swapBtn->toggle();
        return;
    }

    // Signal that the selected element has been selected for swap
    m_swapRunning = true;
}

void MainWindowEditMode::closeEvent(QCloseEvent *)
{
    // Save the last selected element's data
    saveEverythingOnThePanesToMemory();
    // Finally save to disk
    saveCurrentLevelDb();
    exit(1);
}

void MainWindowEditMode::on_browseCodeFiles_clicked()
{
    // Search for a new code file and store just the relative position from this app's directory

    QString fn = QFileDialog::getOpenFileName(this, tr("Open C/C++ code file..."),
                                              QString(), tr("C/C++ Files (*.cpp *.c *.h)"));
    if (fn.isEmpty() || !QFile::exists(fn))
        // Nothing to load
        return;

    // If there's no element don't allow the file to be loaded
    if(m_currentGraphElements.count() == 0 || m_selectedElement == NULL)
    {
        QMessageBox::warning(this, "Error associating code file", "An element is needed before associating a code file");
        return;
    }

    QFile file(fn);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;

    // 1) Read the entire file and display it into the code window
    QByteArray data = file.readAll();

    if(m_selectedElement != NULL && m_currentGraphElements.size() > 0)
    {
        // First clear all the highlighted data
        codeEditorWidget->document()->setPlainText("");
        codeEditorWidget->m_selectedLines.clear();

        // This time we need to clear this element's data too
        m_selectedElement->linesNumbers.clear();
        m_selectedElement->firstLineData.clear();
    }

    codeEditorWidget->setPlainText(QString(data));

    // 2) Add its RELATIVE path to the combo box
    if(m_recentFilePaths.size() >= 15)
    {
        // Remove the less recent one
        m_recentFilePaths.remove(0);
    }

    QFileInfo info(file);
    QString finalRelativePath = convertToRelativePath(info.absoluteFilePath());
    // Also add the filename to the current element's
    m_selectedElement->fileName.clear();
    m_selectedElement->fileName.append(finalRelativePath);

    // and finally add this path to the combobox and the vector (if it's not already there)
    if(!m_recentFilePaths.contains(finalRelativePath))
    {
        m_recentFilePaths.append(finalRelativePath);
        // Update combobox
        ui->fileComboBox->clear();
        for(int i=0;i<m_recentFilePaths.size();i++)
            ui->fileComboBox->addItem(m_recentFilePaths[i]);

        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count()-1);
    }
    else
    {
        int index = m_recentFilePaths.indexOf(finalRelativePath);
        ui->fileComboBox->setCurrentIndex(index);
    }

    file.close();
}

// Combo box was pressed
void MainWindowEditMode::on_fileComboBox_activated(const QString &arg1)
{
    if(m_selectedElement == NULL || m_currentGraphElements.size() == 0)
        return;

    QString absoluteNewPath = convertToAbsolutePath(arg1);
    if (!QFile::exists(absoluteNewPath))
        // Nothing to load
        return;

    // First clear all the highlighted data if there was an open document on
    if(!codeEditorWidget->document()->isEmpty())
    {
        codeEditorWidget->document()->setPlainText("");
        codeEditorWidget->m_selectedLines.clear();
    }

    // This time we need to clear this element's data too
    m_selectedElement->linesNumbers.clear();
    m_selectedElement->firstLineData.clear();

    // Load the new code file
    QFile file(absoluteNewPath);

    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;

    // Read the entire file and display it into the code window
    QByteArray data = file.readAll();

    codeEditorWidget->setPlainText(QString(data));
    m_selectedElement->fileName.clear();
    m_selectedElement->fileName.append(arg1);
    qWarning() << "on_fileComboBox_activated() - filename set to: "+arg1;

    file.close();

    // Update the panes data, this will also update the filename of our selectedElement
    saveEverythingOnThePanesToMemory();
}


// Sometimes we don't want a code file associated, clear the codeview and save to ram
void MainWindowEditMode::on_clearCodeFileBtn_clicked()
{


    codeEditorWidget->document()->setPlainText("");
    codeEditorWidget->m_selectedLines.clear();

    m_selectedElement->fileName.clear();
    m_selectedElement->linesNumbers.clear();
    m_selectedElement->firstLineData.clear();


}










//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//                  SYSTEM SERVICE ROUTINES - SAVE/LOAD/GENERIC FUNCTIONS                   //
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////



quint64 MainWindowEditMode::getThisGraphNextFreeID()
{
    if(m_currentGraphElements.size() == 0) // Root
        return 0;

    // Get this graph's next free ID by searching for a uniqueID free value
    QVector<quint32> m_takenIDs;
    for(int i=0; i<m_currentGraphElements.size(); i++)
        m_takenIDs.append(m_currentGraphElements[i]->uniqueID);

    // Search for the next available ID
    quint64 m_newId = 0;
    for(;;)
    {
        if(!m_takenIDs.contains(m_newId))
        {
            // Found a new ID!
            break;
        }
        else
        {
            m_newId++;
        }
    }
    return m_newId;
}

QString MainWindowEditMode::convertToRelativePath(QString fileAbsolutePath)
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

QString MainWindowEditMode::convertToAbsolutePath(QString relativePath)
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


// Saves all consistent data serializing the tree
void MainWindowEditMode::saveCurrentLevelDb()
{
    qWarning() << "saveCurrentLevelDb -> saving memory to disk";

    // If the graph is new and there's no data, save nothing
    if(m_firstTimeGraphInCurrentLevel)
    {
        // Save "nothing" means that we need to check that a previous file (maybe because the root was deleted)
        // is no more present
        QString delFile;
        switch(m_currentActiveLevel)
        {
            case LEVEL_ONE:
            {
                // "level1_general.gds" file
                delFile = QString(GDS_DIR) + "/level1_general.gds";
            }break;
            case LEVEL_TWO:
            {
                // "level2_X1.gds" file
                delFile =  QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";
            }break;
            case LEVEL_THREE:
            {
                // "level3_X1_X2.gds" file
                delFile =  QString(GDS_DIR) + "/level3_" + QString("%1").arg(m_currentLevelOneID) + "_" +
                        QString("%1").arg(m_currentLevelTwoID) + ".gds";
            }break;
        };
        if(QFile::exists(delFile))
        {
            // Delete it
            if(!QFile::remove(delFile))
            {
                QMessageBox::warning(this, "Error deleting database file", "The file \r\n"+delFile+"\r\n is in use, thus cannot be deleted. Documentation might be corrupted.");
            }
        }


        return;
    }

    // This function takes care of converting all memory pointers into
    // QVector<dbDataStructure*> m_currentGraphElements indices
    convertDbDataToStorableData(true);

    // Write out the number of elements and all the data structures on the level-specific file
    switch(m_currentActiveLevel)
    {
        case LEVEL_ONE:
        {
            // "level1_general.gds" file

            // Serialize our current data
            QFile file(QString(GDS_DIR) + "/level1_general.gds");
            file.open(QFile::WriteOnly);

            // Thanks to our << and >> overloads, this will serialize just what we need
            QDataStream out(&file);

            // Write out the number of elements and all the data structures
            out << m_currentGraphElements.size();
            for(int i=0; i<m_currentGraphElements.size(); i++)
            {
                out << *(m_currentGraphElements[i]); // Remember that these are pointers, they need to be dereferenced
            }

            file.close();

        }break;

        case LEVEL_TWO:
        {
            // "level2_X1.gds" file
            QString m_dbFile = QString(GDS_DIR) + "/level2_" + QString("%1").arg(m_currentLevelOneID) + ".gds";

            // Serialize our current data
            QFile file(m_dbFile);
            file.open(QFile::WriteOnly);

            // Thanks to our << and >> overloads, this will serialize just what we need
            QDataStream out(&file);

            // Write out the number of elements and all the data structures
            out << m_currentGraphElements.size();
            for(int i=0; i<m_currentGraphElements.size(); i++)
            {
                out << *(m_currentGraphElements[i]); // Remember that these are pointers, they need to be dereferenced
            }

            file.close();

        }break;

        case LEVEL_THREE:
        {
            // "level3_X1_X2.gds" file
            QString m_dbFile = QString(GDS_DIR) + "/level3_" + QString("%1").arg(m_currentLevelOneID) + "_" +
                    QString("%1").arg(m_currentLevelTwoID) + ".gds";

            // Serialize our current data
            QFile file(m_dbFile);
            file.open(QFile::WriteOnly);

            // Thanks to our << and >> overloads, this will serialize just what we need
            QDataStream out(&file);

            // Write out the number of elements and all the data structures
            out << m_currentGraphElements.size();
            for(int i=0; i<m_currentGraphElements.size(); i++)
            {
                out << *(m_currentGraphElements[i]); // Remember that these are pointers, they need to be dereferenced
            }

            file.close();
        }break;
    }
}

// This function takes care of converting and marshalling all memory pointers of
// QVector<dbDataStructure*> m_currentGraphElements into QVector<quint32> nextItemsIndices indices
// to store on disk
void MainWindowEditMode::convertDbDataToStorableData(bool m_towardsDiskFile)
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
void MainWindowEditMode::clearAllPanes()
{
    codeEditorWidget->setPlainText("");
    codeEditorWidget->m_selectedLines.clear();
    txtEditorWidget->m_textEditorWin->clear();
    ui->spinBox->setValue(0);
    ui->txtLabel->setText("Block");
}

// NOTICE: this doesn't store anything on disk, just stores everything on the currently selected element
void MainWindowEditMode::saveEverythingOnThePanesToMemory()
{
    qWarning() << "saveEverythingOnThePanesToMemory <- saving all panes in selected element";

    // We can't save anything if there's no element
    if(m_currentGraphElements.size() > 0 && m_selectedElement != NULL)
    {
        // If there's data on the right pane save it with the current element
        if(!txtEditorWidget->m_textEditorWin->document()->isEmpty() )
        {
            m_selectedElement->data.clear();
            m_selectedElement->data.append(txtEditorWidget->m_textEditorWin->toHtml());
        }
        else
            m_selectedElement->data.clear();

        // If there's data on the label pane and bla bla.. same as above
        if(!ui->txtLabel->text().isEmpty())
        {
            QString m_data(m_selectedElement->label);
            if(!(m_data == ui->txtLabel->text()))
            {
                // Store it
                m_selectedElement->label = ui->txtLabel->text();
                ui->txtLabel->clear();
                ui->txtLabel->setText(m_data);
            }
        }

        // Save left pane lines and filename if we're on level > 1
        if(m_currentActiveLevel != LEVEL_ONE)
        {
            // If nothing is selected, don't save anything
            if(m_selectedElement->fileName.isEmpty())
            {
                qWarning() << "saveEverythingOnThePanesToMemory() - fileName empty - can't save anything";
                m_selectedElement->firstLineData.clear();
                m_selectedElement->linesNumbers.clear();
                return;
            }
            qWarning() << "saveEverythingOnThePanesToMemory() - saving lines numbers..";
            // Get highlighted lines and normalize them
            if(codeEditorWidget->m_selectedLines.size() > 0)
            {
                // Every element next to the first should be an offset from the first
                m_selectedElement->linesNumbers.clear();
                qSort(codeEditorWidget->m_selectedLines);
                m_selectedElement->linesNumbers.append(codeEditorWidget->m_selectedLines[0]);
                for(int i=1; i<codeEditorWidget->m_selectedLines.size(); i++)
                {
                    m_selectedElement->linesNumbers.append(codeEditorWidget->m_selectedLines[i]
                                                           -codeEditorWidget->m_selectedLines[0]);
                }
                qWarning() << endl;
                qWarning() << " >< THESE LINES ARE STORED";
                for(int j=0;j<m_selectedElement->linesNumbers.size();j++)
                    qWarning() << m_selectedElement->linesNumbers[j] << " ";
                qWarning() << endl;
                // Finally store the first line text data
                m_selectedElement->firstLineData.clear();
                m_selectedElement->firstLineData.append(codeEditorWidget->getLineData(m_selectedElement->linesNumbers[0]));
            }
            else
            {
                // Hey, also 0 selected lines is a value to be stored!
                m_selectedElement->linesNumbers.clear();
                m_selectedElement->firstLineData.clear();
            }
        }
    }

}

// Load everything from the selected element on the panes
void MainWindowEditMode::loadSelectedElementDataInPanes()
{
    qWarning() << "Loading selected element to panes ->";

    //
    // Load the right pane with the new values for the new selected element
    //
    txtEditorWidget->m_textEditorWin->setHtml(QString(m_selectedElement->data));
    if(m_lastSelectedHasBeenDeleted)
        m_lastSelectedHasBeenDeleted = false;

    //
    // Load the label and spin index
    //
    ui->spinBox->setValue(m_selectedElement->userIndex);
    ui->txtLabel->setText(m_selectedElement->label);

    //
    // Load the code file and add it to the combobox IF WE'RE ON LEVEL 2/3, notice that there might not be a file associated
    //
    if(m_currentActiveLevel == LEVEL_ONE || m_selectedElement == NULL)
        return;
    // If there's no file, don't load anything
    if(m_selectedElement->fileName.isEmpty())
    {
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
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;
    // Read the entire file and display it into the code window
    QByteArray data = file.readAll();
    codeEditorWidget->setPlainText(QString(data));
    file.close();
    if(m_recentFilePaths.size() >= 15)
    {
        // Remove the less recent one
        m_recentFilePaths.remove(0);
    }

    // Add this path to the combobox and the vector (if it's not already there)
    if(!m_recentFilePaths.contains(m_selectedElement->fileName))
    {
        m_recentFilePaths.append(m_selectedElement->fileName);
        // Update combobox
        ui->fileComboBox->clear();
        for(int i=0;i<m_recentFilePaths.size();i++)
            ui->fileComboBox->addItem(m_recentFilePaths[i]);

        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count()-1);
    }
    else
    {
        int index = m_recentFilePaths.indexOf(m_selectedElement->fileName);
        ui->fileComboBox->setCurrentIndex(index);
    }

    // Highlight the lines in the file we're associated to (if we have any)
    if(m_selectedElement->linesNumbers.size() == 0)
        return;
    // Check for corruption (source file changed)
    QStringList allLines = QString(data).split(QRegExp("\n"),QString::KeepEmptyParts);
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

void MainWindowEditMode::freeCurrentGraphElements()
{
    // Free memory and clear elements' buffer
    for(int i=0; i<m_currentGraphElements.size(); i++)
    {
        delete m_currentGraphElements[i];
    }
    m_currentGraphElements.clear();

    m_selectedElement = NULL;
}

// Try to load a level database or set the m_firstTimeGraphInCurrentLevel if there isn't any
void MainWindowEditMode::tryToLoadLevelDb(level lvl, bool returnToElement)
{
    // Check if the db directory exists in the current directory
    if(!QDir(GDS_DIR).exists())
    {
        // There's nothing, not even the directory.. first time for every level
        QDir().mkdir(GDS_DIR);
        m_firstTimeGraphInCurrentLevel = true;
    }
    else
    {
        switch(lvl)
        {
            case LEVEL_ONE:
            {
                // "level1_general.gds" file

                // If the file doesn't exist, first time mode
                if(!QFile(QString(GDS_DIR) + "/level1_general.gds").exists())
                {
                    m_firstTimeGraphInCurrentLevel = true;
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

                // Draw loaded data and set root element as selected
                m_selectedElement = m_currentGraphElements[0];
                m_firstTimeGraphInCurrentLevel = false; // We've found data, so no need to start a new graph

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
                m_firstTimeGraphInCurrentLevel = true;
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
            m_firstTimeGraphInCurrentLevel = false; // We've found data, so no need to start a new graph

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
                m_firstTimeGraphInCurrentLevel = true;
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
            m_firstTimeGraphInCurrentLevel = false; // We've found data, so no need to start a new graph

            // This editor always starts in level one, so openGL widget is already loaded, redraw data!
            deferredPaintNow();
        }break;

        }

    }
}

