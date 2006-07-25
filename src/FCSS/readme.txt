
  REALTIME RADIOSITY VIEWER
  -------------------------

  This scene viewer demonstrates concept described in
  "Realtime radiosity integration", http://dee.cz/rri.

  It allows you to freely manipulate camera and light positions in scene
  and see global illumination effects calculated on background.

  It uses no precomputed data.

  You can adjust shadow quality to higher speed or higher quality,
  see built-in help.


  Controls
  --------

  Run view_koupelna or view_sponza to view specific scene.

  Press 'h' for built-in help.


  Commandline options
  -------------------

  scene.3ds    tries to open specified scene, not tested on other scenes
  -window      runs in window instead of fullscreen
  -noAreaLight starts with basic spotlight instead of area light
  -lazyUpdates updates radiosity less often


  Requirements
  ------------

  OpenGL 2.0 capable graphics card is required.
  Should run on NVIDIA GeForce 6xxx/7xxx, ATI Radeon Xxxx/X1xxx
  with latest drivers.


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


  References
  ----------

  [1] Hrbek,S.: Fast correct soft shadows, 2004, http://dee.cz/fcss
  [2] Hrbek,S.: Radiosity in dynamic scenes, 2000, http://dee.cz/rr/rr.pdf (Czech only)
  [3] Lightsprint Vision, 2006, http://lightsprint.com


  Thanks to 
  ---------

  - Petr Stastny for koupelna scene
  - Marko Dabrovic for Sponza atrium scene
  - Matthew Fairfax for .3ds loader and renderer


  Author
  ------

  Stepan Hrbek <dee@dee.cz> http://dee.cz
  January 2006, updated July 2006
