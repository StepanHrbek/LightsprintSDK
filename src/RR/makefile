########################### REALTIME RADIOSITY ###########################
#
# features

  RRINTERSECT =   # ?

  INTERPOL    = 1  # interpolation support            +20% space  +4% calc
  DYNAMIC     =    # dynamic object suport             +5% space  +5% calc

# optimizations

  DEBUG       =    # asserts, fast compilation                  +100% calc
  PROFILE     =    # generate profiler info                      +20% calc
  PACK        =    # pack structures                   -5% space  +2% calc
  HITS_FIXED  =    # low precision hits, less robust  -30% space  +2% calc
  TEST_SCENE  = 1  # test scene for invalid geometry            +0.2% calc

# output

  OPENGL      =    # opengl instead of sw rasterizer
  LIGHTMAP    =    # output cached via lightmaps

##########################################################################


CC = gcc
CPP = gcc
CFLAGS = -Wall
LIBS = -lz -lm -lmgf -lstdc++
LIBSALLEG = -lalleg_s -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lole32 -ldinput -lddraw -ldxguid -lwinmm -ldsound
MAKE = makefile
BIN = ../../bin/

OBJS = spline.o   ldbsp.o   misc.o   ldmgf.o   surface.o
CPPS = spline.cpp ldbsp.cpp misc.cpp ldmgf.cpp surface.cpp geometry.cpp core.cpp intersections.cpp
HHHS = spline.h   ldbsp.h   misc.h   ldmgf.h   surface.h   geometry.h   core.h   intersections.h

ifdef RRINTERSECT
 LIBS += -L../../lib -lRRIntersect
 OBJS += world2rrengine.o world2rrintersect.o RREngine.o
 CFLAGS += -DUSE_RRINTERSECT -I../../include
endif

ifeq ($(OSTYPE), Linux)    # ...jak poznam ze jsem pod unixem?
 UNIX = yez        # klavesnice pres curses, rm misto del
endif

ifdef UNIX
 DEL = rm -f
 LIBS += -lcurses
 GLLIBS = -L/usr/X11R6/lib -lGL -lGLU -lglut
 GLFLAGS = -I/usr/X11R6/include -DKB_VIA_CURSES
else
 DEL = del
 GLLIBS = -lopengl32 -lglu32 -lglut32
endif

ifdef OPENGL
 LIBS += $(GLLIBS)
 OBJS += videogl.o rastergl.o
 HHHS += videogl.h rastergl.h
 CFLAGS += -DRASTERGL $(GLFLAGS)
else
 ALLEGRO = yez
 OBJS += video.o raster.o subglut.o
 HHHS += video.h raster.h subglut.h
 ALLEGRO = yez
endif

ifdef ALLEGRO
 # s allegrem 3 tady staci LIBS+=-lalleg_s, s allegrem 4 je nutny cely seznam
 LIBS += $(LIBSALLEG) 
 CFLAGS += -DKB_VIA_ALLEGRO -DALLEGRO_STATICLINK
endif

ifdef LIGHTMAP
 CFLAGS += -DSUPPORT_LIGHTMAP
 LIBS += -lpng -lz
endif

ifdef INTERPOL
 CFLAGS += -DSUPPORT_INTERPOL
 FASTOBJS += interpol.o
 CPPS += interpol.cpp
 HHHS += interpol.h
endif

ifdef DYNAMIC
 CFLAGS += -DSUPPORT_DYNAMIC -DTRANSFORM_SHOTS
 FASTOBJS += dynamic.o
 CPPS += dynamic.cpp
 HHHS += dynamic.h
endif

ifdef PROFILE
 CFLAGS += -fprofile-generate
 LIBS += -fprofile-generate
else
 LIBS += -s
endif

ifdef DEBUG
 OBJS += rr.o core.o geometry.o intersections.o $(FASTOBJS)
 CFLAGS += -O1 # -O0 bez CLEAN_FACTORS spatne pocita, -O1 ok, nikdo nevi proc
else
 OBJS += rrbig.o       # zkompiluje vic modulu do jednoho, zrychli rr o 20%
 CFLAGS += -O3 -finline -finline-functions -ffast-math -D__OPTIMIZE__ -DNDEBUG
 CFLAGS += -march=athlon-xp -msse -mfpmath=sse --param max-inline-insns-auto=300
 ifndef PROFILE
  CFLAGS += -fomit-frame-pointer
  # -fprofile-use
 endif
endif

ifdef PACK
 CFLAGS += -DPACK
endif

ifdef HITS_FIXED
 CFLAGS += -DHITS_FIXED
endif

ifdef TEST_SCENE
 CFLAGS += -DTEST_SCENE
endif

all: $(BIN)rr.exe $(BIN)dotga.exe $(BIN)dosmooth.exe

$(BIN)rr.exe: $(OBJS) $(MAKE)
	gcc $(OBJS) $(LIBS) -o $(BIN)rr.exe

rrbig.o: rr.cpp $(HHHS) $(CPPS) $(MAKE)
	gcc -DONE $(CFLAGS) -c rr.cpp -o $@
rr.o: rr.cpp $(HHHS) $(MAKE)
	gcc $(CFLAGS) -c rr.cpp -o $@
dynamic.o: dynamic.cpp dynamic.h core.h geometry.h surface.h misc.h $(MAKE)
	gcc $(CFLAGS) -c dynamic.cpp -o $@
interpol.o: interpol.cpp interpol.h core.h geometry.h surface.h misc.h $(MAKE)
	gcc $(CFLAGS) -c interpol.cpp -o $@
intersections.o: intersections.cpp intersections.h core.h geometry.h surface.h misc.h $(MAKE)
	gcc $(CFLAGS) -c intersections.cpp -o $@
core.o: core.cpp core.h geometry.h surface.h misc.h $(MAKE)
	gcc $(CFLAGS) -c core.cpp -o $@
geometry.o: geometry.cpp geometry.h $(MAKE)
	gcc $(CFLAGS) -c geometry.cpp -o $@
surface.o: surface.cpp surface.h geometry.h $(MAKE)
	gcc $(CFLAGS) -c surface.cpp -o $@
misc.o: misc.cpp misc.h $(MAKE)
	gcc $(CFLAGS) -c misc.cpp -o $@
raster.o: raster.cpp raster.h geometry.h $(MAKE)
	gcc $(CFLAGS) -c raster.cpp -o $@
ldbsp.o: ldbsp.cpp geometry.h spline.h ldbsp.h $(MAKE)
	gcc $(CFLAGS) -c ldbsp.cpp -o $@
spline.o: spline.cpp spline.h $(MAKE)
	gcc $(CFLAGS) -c spline.cpp -o $@
video.o: video.cpp video.h $(MAKE)
	gcc $(CFLAGS) -c video.cpp -o $@
rastergl.o: rastergl.cpp rastergl.h geometry.h $(MAKE)
	gcc $(CFLAGS) -c rastergl.cpp -o $@
videogl.o: videogl.cpp videogl.h $(MAKE)
	gcc $(CFLAGS) -c videogl.cpp -o $@
subglut.o: subglut.cpp subglut.h $(MAKE)
	gcc $(CFLAGS) -c subglut.cpp -o $@
ldmgf.o: ldmgf.cpp ldmgf.h geometry.h $(MAKE)
	gcc $(CFLAGS) -c ldmgf.cpp -o $@
world2rrengine.o: world2rrengine.cpp world2rrengine.h world2rrintersect.h ldmgf.h ldbsp.h surface.h RREngine.h
	gcc $(CFLAGS) -c world2rrengine.cpp -o $@
world2rrintersect.o: world2rrintersect.cpp world2rrintersect.h surface.h ldbsp.h RREngine.h
	gcc $(CFLAGS) -c world2rrintersect.cpp -o $@
RREngine.o: RRENgine.cpp RREngine.h ../../include/RRIntersect.h surface.h core.h geometry.h misc.h
	gcc $(CFLAGS) -c RREngine.cpp -o $@

$(BIN)dotga.exe: dotga.c
	gcc -s dotga.c -o $(BIN)dotga.exe $(LIBSALLEG)
	upx $(BIN)dotga.exe

$(BIN)dosmooth.exe: dosmooth.c
	gcc -s dosmooth.c -o $(BIN)dosmooth.exe $(LIBSALLEG)
	upx $(BIN)dosmooth.exe

$(BIN)player.exe: player.cpp geometry.cpp geometry.h core.cpp core.h interpol.cpp interpol.h ldbsp.o spline.o raster.o video.o
	gcc player.cpp -o $(BIN)player.exe ldbsp.o spline.o raster.o video.o

clean:
	$(DEL) *.o
	$(DEL) *.bak
	$(DEL) *.bkp
	$(DEL) $(BIN)rr.exe
	$(DEL) $(BIN)dotga.exe
	$(DEL) $(BIN)dosmooth.exe
