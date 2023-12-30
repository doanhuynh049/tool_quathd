#include <stdio.h>

#include "shader.h"

#define LOG_LENGTH	1024

static GLuint compile_shader(const char *src, GLenum type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(type);
	if (!shader) {
		fprintf(stderr, "Can't create shader\n");
		return 0;
	}

	glShaderSource(shader, 1, (const char **)&src, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[LOG_LENGTH];
		GLsizei len;
		glGetShaderInfoLog(shader, LOG_LENGTH, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len ,log);
		return 0;
	}

	return shader;
}

GLuint shader_build_program(const char *vsrc, const char *fsrc)
{
	GLuint vshader, fshader, program;
	GLint status;

	vshader = compile_shader(vsrc, GL_VERTEX_SHADER);
	if (!vshader)
		return 0;

	fshader = compile_shader(fsrc, GL_FRAGMENT_SHADER);
	if (!fshader) {
		glDeleteShader(vshader);
		return 0;
	}

	program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);
	glDeleteShader(vshader);
	glDeleteShader(fshader);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[LOG_LENGTH];
		GLsizei len;
		glGetProgramInfoLog(program, LOG_LENGTH, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		return 0;
	}

	return program;
}
