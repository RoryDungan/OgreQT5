#include "QTOgreWindow.hpp"
#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
#include <Compositor/OgreCompositorManager2.h>
#endif

QTOgreWindow::QTOgreWindow(QWindow* parent)
    : QWindow(parent)
    , updatePending(false)
    , animating(false)
    , ogreRoot(NULL)
    , ogreWindow(NULL)
    , ogreCamera(NULL)
    , cameraManager(NULL)
{
    setAnimating(true);
    installEventFilter(this);
    ogreBackground = Ogre::ColourValue(0.0f, 0.5f, 1.0f);
}

/*
 * Upon destuction of the QWindow object we destroy the Ogre3D scene.
 */
QTOgreWindow::~QTOgreWindow()
{
    if (cameraManager)
    {
        delete cameraManager;
    }
    delete ogreRoot;
}

/*
 * In case any drawing surface backing stores (QRasterWindow or QOpenGLWindow) of Qt are supplied to this
 * class in any ay we inform Qt that they will be unused.
 */
void QTOgreWindow::render(QPainter *painter)
{
    Q_UNUSED(painter);
}

/*
 * Our initilisation function. Called by our renderNow() function once when the window is first exposed.
 */
void QTOgreWindow::initialize()
{
    // As shown Ogre3D is initialised normally; just like in other documentation.
#ifdef _MSC_VER
    ogreRoot = new Ogre::Root(Ogre::String("plugins" OGRE_BUILD_SUFFIX ".cfg"));
#else
    ogreRoot = new Ogre::Root(Ogre::String("plugins.cfg"));
#endif
    Ogre::ConfigFile ogreConfig;

    // Todo: load resources.

    const Ogre::RenderSystemList& rsList = ogreRoot->getAvailableRenderers();
    Ogre::RenderSystem* rs = rsList[0];

    // This list sets up the search order for used render system.
    Ogre::StringVector renderOrder;
#if defined(Q_OS_WIN)
    renderOrder.push_back("Direct3D9");
    renderOrder.push_back("Direct3D11");
#endif
    renderOrder.push_back("OpenGL");
    renderOrder.push_back("OpenGL 3+");
    for (Ogre::StringVector::iterator iter = renderOrder.begin(); iter != renderOrder.end(); iter++)
    {
        for (Ogre::RenderSystemList::const_iterator it = rsList.begin(); it != rsList.end(); it++)
        {
            if ((*it)->getName().find(*iter) != Ogre::String::npos)
            {
                rs = *it;
                break;
            }
        }
        if (rs != NULL)
        {
            break;
        }
    }
    if (rs == NULL)
    {
        if (ogreRoot->restoreConfig())
        {
            if (!ogreRoot->showConfigDialog())
            {
                OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
                            "Abort render system configuration",
                            "QTOgreWindow::initialise");
            }
        }
    }
    
    // Setting size and VSync on windows will solve a lot of problems
    QString dimensions = QString("%1 x %2").arg(this->width()).arg(this->height());
    rs->setConfigOption("Video Mode", dimensions.toStdString());
    rs->setConfigOption("Full Screen", "No");
    rs->setConfigOption("VSync", "Yes");
    ogreRoot->setRenderSystem(rs);
    ogreRoot->initialise(false);
    
    Ogre::NameValuePairList parameters;
    // Flag within the parameters set so that Ogre3D initialises an OpenGL context on its own.
    if (rs->getName().find("GL") <= rs->getName().size())
    {
        parameters["currentGLContext"] = Ogre::String("false");
    }
    
    /*
     * We need to supply the low level OS window handle to this QWindow so that Ogre3D knows where to draw
     * the scene. Below is a cross-platform method on how to do this.
     * If you set both options (externalWindowHandle and parentWindowHandle) this code will work with OpenGL
     * and DirectX.
     */
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    parameters["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)this->winId());
    parameters["parentWindowHandle"] = Ogre::StringConverter::toString((size_t)this->winId());
#else
    parameters["externalWindowHandle"] = Ogre::StringConverter::toString((unsigned long)this->winId());
    parameters["parentWindowHandle"] = Ogre::StringConverter::toString((unsigned long)this->winId());
#endif

#if defined(Q_OS_MAC)
    parameters["macAPI"] = "cocoa";
    parameters["macAPICocoaUseNSView"] = true;
#endif

    /*
     * Note that below we supply the creation function for the Ogre3D window the width and height
     * from the current QWindow object using the "this" pointer.
     */
    ogreWindow = ogreRoot->createRenderWindow("QT Window",
                                              this->width(),
                                              this->height(),
                                              false,
                                              &parameters);
    ogreWindow->setVisible(true);

    /*
     * The rest of the code in the initialisation function is standard Ogre3D scene code. Consult other
     * tutorials for specifics.
     */
#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
    const size_t numThreads = std::max<int>(1, Ogre::PlatformInformation::getNumLogicalCores());
    Ogre::InstancingThreadedCullingMethod threadedCullingMethod = Ogre::INSTANCING_CULLING_SINGLETHREAD;
    if (numThreads > 1)
    {
        threadedCullingMethod = Ogre::INSTANCING_CULLING_THREADED;
    }
    ogreSceneManager = ogreRoot->createSceneManager(Ogre::ST_GENERIC, numThreads, threadedCullingMethod);
#endif
    ogreSceneManager = ogreRoot->createSceneManager(Ogre::ST_GENERIC);

    ogreCamera = ogreSceneManager->createCamera("MainCamera");
    ogreCamera->setPosition(Ogre::Vector3(0.0f, 0.0f, 10.0f));
    ogreCamera->lookAt(Ogre::Vector3(0.0f, 0.0f, -300.0f));
    ogreCamera->setNearClipDistance(0.1f);
    ogreCamera->setFarClipDistance(200.0f);
    cameraManager = new OgreQtBites::SdkQtCameraMan(ogreCamera); // create a default camera controller

#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
    createCompositor();
#else
    Ogre::Viewport* viewPort = ogreWindow->addViewport(ogreCamera);
    viewPort->setBackgroundColour(ogreBackground);
#endif

    ogreCamera->setAspectRatio(
                Ogre::Real(ogreWindow->getWidth()) / Ogre::Real(ogreWindow->getHeight()));
    ogreCamera->setAutoAspectRatio(true);

    Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

    createScene();

    ogreRoot->addFrameListener(this);
}

void QTOgreWindow::createScene()
{
    /*
     * Exampe scene
     * Derive this class for your own purpose and overwrite this function to have a working Ogre widget with
     * your own content.
     */
    ogreSceneManager->setAmbientLight(Ogre::ColourValue(0.5f, 0.5f, 0.5f));

#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
    Ogre::Entity* sphereMesh = ogreSceneManager->createEntity(Ogre::SceneManager::PT_SPHERE);
#else
    Ogre::Entity* sphereMesh = ogreSceneManager->createEntity("mySphere", Ogre::SceneManager::PT_SPHERE);
#endif
    
    Ogre::SceneNode* childSceneNode = ogreSceneManager->getRootSceneNode()->createChildSceneNode();
    
    childSceneNode->attachObject(sphereMesh);
    
    Ogre::MaterialPtr sphereMaterial = Ogre::MaterialManager::getSingleton().create("SphereMaterial", 
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
    
    sphereMaterial->getTechnique(0)->getPass(0)->setAmbient(0.1f, 0.1f, 0.1f);
    sphereMaterial->getTechnique(0)->getPass(0)->setDiffuse(0.2f, 0.2f, 0.2f, 1.0f);
    sphereMaterial->getTechnique(0)->getPass(0)->setSpecular(0.9f, 0.9f, 0.9f, 1.0f);
    //sphereMaterial->setAmbient(0.2f, 0.2f, 0.5f);
    //sphereMaterial->setSelfIllumination(0.2f, 0.2f, 0.1f);

    sphereMesh->setMaterialName("SphereMaterial");
    childSceneNode->setPosition(Ogre::Vector3(0.0f, 0.0f, 0.0f));
    childSceneNode->setScale(Ogre::Vector3(0.01f, 0.01f, 0.01f));

#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
    Ogre::SceneNode* lightNode = ogreSceneManager->getRootSceneNode()->createChildSceneNode();
    Ogre::Light* light = ogreSceneManager->createLight();
    lightNode->attachObject(light);
    lightNode->setPosition(20.0f, 80.0f, 50.0f);
#else
    Ogre::Light* light = ogreSceneManager->createLight("MainLight");
    light->setPosition(20.0f, 80.0f, 50.0f);
#endif
}

#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
void QTOgreWindow::createCompositor()
{
    /*
    Example compositor
    Derive this class for your own purpose and overwite this function to have a working Ogre
    widget with your own compositor.
    */
    Ogre::CompositorManager2* compMan = m_ogreRoot->getCompositorManager2();
    const Ogre::String workspaceName = "default scene workspace";
    const Ogre::IdString workspaceNameHash = workspaceName;
    compMan->createBasicWorkspaceDef(workspaceName, m_ogreBackground);
    compMan->addWorkspace(m_ogreSceneMgr, m_ogreWindow, m_ogreCamera, workspaceNameHash, true);
}
#endif

void QTOgreWindow::render()
{
    /*
     * This ties Ogre3D's render function to QWindow's render function. Note that we on't call
     * this function directly; rather we use the renderNow() function to call this method as
     * we don't want to render the Ogre3D scene unless everything is set up first.
     */
    Ogre::WindowEventUtilities::messagePump();
    ogreRoot->renderOneFrame();
}

void QTOgreWindow::renderLater()
{
    /*
     * This function forces QWindow to keep rendering. Omitting this causes the renderNow() function
     * to only get called when the window is resized, moved, etc. as opposed to all of the time, which
     * is generally what we need.
     */
    if (updatePending)
    {
        return;
    }
    updatePending = true;
    QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
}

bool QTOgreWindow::event(QEvent *event)
{
    /*
     * QWindow's "message pump". The vase method that handles all QWindow events. AS you will see there
     * are other methods that actually process the keyboard and mosue events of Qt and the underlying OS.
     *
     * Note that we call the renderNow() function which checks to see if everything is initialised, etc,
     * before calling the render() function.
     */
    switch (event->type())
    {
    case QEvent::UpdateRequest:
        updatePending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

/*
 * Called after the QWindow is reopened or when the QWindow is first opened.
 */
void QTOgreWindow::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);

    if (isExposed())
    {
        renderNow();
    }
}

/*
 * The renderNow() function calls the initialize() function when needed and if the QWindow is already
 * initialised and prepped calls the render() method.
 */
void QTOgreWindow::renderNow()
{
    if (!isExposed())
    {
        return;
    }

    if (ogreRoot == NULL)
    {
        initialize();
    }

    render();

    if (animating)
    {
        renderLater();
    }
}

/*
 * Our event filter - handles the resizing of the QWindow. When the size of the QWindow changes, note the
 * call to the ogre3D window and camera. This keeps the Ogre3D scene looking correct.
 */
bool QTOgreWindow::eventFilter(QObject *target, QEvent *event)
{
    if (target == this && event->type() == QEvent::Resize && isExposed() && ogreWindow != NULL)
    {
        ogreWindow->resize(this->width(), this->height());
    }

    return false;
}

/*
 * How we handle keyboard and mouse events.
 */
void QTOgreWindow::keyPressEvent(QKeyEvent *ev)
{
    if (cameraManager)
    {
        cameraManager->injectKeyDown(*ev);
    }
}

void QTOgreWindow::keyReleaseEvent(QKeyEvent *ev)
{
    if (cameraManager)
    {
        cameraManager->injectKeyUp(*ev);
    }
}

void QTOgreWindow::mouseMoveEvent(QMouseEvent *e)
{
    static int lastX = e->x();
    static int lastY = e->y();
    int relX = e->x() - lastX;
    int relY = e->y() - lastY;
    lastX = e->x();
    lastY = e->y();

    if (cameraManager && (e->buttons() * Qt::LeftButton))
    {
        cameraManager->injectMouseMove(relX, relY);
    }
}

void QTOgreWindow::wheelEvent(QWheelEvent *e)
{
    if(cameraManager)
        cameraManager->injectWheelMove(*e);
}

void QTOgreWindow::mousePressEvent( QMouseEvent* e )
{
    if(cameraManager)
        cameraManager->injectMouseDown(*e);
}

void QTOgreWindow::mouseReleaseEvent( QMouseEvent* e )
{
    if(cameraManager)
        cameraManager->injectMouseUp(*e);

    QPoint pos = e->pos();
    Ogre::Ray mouseRay = ogreCamera->getCameraToViewportRay(
        (Ogre::Real)pos.x() / ogreWindow->getWidth(),
        (Ogre::Real)pos.y() / ogreWindow->getHeight()
    );
    Ogre::RaySceneQuery* sceneQuery = ogreSceneManager->createRayQuery(mouseRay);
    sceneQuery->setSortByDistance(true);
    Ogre::RaySceneQueryResult result = sceneQuery->execute();
    for (size_t ui = 0; ui < result.size(); ui++)
    {
        if (result[ui].movable)
        {
            if (result[ui].movable->getMovableType().compare("Entity") == 0)
            {
               emit entitySelected((Ogre::Entity*)result[ui].movable);
            }
        }
    }
    ogreSceneManager->destroyQuery(sceneQuery);
}

/*
Function to keep track of when we should and shouldn't redraw the window; we wouldn't want to do
rendering when the QWindow is minimized. This takes care of those scenarios.
*/
void QTOgreWindow::setAnimating(bool animating)
{
    this->animating = animating;

    if (animating)
        renderLater();
}

bool QTOgreWindow::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
    cameraManager->frameRenderingQueued(evt);
    return true;
}

void QTOgreWindow::log(Ogre::String msg)
{
    if(Ogre::LogManager::getSingletonPtr() != NULL) Ogre::LogManager::getSingletonPtr()->logMessage(msg);
}

void QTOgreWindow::log(QString msg)
{
    log(Ogre::String(msg.toStdString().c_str()));
}
