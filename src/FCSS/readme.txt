
  REALTIME RADIOSITY VIEWER
  -------------------------

  This .3ds scene viewer demonstrates concept described in
  "Realtime Radiosity Integration", http://dee.cz/rri.

  It allows you to freely manipulate camera and light positions in scene
  and see global illumination effects calculated on background.

  It uses no precomputed data.

  You can adjust shadow quality to higher speed or higher quality,
  see built-in help.


  Controls
  --------

  Run view_koupelna, view_sponza or view_sibenik to view specific scene.

  Press 'h' for built-in help.


  Commandline options
  -------------------

  scene.3ds    tries to open specified scene
  -window      runs in window instead of fullscreen
  -noAreaLight starts with basic spotlight instead of area light
  -lazyUpdates updates radiosity less often


  Requirements
  ------------

  OpenGL 2.0 capable graphics card is required.
  Should run on NVIDIA GeForce 6xxx/7xxx, ATI Radeon 95xx+/Xxxx/X1xxx
  with latest drivers.

  CPU with SSE instructions is required.
  (from Intel: Pentium III or newer, from AMD: Athlon XP or newer)

  Windows OS is required.
  Tested only on Win XP and Win XP 64bit.


  Notes
  -----

  Building stones for this viewer were:

  - Local illumination renderer with soft shadows from [1]. We modified it to support
    any OpenGL 2.0 compliant graphics card and to load .3ds files with diffuse surfaces.
    Local illumination model can be arbitrarily changed via GLSL shaders.

  - Radiosity calculator with progressive refinement from [2].
    We improved it using new acceleration structure (to be published later)
    and made available as library [3].

  Complete integration of our radiosity calculator library into renderer took less than 
  two days.
  For comparison, the same integration process with very complex and very feature-rich
  realtime renderer for next generation games took approx. two weeks.

  Light in viewer has no distance attenuation, but anyone can add
  it by modifying GLSL shaders. Changes of local illumination model are 
  completely transparent and don't require recompilation.

  Currently there is no possibility to manipulate objects in scene, 
  but it may be added in future version, since it would make only small 
  performance loss.

  If you see wrong illumination in Sibenik scene, it is caused by wrong normals
  in original scene, not by radiosity solver.


  References
  ----------

  [1] Hrbek,S.: Fast correct soft shadows, 2004, http://dee.cz/fcss
  [2] Hrbek,S.: Radiosity in dynamic scenes, 2000, http://dee.cz/rr/rr.pdf (Czech only)
  [3] Lightsprint Vision, 2006, http://lightsprint.com


  Credits & thanks
  ----------------

  - Petr Stastny - koupelna scenes
  - Amethyst7 - starfield map
  - Marko Dabrovic - Sponza atrium scene
  - Matthew Fairfax - .3ds scene loader
  - Nicolas Baudrey - .bsp scene loader
  - FreeImage - image loaders


  Author
  ------

  Stepan Hrbek <dee@dee.cz> http://dee.cz
  January 2006, updated July 2006
