
   LIGHTSMARK 2011                                           v3.0, 2011-08-xx
   __________________________________________________________________________

   Lightsmark is multiplatform benchmark based on next generation 3D engine.

   Details and updates: http://dee.cz/lightsmark

1. Features
   - realtime global illumination
   - realtime penumbra shadows
   - realtime color bleeding
   - infinite light bounces
   - fully dynamic HDR lighting
   - 220000 triangles in scene
   - 300 fps on mainstream hw
   - interactive (press F1 while benchmarking)

2. Requirements
   - Windows (32bit or 64bit) or Linux (32bit or 64bit) or OSX (64bit)
   - OpenGL 2.0 compliant GPU with at least 32 MB RAM
     * NVIDIA GPUs since GeForce FX released in 2003
     * AMD/ATI GPUs since Radeon 9500 released in 2002
     * Intel graphics not tested yet
     * S3 graphics not tested yet
     * use the most recent drivers
   - CPU: x86/x64 with SSE
   - RAM: at least 512 MB
   - free disk space: 100 MB

3. What's new in Lightsmark 2011
   - faster engine (up to 2x higher fps, so scores differ from 2008)
   - better image quality (per-pixel indirect shadows and color bleeding)
   - Mac support (was Windows+Linux only)
   - new integrated scene editor

   For reference, what was new in Lightsmark 2008
   - faster engine (up to 3x higher fps, so scores differ from 2007)
   - better image quality (per-pixel indirect shadows and color bleeding)
   - Linux support (was Windows only)
   - native 64bit support (was 32bit only)
   - all GPUs use the same render path (in 2007 they didn't due to driver bug)
   - minor fixes (sun's shadow, robot's back faces)

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
   - Score is printed to console. 10*score is returned as a result code.

   Vista/7 notes
   - If you use installer, screenshots can't be saved to data directory,
     they go to %LOCALAPPDATA%\Lightsmark 2011

5. Credits
   Created by Stepan Hrbek using
   - Lightsprint SDK realtime global illumination engine
   - Attic scene from World of Padman game
   - Hey you song by In-sist under http://creativecommons.org/licenses/by-nc/2.5/
   - I Robot model by orillionbeta
   - libraries in benchmark: GLUT, GLEW, FreeImage, FMOD
   - libraries in editor: wxWidgets, Assimp, OpenCollada
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
