
   LIGHTSMARK 2015                                           v3.0, 2015-0x-xx
   __________________________________________________________________________

   Lightsmark is free open source multiplatform GPU benchmark.

   Details and updates: http://dee.cz/lightsmark

1. Features
   - realtime global illumination
   - realtime penumbra shadows
   - realtime color bleeding
   - infinite light bounces
   - fully dynamic HDR lighting
   - 220000 triangles in scene
   - 1000 fps on mainstream hw
   - interactive (press F1 while benchmarking)

2. Requirements
   - Windows (32bit or 64bit) or Linux (32bit or 64bit) or OSX (64bit)
   - OpenGL 2.0 compliant GPU with at least 32 MB RAM
     * AMD/ATI GPUs since Radeon 9500 released in 2002
     * NVIDIA GPUs since GeForce FX released in 2003
     * Intel GPUs since GMA 4500 released in 2008
     * use the most recent drivers
   - CPU: x86/x64 with SSE
   - RAM: at least 512 MB
   - free disk space: 100 MB

3. What's new in Lightsmark 2015
   - smoother framerate, because GI updates in every frame,
     rather than just 20x per second
   - OSX support (was Windows+Linux only)
   - new integrated scene editor

4. Platforms

   Multiplatform package contains binaries for all platforms.
   - Windows 32bit
     run Lightsmark.exe in bin/win32
   - Windows 64bit
     run Lightsmark.exe in bin/win32
     to use native 64bit code, click image in dialog window, check 64bit
   - Linux 32bit
     run backend in bin/pc-linux32
   - Linux 64bit
     run backend in bin/pc-linux64
   - OSX
     run backend in bin/pc-osx

   Linux notes
   - First of all, run bin/install_dependencies.sh
   - To see list of options, run backend ?
   - Score is printed to console and returned as a result code.

   Vista/7 notes
   - If you use installer, screenshots can't be saved to data directory,
     they go to %LOCALAPPDATA%\Lightsmark 2015

5. Credits
   Created by Stepan Hrbek using
   - Lightsprint SDK realtime global illumination engine
   - Attic scene from World of Padman game
   - Hey you song by In-sist under http://creativecommons.org/licenses/by-nc/2.5/
   - I Robot model by orillionbeta
   - GLUT, GLEW, FreeImage, libav or FFmpeg, PortAudio
   - code fragments by Nicolas Baudrey, Matthew Fairfax, Daniel Sykora

6. License
   http://creativecommons.org/licenses/by-nc/3.0/
   Exception: Files from World of Padman under original game's license.

   What does it mean: you can use in product reviews/comparisons,
   modify source code e.g. to send results to your web,
   release new versions etc.

7. Contact
   Stepan Hrbek <dee@dee.cz>
   http://dee.cz/lightsmark
