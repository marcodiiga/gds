#ifndef QGLDIAGRAMWIDGET_H
#define QGLDIAGRAMWIDGET_H

#include "gl/glew.h" // Include this before other QtGL files
#include <QGLWidget>
#include <QGLShader>
#include <QResizeEvent>
#include <QVector>
#include <QtAlgorithms>
#include <QTimer>
#include <QMainWindow>

// Forward declaration
class MainWindowEditMode;
class MainWindowViewMode;

// Forward declaration
class dataToDraw;

// The data to be drawn
class dataToDraw
{
private:
    static unsigned char gColorID[3]; // A static variable to keep track of the next color available for all objects

public:
    QString m_label;        // The data label
    long m_depth;   // The depth this node lies at

    // Positioning data (calculated once at the beginning of this diagram)
    long m_Xdisp;
    long m_Ydisp;
    // Color picking data, a unique color assigned to this object
    unsigned char m_colorID[3];

    // All the next items linked to this one
    QVector<dataToDraw*> m_nextItems;

    // Constructor to initialize the unique color
    dataToDraw();
};

// Every depth level of the tree has an array of elements to let the parent
// of these nodes calculate its own displacement with respect of their children's
struct dataSeries
{
    QVector<long> m_allElementsForThisLevel;
};

// Based on how our rounded blocks are drawn, we have a minimum (object coords) on the
// model matrix to avoid blocks overlap
#define MINSPACE_BLOCKS_X 10
#define MINSPACE_BLOCKS_Y 5


class QGLDiagramWidget : public QGLWidget
{
    Q_OBJECT

    friend class dataToDraw; // Let dataToDraw access "m_backgroundColor" to choose a unique color for each element

public:
    explicit QGLDiagramWidget(QMainWindow *referringWindow, QWidget *parent = 0);
    ~QGLDiagramWidget();

    void *insertTreeData(QString label, void *father);
    void calculateDisplacement();
    void changeSelectedElement(void *newElement);
    void clearGraphData();

    // Other classes' support variables
    bool m_gdsEditMode; // If this is true, we don't need to animate the selection of an element
    bool m_swapInProgress; // If this is true, a swap is in progress and we can't repaint until it has finished
    bool m_readyToDraw; // Compiling shaders takes time, if this is true the associated window can send us data to draw
    bool m_associatedWindowRepaintScheduled; // As soon as we are ready to draw, call the associated window's handler
    bool firstTimeDrawing; // If this is set, a root element is selected and base view matrix operations (adjustView) are made
signals:
    
private slots:
    void slotTransitionSelected();

protected:
    // Overrides
    void closeEvent(QCloseEvent *evt);
    void paintEvent(QPaintEvent *event);
    void initializeGL();
    void paintGL();

    void mousePressEvent(QMouseEvent * e);
    void wheelEvent(QWheelEvent * e);
    void keyPressEvent(QKeyEvent *e);
    bool eventFilter( QObject *o, QEvent *e );

private:
    QMainWindow *m_referringWindow; // The main window we're being created on

    dataToDraw *m_diagramData; // The main data pointer, this points to the tree's root
    QVector<dataToDraw*> m_diagramDataVector; // A list sometimes can be slow and uneasy to access, this is equivalent to the above

    static float m_backgroundColor[3]; // This ensures that we won't be interfering with the background in color picking

    QGLShaderProgram *ShaderProgramNormal, *ShaderProgramSelected, *ShaderProgramPicking;
    QGLShader *VertexShader, *FragmentShader;
    void loadShadersFromResources(QString vShader, QString fShader, QGLShaderProgram **progShader);
    void initBlockBuffers();
    void freeBlockBuffers();
    void initBlockTextures();
    void freeBlockTextures();
    void postOrderTraversal(dataToDraw *tree);
    long findMaximumTreeDepth(dataToDraw *tree);
    void postOrderDrawBlocks(dataToDraw *tree, QMatrix4x4 gl_view, QMatrix4x4 gl_model, GLuint uMVMatrix,
                                               GLuint TextureID);
    // A depthMax values vector, each one contains all the children of a given node and their X displacement,
    // it's used to calculate their parent's middle position among them
    QVector<dataSeries> m_depthIntervals;
    long m_maximumXreached;
    bool dataDisplacementComplete; // Used to indicate whether the data is ready to be painted
    void deallocateAllMemory();

    void adjustView();
    // Mouse zooming factor (wheel)
    float zoomFactor;
    bool needForZoomRepaint;

    // This is updated by keyboard arrows and specify a new view direction to be drawn
    bool needForDirectionRepaint;
    QVector3D moveNewDirection;

    bool m_pickingRunning;
    QVector2D m_mouseClickPoint;
    // A selected element will have a different texture (not normal gradient)
    dataToDraw *m_selectedItem;
    bool m_goToSelectedRunning;

    //-> Block GL data
        // This will identify our vertex/normal/UVcoords buffer
        GLuint blockVertexBuffer;
        // This will identify our indices buffer
        GLuint blockIndicesBuffer;
        // Create OpenGL textures
        GLuint blockTextureID_normal;
        GLuint blockTextureID_selected;
    //<-

    void drawConnectionLinesBetweenBlocks();
    QTimer *m_selectionTransitionTimer;
    QMatrix4x4 m_destinationViewMatrix;
    qreal xAmount, yAmount, zAmount;

    QMatrix4x4 gl_projection;
    QMatrix4x4 gl_view;
    QMatrix4x4 gl_previousUserView;
    QMatrix4x4 gl_model;
    QMatrix4x4 gl_MVP;
    QMatrix4x4 gl_modelView;
};

#endif // QGLDIAGRAMWIDGET_H
