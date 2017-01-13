#/bin/sh

# Install Lightsprint SDK development dependencies in Debian (most likely works also in Ubuntu)
# In case that you need only binary dependencies, try removing all -dev.

# for build system
sudo apt-get install g++ make

# for LightsprintCore
sudo apt-get install libboost-dev libboost-filesystem-dev libboost-regex-dev

# for LightsprintGL
sudo apt-get install libglew-dev

# for LightsprintIO
sudo apt-get install libboost-serialization-dev libboost-iostreams-dev libboost-locale-dev libboost-thread-dev libfreeimage-dev libassimp-dev portaudio19-dev libavdevice-dev libavfilter-dev

# for LightsprintEd
sudo apt-get install libwxgtk3.0-dev

# for samples
sudo apt-get install freeglut3-dev
