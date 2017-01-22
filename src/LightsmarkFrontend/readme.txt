
   LIGHTSMARK 2017                                           v3.0, 2017-0x-xx
_____________________________________________________________________________

   Lightsmark is a free GPU benchmark.

   This is anniversary release. It does not change contents, so it is kind of
   retro by now, but it evolves to be better benchmark - more platforms and
   more fair scores.

   Details and updates: http://dee.cz/lightsmark


History
_______

   2017
   - update global illumination in every frame (lower fps,
     but smoother framerate and more fair scores)
   - reduce importance of CPU, thus making it GPU benchmark
   - increase default res to 1920x1080
   - add OSX version

   2008
   - add Linux version
   - add 64bit version

   2007
   - first release

Requirements
____________

   - CPU: x86/x64 with SSE
   - GPU: anything that supports OpenGL 2.0
   - OS : Windows, Linux or OSX
   - RAM: at least 512 MB

Linux+OSX notes
_______________

   - So far no installer, no frontend, only backend.
   - First of all, see bin/install_dependencies.sh
   - To see list of options, run bin/<os>/backend ?
   - Score is printed to console and returned as a result code.

Credits
_______

   Created by Stepan Hrbek using
   - Lightsprint SDK realtime global illumination engine
   - Attic scene from World of Padman game
   - Hey you song by In-sist under http://creativecommons.org/licenses/by-nc/2.5/
   - I Robot model by orillionbeta
   - GLUT, GLEW, FreeImage, libav, PortAudio
   - code fragments by Nicolas Baudrey, Matthew Fairfax, Daniel Sykora

License
_______

   http://creativecommons.org/licenses/by-nc/4.0/
   Exception: Files from World of Padman under original game's license.

Contact
_______

   Stepan Hrbek <dee@dee.cz>
   http://dee.cz/lightsmark
