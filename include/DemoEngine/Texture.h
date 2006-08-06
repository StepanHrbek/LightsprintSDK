#ifndef TEXTURE
#define TEXTURE

#include <cstdio>
#include <GL/glut.h>
//#include <jpeglib.h>

/*
class Texture
{
public:
	Texture(unsigned width, unsigned height, unsigned type,
		int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR, 
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	virtual ~Texture();
	void bind();
protected:
	unsigned int id;
	int channels;
	int width;
	int height;
};

class TextureFromDisk : public Texture
{
public:
	class xFileNotFound {};
	class xNotSuchFormat {};
	class xNotASupportedFormat {};

	TextureFromDisk(char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
protected:
	void changeNameConvention(char *filename) const;
	unsigned char *loadData(char *filename);
	//unsigned char *loadJpeg(char *filename);
	//unsigned char *decodeJpeg(struct jpeg_decompress_struct *cinfo);
	unsigned char *loadBmp(char *filename);
	unsigned char *loadBmpData(FILE *file);
	unsigned char *loadTga(char *filename);
	void checkFileOpened(FILE *file);

	unsigned char* pixels;
	const static char *JPG_EXT;
	const static char *BMP_EXT;
};
*/

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
	Texture(unsigned char* rgb, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	virtual ~Texture();
	void bindTexture();
	int hasAlpha() { return channels == 4; };
	void getPixel(float x, float y, float* rgb);
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
	unsigned char* pixels;
	const static char *JPG_EXT;
	const static char *BMP_EXT;
};

#endif //TEXTURE
