#ifndef RENDERER_H
#define RENDERER_H


//////////////////////////////////////////////////////////////////////////////
//
//! Renderer - interface

class Renderer
{
public:
	//! Renders.
	virtual void render() = 0;

	//! When renderer instance has parameters that modify output, this returns them.
	//! By default, renderer is expected to have no parameters and render always the same.
	virtual const void* getParams(unsigned& length) const
	{
		length = 0;
		return 0;
	}

	virtual ~Renderer() {};
};

#endif
