#ifndef QTOGREWINDOW_HPP
#define QTOGREWINDOW_HPP

/*
 * Qt headers
 */
#include <QtWidgets/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>

/*
 * Ogre3d header
 */
#include <Ogre.h>

class QTOgreWindow : public QWindow, public Ogre::FrameListener
{
public:
    QTOgreWindow();
};

#endif // QTOGREWINDOW_HPP
