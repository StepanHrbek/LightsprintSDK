# makefile pro prehravani .bsp+.mgf


errors:

 ifndef NAME
	echo usage: make -f d.mak name=jidelna [dir=bsp] [lights=1] [frames=20] [seconds=10] [vertices=10000] [loop=1] [smooth=3] [jpg=60] [play]
 # jak rict ze tady chceme skoncit? jak v dosu zavolat false?
 endif


# aby sly parametry zadavat stylem "f=20" "s=5"
# jak je sem procpat bez uvozovek zatim nevim

 ifdef d
  DIR=$(d)
 endif
 ifdef n
  NAME=$(n)
 endif
 ifdef li
  LIGHTS=$(li)
 endif
 ifdef f
  FRAMES=$(f)
 endif
 ifdef s
  SECONDS=$(s)
 endif
 ifdef v
  VERTICES=$(v)
 endif
 ifdef sm
  SMOOTH=$(sm)
 endif
 ifdef lo
  LOOP=$(lo)
 endif
 ifdef j
  JPG=$(j)
 endif


rollback:

 ifdef SECONDS
	if exist $(DIR)\$(NAME).r.000 del $(DIR)\$(NAME).r.000
 endif
 ifdef VERTICES
	if exist $(DIR)\$(NAME).mes del $(DIR)\$(NAME).mes
 endif
 ifdef SMOOTH
	if exist $(DIR)\$(NAME).smooth.b.tga del $(DIR)\$(NAME).smooth.b.tga
	if exist $(DIR)\$(NAME).smooth.g.tga del $(DIR)\$(NAME).smooth.g.tga
	if exist $(DIR)\$(NAME).smooth.r.tga del $(DIR)\$(NAME).smooth.r.tga
 endif
 ifdef LOOP
	if exist $(DIR)\$(NAME).smooth.b.tga del $(DIR)\$(NAME).smooth.b.tga
	if exist $(DIR)\$(NAME).smooth.g.tga del $(DIR)\$(NAME).smooth.g.tga
	if exist $(DIR)\$(NAME).smooth.r.tga del $(DIR)\$(NAME).smooth.r.tga
 endif
 ifdef JPG
	if exist $(DIR)\$(NAME).jpg del $(DIR)\$(NAME).jpg
 endif

 ifndef DIR
  DIR=bsp
 endif
 ifndef LIGHTS
  LIGHTS=1
 endif
 ifndef FRAMES
  FRAMES=20
 endif
 ifndef SECONDS
  SECONDS=10
 endif
 ifndef VERTICES
  VERTICES=10000
 endif
 ifndef SMOOTH
  SMOOTH=3
 endif
 ifndef LOOP
  LOOP=1
 endif
 ifndef JPG
  JPG=60
 endif


info:
	echo DIR=$(DIR) NAME=$(NAME) LIGHTS=$(LIGHTS) FRAMES=$(FRAMES) SECONDS=$(SECONDS) VERTICES=$(VERTICES) SMOOTH=$(SMOOTH) LOOP=$(LOOP) JPG=$(JPG)

play: $(DIR)/$(NAME).dat
	call doplay.bat $(DIR) $(NAME) $(LIGHTS)

all: errors info rollback $(DIR)/$(NAME).dat

$(DIR)/$(NAME).dat: $(DIR)/$(NAME).bzp $(DIR)/$(NAME).mez $(DIR)/$(NAME).orz $(DIR)/$(NAME).3dz
	echo $(NAME).bzp >$(DIR)\$(NAME).lst
	echo $(NAME).mez >>$(DIR)\$(NAME).lst
	echo $(NAME).orz >>$(DIR)\$(NAME).lst
	echo $(NAME).3dz >>$(DIR)\$(NAME).lst
	call dodat.bat $(DIR) $(NAME).lst $(NAME).dat

$(DIR)/$(NAME).bzp: $(DIR)/$(NAME).bsp
	dogz $(DIR)/$(NAME).bsp $(DIR)/$(NAME).bzp

$(DIR)/$(NAME).mez: $(DIR)/$(NAME).mes
	dogz $(DIR)/$(NAME).mes $(DIR)/$(NAME).mez

$(DIR)/$(NAME).orz: $(DIR)/$(NAME).ora
	dogz $(DIR)/$(NAME).ora $(DIR)/$(NAME).orz

$(DIR)/$(NAME).3dz: $(DIR)/$(NAME).jpg
	doxor $(DIR)/$(NAME).jpg $(DIR)/$(NAME).3dz
#	copy $(DIR)\$(NAME).jpg $(DIR)\$(NAME).3dz

$(DIR)/$(NAME).ora: $(DIR)/$(NAME).bsp $(DIR)/$(NAME).mes
	rr $(DIR)/$(NAME).bsp -t1 -nogfx

$(DIR)/$(NAME).jpg: $(DIR)/$(NAME).smooth.tga
	dojpg -quality $(JPG) -optimize -outfile $(DIR)/$(NAME).jpg -targa $(DIR)/$(NAME).smooth.tga

$(DIR)/$(NAME).smooth.tga: $(DIR)/$(NAME).smooth.r.tga $(DIR)/$(NAME).smooth.g.tga $(DIR)/$(NAME).smooth.b.tga
	dotga $(DIR)/$(NAME).smooth

$(DIR)/$(NAME).smooth.r.tga: $(DIR)/$(NAME).mes
	dosmooth $(DIR)/$(NAME).r.tga $(DIR)/$(NAME).smooth.r.tga $(SMOOTH) $(LOOP)

$(DIR)/$(NAME).smooth.g.tga: $(DIR)/$(NAME).mes
	dosmooth $(DIR)/$(NAME).g.tga $(DIR)/$(NAME).smooth.g.tga $(SMOOTH) $(LOOP)

$(DIR)/$(NAME).smooth.b.tga: $(DIR)/$(NAME).mes
	dosmooth $(DIR)/$(NAME).b.tga $(DIR)/$(NAME).smooth.b.tga $(SMOOTH) $(LOOP)

ifdef GRAB_SEPARATE

$(DIR)/$(NAME).mes: $(DIR)/$(NAME).r.000
	rr $(DIR)/$(NAME).bsp -f$(FRAMES) -v$(VERTICES) -l$(LIGHTS) -g -nogfx

$(DIR)/$(NAME).r.000: $(DIR)/$(NAME).bsp $(DIR)/$(NAME).mgf
	dograb $(DIR)\$(NAME) $(FRAMES) $(SECONDS) -l$(LIGHTS) -nogfx

else

$(DIR)/$(NAME).mes: $(DIR)/$(NAME).bsp $(DIR)/$(NAME).mgf
	rr $(DIR)/$(NAME).bsp -f$(FRAMES) -s$(SECONDS) -v$(VERTICES) -l$(LIGHTS) -nogfx -g

endif

clean:
	del $(DIR)\$(NAME).dat
	del $(DIR)\$(NAME).lst
	del $(DIR)\$(NAME).bzp
	del $(DIR)\$(NAME).mez
	del $(DIR)\$(NAME).orz
	del $(DIR)\$(NAME).3dz
	del $(DIR)\$(NAME).ora
	del $(DIR)\$(NAME).jpg
	del $(DIR)\$(NAME).smooth.*
	del $(DIR)\$(NAME).mes
	del $(DIR)\$(NAME).b.*
	del $(DIR)\$(NAME).g.*
	del $(DIR)\$(NAME).r.*
	del $(DIR)\$(NAME).w.*
