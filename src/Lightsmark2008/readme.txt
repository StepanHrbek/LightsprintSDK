
   LIGHTSMARK 2008                                          beta1, 2008-07-29
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
   - 300-400 average fps
   - fully moddable, data and source code in install directory

2. Requirements
   - Windows XP/Vista (32bit, 64bit) or Linux (32bit, 64bit)
   - OpenGL 2.0 compliant GPU with at least 32 MB RAM
     * NVIDIA: GeForce 5xxx, 6xxx, 7xxx, 8xxx, 9xxx, 2xx (partially GeForce Go, Quadro)
     * AMD: Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx, HD4xxx (partially Mobility, FireGL)
     * to my knowledge, other vendors don't support OpenGL 2.0 yet
     * use the latest drivers
   - CPU: x86/x64 with SSE
   - RAM: 512 MB
   - free disk space: 100 MB

3. How does it differ from Lightsmark 2007
   - faster engine (up to 2x higher fps -> different scores -> renamed to 2008)
   - better image quality (per-pixel indirect shadows and color bleeding)
   - Linux support (was Windows only)
   - 64bit support (was 32bit only)
   - all GPUs use the same render path (in 2007 they didn't due to driver bug)

4. Platforms

   Multiplatform package contains binaries for 4 platforms.
   - Windows 32bit
     run Lightsmark2008.exe in bin/win32
   - Windows 64bit
     run Lightsmark2008.exe in bin/win32
     to use native 64bit code, click image in dialog window, check 64bit
   - Linux 32bit
     run backend in bin/pc-linux32
   - Linux 64bit
     run backend in bin/pc-linux64

   Linux notes
   - First of all, run bin/install_dependencies
   - To my knowledge, open source graphics drivers don't supports OpenGL 2.0
     properly yet, use the latest proprietary driver.
   - To see list of options, run backend with random argument.
   - Score is printed to console. 10*score is returned as a result code.

5. Credits
   Created by Stepan Hrbek using
   - Lightsprint SDK realtime global illumination engine
   - Attic scene from World of Padman game
   - Hey you song by In-sist under http://creativecommons.org/licenses/by-nc/2.5/
   - I Robot model by orillionbeta
   - GLUT library
   - GLEW library
   - FreeImage library under FIPS license
   - FMOD library
   - bsp loader by Nicolas Baudrey
   - 3ds loader by Matthew Fairfax

6. License
   http://creativecommons.org/licenses/by-nc/3.0/
   Exception: Files from World of Padman under original game's license.
   Use in product reviews/comparisons is welcome.
   When modifying Lightsmark, you must make clear that your work is
   Lightsmark 2008 mod and link to http://dee.cz/lightsmark.

7. Contact
   Stepan Hrbek <dee@dee.cz>
   http://dee.cz/lightsmark
