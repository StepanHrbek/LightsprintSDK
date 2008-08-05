#/bin/sh

echo "This script attempts to install freeimage, glew and glut libraries."
echo "If it fails and you can fix it, please send me updated version back."


##############
### FEDORA ###
##############

echo "1) yum might work on RedHat based systems (tested in Fedora 9)"
yum install freeimage glew freeglut


##############
### UBUNTU ###
##############

echo "2) apt-get might work on Debian based systems (tested in Ubuntu 8.04)"
sudo apt-get install libfreeimage3 libglew1.5 libglut3


############
### SUSE ###
############

# contributed by David
echo "3) zypper works in Suse"
su
zypper in glew
zypper ar http://download.opensuse.org/repositories/games/openSUSE_11.0/games.repo
zypper in libfreeimage3
# You can remove the repo later with "zypper rr games" if you want.
# In my case glut was already installed.
# Alternatively you can open a browser and go to "http://software.opensuse.org/search", type the library to install (glew, freeimage, etc), and click on the "1-Click install" icon.
