
----> work in progress - please don't spread


  REALTIME RADIOSITY VIEWER
  -------------------------

  Abstract
  --------

  We present novel approach to integration of global illumination solver
  into arbitrary realtime local illumination rendering systems.
  Our approach makes no assumptions about local illumination model
  and extends it to global illumination while not slowing it down.
  Our algorithm uses local illumination renderer to output specific images,
  those are analyzed and complete input information for global illumination
  renderer is automatically extracted.
  An example is presented in form of realtime radiosity viewer.


  Introduction
  ------------

  Incorporating global illumination is important step towards realism 
  in computer graphics.
  There are many areas where graphics realism is high priority.
  Yet global illumination is still not as widespread as we could expect.

  Closer look reveals possible cause,
  there remain serious usability problems related to typical global illumination
  software, mainly
  - noninteractive nature (set parameters, wait considerable amount of 
    time, see the result)
  - need to set up many parameters (light and material properties)
    Global illumination solver inputs are typically incompatible with data available
    to local illumination renderer, so user is required to enter missing data.
    Even when automatic conversion of data is possible, it must be updated each time
    local illumination model changes.

  Problem of interactivity has been thoroughly studied

   [1] Instant radiosity, 1997
   - fully dynamic and realtime, slower rendering, but may be cached in lightmaps
   (with our implementation on nVidia GeForce 6600, 30000 triangle scene with 1000
    light samples was rendered at 3fps, including generation of 1000 shadowmaps)

   [2] Precomputed Local Radiance Transfer for Real-Time Lighting Design, 2005
   - only for static scene, expensive precomputations

  Problem of incompatibilities between global illumination solver
  and arbitrary local illumination renderer and ways to smooth integration
  probably haven't been studied.

  We present novel approach to integration of global illumination solver
  into arbitrary realtime local illumination rendering systems.
  Our approach makes no assumptions about local illumination model
  and extends it to global illumination while not slowing it down.
  So when applied to arbitrary realtime local illumination renderer,
  it contributes also to domain of interactive radiosity.

  Our process uses local illumination renderer to output specific images,
  those are analyzed and complete input information for global illumination
  renderer is automatically extracted.

  An example is presented in form of realtime radiosity viewer.


  Realtime radiosity integration
  ------------------------------

  (to be written)


  Realtime radiosity viewer
  -------------------------

  Our building stones were:

  - Local illumination renderer with soft shadows from [3]. We modified it to support
    any OpenGL 2.0 compliant graphics card and to load .3ds files with diffuse surfaces.
    Local illumination model can be arbitrarily changed via GLSL shaders.

  - Radiosity calculator with progressive refinement from [4].
    We improved it using novel acceleration structure (to be published later)
    and converted it into library.

  Complete integration of our radiosity calculator library into renderer took less than 
  two days.
  For comparison, the same integration process with very complex realtime renderer 
  for next generation games took approx. three weeks.

  Viewer may freely manipulate camera and light positions in scene.
  Global illumination quality is temporarily decreased after each light manipulation.
  Currently there is no possibility to manipulate scene, but it may be added in
  future version, since it would make only small performance loss.

  Light in viewer has no distance attenuation, but anyone can add
  it by modifying GLSL shaders. Changes of local illumination model are 
  completely transparent and don't require recompilation.

  Download Windows binary.
  OpenGL 1.5 + GLSL capable graphics card required, tested on nVidia GeForce 6600.
  May work with other .3ds scenes, but it wasn't tested.

  Thanks to 
  - Petr Stastny for koupelna3 scene
  - Marko Dabrovic for Sponza atrium scene
  - Matthew Fairfax for .3ds loader


  Conclusion
  ----------

  We have presented novel approach to integration of global illumination calculator
  into arbitrary local illumination renderer without common usability problems.
  In our sample, which is provided, global illumination was transparently added into 
  realtime renderer with diffuse surfaces and soft shadows in less than two days.
  Global illumination is correctly calculated without need to recompile even when 
  local illumination model is arbitrarily changed via GLSL shaders.
  Key factor is that all input data for global illumination calculator are extracted
  from images generated by local illumination renderer.


  References
  ----------
   [1] Keller,A.: Instant radiosity, 1997
   [2] Kristensen,A.W.: Precomputed Local Radiance Transfer for Real-Time Lighting Design, 2005
   [3] Hrbek,S.: Fast correct soft shadows, 2004, http://dee.cz/fcss
   [4] Hrbek,S.: Radiosity in dynamic scenes, 2000, http://dee.cz/rr/rr.pdf (Czech only)


  Author
  ------

  Stepan Hrbek <dee@dee.cz> http://dee.cz
  January 2006
  
