/* This program is based on simple-egl in weston 1.11.0 code.
 * The auther and licence of simple-egl is following:
 *
 *-------------------------------------------------------------
 *
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
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>
#include <wayland-client-protocol.h>

#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_REL.h>

#include <xf86drm.h>
#include <libkms/libkms.h>
#include <drm_fourcc.h>

#include "xdg-shell-unstable-v6-client-protocol.h"
#include "protocol/ivi-application-client-protocol.h"
#include "shared/os-compatibility.h"
#define IVI_SURFACE_ID 9000

#include "shared/platform.h"

#include "shader.h"
#include "shared/helpers.h"
#include "shared/weston-egl-ext.h"

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;

#define WINDOW_WIDTH		320
#define WINDOW_HEIGHT		240

#define PIXMAP_WIDTH		320
#define PIXMAP_HEIGHT		240

#define PANEL_HEIGHT		32

struct window;
struct seat;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *xshell;
	struct wl_seat *seat;
	struct wl_output *output;
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

	int drm_fd;
	struct kms_driver *kms;
};

struct geometry {
	int width, height;
};

struct window {
	struct display *display;
	struct geometry geometry, window_size;
	struct {
		GLuint sh_position;
		GLuint sh_texcoord;
		GLuint sh_texture;
		GLuint sh_proj;
		GLuint sh_model;
		GLuint texture;
		GLuint buffers[3];
		int num_point;
	} gl;

	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct ivi_surface *ivi_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, opaque, buffer_size, frame_sync;

	struct kms_bo *bo;
	uint8_t *mmap;
	EGLImageKHR eglImage;
	bool wait_for_configure;
};

#define TO_STRING(x)	#x
static const char *vert_shader_text = TO_STRING(
	attribute vec3 position;
	attribute vec2 texcoord;
	varying vec2 texcoordVarying;
	void main()
	{
		gl_Position = vec4(position, 1.0);
		texcoordVarying = texcoord;
	});

static const char *frag_shader_text = TO_STRING(
	precision mediump float;
	varying vec2 texcoordVarying;
	uniform sampler2D texture;
	void main() {
		gl_FragColor = texture2D(texture, texcoordVarying);
	});

static int running = 1;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;
static char bufer_id = 0;
static char lock_image = 0;

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

/* dump texture data */
static int enable_dump = 0;

static int
drm_connect(struct display *d)
{
	struct kms_driver *kms;
	int fd;

	fd = drmOpen("rcar-du", NULL);
	if (fd <= 0) {
		fprintf(stderr, "failed drmOpen\n");
		return 0;
	}
	kms_create(fd, &kms);

	d->drm_fd = fd;
	d->kms = kms;

	return 1;
}

static void
drm_shutdown(struct display *d)
{
	kms_destroy(&d->kms);
	close(d->drm_fd);
}

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

	eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
		eglGetProcAddress("eglCreateImageKHR");
	if (eglCreateImageKHR == NULL)
		printf("eglCreateImageKHR load failed.\n");

	eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
		eglGetProcAddress("eglDestroyImageKHR");
	if (eglDestroyImageKHR == NULL)
		printf("eglDestroyImageKHR load failed.\n");

	glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
		eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if (glEGLImageTargetTexture2DOES == NULL)
		printf("glEGLImageTargetTexture2DOES load failed.\n");
}

static void
fini_egl(struct display *display)
{
	eglTerminate(display->egl.dpy);
	eglReleaseThread();
}

static struct kms_bo *
alloc_bo(struct display *d, int width, int height)
{
	struct kms_bo *bo;
	unsigned int attr[] = {
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_WIDTH, width,
		KMS_HEIGHT, height,
		KMS_TERMINATE_PROP_LIST
	};
	int err;

	err = kms_bo_create(d->kms, attr, &bo);
	assert(err == 0);

	return bo;
}

static void
free_bo(struct kms_bo *bo)
{
	kms_bo_destroy(&bo);
}

static void *
map_bo(struct kms_bo *bo)
{
	uint8_t *mmap;
	int err;

	err = kms_bo_map(bo, (void **)&mmap);
	assert(err == 0);

	return mmap;
}

static void
unmap_bo(struct kms_bo *bo)
{
	kms_bo_unmap(bo);
}

static void
create_color_bar_rgb32(uint8_t *buf, int width, int height)
{
	uint32_t color1[] = {
		0xFFFFFFFF,
		0xFFFFFF00,
		0xFF00FFFF,
		0xFF00FF00,
		0xFFFF00FF,
		0xFFFF0000,
		0xFF0000FF
	};
	uint32_t color2[] = {
		0xFF0000FF,
		0xFF000000,
		0xFFFF00FF,
		0xFF000000,
		0xFF00FFFF,
		0xFF000000,
		0xFFFFFFFF,
	};
	uint32_t color3[] = {
		0xFF000080,
		0xFFFFFFFF,
		0xFF0080FF,
		0xFF000000,
		0xFF131313,
	};
	uint32_t *p;
	int x, y;

	p = (uint32_t *)buf;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color1[x * 7 / width];
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color2[x * 7 / width];
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x++) {
			if (x < 4 * width / 6)
				*p++ = color3[x * 6 / width];
			else if (x < 4 * width / 6 + width / 12)
				*p++ = color3[4];
			else
				if (lock_image)
					*p++ = (x + y + bufer_id) % 3 == 0 ?
					0xFF000000 : 0xFFFFFFFF;
				else
					*p++ = rand() % 2 == 0 ?
					0xFF000000 : 0xFFFFFFFF;
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % 3;
}

static void create_color_bar_rgb16(uint16_t *buf, int width, int height,
				   uint16_t color1[], uint16_t color2[],
				   uint16_t color3[], uint16_t blk_color)
{
	uint16_t *p = buf;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color1[x * 7 / width];
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color2[x * 7 / width];
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x++) {
			if (x < 4 * width / 6)
				*p++ = color3[x * 6 / width];
			else if (x < 4 * width / 6 + width / 12)
				*p++ = color3[4];
			else
				if (lock_image)
					*p++ = (x + y + bufer_id) % 3 == 0 ?
						blk_color : 0xFFFF;
				else
					*p++ = rand() % 2 == 0 ?
						blk_color : 0xFFFF;
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % 3;
}

static void
create_color_bar_rgb565(uint8_t *buf, int width, int height)
{
	uint16_t color1[] = {
		0xFFFF,
		0xFFE0,
		0x07FF,
		0x07E0,
		0xF81F,
		0xF800,
		0x001F
	};
	uint16_t color2[] = {
		0x001F,
		0x0000,
		0xF81F,
		0x0000,
		0x07FF,
		0x0000,
		0xFFFF,
	};
	uint16_t color3[] = {
		0x0010,
		0xFFFF,
		0x041F,
		0x0000,
		0x1082
	};

	create_color_bar_rgb16((uint16_t*)buf, width, height,
			       color1, color2, color3, 0x0000);
}

static void
create_color_bar_argb1555(uint8_t *buf, int width, int height)
{
	uint16_t color1[] = {
		0xFFFF,
		0xFFE0,
		0x83FF,
		0x83E0,
		0xFC1F,
		0xFC00,
		0x801F
	};
	uint16_t color2[] = {
		0x801F,
		0x8000,
		0xFC1F,
		0x8000,
		0x83FF,
		0x8000,
		0xFFFF,
	};
	uint16_t color3[] = {
		0x8010,
		0xFFFF,
		0x821F,
		0x8000,
		0x8842
	};

	create_color_bar_rgb16((uint16_t*)buf, width, height,
			       color1, color2, color3, 0x8000);
}

static void
create_color_bar_argb4444(uint8_t *buf, int width, int height)
{
	uint16_t color1[] = {
		0xFFFF,
		0xFFF0,
		0xF0FF,
		0xF0F0,
		0xFF0F,
		0xFF00,
		0xF00F
	};
	uint16_t color2[] = {
		0xF00F,
		0xF000,
		0xFF0F,
		0xF000,
		0xF0FF,
		0xF000,
		0xFFFF,
	};
	uint16_t color3[] = {
		0xF008,
		0xFFFF,
		0xF08F,
		0xF000,
		0xF111
	};

	create_color_bar_rgb16((uint16_t*)buf, width, height,
			       color1, color2, color3, 0xF000);
}

static void set_packed_yuv(uint8_t *p, uint8_t y, uint8_t u, uint8_t v,
			   uint32_t format)
{
	switch (format) {
	case EGL_NATIVE_PIXFORMAT_UYVY_REL:
		*p++ = u;
		*p++ = y;
		*p++ = v;
		*p++ = y;
		break;
	}
}

static void
create_color_bar_packed_yuv(uint8_t *buf, int width, int height, int format)
{
	uint8_t color1[7][3] = {
		{0xEB, 0x80, 0x80},
		{0xD2, 0x10, 0x92},
		{0xAA, 0xA6, 0x10},
		{0x91, 0x36, 0x22},
		{0x6A, 0xCA, 0xDE},
		{0x51, 0x5A, 0xF0},
		{0x29, 0xF0, 0x6E}
	};
	uint8_t color2[7][3] = {
		{0x29, 0xF0, 0x6E},
		{0x00, 0x80, 0x80},
		{0x6A, 0xCA, 0xDE},
		{0x00, 0x80, 0x80},
		{0xAA, 0xA6, 0x10},
		{0x00, 0x80, 0x80},
		{0xEB, 0x80, 0x80}
	};
	uint8_t color3[5][3] = {
		{0x0F, 0xC0, 0x76},
		{0xEB, 0x80, 0x80},
		{0x68, 0xD6, 0x36},
		{0x00, 0x80, 0x80},
		{0x10, 0x80, 0x80}
	};
	uint8_t *p = buf;
	int x, y, i;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x += 2) {
			i = x * 7 / width;
			set_packed_yuv(p, color1[i][0], color1[i][1],
				       color1[i][2], format);
			p += 4;
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x += 2) {
			i = x * 7 / width;
			set_packed_yuv(p, color2[i][0], color2[i][1],
				       color2[i][2], format);
			p += 4;
		}
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x += 2) {
			if (x < 4 * width / 6)
				i = x * 6 / width;
			else if (x < 4 * width / 6 + width / 12)
				i = 4;
			else
				if (lock_image)
					i = (x + y + bufer_id) % 3 == 0 ? 1 : 3;
				else
					i = rand() % 2 == 0 ? 1 : 3;
			set_packed_yuv(p, color3[i][0], color3[i][1],
				   color3[i][2], format);
			p += 4;
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % 3;
}

static void
create_color_bar_nv12(uint8_t *buf, int width, int height)
{
	uint8_t color1[7][3] = {
		{0xEB, 0x80, 0x80},
		{0xD2, 0x10, 0x92},
		{0xAA, 0xA6, 0x10},
		{0x91, 0x36, 0x22},
		{0x6A, 0xCA, 0xDE},
		{0x51, 0x5A, 0xF0},
		{0x29, 0xF0, 0x6E}
	};
	uint8_t color2[7][3] = {
		{0x29, 0xF0, 0x6E},
		{0x00, 0x80, 0x80},
		{0x6A, 0xCA, 0xDE},
		{0x00, 0x80, 0x80},
		{0xAA, 0xA6, 0x10},
		{0x00, 0x80, 0x80},
		{0xEB, 0x80, 0x80}
	};
	uint8_t color3[5][3] = {
		{0x0F, 0xC0, 0x76},
		{0xEB, 0x80, 0x80},
		{0x68, 0xD6, 0x36},
		{0x00, 0x80, 0x80},
		{0x10, 0x80, 0x80}
	};
	uint8_t *p = buf;
	uint8_t *uv = buf + width * height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color1[x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color1[x * 7 / width][1];
				*uv++ = color1[x * 7 / width][2];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color2[x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color2[x * 7 / width][1];
				*uv++ = color2[x * 7 / width][2];
			}
		}
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x++) {
			int i;
			if (x < 4 * width / 6)
				i = x * 6 / width;
			else if (x < 4 * width / 6 + width / 12)
				i = 4;
			else
				if (lock_image)
					i = (x + y + bufer_id) % 3 == 0 ? 1 : 3;
				else
					i = rand() % 2 == 0 ? 1 : 3;
			*p++ = color3[i][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color3[i][1];
				*uv++ = color3[i][2];
			}
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % 3;
}

static void
create_color_bar_nv16(uint8_t *buf, int width, int height)
{
	uint8_t color1[7][3] = {
		{0xEB, 0x80, 0x80},
		{0xD2, 0x10, 0x92},
		{0xAA, 0xA6, 0x10},
		{0x91, 0x36, 0x22},
		{0x6A, 0xCA, 0xDE},
		{0x51, 0x5A, 0xF0},
		{0x29, 0xF0, 0x6E}
	};
	uint8_t color2[7][3] = {
		{0x29, 0xF0, 0x6E},
		{0x00, 0x80, 0x80},
		{0x6A, 0xCA, 0xDE},
		{0x00, 0x80, 0x80},
		{0xAA, 0xA6, 0x10},
		{0x00, 0x80, 0x80},
		{0xEB, 0x80, 0x80}
	};
	uint8_t color3[5][3] = {
		{0x0F, 0xC0, 0x76},
		{0xEB, 0x80, 0x80},
		{0x68, 0xD6, 0x36},
		{0x00, 0x80, 0x80},
		{0x10, 0x80, 0x80}
	};
	uint8_t *p = buf;
	uint8_t *uv = buf + width * height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color1[x * 7 / width][0];
			if (x % 2 == 0) {
				*uv++ = color1[x * 7 / width][1];
				*uv++ = color1[x * 7 / width][2];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color2[x * 7 / width][0];
			if (x % 2 == 0) {
				*uv++ = color2[x * 7 / width][1];
				*uv++ = color2[x * 7 / width][2];
			}
		}
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x++) {
			int i;
			if (x < 4 * width / 6)
				i = x * 6 / width;
			else if (x < 4 * width / 6 + width / 12)
				i = 4;
			else
				if (lock_image)
					i = (x + y + bufer_id) % 3 == 0 ? 1 : 3;
				else
					i = rand() % 2 == 0 ? 1 : 3;
			*p++ = color3[i][0];
			if (x % 2 == 0) {
				*uv++ = color3[i][1];
				*uv++ = color3[i][2];
			}
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % 3;
}

static GLuint
create_texture(struct window *window, int format)
{
	GLuint texture;
	int size;
	EGLNativePixmapTypeREL nativePixmap = {
		window->window_size.width, window->window_size.height,
		window->window_size.width,
		0,
		format,
		NULL,
	};

	switch (format) {
	case EGL_NATIVE_PIXFORMAT_ARGB8888_REL:
		size = window->window_size.width * window->window_size.height * 4;
		create_color_bar_rgb32(window->mmap,
				       window->window_size.width, window->window_size.height);
		break;
	case EGL_NATIVE_PIXFORMAT_RGB565_REL:
		size = window->window_size.width * window->window_size.height * 2;
		create_color_bar_rgb565(window->mmap,
					window->window_size.width, window->window_size.height);
		break;
	case EGL_NATIVE_PIXFORMAT_ARGB1555_REL:
		size = window->window_size.width * window->window_size.height * 2;
		create_color_bar_argb1555(window->mmap,
					  window->window_size.width, window->window_size.height);
		break;
	case EGL_NATIVE_PIXFORMAT_ARGB4444_REL:
		size = window->window_size.width * window->window_size.height * 2;
		create_color_bar_argb4444(window->mmap,
					  window->window_size.width, window->window_size.height);
		break;
	case EGL_NATIVE_PIXFORMAT_UYVY_REL:
		size = window->window_size.width * window->window_size.height;
		create_color_bar_packed_yuv(window->mmap,
					    window->window_size.width, window->window_size.height,
					    format);
		break;
	case EGL_NATIVE_PIXFORMAT_NV12_REL:
		size = window->window_size.width * window->window_size.height * 3 / 2;
		create_color_bar_nv12(window->mmap,
				      window->window_size.width, window->window_size.height);
		break;
	case EGL_NATIVE_PIXFORMAT_NV16_REL:
		size = window->window_size.width * window->window_size.height * 2;
		create_color_bar_nv16(window->mmap,
				      window->window_size.width, window->window_size.height);
		break;
	default:
		return 0;
	}

	if (enable_dump) {
		FILE *fp = fopen("texture.dat", "wb");
		if (fp != NULL) {
			fwrite(window->mmap, size, 1, fp);
			fclose(fp);
			fprintf(stderr, "dump texture data: size=%dx%d\n",
				window->window_size.width, window->window_size.height);
		} else {
			fprintf(stderr, "Can't open file for dump\n");
		}
	}

	nativePixmap.usage = size;
	nativePixmap.pixelData = window->mmap;
	window->eglImage = eglCreateImageKHR(window->display->egl.dpy,
					     EGL_NO_CONTEXT,
					     EGL_NATIVE_PIXMAP_KHR,
					     &nativePixmap, NULL);
	assert(window->eglImage);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, window->eglImage);
	return texture;
}

static void
init_gl(struct window *window, int format)
{
	GLuint program;

	window->gl.texture = create_texture(window, format);

	program = shader_build_program(vert_shader_text, frag_shader_text);
	assert(program);
	glUseProgram(program);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	window->gl.sh_position = glGetAttribLocation(program, "position");
	window->gl.sh_texcoord = glGetAttribLocation(program, "texcoord");
	window->gl.sh_texture = glGetUniformLocation(program, "texture");
	window->gl.sh_proj = glGetUniformLocation(program, "proj");
	window->gl.sh_model = glGetUniformLocation(program, "model");
	glEnableVertexAttribArray(window->gl.sh_position);
	glEnableVertexAttribArray(window->gl.sh_texcoord);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
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

	window->fullscreen = 0;
	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
			window->fullscreen = 1;
			break;
		}
	}

	if (width > 0 && height > 0) {
		if (!window->fullscreen) {
			window->window_size.width = width;
			window->window_size.height = height;
		}
		window->geometry.width = width;
		window->geometry.height = height;
	} else if (!window->fullscreen) {
		window->geometry = window->window_size;
	}

	if (window->native)
		wl_egl_window_resize(window->native,
				     window->geometry.width,
				     window->geometry.height, 0, 0);
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

static void xdg_do_fullscreen(struct window *window)
{
	if (window->fullscreen)
		zxdg_toplevel_v6_unset_fullscreen(window->xdg_toplevel);
	else
		zxdg_toplevel_v6_set_fullscreen(window->xdg_toplevel, NULL);
}

static void
handle_ivi_surface_configure(void *data, struct ivi_surface *ivi_surface,
                             int32_t width, int32_t height)
{
	struct window *window = data;

	wl_egl_window_resize(window->native, width, height, 0, 0);

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
	zxdg_toplevel_v6_set_title(window->xdg_toplevel, "gl-relext-test");

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

static void init_gl_buffer(struct window *window)
{
	static const GLfloat vertex[] = {
		-1, 1,  0,
		-1, -1,  0,
		1,  -1,  0,
		1,  1,  0,
	};
	static const GLfloat texcoord[] = {
		0, 0,
		0, 1,
		1, 1,
		1, 0,
	};

	static const GLushort index[] = {
		0, 1, 2, 0, 2, 3,
	};

	window->gl.num_point = sizeof(index) / sizeof(index[0]);

	glGenBuffers(3, window->gl.buffers);

	glBindBuffer(GL_ARRAY_BUFFER, window->gl.buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, window->gl.buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texcoord), texcoord,
		     GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, window->gl.buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index,
		     GL_STATIC_DRAW);
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct display *display = window->display;
	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;

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
			window->geometry = window->window_size;
			wl_egl_window_resize(window->native,
						 window->geometry.width,
						 window->geometry.height,
						 pos_x, pos_y);
		}
	}

	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, window->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	glViewport(0, 0, window->geometry.width, window->geometry.height);
	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, window->gl.texture);
	glUniform1i(window->gl.sh_texture, 0);

	glBindBuffer(GL_ARRAY_BUFFER, window->gl.buffers[0]);
	glVertexAttribPointer(window->gl.sh_position, 3, GL_FLOAT, false, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, window->gl.buffers[1]);
	glVertexAttribPointer(window->gl.sh_texcoord, 2, GL_FLOAT, false, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, window->gl.buffers[2]);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0,
			      window->geometry.width,
			      window->geometry.height);
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
		image = display->default_cursor->images[0];
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

	if ((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
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

	if (!d->xshell)
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
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = data;
	if (!d->xshell)
		return;

	if (key == KEY_F11  && state)
		xdg_do_fullscreen(d->window);
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
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->xshell = wl_registry_bind(registry, name,
					    &zxdg_shell_v6_interface, 1);
		zxdg_shell_v6_add_listener(d->xshell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, name,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
		}
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
usage(int argc, char **argv)
{
        fprintf(stdout,
                "Usage: %s [options]\n\n"
                "Options:\n"
                "-f | --format color    Set color format\n"
				"-d | --dump			dump texture to ./texture.dat\n"
				"-p | --pos x,y			Set application position\n"
				"-s | --size w h		Set application size\n"
				"-l | --lock			Use fix image for random part\n"
                "-h | --help            Print this message\n"
                "",
                argv[0]);
}

static int
buffer_color_format(char *arg)
{
	int i;

	for (i = 0; i < strlen(arg); i++)
		arg[i] = tolower(arg[i]);
	if (!strcmp(arg, "uyvy"))
		return EGL_NATIVE_PIXFORMAT_UYVY_REL;
	else if (!strcmp(arg, "nv12"))
		return EGL_NATIVE_PIXFORMAT_NV12_REL;
	else if (!strcmp(arg, "nv16"))
		return EGL_NATIVE_PIXFORMAT_NV16_REL;
	else if (!strcmp(arg, "rgb565"))
		return EGL_NATIVE_PIXFORMAT_RGB565_REL;
	else if (!strcmp(arg, "argb1555"))
		return EGL_NATIVE_PIXFORMAT_ARGB1555_REL;
	else if (!strcmp(arg, "argb4444"))
		return EGL_NATIVE_PIXFORMAT_ARGB4444_REL;
	else
		fprintf(stderr, "Invalid color format. Select default color format: ARGB8888\n");

	return EGL_NATIVE_PIXFORMAT_ARGB8888_REL;
}

static const char short_options[] = "p:f:ds:lh";
static const struct option long_options[] = {
	{"format", required_argument, NULL, 'f'},
	{"dump", no_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"pos", required_argument, NULL, 'p'},
	{"size", required_argument, NULL, 's'},
	{0, 0, 0, 0},
};

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	struct window  window  = { 0 };
	int ret = 0, format = EGL_NATIVE_PIXFORMAT_ARGB8888_REL;

	window.display = &display;
	display.window = &window;
	window.geometry.width  = WINDOW_WIDTH;
	window.geometry.height = WINDOW_HEIGHT;
	window.buffer_size = 32;
	window.frame_sync = 1;

	for (;;) {
		int idx;
		int c;
		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c) {
		case 'f':
			format = buffer_color_format(optarg);
			break;
		case 'd':
			enable_dump = 1;
			break;
		case 'p':
			if (sscanf(optarg, "%d,%d", &pos_x, &pos_y) == 2) {
				pos_y -= PANEL_HEIGHT;
				step_set_pos = 2;
				break;
			} else {
				usage(argc, argv);
				exit(EXIT_FAILURE);
			}
		case 's':
			window.geometry.width = atoi(optarg);
			window.geometry.height = atoi(argv[optind]);
			if ((window.geometry.width <= 0) ||
			    (window.geometry.height <= 0)) {
				usage(argc, argv);
				exit(EXIT_FAILURE);
			}
			break;
		case 'l':
			lock_image = 1;
			break;
		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		}
	}

	window.window_size = window.geometry;

	display.display = wl_display_connect(NULL);
	assert(display.display);

	if (!drm_connect(&display)) {
		fprintf(stderr, "drm_connect_failed\n");
		return 1;
	}
	window.bo = alloc_bo(&display, window.geometry.width, window.geometry.height);
	window.mmap = map_bo(window.bo);

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
	} else {
		window.geometry = window.window_size;
	}
	init_egl(&display, &window);
	create_surface(&window);
	init_gl(&window, format);
	init_gl_buffer(&window);

	display.cursor_surface =
		wl_compositor_create_surface(display.compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);
	sigaction(SIGTERM, &sigint, NULL);
	sigaction(SIGQUIT, &sigint, NULL);

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

	glDeleteTextures(1, &window.gl.texture);
	eglDestroyImageKHR(display.egl.dpy, window.eglImage);

	fini_egl(&display);

	unmap_bo(window.bo);
	free_bo(window.bo);
	drm_shutdown(&display);

	wl_surface_destroy(display.cursor_surface);
	if (display.cursor_theme)
		wl_cursor_theme_destroy(display.cursor_theme);

	if (display.xshell)
		zxdg_shell_v6_destroy(display.xshell);

	if (display.ivi_application)
		ivi_application_destroy(display.ivi_application);

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	if (display.output)
		wl_output_destroy(display.output);

	if (display.shm)
		wl_shm_destroy(display.shm);

	wl_registry_destroy(display.registry);
	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	return 0;
}
