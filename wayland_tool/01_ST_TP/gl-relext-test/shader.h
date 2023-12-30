#ifndef __SHADER_H__
#define __SHADER_H__

#include <GLES3/gl3.h>

GLuint shader_build_program(const char *vsrc, const char *fsrc);

#endif
