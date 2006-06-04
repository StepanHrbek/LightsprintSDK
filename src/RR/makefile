########################### REALTIME RADIOSITY ###########################
#
# features

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
CFLAGS = -Wall -I. -I../../include -I../RRCollider/include -I../RRVision/include -I../LicGen/include -I../World/include -I../../3rd/mgflib -DSUPPORT_INTERPOL -DRR_STATIC
LIBS = -lz -lm -lmgf -L../../lib -lRRCollider -lRRVision -lWorld -lstdc++
LIBSALLEG = -lalleg_s -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lole32 -ldinput -lddraw -ldxguid -lwinmm -ldsound
MAKE = makefile
BIN = ../../bin/

OBJS = render.o   misc.o   ldmgf.o   world2rrvision.o   world2rrintersect.o   surface.o   geometry.o   rr.o
CPPS = render.cpp misc.cpp ldmgf.cpp world2rrvision.cpp world2rrintersect.cpp surface.cpp geometry.cpp rr.cpp
HHHS = render.h   misc.h   ldmgf.h   world2rrvision.h   world2rrintersect.h   surface.h   geometry.h

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

ifdef PROFILE
 CFLAGS += -fprofile-generate
 LIBS += -fprofile-generate
else
 LIBS += -s
endif

ifdef DEBUG
 CFLAGS += -O0
else
 CFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
 CFLAGS += -ffast-math
 ifndef PROFILE
  CFLAGS += -fomit-frame-pointer
  # -fprofile-use
 endif
 CFLAGS += -march=athlon-xp -msse
 # CFLAGS += -mfpmath=sse
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

$(BIN)rr.exe: $(OBJS) $(MAKE) ../../lib/libRRCollider.a ../../lib/libRRVision.a
	gcc $(OBJS) $(LIBS) -o $(BIN)rr.exe

rr.o: rr.cpp $(HHHS) $(MAKE)
	gcc $(CFLAGS) -c rr.cpp -o $@
render.o: render.cpp render.h $(MAKE)
	gcc $(CFLAGS) -c render.cpp -o $@
surface.o: surface.cpp $(HHHS) $(MAKE)
	gcc $(CFLAGS) -c surface.cpp -o $@
geometry.o: geometry.cpp $(HHHS) $(MAKE)
	gcc $(CFLAGS) -c geometry.cpp -o $@
misc.o: misc.cpp misc.h $(MAKE)
	gcc $(CFLAGS) -c misc.cpp -o $@
raster.o: raster.cpp raster.h $(MAKE)
	gcc $(CFLAGS) -c raster.cpp -o $@
video.o: video.cpp video.h $(MAKE)
	gcc $(CFLAGS) -c video.cpp -o $@
rastergl.o: rastergl.cpp rastergl.h $(MAKE)
	gcc $(CFLAGS) -c rastergl.cpp -o $@
videogl.o: videogl.cpp videogl.h $(MAKE)
	gcc $(CFLAGS) -c videogl.cpp -o $@
subglut.o: subglut.cpp subglut.h $(MAKE)
	gcc $(CFLAGS) -c subglut.cpp -o $@
ldmgf.o: ldmgf.cpp ldmgf.h $(MAKE)
	gcc $(CFLAGS) -c ldmgf.cpp -o $@
world2rrvision.o: world2rrvision.cpp world2rrvision.h world2rrintersect.h ldmgf.h surface.h
	gcc $(CFLAGS) -c world2rrvision.cpp -o $@
world2rrintersect.o: world2rrintersect.cpp world2rrintersect.h surface.h
	gcc $(CFLAGS) -c world2rrintersect.cpp -o $@

$(BIN)dotga.exe: dotga.c
	gcc -s dotga.c -o $(BIN)dotga.exe $(LIBSALLEG)
	rem upx $(BIN)dotga.exe

$(BIN)dosmooth.exe: dosmooth.c
	gcc -s dosmooth.c -o $(BIN)dosmooth.exe $(LIBSALLEG)
	rem upx $(BIN)dosmooth.exe

$(BIN)player.exe: player.cpp geometry.cpp core.cpp core.h interpol.cpp interpol.h ldbsp.o spline.o raster.o video.o
	gcc player.cpp -o $(BIN)player.exe ldbsp.o spline.o raster.o video.o

clean:
	$(DEL) *.o
	$(DEL) ..\..\bin\rr.exe
	$(DEL) ..\..\bin\dotga.exe
	$(DEL) ..\..\bin\dosmooth.exe
