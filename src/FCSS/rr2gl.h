//
// high level mgf loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include "RRVision.h"

class RRObjectRenderer
{
public:
	RRObjectRenderer(rrVision::RRObjectImporter* objectImporter, rrVision::RRScene* radiositySolver);

	enum ColorChannel
	{
		CC_NO_COLOR,
		CC_TRIANGLE_INDEX,
		CC_DIFFUSE_REFLECTANCE,
		CC_SOURCE_IRRADIANCE,
		CC_SOURCE_EXITANCE,
		CC_REFLECTED_IRRADIANCE,
		CC_REFLECTED_EXITANCE,
		CC_LAST
	};
	void compile(ColorChannel cc);
	void render(ColorChannel cc);
	GLfloat* getChannel(ColorChannel cc);
private:
	GLuint displayLists[CC_LAST];
};

void     rr2gl_compile(rrVision::RRObjectImporter* objectImporter, rrVision::RRScene* radiositySolver);
void     rr2gl_draw_onlyz();
unsigned rr2gl_draw_indexed(); // returns number of triangles
void     rr2gl_draw_colored(bool direct);

#endif
