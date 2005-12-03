#ifndef __tga_h__
#define __tga_h__

#ifdef __cplusplus
extern "C" {
#endif

/* tga.h - interface for TrueVision (TGA) image file loader */

/* Copyright NVIDIA Corporation, 1999. */

/* A lightweight TGA image file loader for OpenGL programs. */

/* $Id: //sw/main/apps/OpenGL/mjk/shadowcast/tga.h#2 $ */

#include <stdio.h>
#include <GL/glut.h>

typedef struct {

  GLsizei  width;
  GLsizei  height;
  GLint    components;
  GLenum   format;

  GLsizei  cmapEntries;
  GLenum   cmapFormat;
  GLubyte *cmap;

  GLubyte *pixels;
  
} gliGenericImage;

typedef struct {
  unsigned char idLength;
  unsigned char colorMapType;

  /* The image type. */
#define TGA_TYPE_MAPPED 1
#define TGA_TYPE_COLOR 2
#define TGA_TYPE_GRAY 3
#define TGA_TYPE_MAPPED_RLE 9
#define TGA_TYPE_COLOR_RLE 10
#define TGA_TYPE_GRAY_RLE 11
  unsigned char imageType;

  /* Color Map Specification. */
  /* We need to separately specify high and low bytes to avoid endianness
     and alignment problems. */
  unsigned char colorMapIndexLo, colorMapIndexHi;
  unsigned char colorMapLengthLo, colorMapLengthHi;
  unsigned char colorMapSize;

  /* Image Specification. */
  unsigned char xOriginLo, xOriginHi;
  unsigned char yOriginLo, yOriginHi;

  unsigned char widthLo, widthHi;
  unsigned char heightLo, heightHi;

  unsigned char bpp;

  /* Image descriptor.
     3-0: attribute bpp
     4:   left-to-right ordering
     5:   top-to-bottom ordering
     7-6: zero
     */
#define TGA_DESC_ABITS 0x0f
#define TGA_DESC_HORIZONTAL 0x10
#define TGA_DESC_VERTICAL 0x20
  unsigned char descriptor;

} TgaHeader;

typedef struct {
  unsigned int extensionAreaOffset;
  unsigned int developerDirectoryOffset;
#define TGA_SIGNATURE "TRUEVISION-XFILE"
  char signature[16];
  char dot;
  char null;
} TgaFooter;

extern gliGenericImage *gliReadTGA(FILE *fp, char *name);
extern int gliVerbose(int newVerbose);

#ifdef __cplusplus
}
#endif

#endif /* __tga_h__ */
