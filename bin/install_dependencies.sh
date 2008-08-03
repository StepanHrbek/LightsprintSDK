#/bin/sh

echo "This script attempts to install freeimage, glew and glut libraries."
echo "If it fails and you can fix it, please send me updated version back."
echo "1) yum might work on RedHat based systems (tested in Fedora 9)"
yum install freeimage glew freeglut

echo "2) apt-get might work on Debian based systems (tested in Ubuntu 8.04)"
sudo apt-get install libfreeimage3 libglew1.5 libglut3
