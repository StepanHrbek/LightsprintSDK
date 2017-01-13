#/bin/sh

# Run this as root
# to install Lightsprint SDK development dependencies in Debian (most likely works also in Ubuntu)
# In case that you need only binary dependencies, try removing all -dev.

# for build system
apt install g++ make

# for LightsprintCore
apt install libboost-dev libboost-filesystem-dev libboost-regex-dev

# for LightsprintGL
apt install libglew-dev

# for LightsprintIO
apt install libboost-serialization-dev libboost-iostreams-dev libboost-locale-dev libboost-thread-dev libfreeimage-dev libassimp-dev portaudio19-dev libavdevice-dev libavfilter-dev

# for LightsprintEd
apt install libwxgtk3.0-dev

# for samples
apt install freeglut3-dev
