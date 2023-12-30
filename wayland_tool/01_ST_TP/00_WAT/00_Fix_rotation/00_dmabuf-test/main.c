/* This program is based on simple-dmabuf-intel in weston 1.11.0
 *
 * The licence of simple-dmabuf-intel is following:
 *------------------------------------------------------
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
 * Copyright © 2014 Collabora Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <time.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <linux/input.h>

#include <wayland-client.h>
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "shared/os-compatibility.h"

#include "color_data.h"

struct window;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct zxdg_shell_v6 *shell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	int drm_fd;
	struct window *window;
	int yflip;
	struct color_bar *color_bar;
	struct wp_viewport *viewport;
	struct wp_viewporter *viewporter;
	struct wl_output *output;
};

#define MAX_PLANES	3
#define IMG_WIDTH	320
#define IMG_HEIGHT	240
#define PANEL_HEIGHT	32

struct buffer_plane {
	int fd;
	uint32_t handle;
	uint32_t fb_id;
	uint8_t *map;
	uint32_t size;
	uint32_t stride;
};

struct buffer;

struct color_bar {
	uint32_t format;
	char *color_name;
	void (*create_color_bar)(struct buffer *buf);
};

struct buffer {
	struct wl_buffer *buffer;
	int busy;
	int resized;

	int width;
	int height;
	struct color_bar *color_bar;
	int num_planes;
	struct buffer_plane planes[MAX_PLANES];
};

#define MAX_BUFFERS	3

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct buffer buffers[MAX_BUFFERS];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
	int fullscreen;
	bool wait_for_configure;
	bool initialized;
};

#define ARRAY_SIZE(a)		sizeof((a)) / sizeof((a)[0])

static int running = 1;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;
static char bufer_id = 0;
static char lock_image = 0;


static void free_buffer(int fd, struct buffer *my_buf);
static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct buffer *mybuf = data;

	mybuf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static int
drm_connect(struct display *d)
{
	int fd;

	fd = drmOpen("rcar-du", NULL);
	if (fd <= 0) {
		fprintf(stderr, "failed drmOpen\n");
		return 0;
	}
	d->drm_fd = fd;

	return 1;
}

static void
drm_shutdown(struct display *d)
{
	close(d->drm_fd);
}

static int
create_dumb(int fd, int width, int height, struct buffer_plane *plane)
{
	struct drm_mode_create_dumb create_arg;
	struct drm_mode_destroy_dumb destroy_arg;
	int ret;

	memset(&create_arg, 0, sizeof(create_arg));
	create_arg.bpp = 32;
	create_arg.width = width;
	create_arg.height = height;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
	if (ret)
		goto err_create;

	plane->handle = create_arg.handle;
	plane->size = create_arg.size;

	ret = drmModeAddFB(fd, width, height, 24, 32, create_arg.pitch,
			   plane->handle, &plane->fb_id);
	if (ret)
		goto err_add;

	ret = drmPrimeHandleToFD(fd, plane->handle, DRM_CLOEXEC, &plane->fd);
	if (ret)
		goto err_fd;

	return 0;

err_fd:
	drmModeRmFB(fd, plane->fb_id);
err_add:
	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = plane->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
err_create:
	return -1;
}

static int
alloc_bo(struct display *d, struct buffer *my_buf)
{
	int pw[MAX_PLANES], ph[MAX_PLANES];
	struct buffer_plane *planes = my_buf->planes;
	int w = my_buf->width;
	int h = my_buf->height;
	int i, err;

	my_buf->color_bar = d->color_bar;

	switch (my_buf->color_bar->format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		my_buf->num_planes = 1;
		pw[0] = w;
		ph[0] = h;
		planes[0].stride = w * 2;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
		my_buf->num_planes = 2;
		ph[0] = h;
		ph[1] = h / 2;
		planes[0].stride = w;
		planes[1].stride = w + (w % 2);
		for (i = 0; i < my_buf->num_planes; i++)
			pw[i] = w;
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		my_buf->num_planes = 2;
		planes[0].stride = w;
		planes[1].stride = w + (w % 2);
		for (i = 0; i < my_buf->num_planes; i++) {
			pw[i] = w;
			ph[i] = h;
		}
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		my_buf->num_planes = 3;
		planes[0].stride = pw[0] = w;
		pw[1] = pw[2] = w / 2;
		planes[1].stride = planes[2].stride = w / 2 + (w % 2);
		ph[0] = h;
		ph[1] = ph[2] = h / 2;
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		my_buf->num_planes = 3;
		pw[0] = planes[0].stride = w;
		pw[1] = pw[2] = w / 2;
		planes[1].stride = planes[2].stride = w / 2 + (w % 2);
		for (i = 0; i < my_buf->num_planes; i++)
			ph[i] = h;
		break;
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		my_buf->num_planes = 3;
		for (i = 0; i < my_buf->num_planes; i++) {
			pw[i] = w;
			ph[i] = h;
			planes[i].stride = pw[i];
		}
		break;
	default:
		my_buf->num_planes = 1;
		pw[0] = w;
		ph[0] = h;
		planes[0].stride = w * 4;
	}

	for (i = 0; i < my_buf->num_planes; i++) {
		err = create_dumb(d->drm_fd, pw[i], ph[i], &my_buf->planes[i]);
		if (err) {
			fprintf(stderr, "bo create failed\n");
			return 0;
		}
	}

	return 1;
}

static void
free_bo(int fd, struct buffer *my_buf)
{
	struct drm_mode_destroy_dumb destroy_arg;
	int i;

	for (i = 0; i < my_buf->num_planes; i++) {
		drmModeRmFB(fd, my_buf->planes[i].fb_id);
		memset(&destroy_arg, 0, sizeof(destroy_arg));
		destroy_arg.handle = my_buf->planes[i].handle;
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
	}
}

static int
map_bo(int fd, struct buffer *my_buf)
{
	struct drm_mode_map_dumb map_arg;
	int err, i;

	for (i = 0; i < my_buf->num_planes; i++) {
		struct buffer_plane *p = &my_buf->planes[i];
		memset(&map_arg, 0, sizeof map_arg);
		map_arg.handle = p->handle;
		err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
		if (err) {
			fprintf(stderr, "memory map failed.\n");
			return 0;
		}
		p->map = mmap(NULL, p->size, PROT_READ | PROT_WRITE,
			      MAP_SHARED, fd, map_arg.offset);
		if (p->map == MAP_FAILED) {
			fprintf(stderr, "memory map failed.\n");
			return 0;
		}
	}

	return 1;
}

static void
draw_color_bar_rgb32(struct buffer *buf, uint32_t color[][7], uint32_t black)
{
	uint32_t *p = (uint32_t *)buf->planes[0].map;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color[0][x * 7 / width];
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++)
			*p++ = color[1][x * 7 / width];
	}
	for (y = 0; y < height * 3 / 12; y++) {
		for (x = 0; x < width; x++) {
			if (x < 4 * width / 6)
				*p++ = color[2][x * 6 / width];
			else if (x < 4 * width / 6 + width / 12)
				*p++ = color[2][4];
			else
				if (lock_image)
					*p++ = ((x + y + bufer_id) % 3) * 0xFFFFFFFF;
				else
					*p++ = rand() % 2 == 0 ? black : 0xFFFFFFFF;
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_argb32(struct buffer *buf)
{
	draw_color_bar_rgb32(buf, color_data_rgb[COLOR_DATA_XRGB32],
			     0xFF000000);
}

static void
create_color_bar_abgr32(struct buffer *buf)
{
	draw_color_bar_rgb32(buf, color_data_rgb[COLOR_DATA_XBGR32],
			     0xFF000000);
}

static void
create_color_bar_bgra32(struct buffer *buf)
{
	draw_color_bar_rgb32(buf, color_data_rgb[COLOR_DATA_BGRX32],
			     0x000000FF);
}

static void
set_packed_yuv(uint8_t *p, uint8_t yuv[], uint32_t format)
{
	uint8_t y = yuv[0];
	uint8_t u = yuv[1];
	uint8_t v = yuv[2];

	switch (format) {
	case DRM_FORMAT_YUYV:
		*p++ = y;
		*p++ = u;
		*p++ = y;
		*p++ = v;
		break;
	case DRM_FORMAT_YVYU:
		*p++ = y;
		*p++ = v;
		*p++ = y;
		*p++ = u;
		break;
	case DRM_FORMAT_UYVY:
		*p++ = u;
		*p++ = y;
		*p++ = v;
		*p++ = y;
		break;
	case DRM_FORMAT_VYUY:
		*p++ = v;
		*p++ = y;
		*p++ = u;
		*p++ = y;
		break;
	}
}

static void
create_color_bar_packed_yuv(struct buffer *buf)
{
	uint8_t *p = buf->planes[0].map;
	int width = buf->width;
	int height = buf->height;
	int x, y, i;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x += 2) {
			i = x * 7 / width;
			set_packed_yuv(p, color_data_yuv[0][i],
				       buf->color_bar->format);
			p += 4;
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x += 2) {
			i = x * 7 / width;
			set_packed_yuv(p, color_data_yuv[1][i],
				       buf->color_bar->format);
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
			set_packed_yuv(p, color_data_yuv[2][i],
				       buf->color_bar->format);
			p += 4;
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_nv12(struct buffer *buf)
{
	uint8_t *p = buf->planes[0].map;
	uint8_t *uv = buf->planes[1].map;
	int idx1 = (buf->color_bar->format == DRM_FORMAT_NV12) ? 1 : 2;
	int idx2 = (buf->color_bar->format == DRM_FORMAT_NV12) ? 2 : 1;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color_data_yuv[0][x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color_data_yuv[0][x * 7 / width][idx1];
				*uv++ = color_data_yuv[0][x * 7 / width][idx2];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color_data_yuv[1][x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color_data_yuv[1][x * 7 / width][idx1];
				*uv++ = color_data_yuv[1][x * 7 / width][idx2];
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
			*p++ = color_data_yuv[2][i][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*uv++ = color_data_yuv[2][i][idx1];
				*uv++ = color_data_yuv[2][i][idx2];
			}
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_nv16(struct buffer *buf)
{
	uint8_t *p = buf->planes[0].map;
	uint8_t *uv = buf->planes[1].map;
	int idx1 = (buf->color_bar->format == DRM_FORMAT_NV16) ? 1 : 2;
	int idx2 = (buf->color_bar->format == DRM_FORMAT_NV16) ? 2 : 1;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color_data_yuv[0][x * 7 / width][0];
			if (x % 2 == 0) {
				*uv++ = color_data_yuv[0][x * 7 / width][idx1];
				*uv++ = color_data_yuv[0][x * 7 / width][idx2];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p++ = color_data_yuv[1][x * 7 / width][0];
			if (x % 2 == 0) {
				*uv++ = color_data_yuv[1][x * 7 / width][idx1];
				*uv++ = color_data_yuv[1][x * 7 / width][idx2];
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
			*p++ = color_data_yuv[2][i][0];
			if (x % 2 == 0) {
				*uv++ = color_data_yuv[2][i][idx1];
				*uv++ = color_data_yuv[2][i][idx2];
			}
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_yuv420(struct buffer *buf)
{
	uint8_t *p1 = buf->planes[0].map;
	uint8_t *p2 = buf->planes[1].map;
	uint8_t *p3 = buf->planes[2].map;
	int idx1 = (buf->color_bar->format == DRM_FORMAT_YUV420) ? 1 : 2;
	int idx2 = (buf->color_bar->format == DRM_FORMAT_YUV420) ? 2 : 1;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[0][x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*p2++ = color_data_yuv[0][x * 7 / width][idx1];
				*p3++ = color_data_yuv[0][x * 7 / width][idx2];
			}
		}
		for (;x < buf->planes[0].stride; x++) {
		    *p1++ = 0;
		    if (y % 2 == 0 && x % 2 == 0) {
			*p2++ = 0;
			*p3++ = 0;
		    }
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[1][x * 7 / width][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*p2++ = color_data_yuv[1][x * 7 / width][idx1];
				*p3++ = color_data_yuv[1][x * 7 / width][idx2];
			}
		}
		for (;x < buf->planes[0].stride; x++) {
		    *p1++ = 0;
		    if (y % 2 == 0 && x % 2 == 0) {
			*p2++ = 0;
			*p3++ = 0;
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
			*p1++ = color_data_yuv[2][i][0];
			if (y % 2 == 0 && x % 2 == 0) {
				*p2++ = color_data_yuv[2][i][idx1];
				*p3++ = color_data_yuv[2][i][idx2];
			}
		}
		for (;x < buf->planes[0].stride; x++) {
		    *p1++ = 0;
		    if (y % 2 == 0 && x % 2 == 0) {
			*p2++ = 0;
			*p3++ = 0;
		    }
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_yuv422(struct buffer *buf)
{
	uint8_t *p1 = buf->planes[0].map;
	uint8_t *p2 = buf->planes[1].map;
	uint8_t *p3 = buf->planes[2].map;
	int idx1 = (buf->color_bar->format == DRM_FORMAT_YUV422) ? 1 : 2;
	int idx2 = (buf->color_bar->format == DRM_FORMAT_YUV422) ? 2 : 1;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[0][x * 7 / width][0];
			if (x % 2 == 0) {
				*p2++ = color_data_yuv[0][x * 7 / width][idx1];
				*p3++ = color_data_yuv[0][x * 7 / width][idx2];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[1][x * 7 / width][0];
			if (x % 2 == 0) {
				*p2++ = color_data_yuv[1][x * 7 / width][idx1];
				*p3++ = color_data_yuv[1][x * 7 / width][idx2];
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
			*p1++ = color_data_yuv[2][i][0];
			if (x % 2 == 0) {
				*p2++ = color_data_yuv[2][i][idx1];
				*p3++ = color_data_yuv[2][i][idx2];
			}
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_color_bar_yuv444(struct buffer *buf)
{
	uint8_t *p1 = buf->planes[0].map;
	uint8_t *p2 = buf->planes[1].map;
	uint8_t *p3 = buf->planes[2].map;
	int idx1 = (buf->color_bar->format == DRM_FORMAT_YUV444) ? 1 : 2;
	int idx2 = (buf->color_bar->format == DRM_FORMAT_YUV444) ? 2 : 1;
	int width = buf->width;
	int height = buf->height;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[0][x * 7 / width][0];
			*p2++ = color_data_yuv[0][x * 7 / width][idx1];
			*p3++ = color_data_yuv[0][x * 7 / width][idx2];
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width; x++) {
			*p1++ = color_data_yuv[1][x * 7 / width][0];
			*p2++ = color_data_yuv[1][x * 7 / width][idx1];
			*p3++ = color_data_yuv[1][x * 7 / width][idx2];
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
			*p1++ = color_data_yuv[2][i][0];
			*p2++ = color_data_yuv[2][i][idx1];
			*p3++ = color_data_yuv[2][i][idx2];
		}
	}
	bufer_id = (bufer_id + 1) % 2;
}

static void
create_image(struct buffer *my_buf)
{
	if (!my_buf->color_bar->create_color_bar)
		return;

	my_buf->color_bar->create_color_bar(my_buf);
}

static void
unmap_bo(struct buffer *my_buf)
{
	int i;

	for (i = 0; i < my_buf->num_planes; i++)
		munmap(my_buf->planes[i].map, my_buf->planes[i].size);
}

static void
free_buffer(int fd, struct buffer *my_buf)
{
	int i;

	wl_buffer_destroy(my_buf->buffer);
	free_bo(fd, my_buf);
	for (i = 0; i < my_buf->num_planes; i++)
		close(my_buf->planes[i].fd);
	memset(my_buf, 0x00, sizeof(struct buffer));
}

static void
free_buffers(struct window *window)
{
	int i;
	for (i = 0; i < MAX_BUFFERS; i++) {
		if (!window->buffers[i].buffer)
			continue;

		free_buffer(window->display->drm_fd, &window->buffers[i]);
	}
}

static void
create_succeeded(void *data,
		 struct zwp_linux_buffer_params_v1 *params,
		 struct wl_buffer *new_buffer)
{
	struct buffer *buffer = data;

	buffer->buffer = new_buffer;
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);

	zwp_linux_buffer_params_v1_destroy(params);
}

static void
create_failed(void *data, struct zwp_linux_buffer_params_v1 *params)
{
	struct buffer *buffer = data;

	buffer->buffer = NULL;
	running = 0;

	zwp_linux_buffer_params_v1_destroy(params);

	fprintf(stderr, "Error: zwp_linux_buffer_params.create failed.\n");
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
	create_succeeded,
	create_failed
};

static int
create_dmabuf_buffer(struct display *display, struct buffer *buffer,
		     int width, int height)
{
	struct zwp_linux_buffer_params_v1 *params;
	uint64_t modifier;
	uint32_t flags;
	int i;

	buffer->width = width;
	buffer->height = height;

	if (!alloc_bo(display, buffer)) {
		fprintf(stderr, "alloc_bo failed\n");
		goto error1;
	}

	if (!map_bo(display->drm_fd, buffer)) {
		fprintf(stderr, "map_bo failed\n");
		goto error2;
	}
	create_image(buffer);
	unmap_bo(buffer);

	/* We now have a dmabuf! It should contain 2x2 tiles (i.e. each tile
	 * is 256x256) of misc colours, and be mappable, either as ARGB8888, or
	 * XRGB8888. */
	modifier = 0;
	flags = display->yflip ? ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT : 0;

	params = zwp_linux_dmabuf_v1_create_params(display->dmabuf);
	for (i = 0; i < buffer->num_planes; i++) {
		zwp_linux_buffer_params_v1_add(params,
					       buffer->planes[i].fd,
					       i, /* plane_idx */
					       0, /* offset */
					       buffer->planes[i].stride,
					       modifier >> 32,
					       modifier & 0xffffffff);
	}
	zwp_linux_buffer_params_v1_add_listener(params, &params_listener,
						buffer);
	zwp_linux_buffer_params_v1_create(params, buffer->width, buffer->height,
					  buffer->color_bar->format, flags);

	return 0;

error2:
	free_bo(display->drm_fd, buffer);
error1:
	return -1;
}

static void
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *xdg_surface, uint32_t serial)
{
	struct window *window = data;

	zxdg_surface_v6_ack_configure(xdg_surface, serial);

	if (window->initialized && window->wait_for_configure)
		redraw(window, NULL, 0);
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
		window->width = width;
		window->height = height;
	} else {
		window->width = IMG_WIDTH;
		window->height = IMG_HEIGHT;
	}
	if (window->buffers[0].width != window->width ||
	    window->buffers[0].height != window->height) {
		int i;
		for (i = 0; i < MAX_BUFFERS; i++) {
			struct buffer *buf = &window->buffers[i];
			if (!buf->buffer)
				continue;
			buf->resized = 1;
		}
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

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;
	int i;
	int ret;

	window = calloc(1, sizeof *window);
	if (!window)
		return NULL;

	window->callback = NULL;
	window->display = display;
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface(display->compositor);

	if (display->viewporter)
		display->viewport = wp_viewporter_get_viewport(display->viewporter,
							    window->surface);

	if (display->shell) {
		window->xdg_surface =
			zxdg_shell_v6_get_xdg_surface(display->shell,
						  window->surface);

		assert(window->xdg_surface);

		zxdg_surface_v6_add_listener(window->xdg_surface,
					 &xdg_surface_listener, window);

		window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
				      &xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "dmabuf-test");
		window->wait_for_configure = true;
		wl_surface_commit(window->surface);
	} else if (display->fshell) {
		zwp_fullscreen_shell_v1_present_surface(display->fshell,
							window->surface,
							ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
							NULL);
	} else {
		assert(0);
	}

	for (i = 0; i < MAX_BUFFERS; ++i) {
		ret = create_dmabuf_buffer(display, &window->buffers[i],
					   width, height);

		if (ret < 0)
			return NULL;
	}

	return window;
}

static void
destroy_window(struct window *window)
{
	if (window->callback)
		wl_callback_destroy(window->callback);

	free_buffers(window);

	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);
	wl_surface_destroy(window->surface);
	free(window);
}

static struct buffer *
window_next_buffer(struct window *window)
{
	struct buffer *buffer = NULL;
	int i;

	for (i = 0; i < MAX_BUFFERS; i++) {
		if (!window->buffers[i].busy) {
			buffer = &window->buffers[i];
			break;
		}
	}

	if (!buffer)
		return NULL;

	if (buffer->resized) {
		free_buffer(window->display->drm_fd, buffer);
		create_dmabuf_buffer(window->display, buffer, window->width,
				     window->height);
		wl_display_roundtrip(window->display->display);
	}
	return buffer;
}

static void
dummy_buffer_release(void *data, struct wl_buffer *buffer)
{
	wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener dummy_buffer_listener = {
	dummy_buffer_release
};

static struct wl_buffer *
create_dummy_buffer(struct display *display, int width, int height)
{
	struct wl_shm_pool *pool;
	int fd, size, stride;
	struct wl_buffer *buffer;

	stride = width * 4;
	size = stride * height;

	fd = os_create_anonymous_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
			size);
		return NULL;
	}

	pool = wl_shm_create_pool(display->shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0,
					   width, height, stride,
					   WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(buffer, &dummy_buffer_listener, NULL);
	wl_shm_pool_destroy(pool);
	close(fd);

	return buffer;
}

static void
set_dummy_buffer(struct window *window)
{
	struct wl_buffer *buffer;

	buffer = create_dummy_buffer(window->display, dummy_w, dummy_h);
	if (!buffer) {
		fprintf(stderr, "create_dummy_buffer failed.\n");
		abort();
	}
	wl_surface_attach(window->surface, buffer, 0, 0);
	wl_surface_damage(window->surface, 0, 0, dummy_w, dummy_h);
	wl_surface_commit(window->surface);
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct buffer *buffer;

	if (step_set_pos) {
		set_dummy_buffer(window);
	}

	buffer = window_next_buffer(window);
	if (!buffer) {
		fprintf(stderr,
			!callback ? "Failed to create the first buffer.\n" :
			"Both buffers busy at redraw(). Server bug?\n");
		abort();
	}

	if (window->fullscreen) {
		struct wl_region *region = wl_compositor_create_region(
			window->display->compositor);
		wl_region_add(region, 0, 0, window->width, window->height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	if (step_set_pos) {
		wl_surface_attach(window->surface, buffer->buffer,
				  pos_x, pos_y);
		step_set_pos--;
	} else {
		wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	}

	wl_surface_damage(window->surface, 0, 0, window->width, window->height);

	if (callback)
		wl_callback_destroy(callback);

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);
	wl_surface_commit(window->surface);
	buffer->busy = 1;
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

static void
dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf, uint32_t format)
{
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
	dmabuf_format
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
do_scaling(struct display *d)
{
	static int count = 0;

	count++;
	if (count % 3 == 0) {
		wp_viewport_set_destination(d->viewport, -1, -1);
		fprintf(stderr, "normal size\n");
	} else if (count % 3 == 1) {
		wp_viewport_set_destination(d->viewport,
					    IMG_WIDTH * 2, IMG_HEIGHT *2);
		fprintf(stderr, "double size\n");
	} else {
		wp_viewport_set_destination(d->viewport,
					    IMG_WIDTH / 2, IMG_HEIGHT / 2);
		fprintf(stderr, "harf size\n");
	}
}

static void
do_fullscreen(struct display *d)
{
	if (d->viewport)
		wp_viewport_set_destination(d->viewport, -1, -1);

	if (d->window->fullscreen)
		zxdg_toplevel_v6_unset_fullscreen(d->window->xdg_toplevel);
	else
		zxdg_toplevel_v6_set_fullscreen(d->window->xdg_toplevel, NULL);
}

static void
do_resize(struct display *d)
{
	int i;

	if (d->viewport)
		wp_viewport_set_destination(d->viewport, -1, -1);

	if (d->window->width == IMG_WIDTH) {
		d->window->width = IMG_WIDTH * 2;
		d->window->height = IMG_HEIGHT * 2;
	} else {
		d->window->width = IMG_WIDTH;
		d->window->height = IMG_HEIGHT;
	}

	for (i = 0; i < MAX_BUFFERS; i++) {
		struct buffer *buf = &d->window->buffers[i];
		if (!buf->buffer)
			continue;
		buf->resized = 1;
	}
}

static void do_clipping(struct display *d)
{
	static int count = 0;
	wl_fixed_t x, y, w, h;

	if (!d->viewport)
		return;

	if (count % 2 == 0) {
		x = wl_fixed_from_double(50);
		y = wl_fixed_from_double(50);
		w = wl_fixed_from_double(250);
		h = wl_fixed_from_double(180);
	} else {
		x = y = w = h = wl_fixed_from_int(-1);
	}
	wp_viewport_set_source(d->viewport, x, y, w, h);
	count++;
}

static void
do_transform(struct display *d)
{
	static int count = 1;
	int32_t transform;

	transform = count % 8;
	wl_surface_set_buffer_transform(d->window->surface, transform);
	fprintf(stderr, "transform = %d\n", transform);
	count++;
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = data;

	if (!d->shell)
		return;

	if (key == KEY_F10 && state) {
		do_scaling(d);
	} else if (key == KEY_F11 && state) {
		do_fullscreen(d);
	} else if (key == KEY_F12 && state) {
		do_resize(d);
	} else if (key == KEY_F9 && state) {
		do_clipping(d);
	} else if (key == KEY_F8 && state) {
		do_transform(d);
	} else if (key == KEY_ESC && state) {
		running = 0;
	}
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

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
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
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 3);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_registry_bind(registry,
					   id, &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, id,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
		}
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->shell = wl_registry_bind(registry,
					    id, &zxdg_shell_v6_interface, 1);
		zxdg_shell_v6_add_listener(d->shell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
		d->dmabuf = wl_registry_bind(registry,
					     id, &zwp_linux_dmabuf_v1_interface, 1);
		zwp_linux_dmabuf_v1_add_listener(d->dmabuf, &dmabuf_listener, d);
	} else if (strcmp(interface, "wp_viewporter") == 0) {
		d->viewporter = wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
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

static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	if (display == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_roundtrip(display->display);
	if (display->dmabuf == NULL) {
		fprintf(stderr, "No zwp_linux_dmabuf global\n");
		exit(1);
	}

	wl_display_roundtrip(display->display);

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->dmabuf)
		zwp_linux_dmabuf_v1_destroy(display->dmabuf);

	if (display->shell)
		zxdg_shell_v6_destroy(display->shell);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	if (display->output)
		wl_output_destroy(display->output);

	if (display->shm)
		wl_shm_destroy(display->shm);

	wl_registry_destroy(display->registry);
	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);
}

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
		"-f | --format color		Set color format\n"
		"-y | --yflip			Set Y-FLIP flag for dmabuf\n"
		"-p <x>,<y>| --pos <x>,<y>	Set application position\n"
		"-l | --lock			Use fix image for random part\n"
		"-h | --help			Print this message\n"
		"",
		argv[0]);
}

static struct color_bar support_formats[] = {
	{
		.format = DRM_FORMAT_XRGB8888,
		.color_name = "xrgb8888",
		.create_color_bar = create_color_bar_argb32,
	},
	{
		.format = DRM_FORMAT_ARGB8888,
		.color_name = "argb8888",
		.create_color_bar = create_color_bar_argb32,
	},
	{
		.format = DRM_FORMAT_XBGR8888,
		.color_name = "xbgr8888",
		.create_color_bar = create_color_bar_abgr32,
	},
	{
		.format = DRM_FORMAT_ABGR8888,
		.color_name = "abgr8888",
		.create_color_bar = create_color_bar_abgr32,
	},
	{
		.format = DRM_FORMAT_BGRX8888,
		.color_name = "bgrx8888",
		.create_color_bar = create_color_bar_bgra32,
	},
	{
		.format = DRM_FORMAT_BGRA8888,
		.color_name = "bgra8888",
		.create_color_bar = create_color_bar_bgra32,
	},
	{
		.format = DRM_FORMAT_YUYV,
		.color_name = "yuyv",
		.create_color_bar = create_color_bar_packed_yuv,
	},
	{
		.format = DRM_FORMAT_YVYU,
		.color_name = "yvyu",
		.create_color_bar = create_color_bar_packed_yuv,
	},
	{
		.format = DRM_FORMAT_UYVY,
		.color_name = "uyvy",
		.create_color_bar = create_color_bar_packed_yuv,
	},
	{
		.format = DRM_FORMAT_VYUY,
		.color_name = "vyuy",
		.create_color_bar = create_color_bar_packed_yuv,
	},
	{
		.format = DRM_FORMAT_NV12,
		.color_name = "nv12",
		.create_color_bar = create_color_bar_nv12,
	},
	{
		.format = DRM_FORMAT_NV21,
		.color_name = "nv21",
		.create_color_bar = create_color_bar_nv12,
	},
	{
		.format = DRM_FORMAT_NV16,
		.color_name = "nv16",
		.create_color_bar = create_color_bar_nv16,
	},
	{
		.format = DRM_FORMAT_NV61,
		.color_name = "nv61",
		.create_color_bar = create_color_bar_nv16,
	},
	{
		.format = DRM_FORMAT_YUV420,
		.color_name = "yuv420",
		.create_color_bar = create_color_bar_yuv420,
	},
	{
		.format = DRM_FORMAT_YVU420,
		.color_name = "yvu420",
		.create_color_bar = create_color_bar_yuv420,
	},
	{
		.format = DRM_FORMAT_YUV422,
		.color_name = "yuv422",
		.create_color_bar = create_color_bar_yuv422,
	},
	{
		.format = DRM_FORMAT_YVU422,
		.color_name = "yvu422",
		.create_color_bar = create_color_bar_yuv422,
	},
	{
		.format = DRM_FORMAT_YUV444,
		.color_name = "yuv444",
		.create_color_bar = create_color_bar_yuv444,
	},
	{
		.format = DRM_FORMAT_YVU444,
		.color_name = "yvu444",
		.create_color_bar = create_color_bar_yuv444,
	},
	/* for error check */
	{
		.format = DRM_FORMAT_AYUV,
		.color_name = "ayuv",
		.create_color_bar = NULL,
	},
};

static int
buffer_color_format(char *arg)
{
	int i;

	for (i = 0; i < strlen(arg); i++)
		arg[i] = tolower(arg[i]);

	for (i = 0; i < ARRAY_SIZE(support_formats); i++) {
	    if (!strcmp(arg, support_formats[i].color_name))
		return i;
	}

	fprintf(stderr, "Invalid color format. Select default color format: XRGB8888\n");
	return 0;
}

static const char short_options[] = "p:f:ylh";
static const struct option long_options[] = {
	{"format", required_argument, NULL, 'f'},
	{"yflip", no_argument, NULL, 'y'},
	{"pos", required_argument, NULL, 'p'},
	{"lock", no_argument, NULL, 'l'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret = 0;
	int yflip = 0;
	int format_index = 0; /* default XRGB8888 */

	for (;;) {
		int idx;
		int c;
		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c) {
		case 0:
			break;
		case 'f':
			format_index = buffer_color_format(optarg);
			break;
		case 'y':
			yflip = 1;
			break;
		case 'p':
			if (sscanf(optarg, "%d,%d", &pos_x, &pos_y) == 2) {
				pos_y -= PANEL_HEIGHT;
				step_set_pos = 1;
				break;
			} else {
				usage(argc, argv);
				exit(1);
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

	display = create_display();
	if (step_set_pos) {
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(1);
		}
	}
	display->yflip = yflip;
	display->color_bar = &support_formats[format_index];

	if (!drm_connect(display)) {
		fprintf(stderr, "drm_connect_failed\n");
		return 1;
	}

	window = create_window(display, IMG_WIDTH, IMG_HEIGHT);
	if (!window)
		return 1;

	display->window = window;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	srand(time(NULL));

	/* Here we retrieve the linux-dmabuf objects, or error */
	wl_display_roundtrip(display->display);

	if (!running)
		return 1;

	window->initialized = true;

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-dmabuf exiting\n");
	destroy_window(window);
	drm_shutdown(display);
	destroy_display(display);

	return 0;
}
