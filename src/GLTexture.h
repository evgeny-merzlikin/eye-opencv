#ifndef _GLTEXTURE_H_
#define _GLTEXTURE_H_

#include "GLFW/glfw3.h"
#include <algorithm>
#include <vector>
#include <mutex>

extern std::mutex mtx;

struct GLTexture {
	GLTexture(): data(nullptr) {}

	void init(int _width, int _height, GLint _format) 
	{

		dataWidth = _width;
		dataHeight = _height; 
		format = _format;

		if ( format == GL_LUMINANCE )
			channels = 1;
		if ( format == GL_RGB )
			channels = 3;
		if ( format == GL_RGBA )
			channels = 4;

		auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };

		POTWidth = std::max(dataWidth, pow2(dataWidth));
		POTHeight = std::max(dataHeight, pow2(dataHeight));

		u = (float) dataWidth / (float) POTWidth;
		v = (float) dataHeight / (float) POTHeight;

	}

	void glCreateTexture()
	{
		glGenTextures( 1, &ID );

		glBindTexture( GL_TEXTURE_2D, ID ); 
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ); 


		std::vector<GLubyte> pixels(POTWidth * POTHeight * channels);

		glTexImage2D( GL_TEXTURE_2D, 0, format, POTWidth, POTHeight, 0, format, GL_UNSIGNED_BYTE, &pixels[0] ); 


		glBindTexture( GL_TEXTURE_2D, 0 );

	}


	void glUpdate()
	{
		if ( !data )
			return;

		glBindTexture( GL_TEXTURE_2D, ID );
		// lock mutex
		mtx.lock();

		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dataWidth, dataHeight, format, GL_UNSIGNED_BYTE, data ); 
		// lock mutex
		mtx.unlock();

		glBindTexture( GL_TEXTURE_2D, 0 );

	}

	void glRenderTexture(float width, float height)
	{
		if ( !data )
			return;

		glBindTexture( GL_TEXTURE_2D, ID );

		glBegin( GL_QUADS ); 
		glTexCoord2f( 0.f, 0.f ); glVertex2f( 0.0f, 0.0f ); 
		glTexCoord2f( u, 0.f ); glVertex2f( width, 0.0f ); 
		glTexCoord2f( u, v ); glVertex2f( width, height); 
		glTexCoord2f( 0.f, v ); glVertex2f( 0.0f, height ); 
		glEnd();


		glBindTexture( GL_TEXTURE_2D, 0 );

	}

	void glDestroyTexture()
	{
		if ( ID )
			glDeleteTextures( 1, &ID );  
	}


	// closest point-of-two width and height
	int POTWidth, POTHeight; 

	// pixel buffer width and height
	int dataWidth, dataHeight; 

	int channels;

	GLfloat u, v;


	GLvoid* data;
	GLint format; 
	GLuint ID;    

};

#endif