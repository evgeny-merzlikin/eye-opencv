#include "GLError.h"
#include <iostream>
#include <string>

#include <Windows.h>
#include <GL/Gl.h>


using namespace std;

void _check_gl_error(const char *file, int line) {
        GLenum err;
		err = glGetError();

        while(err!=GL_NO_ERROR) {
                string error;

                switch(err) {
                        case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
                        case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
                        case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
                        case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
                        
                }

                cerr << "GL_" << error.c_str() <<" - "<<file<<":"<<line<<endl;
                err = glGetError();
        }
}
