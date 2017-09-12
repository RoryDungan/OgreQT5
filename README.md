# OgreQT5
*Qt5 Ogre3d integration*

Ogre3D Qt Window based on code from the [Ogre wiki](http://www.ogre3d.org/tikiwiki/tiki-index.php?page=Integrating+Ogre+into+QT5). Tested with Qt 5.5 and Ogre3D 1.9.0.
# Building
## Ubuntu
*Tested on Ubuntu 16.04 with Qt 5.5 and both Ogre 1.9 and 1.10*

First install dependencies:
```
sudo apt-get install libogre-1.9-dev qtbase5-dev qtcreator build-essential
```
Then simply open the project in Qt Creator and build! In order to run correctly, the working directory must be the same directory that `plugins.cfg` is located in.

