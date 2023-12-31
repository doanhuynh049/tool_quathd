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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <linux/input.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>







#include "shared/helpers.h"
#include "shared/platform.h"
#include "shared/os-compatibility.h"
#include "shared/weston-egl-ext.h"

#include "xdg-shell-unstable-v6-client-protocol.h"
#include "protocol/ivi-application-client-protocol.h"
#define IVI_SURFACE_ID 9000
#define PANEL_HEIGHT 32

#define HEIGHT_UP 1
#define HEIGHT_DOWN 2
#define WIDTH_UP 3
#define WIDTH_DOWN 4

static int opaque_x=0;
static int opaque_y=0;
static int opaque_width=250;
static int opaque_height=250;
static int move_stepsize=10;
static int resize_stepsize=10;
static int running = 1;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;
static int step_handle_resize = 0;
static int flag_ft = 0;
static uint32_t time_fix = 0;
static int spinnin_format = 0,spinnin_lock = 0;
GLfloat angle;

struct window;
static void create_surface(struct window *window);
static void redraw(void *data, struct wl_callback *callback, uint32_t time);
void handle_resize_A (void *data);
void handle_resize_S (void *data);
void handle_resize_D (void *data);
void handle_resize_W (void *data);

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *xshell;
	struct wl_output *output;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *default_cursor;
	struct wl_surface *cursor_surface;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	struct window *window;
	struct ivi_application *ivi_application;

	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct geometry {
	int width, height;
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
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct ivi_surface *ivi_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, opaque, buffer_size, frame_sync, maximized;
	bool wait_for_configure;
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

	if (window->opaque || window->buffer_size == 16)
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
		if (window->buffer_size == size) {
			display->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (display->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			window->buffer_size);
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
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *xdg_surface, uint32_t serial)
{
	struct window *window = data;
	zxdg_surface_v6_ack_configure(xdg_surface, serial);

	window->wait_for_configure = false;
}

static void
handle_xdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *xdg_toplevel,
			  int32_t width, int32_t height,
			  struct wl_array *states)
{
	struct window *window = data;
	uint32_t *p;
	window->maximized = 0;
	window->fullscreen = 0;
	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
			window->fullscreen = 1;
			break;
		case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
			window->maximized = 1;
			break;
		}
	}

	if (width > 0 && height > 0) {
		if (!window->fullscreen) {
			window->window_size.width = width;
			window->window_size.height = height;
		}
		if (window->maximized) {
			window->window_size.width = 250;
			window->window_size.height = 250;
		}
		window->geometry.width = width;
		window->geometry.height = height;
	} else if (!window->fullscreen) {
		window->geometry = window->window_size;
	}

	if (window->native) {
		wl_egl_window_resize(window->native,
				     window->geometry.width,
				     window->geometry.height, 0, 0);
	}
}

static void
handle_xdg_toplevel_close(void *data, struct zxdg_toplevel_v6 *xdg_toplevel)
{
	running = 0;
}

static const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
	handle_xdg_toplevel_configure,
	handle_xdg_toplevel_close,
};

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};

static void
handle_ivi_surface_configure(void *data, struct ivi_surface *ivi_surface,
                             int32_t width, int32_t height)
{
	struct window *window = data;

	window->geometry.width = width;
	window->geometry.height = height;

	if (!window->fullscreen)
		window->window_size = window->geometry;
}

static const struct ivi_surface_listener ivi_surface_listener = {
	handle_ivi_surface_configure,
};

static void
create_xdg_surface(struct window *window, struct display *display)
{
	window->xdg_surface = zxdg_shell_v6_get_xdg_surface(display->xshell,
							window->surface);

	zxdg_surface_v6_add_listener(window->xdg_surface,
				 &xdg_surface_listener, window);
	window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
	zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
				 &xdg_toplevel_listener, window);

	zxdg_toplevel_v6_set_title(window->xdg_toplevel, "weston-simple-egl-tp");

	window->wait_for_configure = true;
	wl_surface_commit(window->surface);
}

static void
create_ivi_surface(struct window *window, struct display *display)
{
	uint32_t id_ivisurf = IVI_SURFACE_ID + (uint32_t)getpid();
	window->ivi_surface =
		ivi_application_surface_create(display->ivi_application,
					       id_ivisurf, window->surface);

	if (window->ivi_surface == NULL) {
		fprintf(stderr, "Failed to create ivi_client_surface\n");
		abort();
	}

	ivi_surface_add_listener(window->ivi_surface,
				 &ivi_surface_listener, window);
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

	wl_egl_window_resize(window->native,
					 window->geometry.width,
					 window->geometry.height, pos_x, pos_y);

	window->egl_surface =
		weston_platform_create_egl_surface(display->egl.dpy,
						   display->egl.conf,
						   window->native, NULL);

	if (display->xshell) {
		create_xdg_surface(window, display);
	} else if (display->ivi_application ) {
		create_ivi_surface(window, display);
	} else {
		assert(0);
	}

	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			     window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	if (!window->frame_sync)
		eglSwapInterval(display->egl.dpy, 0);

	if (!display->xshell)
		return;

	if (window->fullscreen) {
		zxdg_toplevel_v6_set_fullscreen(window->xdg_toplevel, NULL);
	}

}

static void
destroy_surface(struct window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglDestroySurface(window->display->egl.dpy, window->egl_surface);
	wl_egl_window_destroy(window->native);

	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);
	if (window->display->ivi_application)
		ivi_surface_destroy(window->ivi_surface);
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

	GLfloat rotation[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const uint32_t speed_div = 5, benchmark_interval = 5;
	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;
	struct timeval tv;

	assert(window->callback == callback);
	window->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (step_set_pos) {
		step_set_pos--;
		if (step_set_pos) {	/* 1st. frame */
			eglSwapBuffers(display->egl.dpy, window->egl_surface);
			return;
		} else {		/* 2nd. frame */
			window->geometry.width  = 250;
			window->geometry.height = 250;
			wl_egl_window_resize(window->native,
						 window->geometry.width,
						 window->geometry.height,
						 pos_x, pos_y);
		}
	}
	if (step_handle_resize != 0)
	{
		switch(step_handle_resize)
		{
			case HEIGHT_UP:
				handle_resize_S(display);
				break;
			case HEIGHT_DOWN:
				handle_resize_W(display);
				break;
			case WIDTH_UP:
				handle_resize_D(display);
				break;
			case WIDTH_DOWN:
				handle_resize_A(display);
				break;
			default:
				printf("Default widthxheight = 250x250\n");
		}
		//unset handle resize flag
		step_handle_resize = 0; 
	}
	
	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	if (window->frames == 0)
		window->benchmark_time = time;
	if (time - window->benchmark_time > (benchmark_interval * 1000)) {
		printf("%d frames in %d seconds: %f fps\n",
		       window->frames,
		       benchmark_interval,
		       (float) window->frames / benchmark_interval);
		window->benchmark_time = time;
		window->frames = 0;
	}

	if ( spinnin_lock == 1 )
		angle += 0.05f;
	else
		angle = (time / speed_div) % 360 * M_PI / 180.0;

	 if (angle >= 6.25f)
		angle = 0.00f;

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

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(window->gl.pos);
	glEnableVertexAttribArray(window->gl.col);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(window->gl.pos);
	glDisableVertexAttribArray(window->gl.col);

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, opaque_x, opaque_y,
			      opaque_width, opaque_height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

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
	window->frames++;
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *display = data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = display->default_cursor;
	struct wl_cursor_image *image;

	if (display->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		display->default_cursor = wl_cursor_theme_get_cursor(display->cursor_theme, "left_ptr");
		image = display->cursor_theme;
		buffer = wl_cursor_image_get_buffer(image);
		if (!buffer)
			return;
		wl_pointer_set_cursor(pointer, serial,
				      display->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(display->cursor_surface, buffer, 0, 0);
		wl_surface_damage(display->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(display->cursor_surface);
	}
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct display *display = data;

	if (!display->window->xdg_surface)
		return;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		zxdg_toplevel_v6_move(display->window->xdg_toplevel,
						 display->seat, serial);
	}
 }

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, struct wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *d = (struct display *)data;

	if (!d->xshell )
		return;

	zxdg_toplevel_v6_move(d->window->xdg_toplevel, d->seat, serial);

}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void
xdg_do_fullscreen(struct window *window)
{
	struct wl_callback *callback;

	if (window->fullscreen)
		zxdg_toplevel_v6_unset_fullscreen(window->xdg_toplevel);
	else
		zxdg_toplevel_v6_set_fullscreen(window->xdg_toplevel, NULL);
}

void handle_resize_W (void *data)
{
	struct display *d = data;
	if (d->window->native && (d->window->geometry.height > resize_stepsize))
	{
		d->window->geometry.height   -= resize_stepsize;
		d->window->window_size.height = d->window->geometry.height;
		wl_egl_window_resize(d->window->native,
							 d->window->geometry.width,
							 d->window->geometry.height,
							 pos_x, pos_y);
		printf("Height Down\n");
	}
}

void handle_resize_A (void *data)
{
	struct display *d = data;
	if (d->window->native && (d->window->geometry.width > resize_stepsize))
	{
		d->window->geometry.width    -= resize_stepsize;
		d->window->window_size.width  = d->window->geometry.width;
		wl_egl_window_resize(d->window->native,
							 d->window->geometry.width,
							 d->window->geometry.height,
							 pos_x, pos_y);
		printf("Width Down\n");
	}
}

void handle_resize_D (void *data)
{
	struct display *d = data;
	if (d->window->native)
	{
		d->window->geometry.width    += resize_stepsize;
		d->window->window_size.width  = d->window->geometry.width;
		wl_egl_window_resize(d->window->native,
							 d->window->geometry.width,
							 d->window->geometry.height,
							 pos_x, pos_y);
		printf("Width Up\n");
	}
}

void handle_resize_S (void *data)
{
	struct display *d = data;
	if (d->window->native)
	{
		d->window->geometry.height   += resize_stepsize;
		d->window->window_size.height = d->window->geometry.height;
		wl_egl_window_resize(d->window->native,
							 d->window->geometry.width,
							 d->window->geometry.height,
							 pos_x, pos_y);
		printf("Height Up\n");
	}
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = data;

	if (!d->xshell)
		return;

	if (key == KEY_F11 && state) {
		xdg_do_fullscreen(d->window);
	}
	else if (key == KEY_RIGHT && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     move_stepsize, 0);
			printf("Right\n");
		}
	}
	else if (key == KEY_LEFT && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     -move_stepsize, 0);
			printf("Left\n");
		}
	}
	else if (key == KEY_UP && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, -move_stepsize);
			printf("Up\n");
		}
	}
	else if (key == KEY_DOWN && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, move_stepsize);
			printf("Down\n");
		}
	}
	else if (key == KEY_L && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     move_stepsize*50, 0);
			printf("Right x 50\n");
		}
	}
	else if (key == KEY_J && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     -move_stepsize*50, 0);
			printf("Left x 50\n");
		}
	}
	else if (key == KEY_I && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, -move_stepsize*50);
			printf("Up x 50\n");
		}
	}
	else if (key == KEY_K && state)
	{
		if (d->window->native)
		{
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, move_stepsize*50);
			printf("Down x 50\n");
		}
	}
	else if (key == KEY_W && state)
	{
		if (d->window->native && (d->window->geometry.height > resize_stepsize))
		{
			d->window->geometry.height   -= resize_stepsize;
			d->window->window_size.height = d->window->geometry.height;
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, 0);
			printf("Height Down\n");
		}
	}
	else if (key == KEY_A && state)
	{
		if (d->window->native && (d->window->geometry.width > resize_stepsize))
		{
			d->window->geometry.width    -= resize_stepsize;
			d->window->window_size.width  = d->window->geometry.width;
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, 0);
			printf("Width Down\n");
		}
	}
	else if (key == KEY_S && state)
	{
		if (d->window->native)
		{
			d->window->geometry.height   += resize_stepsize;
			d->window->window_size.height = d->window->geometry.height;
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, 0);
			printf("Height Up\n");
		}
	}
	else if (key == KEY_D && state)
	{
		if (d->window->native)
		{
			d->window->geometry.width    += resize_stepsize;
			d->window->window_size.width  = d->window->geometry.width;
			wl_egl_window_resize(d->window->native,
			                     d->window->geometry.width,
			                     d->window->geometry.height,
			                     0, 0);
			printf("Width Up\n");
		}
	}
	else if (key == KEY_ESC && state)
		running = 0;
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct display *d = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
		d->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(d->pointer, &pointer_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
		wl_pointer_destroy(d->pointer);
		d->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
		d->touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(d->touch, d);
		wl_touch_add_listener(d->touch, &touch_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
		wl_touch_destroy(d->touch);
		d->touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void
xdg_shell_ping(void *data, struct zxdg_shell_v6 *shell, uint32_t serial)
{
	zxdg_shell_v6_pong(shell, serial);
}

static const struct zxdg_shell_v6_listener xdg_shell_listener = {
	xdg_shell_ping,
};

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
		dummy_w = width;
		dummy_h = height;
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
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, name,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
		}
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->xshell = wl_registry_bind(registry,
					    name, &zxdg_shell_v6_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_registry_bind(registry, name,
					   &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
		d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
		if (!d->cursor_theme) {
			fprintf(stderr, "unable to load default theme\n");
			return;
		}
		d->default_cursor =
			wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
		if (!d->default_cursor) {
			fprintf(stderr, "unable to load default left pointer\n");
			// TODO: abort ?
		}
	} else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application =
			wl_registry_bind(registry, name,
					 &ivi_application_interface, 1);
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
	fprintf(stderr, "Usage: simple-egl [OPTIONS]\n\n"
		"\t-f\tRun in fullscreen mode\n"
		"\t-o\tCreate an opaque surface\n"
		"\t-s\tUse a 16 bpp EGL config\n"
		"\t-b\tDon't sync to compositor redraw (eglSwapInterval 0)\n"
		"\t-p=<pos_x>,<pos_y>\tSet pos x y for windows\n"
		"\t-opaque=X,Y,WIDTH,HEIGHT\n"
		"\t\tCoordinate and size of opaque region\n"
		"\t\tdefault: 0,0,250,250\n"
		"\t-size=WIDTH,HEIGHT\n"
		"\t\tWindow size\n"
		"\t\tdefault: 250,250\n"
		"\t-resize=UNIT_SIZE\n"
		"\t\tThe value of change in size when you press D/A/S/W key\n"
		"\t\tdefault: 10\n"
		"\t-move=UNIT_DISTANCE\n"
		"\t\tMoving distance when you press Left/Right/Up/Down key\n"
		"\t\tdefault: 10\n"
		"\t-h\tThis help text\n\n"
		);

	exit(error_code);
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	struct window  window  = { 0 };
	int i, ret = 0;

	window.display = &display;
	display.window = &window;
	window.geometry.width  = 250;
	window.geometry.height = 250;
	window.window_size = window.geometry;
	window.buffer_size = 32;
	window.frame_sync = 1;

	for (i = 1; i < argc; i++) {
		if (sscanf(argv[i], "-opaque=%d,%d,%d,%d", &opaque_x,&opaque_y,&opaque_width,&opaque_height) == 4 )
			window.opaque = 1;
		else if (sscanf(argv[i], "-size=%d,%d", &window.window_size.width ,&window.window_size.height) == 2 );
		else if (sscanf(argv[i], "-resize=%d", &resize_stepsize) == 1 );
		else if (sscanf(argv[i], "-move=%d", &move_stepsize) == 1 );
		else if (sscanf(argv[i], "-p=%d,%d", &pos_x,&pos_y) == 2) {
			pos_y -= PANEL_HEIGHT;
			step_set_pos = 2;
		} else if (strcmp("-t", argv[i]) == 0) {
			flag_ft = 1;
		} else if (strcmp("-f", argv[i]) == 0)
			window.fullscreen = 1;
		else if (strcmp("-o", argv[i]) == 0)
			window.opaque = 1;
		else if (strcmp("-s", argv[i]) == 0)
			window.buffer_size = 16;
		else if (strcmp("-b", argv[i]) == 0)
			window.frame_sync = 0;
		else if (strcmp("-sf", argv[i]) == 0)
			spinnin_format = 1;
		else if (strcmp("-lock", argv[i]) == 0)
			spinnin_lock = 1;
		else if (strcmp("-heightup", argv[i]) == 0)
			step_handle_resize = HEIGHT_UP;
		else if (strcmp("-heightdown", argv[i]) == 0)
			step_handle_resize = HEIGHT_DOWN;
		else if (strcmp("-widthup", argv[i]) == 0)
			step_handle_resize = WIDTH_UP;
		else if (strcmp("-widthdown", argv[i]) == 0)
			step_handle_resize = WIDTH_DOWN;
		else if (strcmp("-h", argv[i]) == 0)
			usage(EXIT_SUCCESS);
		else
			usage(EXIT_FAILURE);
	}

	display.display = wl_display_connect(NULL);
	assert(display.display);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry,
				 &registry_listener, &display);

	wl_display_roundtrip(display.display);

	if (step_set_pos) {
		wl_display_roundtrip(display.display);
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(EXIT_FAILURE);
		}
		window.geometry.width = dummy_w;
		window.geometry.height = dummy_h;
	}

	init_egl(&display, &window);

	
	create_surface(&window);
	init_gl(&window);
	
	display.cursor_surface =
		wl_compositor_create_surface(display.compositor);
	
	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	while (running && ret != -1) {
		if (window.wait_for_configure) {
			wl_display_dispatch(display.display);
		} else {
			wl_display_dispatch_pending(display.display);
			redraw(&window, NULL, 0);
		}
	}

	fprintf(stderr, "simple-egl exiting\n");

	destroy_surface(&window);
	fini_egl(&display);

	wl_surface_destroy(display.cursor_surface);
	if (display.cursor_theme)
		wl_cursor_theme_destroy(display.cursor_theme);

	if (display.xshell)
		zxdg_shell_v6_destroy(display.xshell);

	if (display.output)
		wl_output_destroy(display.output);

	if (display.ivi_application)
		ivi_application_destroy(display.ivi_application);

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	if (display.shm)
		wl_shm_destroy(display.shm);

	wl_registry_destroy(display.registry);
	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	return 0;
}
