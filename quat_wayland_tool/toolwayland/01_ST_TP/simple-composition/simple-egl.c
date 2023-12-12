/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <getopt.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/types.h>
#include <unistd.h>

#include "shared/helpers.h"
#include "shared/platform.h"
#include "weston-egl-ext.h"

struct window;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	struct window *window;

	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;

	struct wl_output *output;
};

struct geometry {
	int width, height;
};

enum SET_POS {
	DO_NOTHING = 0,
	SET_POSITION,
	GET_OUTPUT_SIZE,
};

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct {
		GLuint rotation_uniform;
		GLuint pos;
		GLuint col;
	} gl;

	uint32_t benchmark_time, frames;
	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int opaque, frame_sync;

	int pos_x, pos_y;
	enum SET_POS set_pos;
	int dx, dy;
	int fps;
	int x, y, w, h;
	struct wl_region *opaque_region;
};

static const char *vert_shader_text =
	"uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = rotation * pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static int running = 1;

static void
init_egl(struct display *display, struct window *window)
{
	static const struct {
		char *extension, *entrypoint;
	} swap_damage_ext_to_entrypoint[] = {
		{
			.extension = "EGL_EXT_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageEXT",
		},
		{
			.extension = "EGL_KHR_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageKHR",
		},
	};

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	const char *extensions;

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;

	if (window->opaque)
		config_attribs[9] = 0;

	display->egl.dpy =
		weston_platform_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
						display->display, NULL);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
		assert(0);

	configs = calloc(count, sizeof *configs);
	assert(configs);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);

	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(display->egl.dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (size == 32) {
			display->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (display->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size 32\n");
		exit(EXIT_FAILURE);
	}

	display->egl.ctx = eglCreateContext(display->egl.dpy,
					    display->egl.conf,
					    EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

	display->swap_buffers_with_damage = NULL;
	extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
	if (extensions &&
	    weston_check_egl_extension(extensions, "EGL_EXT_buffer_age")) {
		for (i = 0; i < (int) ARRAY_LENGTH(swap_damage_ext_to_entrypoint); i++) {
			if (weston_check_egl_extension(extensions,
						       swap_damage_ext_to_entrypoint[i].extension)) {
				/* The EXTPROC is identical to the KHR one */
				display->swap_buffers_with_damage =
					(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
					eglGetProcAddress(swap_damage_ext_to_entrypoint[i].entrypoint);
				break;
			}
		}
	}

	if (display->swap_buffers_with_damage)
		printf("has EGL_EXT_buffer_age and %s\n", swap_damage_ext_to_entrypoint[i].extension);

}

static void
fini_egl(struct display *display)
{
	eglTerminate(display->egl.dpy);
	eglReleaseThread();
}

static GLuint
create_shader(struct window *window, const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

static void
init_gl(struct window *window)
{
	GLuint frag, vert;
	GLuint program;
	GLint status;

	frag = create_shader(window, frag_shader_text, GL_FRAGMENT_SHADER);
	vert = create_shader(window, vert_shader_text, GL_VERTEX_SHADER);

	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(program);

	window->gl.pos = 0;
	window->gl.col = 1;

	glBindAttribLocation(program, window->gl.pos, "pos");
	glBindAttribLocation(program, window->gl.col, "color");
	glLinkProgram(program);

	window->gl.rotation_uniform =
		glGetUniformLocation(program, "rotation");
}

static void
shell_handle_ping(void *data, struct wl_shell_surface *shell_surface,
		  uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_handle_configure(void *data, struct wl_shell_surface *shell_surface,
		       uint32_t edges, int32_t width, int32_t height)
{
}

static void
shell_handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	shell_handle_ping,
	shell_handle_configure,
	shell_handle_popup_done
};

static void
create_shell_surface(struct window *window, struct display *display)
{
	window->shell_surface = wl_shell_get_shell_surface(display->shell,
							   window->surface);
	assert(window->shell_surface);

	wl_shell_surface_add_listener(window->shell_surface,
				      &shell_surface_listener, window);
	wl_shell_surface_set_title(window->shell_surface,
				   "simple-composition");
	wl_shell_surface_set_toplevel(window->shell_surface);

	wl_surface_commit(window->surface);
}

static void
create_surface(struct window *window)
{
	struct display *display = window->display;
	EGLBoolean ret;

	window->surface = wl_compositor_create_surface(display->compositor);

	window->native =
		wl_egl_window_create(window->surface,
				     window->geometry.width,
				     window->geometry.height);
	window->egl_surface =
		weston_platform_create_egl_surface(display->egl.dpy,
						   display->egl.conf,
						   window->native, NULL);

	if (window->opaque) {
		window->opaque_region = wl_compositor_create_region(
						window->display->compositor);
		wl_region_add(window->opaque_region,
			      window->x, window->y, window->w, window->h);
	}

	if (display->shell)
		create_shell_surface(window, display);
	else
		assert(0);

	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			     window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	if (!window->frame_sync)
		eglSwapInterval(display->egl.dpy, 0);

	if (!display->shell)
		return;
}

static void
destroy_surface(struct window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	weston_platform_destroy_egl_surface(window->display->egl.dpy,
					    window->egl_surface);
	wl_egl_window_destroy(window->native);


	if (window->opaque_region)
		wl_region_destroy(window->opaque_region);

	if (window->shell_surface)
		wl_shell_surface_destroy(window->shell_surface);

	wl_surface_destroy(window->surface);

	if (window->callback)
		wl_callback_destroy(window->callback);
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct display *display = window->display;
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5 },
		{  0.5, -0.5 },
		{  0,    0.5 }
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	};
	GLfloat angle;
	GLfloat rotation[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const uint32_t speed_div = 5, benchmark_interval = 5;
	EGLint rect[4];
	EGLint buffer_age = 0;
	struct timeval tv;

	assert(window->callback == callback);
	window->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (window->fps) {
		if (window->frames == 0)
			window->benchmark_time = time;
		if (time - window->benchmark_time >
		   (benchmark_interval * 1000)) {
			printf("%d frames in %d seconds: %f fps\n",
			       window->frames,
			       benchmark_interval,
		       (float) window->frames / benchmark_interval);
			window->benchmark_time = time;
			window->frames = 0;
		}
	}

	angle = (time / speed_div) % 360 * M_PI / 180.0;
	rotation[0][0] =  cos(angle);
	rotation[0][2] =  sin(angle);
	rotation[2][0] = -sin(angle);
	rotation[2][2] =  cos(angle);

	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, window->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	glViewport(0, 0, window->geometry.width, window->geometry.height);

	glUniformMatrix4fv(window->gl.rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) rotation);

	glClearColor(0.5, 0.5, 0.5, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(window->gl.pos);
	glEnableVertexAttribArray(window->gl.col);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(window->gl.pos);
	glDisableVertexAttribArray(window->gl.col);

	if (window->opaque) {
		wl_surface_set_opaque_region(window->surface,
					     window->opaque_region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	wl_egl_window_resize(window->native,
			     window->geometry.width, window->geometry.height,
			     window->dx, window->dy);

	if (display->swap_buffers_with_damage && buffer_age > 0) {
		rect[0] = window->geometry.width / 4 - 1;
		rect[1] = window->geometry.height / 4 - 1;
		rect[2] = window->geometry.width / 2 + 2;
		rect[3] = window->geometry.height / 2 + 2;
		display->swap_buffers_with_damage(display->egl.dpy,
						  window->egl_surface,
						  rect, 1);
	} else {
		eglSwapBuffers(display->egl.dpy, window->egl_surface);
	}

	if (window->fps)
		window->frames++;
}

static void
set_dummy_window(struct display *display, struct window *window)
{
	/* Set fullscreen window position to 0,0 */
	wl_display_dispatch_pending(display->display);
	eglSwapBuffers(display->egl.dpy, window->egl_surface);

	/* Set window position to specified coordiantes */
	wl_display_dispatch_pending(display->display);
	window->geometry = window->window_size;
	wl_egl_window_resize(window->native,
			     window->geometry.width, window->geometry.height,
			     window->pos_x, window->pos_y);
	eglSwapBuffers(display->egl.dpy, window->egl_surface);

	window->set_pos = DO_NOTHING;
}

static void
output_handle_geometry(void *data, struct wl_output *wl_output,
		       int32_t x, int32_t y,
		       int32_t physical_width, int32_t physical_height,
		       int32_t subpixel, const char *make,
		       const char *model, int32_t transform)
{
}

static void
output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
		   int32_t width, int32_t height, int32_t refresh)
{
	if (flags & WL_OUTPUT_MODE_CURRENT) {
		struct window *window = (struct window *)data;

		if (window->set_pos == GET_OUTPUT_SIZE) {
			window->geometry.width = width;
			window->geometry.height = height;
			window->set_pos = SET_POSITION;
		}
	}
}

static void
output_handle_done(void *data, struct wl_output *wl_output)
{
}

static void
output_handle_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
}

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_registry_bind(registry, name,
					    &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, name,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener,
					       d->window);
		}
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static void
signal_int(int signum)
{
	running = 0;
}

static void
usage(int error_code)
{
	fprintf(stderr, "Usage: simple-composition [OPTIONS]\n\n"
		"  -s, --size <w>x<h>\tWindow width and height "
					"(default: 250x250)\n"
		"  -p, --pos <x>,<y>\tWindow position "
					"(default: random)\n"
		"  -m, --move <dx>,<dy>\tMotion vector of window "
					"(default: 0,0)\n"
		"  -r, --range <x1>,<y1>,<x2>,<y2>\n"
		"  \t\t\tRange of window position (-p option is ignored)\n"
		"  -o, --opaque <x>,<y>,<w>,<h>\n"
		"  \t\t\tCreate an opaque surface\n"
		"  -b, --go-faster\tDon't sync to compositor redraw "
					"(eglSwapInterval 0)\n"
		"  -f, --fps\t\tOutput frame rate in FPS\n"
		"  -h, --help\t\tThis help text\n\n");

	exit(error_code);
}

static void
parse_options(struct window *window, int argc, char **argv)
{
	int c;
	const char *short_options = "s:p:m:r:o:bfh";
	const struct option long_options[] = {
		{"size", required_argument, NULL, 's'},
		{"pos", required_argument, NULL, 'p'},
		{"move", required_argument, NULL, 'm'},
		{"range", required_argument, NULL, 'r'},
		{"opaque", required_argument, NULL, 'o'},
		{"go-faster", no_argument, NULL, 'b'},
		{"fps", no_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};
	int range = 0;
	int x1, y1, x2, y2;

	while (1) {
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 's':
			if (sscanf(optarg, "%dx%d", &window->geometry.width,
				   &window->geometry.height) != 2)
				usage(EXIT_FAILURE);
			window->window_size = window->geometry;
			break;
		case 'p':
			if (sscanf(optarg, "%d,%d", &window->pos_x,
				   &window->pos_y) != 2)
				usage(EXIT_FAILURE);
			window->set_pos = GET_OUTPUT_SIZE;
			break;
		case 'm':
			if (sscanf(optarg, "%d,%d", &window->dx,
				   &window->dy) != 2)
				usage(EXIT_FAILURE);
			break;
		case 'r':
			if (sscanf(optarg, "%d,%d,%d,%d", &x1, &y1,
				   &x2, &y2) != 4)
				usage(EXIT_FAILURE);
			if ((x1 > x2) || (y1 > y2))
				usage(EXIT_FAILURE);
			range = 1;
			break;
		case 'o':
			if (sscanf(optarg, "%d,%d,%d,%d",
				   &window->x, &window->y,
				   &window->w, &window->h) != 4)
				usage(EXIT_FAILURE);
			if ((window->w <= 0) || (window->h <= 0))
				usage(EXIT_FAILURE);
			window->opaque = 1;
			break;
		case 'b':
			window->frame_sync = 0;
			break;
		case 'f':
			window->fps = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		default:
			usage(EXIT_FAILURE);
			break;
		}
	}

	if (range) {
		int x, y;
		struct timeval tv;
		
		x = x2 - x1 - window->window_size.width;
		y = y2 - y1 - window->window_size.height;
		if ((x <= 0) || (y <= 0))
			usage(EXIT_FAILURE);

		gettimeofday(&tv, NULL);
		srand(tv.tv_usec);
		window->pos_x = rand() % x + x1;
		window->pos_y = rand() % y + y1;
		window->set_pos = GET_OUTPUT_SIZE;
	}
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	struct window  window  = { 0 };

	window.display = &display;
	display.window = &window;
	window.geometry.width  = 250;
	window.geometry.height = 250;
	window.window_size = window.geometry;
	window.frame_sync = 1;
	window.set_pos = DO_NOTHING;

	parse_options(&window, argc, argv);

	display.display = wl_display_connect(NULL);
	assert(display.display);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry,
				 &registry_listener, &display);

	wl_display_roundtrip(display.display);
	wl_display_roundtrip(display.display);

	if (window.set_pos == GET_OUTPUT_SIZE) {
		fprintf(stderr, "output mode is not received.\n");
		exit(EXIT_FAILURE);
	}

	init_egl(&display, &window);
	create_surface(&window);
	init_gl(&window);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	if (window.set_pos)
		set_dummy_window(&display, &window);

	/* The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	while (running) {
		wl_display_dispatch_pending(display.display);
		redraw(&window, NULL, 0);
	}

	fprintf(stderr, "simple-composition exiting\n");

	destroy_surface(&window);
	fini_egl(&display);

	if (display.shell)
		wl_shell_destroy(display.shell);

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	if (display.output)
		wl_output_destroy(display.output);

	wl_registry_destroy(display.registry);
	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	return 0;
}
