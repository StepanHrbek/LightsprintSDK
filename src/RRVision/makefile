LIB = ../../lib/libRRVision.a
TMP = #../../tmp/RRVision/

CPPFLAGS = -Iinclude -I../RRCollider/include -I../LicGen/include -DSUPPORT_INTERPOL
CPPFLAGS += -O3 -finline -finline-functions -D__OPTIMIZE__ -DNDEBUG
#CPPFLAGS += -ffast-math // zpusobuje crash v soda
CPPFLAGS += -fomit-frame-pointer
CPPFLAGS += -march=athlon-xp -msse
#CPPFLAGS += -mfpmath=sse
CC = gcc

OBJS = $(TMP)dynamic.o $(TMP)geometry.o $(TMP)interpol.o $(TMP)intersections.o $(TMP)rrcore.o $(TMP)RRObjectImporter.o $(TMP)RRScaler.o $(TMP)RRScene.o $(TMP)RRVision.o

all: makefile $(LIB)

$(LIB): makefile $(OBJS)
	ar rc $(LIB) $(OBJS)
	ranlib $(LIB)

$(TMP)%.o: %.cpp makefile

clean:
	del $(TMP)*.o
	del ..\..\lib\libRRVision.a

