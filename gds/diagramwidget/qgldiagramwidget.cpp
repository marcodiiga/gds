#include "qgldiagramwidget.h"
#include "roundedRectangle.h"
#include "mainwindoweditmode.h" // Forward declaration
#include "mainwindowviewmode.h" // Forward declaration

// The data needed to draw rounded 3D rectangles
#define BUFFER_OFFSET(x)((char *)NULL+(x))

// Initialize static variable to the first color available
unsigned char dataToDraw::gColorID[3] = {0, 0, 0};

// Set a static dark blue background (51;0;123)
float QGLDiagramWidget::m_backgroundColor[3] = {0.2f, 0.0f, 0.6f};

// Constructor to initialize the unique color
dataToDraw::dataToDraw()
{
    m_colorID[0] = gColorID[0];
    m_colorID[1] = gColorID[1];
    m_colorID[2] = gColorID[2];

    gColorID[0]++;
    if(gColorID[0] > 255)
    {
        gColorID[0] = 0;
        gColorID[1]++;
        if(gColorID[1] > 255)
        {
            gColorID[1] = 0;
            gColorID[2]++;
        }
    }

    // Background color is reserved, so don't assign it
    if(gColorID[0] == (QGLDiagramWidget::m_backgroundColor[0]*255.0f)
            && gColorID[1] == (QGLDiagramWidget::m_backgroundColor[1]*255.0f)
                               && gColorID[2] == (QGLDiagramWidget::m_backgroundColor[2]*255.0f))
    {
        // Next time we would have picked right this color, change it
        gColorID[0]++;
    }

}


QGLDiagramWidget::QGLDiagramWidget(QMainWindow *referringWindow, QWidget *parent) :
    QGLWidget(QGLFormat(QGL::SampleBuffers), parent)
{
    m_readyToDraw = false;
    m_associatedWindowRepaintScheduled = false;
    // Save our referring window
    m_referringWindow = referringWindow;
    m_swapInProgress = false;
    m_gdsEditMode = false; // By default, will be changed by the parent application if needed

    ShaderProgramNormal = NULL, ShaderProgramPicking = NULL;
    VertexShader = FragmentShader = NULL;
    m_diagramData = NULL;
    m_selectionTransitionTimer = new QTimer();
    m_selectionTransitionTimer->setInterval(50);
    connect(m_selectionTransitionTimer, SIGNAL(timeout()), this, SLOT(slotTransitionSelected()),Qt::QueuedConnection);
    xAmount = yAmount = zAmount = 0;
    dataDisplacementComplete = false;

    firstTimeDrawing = true;
    m_maximumXreached = 0;
    zoomFactor = 0;
    needForZoomRepaint = false;
    needForDirectionRepaint = false;
    m_pickingRunning = false;
    m_goToSelectedRunning = false;

    // This might have caused a lot of pain with paintEvent and a QPainter
    setAutoFillBackground(false);

    // Set auto swap to false, we need this for the colorpicking
    setAutoBufferSwap(false);

    // By default this widget captures directional keyboard arrows
    // by the way we can't just grabKeyboard() because that would eat all the events
    // for other widgets too, we will just wait for the focus and then hook the keyboard
    this->installEventFilter(this);
}

QGLDiagramWidget::~QGLDiagramWidget()
{
    // Clear all data resources
    deallocateAllMemory();
    // Free allocated timer object
    delete m_selectionTransitionTimer;
    // Clear-up VBOs
    freeBlockBuffers();
    // Clear-up textures
    freeBlockTextures();
    // Free keyboard hook (if present)
    releaseKeyboard();
    // Remove event filter
    this->removeEventFilter(this);
}


// Make sure that keyboard events are filtered when we have the focus
bool QGLDiagramWidget::eventFilter(QObject *watched, QEvent *e)
{

    if(e->type() == QEvent::FocusIn && watched == this)
    {
        grabKeyboard();
        qWarning() << "QGLDiagramWidget received focus, keyboard hook is now active";
        return true;
    }
    else if(e->type() == QEvent::FocusOut && watched == this)
    {
        releaseKeyboard();
        qWarning() << "QGLDiagramWidget lost focus, keyboard hook is now deactivated";
        return true;
    }
    else
    {
        return QWidget::eventFilter(watched, e);
    }
}



// This function allows data insertion into the graph tree
// Returns NULL if there's an error
void *QGLDiagramWidget::insertTreeData(QString label, void *father)
{
    // Root element insertion
    if(father == NULL)
    {
        // The father parameter is allowed to be "NULL" just for the first element
        if(m_diagramData != NULL)
        {
            qWarning() << "Root element already set" << endl;
            return NULL;
        }

        dataToDraw *root = new dataToDraw(); // This also assigns a new unique color to this object
        root->m_label = label;
        root->m_depth = 0;
        m_diagramData = root;

        return root;
    }
    else
    {
        // Element insertion (and depth calculation)
        dataToDraw *element = new dataToDraw(); // This also assigns a new unique color to this object
        element->m_label = label;
        element->m_depth = ((dataToDraw*)father)->m_depth + 1;
        // Attach it to its father
        ((dataToDraw*)father)->m_nextItems.append(element);

        return element;
    }
}

// Change the selected element and start the animation to center it
void QGLDiagramWidget::changeSelectedElement(void *newElement)
{
    if(newElement == (void*)m_selectedItem)
    {
        // Do nothing, the element is already selected
        return;
    }

    m_selectedItem = (dataToDraw *)newElement;
    m_goToSelectedRunning = true; // Selection running is on (the interpolation towards the element)

    if(!m_gdsEditMode)
    {
        // View Mode

        // Start the interpolation timer and set the destination view matrix (on the selected element)
        m_destinationViewMatrix.setToIdentity();
        m_destinationViewMatrix.lookAt(QVector3D(0,-4,-30), QVector3D(0,-8,0), QVector3D(0,1,0));
        m_destinationViewMatrix.translate(m_selectedItem->m_Xdisp,-m_selectedItem->m_Ydisp,0);

        ((MainWindowViewMode*)m_referringWindow)->m_animationOnGoing = true;
        m_selectionTransitionTimer->start();
    }
    else
    {
        // Edit mode

        // Immediately select it
        m_goToSelectedRunning = false;

        m_destinationViewMatrix.setToIdentity();
        m_destinationViewMatrix.lookAt(QVector3D(0,-4,-30), QVector3D(0,-8,0), QVector3D(0,1,0));
        m_destinationViewMatrix.translate(m_selectedItem->m_Xdisp,-m_selectedItem->m_Ydisp,0);

        gl_previousUserView = m_destinationViewMatrix;

        if(!m_swapInProgress)
            repaint(); // We don't need this in the timer section because it's automatically called by the timer, but we do here
    }
}


// Reset this graph's data and make sure that nothing is drawn before new data is ready
void QGLDiagramWidget::clearGraphData()
{
    deallocateAllMemory();

    if(!m_swapInProgress)
    {
        firstTimeDrawing = true;
        repaint(); // Repaint the background
    }
}

// Deallocate memory and set every security variable to "data not ready"
void QGLDiagramWidget::deallocateAllMemory()
{
    dataDisplacementComplete = false;

    // Use the m_diagramDataVector to free all the data of the tree
    QVector<dataToDraw*>::iterator itr = m_diagramDataVector.begin();
    while(itr != m_diagramDataVector.end())
    {
        // Free data
        delete (*itr);

        itr++;
    }
    m_diagramData = NULL;
    m_selectedItem = NULL;
    // Empty the vector
    m_diagramDataVector.clear();
    m_depthIntervals.clear();
}

// Override to initialize glew extensions and prepare openGL resources
// This function is called each time openGL context is switched back
void QGLDiagramWidget::initializeGL()
{
    // Set this widget the current context for openGL operations
    makeCurrent();

    if(firstTimeDrawing)
    {
        GLenum init = glewInit();
        if (GLEW_OK != init)
        {
          /* Problem: glewInit failed, something is seriously wrong. */
          qWarning() << glewGetErrorString(init);
        }

        // Load, compile and link two shader programs ready to be bound, one with the normal
        // gradient, the other with the selected gradient
        loadShadersFromResources("VertexShader1.vert", "FragmentShader1.frag", &ShaderProgramNormal);
        loadShadersFromResources("VertexShader2Picking.vert", "FragmentShader2Picking.frag", &ShaderProgramPicking);

        // Clear-up VBOs (if VBO don't exist, this simply ignores them)
        freeBlockBuffers();

        // Initialize VBOs for rounded blocks
        initBlockBuffers();

        // Clear-up texture buffers (if they don't exist, this simply ignores them)
        freeBlockTextures();

        // Initialize texture buffers for rounded blocks
        initBlockTextures();
    }

    if(dataDisplacementComplete) // Load the real resources when we actually have something to draw
    {
        // Load, compile and link two shader programs ready to be bound, one with the normal
        // gradient, the other with the selected gradient
        loadShadersFromResources("VertexShader1.vert", "FragmentShader1.frag", &ShaderProgramNormal);
        loadShadersFromResources("VertexShader2Picking.vert", "FragmentShader2Picking.frag", &ShaderProgramPicking);

        // Clear-up VBOs (if VBO don't exist, this simply ignores them)
        freeBlockBuffers();

        // Initialize VBOs for rounded blocks
        initBlockBuffers();

        // Clear-up texture buffers (if they don't exist, this simply ignores them)
        freeBlockTextures();

        // Initialize texture buffers for rounded blocks
        initBlockTextures();
    }
}

// Paint event, it's called every time the widget needs to be redrawn and
// allows overpainting over the GL rendered scene
void QGLDiagramWidget::paintEvent(QPaintEvent *event)
{
    makeCurrent();

    // QPainter initialization changes a lot of stuff in the openGL context
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Calls base class which calls initializeGL ONCE and then paintGL each time it's needed
    QGLWidget::paintEvent(event);

    // Don't paint anything if the data isn't ready yet
    if(dataDisplacementComplete)
    {
        // Time for overpainting
        QPainter painter( this );

        painter.setPen(QPen(Qt::white));
        painter.setFont(QFont("Arial", 10, QFont::Bold));

        // Draw the text on the rendered screen
        // -----> x
        // |
        // |
        // | y
        // v
        // Use the bounding box features to create a perfect bounding rectangle to include all the necessary text
        QRectF rect(QPointF(10,this->height()-25),QPointF(this->width()-10,this->height()));
        QRectF neededRect = painter.boundingRect(rect, Qt::TextWordWrap, "Current Block: " + m_selectedItem->m_label);
        if(neededRect.bottom() > this->height())
        {
            qreal neededSpace = qAbs(neededRect.bottom() - this->height());
            neededRect.setTop(neededRect.top()-neededSpace-10);
        }

        painter.drawText(neededRect, Qt::TextWordWrap , "Current Block: " + m_selectedItem->m_label);

        painter.end();
    }

    // Actually draw the scene, double rendering
    swapBuffers();

    // Restore previous values
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();

    if(!m_readyToDraw) // If there's still someone waiting to send data to us, awake him
    {
        m_readyToDraw = true;
        qWarning() << "GLWidget ready to paint data";
        if(m_associatedWindowRepaintScheduled)
        {
            qWarning() << "m_associatedWindowRepaintScheduled is set";
            if(m_gdsEditMode)
            {
                qWarning() << "Calling the deferred painting method now..";
                ((MainWindowEditMode*)m_referringWindow)->deferredPaintNow();
            }
            else
            {
                qWarning() << "Calling the deferred painting method now..";
                ((MainWindowViewMode*)m_referringWindow)->deferredPaintNow();
            }
        }
    }
}

// Rendering cycle (this is called every time the widget needs to be redrawn)
void QGLDiagramWidget::paintGL()
{
    // Dark blue background
    //glClearColor(0.2f, 0.0f, 0.5f, 0.0f);
    glClearColor(m_backgroundColor[0], m_backgroundColor[1], m_backgroundColor[2], 1.0f);
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    // No need to activate multisampling, it's already active for this context

    // Restore all depths and color buffers to their original values
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set viewport area
    glViewport(0, 0, this->size().width(), this->size().height());

    if(!dataDisplacementComplete) // Just clear the background if we haven't any data to render
    {
        return;
    }

    // We are not using fixed pipeline here, but programmable ones
    // equivalent to:
    //
    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
    // gluPerspective(45.,((GLfloat)this->size().width())/((GLfloat)this->size().height()),0.1f,100.0f);
    gl_projection.setToIdentity();
    gl_projection.perspective(45.,((GLfloat)this->size().width())/((GLfloat)this->size().height()),0.1f,1000.0f);

    // Set a view matrix (camera)
    gl_view.setToIdentity();
    gl_view.lookAt(QVector3D(0,-4,-30), QVector3D(0,-8,0), QVector3D(0,1,0));

    // and a basic model matrix (this is going to be moved depending on the order of the element to be rendered)
    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();
    gl_model.setToIdentity();

    GLuint uMVMatrix, uPMatrix, uNMatrix, TextureID = 0, uAmbientColor, uPointLightingLocation, uPointLightingColor;
    QGLShaderProgram *currentShaderProgram; // The current shader program used to render

    if(m_pickingRunning)
    {
        //**************************************************************************************************//
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        //                      Picking running, load just what's needed to do it                           //
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        //**************************************************************************************************//

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        //  Phase 1: prepare all shaders uniforms, attribute arrays and data to draw rounded rectangles   //
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        // Do the colorpicking first
        currentShaderProgram = ShaderProgramPicking;
        if(!currentShaderProgram->bind())
        {
            qWarning() << "Shader Program Binding Error" << currentShaderProgram->log();
        }
        // Picking mode, simple shaders

        uMVMatrix = glGetUniformLocation(currentShaderProgram->programId(), "uMVMatrix");
        uPMatrix = glGetUniformLocation(currentShaderProgram->programId(), "uPMatrix");

        // Send our transformation to the currently bound shader,
        // in the right uniform
        float gl_temp_data[16];
        for(int i=0; i<16; i++)
        {
            // Needed to convert from double (on non-ARM architectures qreal are double)
            // to float
            gl_temp_data[i]=(float)gl_projection.data()[i];
        }
        glUniformMatrix4fv(uPMatrix, 1, GL_FALSE, &gl_temp_data[0]);

        gl_modelView = gl_view * gl_model;

        // Bind the array buffer to the one with our data
        glBindBuffer(GL_ARRAY_BUFFER, blockVertexBuffer);

        // Give the vertices to the shader at attribute 0
        glEnableVertexAttribArray(0);
        // Fill the attribute array with our vertices data
        glVertexAttribPointer(
                    0,                                  // attribute. Must match the layout in the shader.
                    3,                                  // size
                    GL_FLOAT,                           // type
                    GL_FALSE,                           // normalized?
                    sizeof (struct vertex_struct),      // stride
                    (void*)0                            // array buffer offset
                    );
        // Bind the element array indices buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, blockIndicesBuffer);

        // First draw all normal elements, so bind texture normal

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_FOG);
        glDisable(GL_LIGHTING);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        //  Phase 2: iterate through all rounded blocks to draw and set for each one its transformation matrix   //
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////

        // **************
        // WARNING: DISPLACEMENTS ARE ASSUMED TO BE CALCULATED BY NOW, IF NOT THE RENDERING MIGHT CRASH
        // **************

        // Adjust the view by recalling how it was displaced (by the user with the mouse maybe) last time
        adjustView();

        // Draw the precalculated-displacement block elements
        postOrderDrawBlocks(m_diagramData, gl_view, gl_model, uMVMatrix, TextureID);

        // Save the view for the next passing
        gl_previousUserView = gl_view;


        // At this point the picking should be ready (all drawn into the backbuffer)
        // At the end of paintGL swapBuffers is automatically called

        // Picking still running, identify the object the mouse was pressed on

        // Get color information from frame buffer
        unsigned char pixel[3];

        glReadPixels(m_mouseClickPoint.x(), m_mouseClickPoint.y(), 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

        // If the background was clicked, do nothing
        if(pixel[0] == (unsigned char)(m_backgroundColor[0]*255.0f)
                && pixel[1] == (unsigned char)(m_backgroundColor[1]*255.0f)
                && pixel[2] == (unsigned char)(m_backgroundColor[2]*255.0f))
        {
            m_goToSelectedRunning = false;
            qWarning() << "background selected..";
            m_pickingRunning = false; // Color picking is over
            this->setFocus(); // The event filter will take care of the keyboard hook
        }
        else
        {
            // Something was actually clicked!

            // Now our picked screen pixel color is stored in pixel[3]
            // so we search through our object list looking for the object that was selected
            QVector<dataToDraw*>::iterator itr = m_diagramDataVector.begin();
            while(itr != m_diagramDataVector.end())
            {

                if((*itr)->m_colorID[0] == pixel[0] && (*itr)->m_colorID[1] == pixel[1] && (*itr)->m_colorID[2] == pixel[2])
                {
                    // Flag object as selected
                    // qWarning() << "SELECTED ELEMENT: " << (*itr)->m_label;
                    // Save the selected element
                    m_selectedItem = (*itr);
                    this->setFocus();

                    m_pickingRunning = false; // Color picking is over

                    // Signal our referring class that the selection has changed
                    if(m_gdsEditMode)
                    {
                        void *m_newSelected = ((MainWindowEditMode*)m_referringWindow)->GLWidgetNotifySelectionChanged(m_selectedItem);
                        if(m_newSelected != NULL) // Something was swapped so everything has changed
                        {
                            m_selectedItem = (dataToDraw*)m_newSelected;
                        }
                    }
                    else
                    {
                        // We can't notify that the selection has changed until it has effectively changed, so just delegate
                        // the "GLWidgetNotifySelectionChanged" calling to the view window at the end of the timer
                    }

                    m_goToSelectedRunning = true; // Selection running is on (the interpolation towards the element)

                    // Just animate if we're not in edit mode
                    if(!m_gdsEditMode)
                    {
                        // View mode

                        // Start the interpolation timer and set the destination view matrix (on the selected element)
                        m_destinationViewMatrix.setToIdentity();
                        m_destinationViewMatrix.lookAt(QVector3D(0,-4,-30), QVector3D(0,-8,0), QVector3D(0,1,0));
                        m_destinationViewMatrix.translate(m_selectedItem->m_Xdisp,-m_selectedItem->m_Ydisp,0);

                        ((MainWindowViewMode*)m_referringWindow)->m_animationOnGoing = true;
                        m_selectionTransitionTimer->start();
                    }
                    else
                    {
                        // Edit mode, immediately select the element without animating
                        m_goToSelectedRunning = false;

                        m_destinationViewMatrix.setToIdentity();
                        m_destinationViewMatrix.lookAt(QVector3D(0,-4,-30), QVector3D(0,-8,0), QVector3D(0,1,0));
                        m_destinationViewMatrix.translate(m_selectedItem->m_Xdisp,-m_selectedItem->m_Ydisp,0);

                        gl_previousUserView = m_destinationViewMatrix;
                    }
                    break;
                }
                itr++;
            }
        }

    }

    //**************************************************************************************************//
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                  No picking running, draw as usual                               //
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //**************************************************************************************************//

    // Restore all depths and color buffers to their original values
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    currentShaderProgram = ShaderProgramNormal;
    if(!currentShaderProgram->bind())
    {
        qWarning() << "Shader Program Binding Error" << currentShaderProgram->log();
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //  Phase 1: prepare all shaders uniforms, attribute arrays and data to draw rounded rectangles   //
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Normal mode, complete shaders

    // Get a handle for our uniforms
    uMVMatrix = glGetUniformLocation(currentShaderProgram->programId(), "uMVMatrix");
    uPMatrix = glGetUniformLocation(currentShaderProgram->programId(), "uPMatrix");
    uNMatrix = glGetUniformLocation(currentShaderProgram->programId(), "uNMatrix");

    // Get a handle for our "myTextureSampler" uniform
    TextureID  = glGetUniformLocation(currentShaderProgram->programId(), "myTextureSampler");

    // Send our transformation to the currently bound shader,
    // in the right uniform
    float gl_temp_data[16];
    for(int i=0; i<16; i++)
    {
        // Needed to convert from double (on non-ARM architectures qreal are double)
        // to float
        gl_temp_data[i]=(float)gl_projection.data()[i];
    }
    glUniformMatrix4fv(uPMatrix, 1, GL_FALSE, &gl_temp_data[0]);

    // We don't set a modelview matrix yet because each element is different
    //    for(int i=0; i<16; i++)
    //    {
    //        // Needed to convert from double (on non-ARM architectures qreal are double)
    //        // to float
    //        gl_temp_data[i]=gl_modelView.data()[i];
    //    }
    //    glUniformMatrix4fv(uMVMatrix, 1, GL_FALSE, &gl_temp_data[0]);


    // Set the global modelView variable to be used (next passes) by the normalMatrix
    // gl_model is totally irrelevant in this context since we just need the view to calculate the normalMatrix to the light
    gl_modelView = gl_view * gl_model;

    // Create normal matrix
    QMatrix4x4 normalMatrix;
    normalMatrix = gl_modelView.inverted();
    normalMatrix = normalMatrix.transposed();
    QMatrix3x3 normalMatrix3x3 = normalMatrix.toGenericMatrix<3,3>();
    for(int i=0; i<9; i++)
    {
        // Needed to convert from double (on non-ARM architectures qreal are double)
        // to float
        gl_temp_data[i]=normalMatrix3x3.data()[i];
    }
    glUniformMatrix3fv(uNMatrix, 1, GL_FALSE, &gl_temp_data[0]);

    // Standard uniform values for the light in the fragment shader
    float ambientLightColor[3] = {0.2f, 0.2f, 0.2f};
    float pointLightLocation[3] = {0.0f, 0.0f, -5.0f};
    float pointLightColor[3] = {0.8f, 0.8f, 0.8f};

    uAmbientColor = glGetUniformLocation(ShaderProgramNormal->programId(), "uAmbientColor");
    glUniform3f(uAmbientColor, ambientLightColor[0],ambientLightColor[1],ambientLightColor[2]);

    uPointLightingLocation = glGetUniformLocation(ShaderProgramNormal->programId(), "uPointLightingLocation");
    glUniform3f(uPointLightingLocation, pointLightLocation[0],pointLightLocation[1],pointLightLocation[2]);

    uPointLightingColor = glGetUniformLocation(ShaderProgramNormal->programId(), "uPointLightingColor");
    glUniform3f(uPointLightingColor, pointLightColor[0],pointLightColor[1],pointLightColor[2]);
    /*
          Set shader vertex, uv coords and normals attribute arrays. Set textures uniforms too
    */

    // Bind the array buffer to the one with our data
    glBindBuffer(GL_ARRAY_BUFFER, blockVertexBuffer);

    // Give the vertices to the shader at attribute 0
    glEnableVertexAttribArray(0);
    // Fill the attribute array with our vertices data
    glVertexAttribPointer(
                0,                                  // attribute. Must match the layout in the shader.
                3,                                  // size
                GL_FLOAT,                           // type
                GL_FALSE,                           // normalized?
                sizeof (struct vertex_struct),      // stride
                (void*)0                            // array buffer offset
                );
    // Uv coords
    glEnableVertexAttribArray(1);
    // Fill the attribute array in the vertex shader with our uv coords data
    glVertexAttribPointer(
                1,                                  // attribute 1
                2,                                  // size
                GL_FLOAT,                           // type
                GL_FALSE,                           // normalized?
                sizeof (struct vertex_struct),      // stride
                BUFFER_OFFSET(6 * sizeof(float))    // array buffer offset (after vertices coords and normals)
                );

    // Last: normals
    glEnableVertexAttribArray(2);
    // Fill the attribute array in the vertex shader with our normals data
    glVertexAttribPointer(
                2,                                  // attribute 2
                3,                                  // size
                GL_FLOAT,                           // type
                GL_FALSE,                           // normalized?
                sizeof (struct vertex_struct),      // stride
                BUFFER_OFFSET(3 * sizeof(float))    // array buffer offset (after vertices coords)
                );

    // Since all our actions are with a generic attribute array used in the shaders, we don't need to select specific arrays
    //    glEnableClientState(GL_VERTEX_ARRAY);
    //    glVertexPointer(3, GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(0));
    //    glEnableClientState(GL_NORMAL_ARRAY);
    //    glNormalPointer(GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(3 * sizeof (float)));
    //    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    //    glTexCoordPointer(2, GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(6 * sizeof(float)));

    // Bind the element array indices buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, blockIndicesBuffer);

    // First draw all normal elements, so bind texture normal

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////
    //  Phase 2: iterate through all rounded blocks to draw and set for each one its transformation matrix   //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////

    // **************
    // WARNING: DISPLACEMENTS ARE ASSUMED TO BE CALCULATED BY NOW, IF NOT THE RENDERING MIGHT CRASH
    // **************

    // Adjust the view by recalling how it was displaced (by the user with the mouse maybe) last time
    adjustView();

    // Draw all the blocks with normal texture, we'll think to the selected one later
    glActiveTexture(GL_TEXTURE0); // Bind our texture on texture unit 0
    glBindTexture(GL_TEXTURE_2D, blockTextureID_normal);
    // Set our "myTextureSampler" sampler to user Texture Unit 0
    glUniform1i(TextureID, 0);

    // Draw the precalculated-displacement block elements
    postOrderDrawBlocks(m_diagramData, gl_view, gl_model, uMVMatrix, TextureID);

    // Save the view for the next passing
    gl_previousUserView = gl_view;

    // Finalize by drawing the connection lines between elements, we don't need this in picking mode (useless overhead)
    drawConnectionLinesBetweenBlocks();


    // Unbind buffers, textures and disable vertex attribute arrays
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glBindTexture(GL_TEXTURE_2D, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glDisable(GL_TEXTURE_2D);
}

// This function takes care of the view matrix interpolation till the selected element's position
// Remember that the entire scene is being "moved" by modifying the view matrix, there's no camera object
// and the real eyeview is always at 0;0;0
void QGLDiagramWidget::slotTransitionSelected()
{
    // Get the translation vector of the matrix view
    /* A view matrix has the following form:

        Rx Ux Fx Tx
        Ry Uy Fy Ty
        Rz Uz Fz Tz
         0  0  0  1

        Where R = unit vector pointing to right
        Where U = unit vector pointing up (up-vector)
        Where F = unit vector pointing to front (or rear, depending on your app's convention)
        Where T = translation vector (i.e. world space position coordinate)
    */
    QVector4D translationVecOrig = gl_previousUserView.column(3);
    QVector4D translationVecDest = m_destinationViewMatrix.column(3);

    // Modify the original translation vector to slightly approach the destination vector

    // We want all the coordinates to proceed in the same amount of time, i.e. 2 seconds = 2000 ms
    // this timer has interval of 50 ms, so 40 steps. For each step each coordinate should be incremented
    // or decremented by an amount that will grant all of them reaching the destination at the same time

    // Calculate the amount for X,Y,Z, do this just once
    if(xAmount == 0.0 && yAmount == 0.0 && zAmount == 0.0)
    {
        xAmount = qAbs(translationVecOrig.x()-translationVecDest.x()) / 40.0;
        yAmount = qAbs(translationVecOrig.y()-translationVecDest.y()) / 40.0;
        zAmount = qAbs(translationVecOrig.z()-translationVecDest.z()) / 40.0;
    }

    if(qAbs(translationVecOrig.x()-translationVecDest.x()) > 0.1)
    {
        if(translationVecOrig.x() > translationVecDest.x())
            translationVecOrig.setX(translationVecOrig.x()-xAmount);
        else
            translationVecOrig.setX(translationVecOrig.x()+xAmount);
    }

    if(qAbs(translationVecOrig.y()-translationVecDest.y()) > 0.1)
    {
        if(translationVecOrig.y() > translationVecDest.y())
            translationVecOrig.setY(translationVecOrig.y()-yAmount);
        else
            translationVecOrig.setY(translationVecOrig.y()+yAmount);
    }

    if(qAbs(translationVecOrig.z()-translationVecDest.z()) > 0.1)
    {
        if(translationVecOrig.z() > translationVecDest.z())
            translationVecOrig.setZ(translationVecOrig.z()-zAmount);
        else
            translationVecOrig.setZ(translationVecOrig.z()+zAmount);
    }

    // Check if we reached the selection point
    if(qAbs(translationVecOrig.x()-translationVecDest.x()) <= 0.1 &&
            qAbs(translationVecOrig.y()-translationVecDest.y()) <= 0.1 &&
                qAbs(translationVecOrig.z()-translationVecDest.z()) <= 0.1)
    {
        // Stop this animation
        m_selectionTransitionTimer->stop();
        // Reset all the amounts
        xAmount = yAmount = zAmount = 0;
        // Signal the animation has finished
        m_goToSelectedRunning = false;

        // Signal our referring class that the selection has changed
        if(!m_gdsEditMode)
        {
            ((MainWindowViewMode*)m_referringWindow)->GLWidgetNotifySelectionChanged(m_selectedItem);
        }
    }
    else
    {
        // Put back the modified vector into the view matrix
        gl_previousUserView.setColumn(3, translationVecOrig);

        // Call for a complete update
        update();
    }

    //qWarning() << translationVecOrig.x() << " " << translationVecOrig.y() << " " << translationVecOrig.z();
}

void QGLDiagramWidget::drawConnectionLinesBetweenBlocks()
{
    // This function is going to draw simple 2D lines with the programmable pipeline

    // The picking shaders are simple enough to let us draw a colored line, we'll use them
    ShaderProgramPicking->bind();
    // NOTICE: since each element's model matrix will be multiplied by the vertex inserted in the vertex array
    // to the shader, this uMVMatrix is actually going to be filled with JUST the VIEW matrix. The result will be
    // the same to the shader
    GLuint uMVMatrix = glGetUniformLocation(ShaderProgramPicking->programId(), "uMVMatrix");
    GLuint uPMatrix = glGetUniformLocation(ShaderProgramPicking->programId(), "uPMatrix");
    // Send our transformation to the currently bound shader,
    // in the right uniform
    float gl_temp_data[16];
    for(int i=0; i<16; i++)
    {
        // Needed to convert from double (on non-ARM architectures qreal are double)
        // to float
        gl_temp_data[i]=(float)gl_projection.data()[i];
    }
    glUniformMatrix4fv(uPMatrix, 1, GL_FALSE, &gl_temp_data[0]);
    for(int i=0; i<16; i++)
    {
        // Needed to convert from double (on non-ARM architectures qreal are double)
        // to float
        gl_temp_data[i]=(float)gl_view.data()[i]; // AGAIN: just the view matrix in the uMVMatrix, the result will be the same
    }
    // Set a color for the lines
    glUniformMatrix4fv(uMVMatrix, 1, GL_FALSE, &gl_temp_data[0]);
    GLuint uPickingColor = glGetUniformLocation(ShaderProgramPicking->programId(), "uPickingColor");
    glUniform3f(uPickingColor, 1.0f,0.0f,0.0f);

    // If there's just one element (root and no connections), exit
    if(m_diagramDataVector.size() == 0 || m_diagramDataVector.size() == 1)
        return;

    // Scroll the diagramDataVector and create the connections for each element
    QVector<dataToDraw*>::iterator itr = m_diagramDataVector.begin();

    // Create a structure to contain all the points for all the lines
    struct Point
    {
        float x,y,z;
        Point(float x,float y,float z)
                        : x(x), y(y), z(z)
        {}
    };

    // This will contain all the point-pairs to draw lines
    std::vector<Point> vertexData;

    while(itr != m_diagramDataVector.end())
    {
        // Set the origin coords (this element's coords)
        QVector3D baseOrig(0.0,0.0,0.0);
        // Adjust them by porting them in world coordinates (*model matrix)
        QMatrix4x4 modelOrigin = gl_model;
        modelOrigin.translate((qreal)(-(*itr)->m_Xdisp),(qreal)((*itr)->m_Ydisp),0.0);
        baseOrig = modelOrigin * baseOrig;

        // Get each children of this node (if any)
        for(int i=0; i< (*itr)->m_nextItems.size(); i++)
        {
            dataToDraw* m_temp = (*itr)->m_nextItems[i];

            // Create destination coords
            QVector3D baseDest(0.0, 0.0, 0.0);
            // Adjust the destination coords by porting them in world coordinates (*model matrix)
            QMatrix4x4 modelDest = gl_model;
            modelDest.translate((qreal)(-m_temp->m_Xdisp),(qreal)(m_temp->m_Ydisp),0.0);
            baseDest = modelDest * baseDest;

            // Add the pair (origin;destination) to the vector
            vertexData.push_back( Point((float)baseOrig.x(), (float)baseOrig.y(), (float)baseOrig.z()) );
            vertexData.push_back( Point((float)baseDest.x(), (float)baseDest.y(), (float)baseDest.z()) );
        }
        itr++;
    }

    // We have everything we need to draw all the lines
    GLuint vao, vbo; // VBO is just a memory buffer, VAO describes HOW the data should be interpreted in the VBO

    // Generate and bind the VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Generate and bind the buffer object
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Fill VBO with data
    size_t numVerts = vertexData.size();
    glBufferData(GL_ARRAY_BUFFER,                           // Select the array buffer on which to operate
                 sizeof(Point)*numVerts,                    // The total size of the VBO
                 &vertexData[0],                            // The initial data of the VBO
                 GL_STATIC_DRAW);                           // STATIC_DRAW mode

    // set up generic attrib pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,                                // Attribute 0 in the shader
                          3,                                // Each vertex has 3 components: x,y,z
                          GL_FLOAT,                         // Each component is a float
                          GL_FALSE,                         // No normalization
                          sizeof(Point),                    // Since it's a struct, x,y,z are defined first, then the constructor
                          (char*)0 + 0*sizeof(GLfloat));    // No initial offset to the data


    // Call the shader to render the lines
    glDrawArrays(GL_LINES, 0, numVerts);

    // "unbind" VAO and VBO
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// This function takes care of adjusting the view of the scene on behalf of mouse/selection events
void QGLDiagramWidget::adjustView()
{
    // Notice: when translating the view matrix, you have to put a vector totally inverse with respect to the
    // one you performed the translation on the model matrix of the element, because you're facing the opposite
    // to the element with the view, you're considering two different axis systems

    if(firstTimeDrawing)
    {
        // First time we are rendering, set the view on the first selected element
        // NOTICE: the coordinates to draw an element perfectly centered on the screen are:
        // (m_Xdisp;-m_Ydisp)
        gl_view.translate(m_diagramData->m_Xdisp,-m_diagramData->m_Ydisp,0);
        gl_previousUserView = gl_view;
        // And we select the first element, too
        m_selectedItem = m_diagramData;

        firstTimeDrawing = false;
    }
    else
    {
        // Restore old view
        gl_view = gl_previousUserView;

        // Need to recall the view history and possibly adjust it with mouse information
        if(needForZoomRepaint)
        {
            // Mouse wheel zoom was requested, let's move it
            gl_view.translate(0,0,-zoomFactor);
            needForZoomRepaint = false;
        }
        if(needForDirectionRepaint)
        {
            // Mouse wheel zoom was requested, let's move it
            gl_view.translate(moveNewDirection);
            needForDirectionRepaint = false;
        }

        // Possibly nothing was requested
    }
}

// Wheel allows us to zoom in the graph
void QGLDiagramWidget::wheelEvent(QWheelEvent *e)
{
    if(this->hasFocus())
    {
        if(!m_goToSelectedRunning) // Don't allow user control while in automatic mode
        {
            zoomFactor  = (- e->delta() / 240.0);
            needForZoomRepaint = true;
            // Ask for a repaint
            repaint();
        }
    }
}


void QGLDiagramWidget::keyPressEvent(QKeyEvent *e)
{
    if(m_goToSelectedRunning) // Don't allow user control while in automatic mode
    {
        e->ignore();
        return;
    }

    // We just care about directional arrows
    switch(e->key())
    {
        case Qt::Key_Left:
        {
            e->accept(); // We handle this

            moveNewDirection.setX(-0.5);
            moveNewDirection.setY(0);
            moveNewDirection.setZ(0);

            needForDirectionRepaint = true;
            // Ask for a repaint
            repaint();
        }break;

        case Qt::Key_Right:
        {
            e->accept();

            moveNewDirection.setX(+0.5);
            moveNewDirection.setY(0);
            moveNewDirection.setZ(0);

            needForDirectionRepaint = true;
            // Ask for a repaint
            repaint();
        }break;

        case Qt::Key_Up:
        {
            e->accept();

            moveNewDirection.setX(0);
            moveNewDirection.setY(-0.5);
            moveNewDirection.setZ(0);

            needForDirectionRepaint = true;
            // Ask for a repaint
            repaint();
        }break;

        case Qt::Key_Down:
        {
            e->accept();

            moveNewDirection.setX(0);
            moveNewDirection.setY(+0.5);
            moveNewDirection.setZ(0);

            needForDirectionRepaint = true;
            // Ask for a repaint
            repaint();
        }break;

        default:
        {
            // This is not handled by us
            e->ignore();
        }break;
    }
}

void QGLDiagramWidget::mousePressEvent(QMouseEvent *e)
{
    if(m_goToSelectedRunning) // Don't allow user control while in automatic mode
    {
        e->ignore();
        return;
    }
    if(e->button() == Qt::LeftButton)
    {
        int posx = e->x();
        int posy = e->y();
        //qWarning()<<"Window: "<<posx<<"\t"<<posy<<endl;
        //qWarning()<<"Passing: "<<posx<<"\t"<<this->size().height()-posy<<endl;

        // Update the clicked position
        m_mouseClickPoint.setX(posx);
        m_mouseClickPoint.setY(this->size().height()-posy);

        // Signal that there's a picking going on
        m_pickingRunning = true;

        // Ask for a repaint
        repaint();
    }
}



// The following function assumes there's a displacement available
// WARNING: THIS MIGHT CRASH IF THE DISPLACEMENT IS NOT UPDATED
// and draws all the block elements on the GL context
void QGLDiagramWidget::postOrderDrawBlocks(dataToDraw *tree, QMatrix4x4 gl_view, QMatrix4x4 gl_model,
                                           GLuint uMVMatrix, GLuint TextureID)
{
    // Recursive drawing in post-order
    for(int i=0; i<tree->m_nextItems.size(); i++)
    {
        postOrderDrawBlocks(tree->m_nextItems[i], gl_view, gl_model, uMVMatrix, TextureID);
    }

    // Draw this element, whatever it is (leaf or parent, doesn't matter.. it has to be drawn)

    gl_model.translate(-tree->m_Xdisp,tree->m_Ydisp,0);
    //qWarning() << tree->m_label << " " << tree->m_Xdisp << tree->m_Ydisp << endl;
    QMatrix4x4 gl_modelView = gl_view * gl_model;
    float gl_temp_data[16];
    for(int i=0; i<16; i++)
    {
        // Needed to convert from double (on non-ARM architectures qreal are double)
        // to float
        gl_temp_data[i]=gl_modelView.data()[i];
    }

    // Load shader's uniforms
    glUniformMatrix4fv(uMVMatrix, 1, GL_FALSE, &gl_temp_data[0]);

    if(m_pickingRunning)
    {
        // Picking running, let's load the uniform color for this object
        GLuint uPickingColor = glGetUniformLocation(ShaderProgramPicking->programId(), "uPickingColor");
//        qWarning() << "element: " << tree->m_label << " color: " << tree->m_colorID[0]/255.0f<<" "<<tree->m_colorID[1]/255.0f<<" "<<tree->m_colorID[2]/255.0f
//                   << endl << " associated color: " << (unsigned char)((tree->m_colorID[0]/255.0f) * 255.0f) << " "
//                   << (unsigned char)((tree->m_colorID[1]/255.0f) * 255.0f) << " "
//                      << (unsigned char)((tree->m_colorID[2]/255.0f) * 255.0f);
        glUniform3f(uPickingColor, tree->m_colorID[0]/255.0f,tree->m_colorID[1]/255.0f,tree->m_colorID[2]/255.0f);
    }
    else
    {
        // If this is the selected element, change the texture to the selected one
        if(tree == m_selectedItem)
            glBindTexture(GL_TEXTURE_2D, blockTextureID_selected);
        else
            glBindTexture(GL_TEXTURE_2D, blockTextureID_normal);
    }

    // Finally draw all the triangles, indices are set and they will help us to determine which are the faces
    glDrawElements(GL_TRIANGLES, faces_count[0] * 3, INX_TYPE, BUFFER_OFFSET(0));
}


// This function is fundamental, calculates the displacement of each element of the tree
// to show a "nice" n-ary tree on the screen
void QGLDiagramWidget::calculateDisplacement()
{
    // Let's find the tree's maximum depth
    long depthMax = findMaximumTreeDepth(m_diagramData);
    // and set up the various depth levels
    m_depthIntervals.resize(depthMax+1);

    // Now traverse the tree and update displacements, add every found element to the m_diagramDataVector too for an easy access
    postOrderTraversal(m_diagramData);

    // Data is ready to be painted
    dataDisplacementComplete = true;

    if(!m_swapInProgress)
        repaint();
}

void QGLDiagramWidget::postOrderTraversal(dataToDraw *tree)
{
    // Traverse each children of this node
    for(int i=0; i<tree->m_nextItems.size(); i++)
    {
        postOrderTraversal(tree->m_nextItems[i]);
    }

    if(tree->m_nextItems.size() == 0)
    {

        // I found a leaf, displacement-update

        // Y are easy: take this leaf's depth and put it on its Y coord * Y_space_between_blocks
        tree->m_Ydisp = - (tree->m_depth * MINSPACE_BLOCKS_Y);

        // X are harder, I need to put myself at least min_space_between_blocks_X away
        tree->m_Xdisp = m_maximumXreached + MINSPACE_BLOCKS_X;

        // Let's add this leaf to the respective depth list, this will be useful for its parent node
        m_depthIntervals[tree->m_depth].m_allElementsForThisLevel.append(tree->m_Xdisp);

        // Update the maximumXreached with my value
        if(tree->m_Xdisp > m_maximumXreached)
            m_maximumXreached = tree->m_Xdisp;
    }
    else
    {
        // Parent node, displacement-update

        // Y are easy: take this leaf's depth and put it on its Y coord * Y_space_between_blocks
        tree->m_Ydisp = - (tree->m_depth * MINSPACE_BLOCKS_Y);

        // X are harder, I need to put the parent in the middle of all its children
        if(tree->m_nextItems.size() == 1)
        {
            // Just one child, no need for a middle calculation, let's just take the children's X coord
            tree->m_Xdisp = tree->m_nextItems[0]->m_Xdisp;
        }
        else
        {
            // Find minimum and maximum for all this node's children (in the X axis)
            qSort(m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel.begin(), m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel.end());
            long min = m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel[0];
            long max = m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel[m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel.size()-1];
            // Let's put this node in the exact middle
            tree->m_Xdisp = (max+min)/2;
        }

        // Let's add this node to the respective depth list, this will be useful for its parent node
        m_depthIntervals[tree->m_depth].m_allElementsForThisLevel.append(tree->m_Xdisp);

        // Update the maximumXreached with my value (if necessary)
        if(tree->m_Xdisp > m_maximumXreached)
            m_maximumXreached = tree->m_Xdisp;

        // Delete the sublevel under my depth, my children are done and they won't be bothering other nodes
        m_depthIntervals[tree->m_depth+1].m_allElementsForThisLevel.clear();
    }

    // However add this node to the m_diagramDataVector
    m_diagramDataVector.append(tree);
}

long QGLDiagramWidget::findMaximumTreeDepth(dataToDraw *tree)
{
    // Simply recurse in post-order inside the tree to find the maximum depth value
    if(tree->m_nextItems.size() == 0)
        return 0;
    else
    {
        int maximumSubTreeDepth = 0;
        for(int i=0; i<tree->m_nextItems.size(); i++)
        {
            long subTreeDepth = findMaximumTreeDepth(tree->m_nextItems[i]);
            if(subTreeDepth > maximumSubTreeDepth)
                maximumSubTreeDepth = subTreeDepth;
        }
        return maximumSubTreeDepth+1; // Plus this node
    }
}



// Initialize rounded blocks textures (normal and selected)
void QGLDiagramWidget::initBlockTextures()
{
    glEnable(GL_TEXTURE_2D);

    // Use two QImages to load the textures
    QImage tex1, tex2, buf;
    if(!buf.load(":/openGL/textures/gradient_normal.png"))
        qWarning() << "Cannot load png file " << "textures/gradient_normal.png";
    tex1 = QGLWidget::convertToGLFormat( buf );
    if(!buf.load(":/openGL/textures/gradient_selected.png"))
        qWarning() << "Cannot load png file " << "textures/gradient_selected.png";
    tex2 = QGLWidget::convertToGLFormat( buf );

    // Generate texture objects
    glGenTextures(1, &blockTextureID_normal);
    glGenTextures(1, &blockTextureID_selected);

    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, blockTextureID_normal);

    // Give the image to OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex1.width(), tex1.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, tex1.bits());

    // When MAGnifying the image (no bigger mipmap available), use LINEAR filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // When MINifying the image, use a LINEAR blend of two mipmaps, each filtered LINEARLY too
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // Generate mipmaps, by the way.
    glGenerateMipmap(GL_TEXTURE_2D);

    // The same for the second texture
    glBindTexture(GL_TEXTURE_2D, blockTextureID_selected);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex2.width(), tex2.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, tex2.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Unbind texture buffer
    glBindTexture(GL_TEXTURE_2D, 0);
}
// Free textures for the rounded blocks
void QGLDiagramWidget::freeBlockTextures()
{
    glDeleteTextures(1, &blockTextureID_normal);
    glDeleteTextures(1, &blockTextureID_selected);
}

// Initialize data for the rounded blocks
void QGLDiagramWidget::initBlockBuffers()
{
    // Initialize a VBO to store the vertex/normals/UVcoords data
    glGenBuffers(1, &blockVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, blockVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof (struct vertex_struct) * vertex_count[0], vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Initialize another VBO for the indices
    glGenBuffers(1, &blockIndicesBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, blockIndicesBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof (indexes[0]) * faces_count[0] * 3, indexes, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
// Free data for the rounded blocks
void QGLDiagramWidget::freeBlockBuffers()
{
    // Deallocate the GPU-resources
    glDeleteBuffers(1, &blockVertexBuffer);
    glDeleteBuffers(1, &blockIndicesBuffer);
}

void QGLDiagramWidget::closeEvent(QCloseEvent *evt)
{
    QGLWidget::closeEvent(evt);
}

void QGLDiagramWidget::loadShadersFromResources(QString vShader, QString fShader, QGLShaderProgram **progShader)
{
    if(*progShader)
    {
        (*progShader)->release();
        (*progShader)->removeAllShaders();
    }
    else
        *progShader = new QGLShaderProgram();

    if(VertexShader)
    {
        delete VertexShader;
        VertexShader = NULL;
    }

    if(FragmentShader)
    {
        delete FragmentShader;
        FragmentShader = NULL;
    }


    // Load vertex and fragment shaders

    VertexShader = new QGLShader(QGLShader::Vertex);
    if(VertexShader->compileSourceFile(":/openGL/shaders/"+vShader))
        (*progShader)->addShader(VertexShader);
    else
        qWarning() << "Vertex Shader Error" << VertexShader->log();

    FragmentShader = new QGLShader(QGLShader::Fragment);
    if(FragmentShader->compileSourceFile(":/openGL/shaders/"+fShader))
        (*progShader)->addShader(FragmentShader);
    else
        qWarning() << "Fragment Shader Error" << FragmentShader->log();

    if(!(*progShader)->link())
    {
        qWarning() << "Shader Program Linker Error" << (*progShader)->log();
    }
//    else
//    {
//        if(!(*progShader)->bind())
//        {
//            qWarning() << "Shader Program Binding Error" << (*progShader)->log();
//        }
//    }
}
