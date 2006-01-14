#ifndef TEXTURE
#define TEXTURE

#include <stdio.h>
#include <GL/glut.h>
//#include <GL/gl.h>
//#include <jpeglib.h>

class Texture
{
public:
  class xFileNotFound {};
  class xNotSuchFormat {};
  class xNotASupportedFormat {};
  
  Texture(char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
	  int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
  Texture(unsigned char *data, int width, int height, int type,
	  int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR, 
	  int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
  virtual ~Texture();
  void bindTexture();
  int hasAlpha() { return channels == 4; };
protected:
  Texture();
  void changeNameConvention(char *filename) const;
  unsigned char *loadData(char *filename);
  //unsigned char *loadJpeg(char *filename);
  //unsigned char *decodeJpeg(struct jpeg_decompress_struct *cinfo);
  unsigned char *loadBmp(char *filename);
  unsigned char *loadBmpData(FILE *file);
  unsigned char *loadTga(char *filename);
  void checkFileOpened(FILE *file);

  unsigned int id;
  int channels;
  int width;
  int height;
  const static char *JPG_EXT;
  const static char *BMP_EXT;
};

#endif //TEXTURE
