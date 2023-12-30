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
#include <libkms/libkms.h>
#include <drm_fourcc.h>
#include <linux/input.h>

#include <wayland-client.h>
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "shared/os-compatibility.h"

struct window;

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct zxdg_shell_v6 *xshell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct zwp_linux_dmabuf_v1 *dmabuf;
	int drm_fd;
	struct kms_driver *kms;
	struct window *window;
	uint32_t buffer_color_format;
	struct wl_output *output;
	struct wp_viewport *viewport;
	struct wp_viewporter *viewporter;
};

#define MAX_PLANES	3
#define IMG_WIDTH	320
#define IMG_HEIGHT	240
#define PANEL_HEIGHT	32

static int img_width = IMG_WIDTH;
static int img_height = IMG_HEIGHT;
static int test_mode = 0;
static int odd_damage = 0;
static char bufer_id = 0;
static char lock_image = 0;

struct buffer {
	struct wl_buffer *buffer;
	int busy;
	int resized;

	int width;
	int height;
	int format;
	int num_planes;
	struct {
		struct kms_bo *bo;
		int fd;
		uint8_t *mmap;
		uint32_t stride;
		uint32_t len;
	} planes[MAX_PLANES];
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
	int fullscreen, opaque;
	bool wait_for_configure;
	bool initialized;
};

struct rectangle {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};
static struct rectangle clp_rect = {50, 50, 250, 180};

static int running = 1;
uint32_t yflip = 0;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;

static void redraw(void *data, struct wl_callback *callback, uint32_t time);
static void free_buffer(struct buffer *my_buf);

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

static int
alloc_bo(struct display *d, struct buffer *my_buf)
{
	unsigned attr[] = {
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_TERMINATE_PROP_LIST
	};
	int width[MAX_PLANES], height[MAX_PLANES];
	int i, err;

	my_buf->format = d->buffer_color_format;

	switch (my_buf->format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		my_buf->num_planes = 1;
		width[0] = my_buf->width;
		height[0] = my_buf->height;
		my_buf->planes[0].stride = (my_buf->width * 2 + 3) & ~0x03;
		my_buf->planes[0].len = my_buf->planes[0].stride * height[0];
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
		my_buf->num_planes = 2;
		height[0] = my_buf->height;
		height[1] = my_buf->height / 2;
		for (i = 0; i < my_buf->num_planes; i++) {
			width[i] = my_buf->width;
			my_buf->planes[i].stride = (width[i] + 3) & ~0x03;
			my_buf->planes[i].len = my_buf->planes[i].stride * height[i];
		}
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		my_buf->num_planes = 2;
		for (i = 0; i < my_buf->num_planes; i++) {
			width[i] = my_buf->width;
			height[i] = my_buf->height;
			my_buf->planes[i].stride = (width[i] + 3) & ~0x03;
			my_buf->planes[i].len = my_buf->planes[i].stride * height[i];
		}
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		my_buf->num_planes = 3;
		width[0] = my_buf->width;
		width[1] = width[2] = my_buf->width / 2;
		height[0] = my_buf->height;
		height[1] = height[2] = my_buf->height / 2;
		for (i = 0; i < my_buf->num_planes; i++) {
			my_buf->planes[i].stride = (width[i] + 3) & ~0x03;
			my_buf->planes[i].len = my_buf->planes[i].stride * height[i];
		}
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		my_buf->num_planes = 3;
		width[0] = my_buf->width;
		width[1] = width[2] = my_buf->width / 2;
		for (i = 0; i < my_buf->num_planes; i++) {
			height[i] = my_buf->height;
			my_buf->planes[i].stride = (width[i] + 3) & ~0x03;
			my_buf->planes[i].len = my_buf->planes[i].stride * height[i];
		}
		break;
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		my_buf->num_planes = 3;
		for (i = 0; i < my_buf->num_planes; i++) {
			width[i] = my_buf->width;
			height[i] = my_buf->height;
			my_buf->planes[i].stride = (width[i] + 3) & ~0x03;
			my_buf->planes[i].len = my_buf->planes[i].stride * height[i];
		}
		break;
	default:	/* DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888,
						DRM_FORMAT_BGRA8888 */
		my_buf->num_planes = 1;
		width[0] = my_buf->width;
		height[0] = my_buf->height;
		my_buf->planes[0].stride = (my_buf->width * 4 + 7) & ~0x07;
		my_buf->planes[0].len = my_buf->planes[0].stride * height[0];
	}

	for (i = 0; i < my_buf->num_planes; i++) {
		attr[3] = width[i];
		attr[5] = height[i];
		err = kms_bo_create(d->kms, attr, &my_buf->planes[i].bo);
		if (err) {
			fprintf(stderr, "bo create failed\n");
			return 0;
		}
	}

	return 1;
}

static void
free_bo(struct buffer *my_buf)
{
	int i;

	for (i = 0; i < my_buf->num_planes; i++)
		kms_bo_destroy(&my_buf->planes[i].bo);
}

static int
map_bo(struct buffer *my_buf)
{
	int err, i;

	for (i = 0; i < my_buf->num_planes; i++) {
		err = kms_bo_map(my_buf->planes[i].bo,
				 (void**)&my_buf->planes[i].mmap);
		if (err) {
			fprintf(stderr, "memory map failed.\n");
			return 0;
		}
	}

	return 1;
}

/* DRM_FORMAT_XRGB8888 only */
static void
copy_buffer_tiled(uint8_t *buf, struct buffer *my_buf)
{
	uint8_t *src;
	uint8_t *dst;
	int xcount, xmod;
	int x, y;

	xcount = my_buf->width / img_width;
	xmod = my_buf->width % img_width;

	src = buf;
	for (y = 0; y < my_buf->height; y++) {
		if (y % img_height == 0)
			src = buf;
		dst = my_buf->planes[0].mmap + my_buf->planes[0].stride * y;
		for (x = 0; x < xcount; x++) {
			memcpy(dst, src, img_width * 4);
			dst += img_width * 4;
		}
		if (xmod)
			memcpy(dst, src, xmod * 4);
		src += img_width * 4;
	}
}

static void
copy_buffer(uint8_t *buf, struct buffer *my_buf)
{
	uint8_t *p = buf;
	int i;

	for (i = 0; i < my_buf->num_planes; i++) {
		memcpy(my_buf->planes[i].mmap, p, my_buf->planes[i].len);
		p += my_buf->planes[i].len;
	}
}

static void
draw_color_bar_rgba32(struct buffer *buf, uint32_t *color1, uint32_t *color2,
		      uint32_t *color3, uint32_t black)
{
	uint32_t *p = (uint32_t *)buf->planes[0].mmap;
	int width_align =  ((buf->width * 4 + 7) & ~0x07) / 4;
	int width = buf->width;
	int height = buf->height;
	int height_remain = 0;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
			} else {
				*p++ = color1[x * 7 / width];
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
			} else {
				*p++ = color2[x * 7 / width];
			}
		}
	}
	height_remain = height - (height * 8 / 12) - (height * 1 / 12) - (height * 3 / 12);
	for (y = 0; y < height * 3 / 12 + height_remain; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
			} else {
				if (x < 4 * width / 6)
					*p++ = color3[x * 6 / width];
				else if (x < 4 * width / 6 + width / 12)
					*p++ = color3[4];
				else
					if (lock_image)
						*p++ = (x + y + bufer_id) % MAX_BUFFERS == 0 ?
							black : 0xFFFFFFFF;
					else
						*p++ = rand() % 2 == 0 ?
							black : 0xFFFFFFFF;
			}
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % MAX_BUFFERS;
}

static void
draw_color_bar_xrgb(struct buffer *buf)
{
	uint32_t color1[] = {
		0xFFFFFFFF, 0xFFFFFF00, 0xFF00FFFF, 0xFF00FF00, 0xFFFF00FF,
		0xFFFF0000, 0xFF0000FF
	};
	uint32_t color2[] = {
		0xFF0000FF, 0xFF000000, 0xFFFF00FF, 0xFF000000, 0xFF00FFFF,
		0xFF000000, 0xFFFFFFFF
	};
	uint32_t color3[] = {
		0xFF000080, 0xFFFFFFFF, 0xFF0080FF, 0xFF000000, 0xFF131313
	};
	draw_color_bar_rgba32(buf, color1, color2, color3,0xFF000000);
}

static void
draw_color_bar_bgrx(struct buffer *buf)
{
	uint32_t color1[] = {
		0xFFFFFFFF, 0x00FFFFFF, 0xFFFF00FF, 0x00FF00FF, 0xFF00FFFF,
		0x0000FFFF, 0xFF0000FF
	};
	uint32_t color2[] = {
		0xFF0000FF, 0x000000FF, 0xFF00FFFF, 0x000000FF, 0xFFFF00FF,
		0x000000FF, 0xFFFFFFFF
	};
	uint32_t color3[] = {
		0x800000FF, 0xFFFFFFFF, 0xFF8000FF, 0x000000FF, 0x131313FF
	};
	draw_color_bar_rgba32(buf, color1, color2, color3,0x000000FF);
}

static void
draw_color_bar_argb(struct buffer *buf)
{
	uint32_t color1[] = {
		0x80808080, 0x80808000, 0x80008080, 0x80008000, 0x80800080,
		0x80800000, 0x80000080
	};
	uint32_t color2[] = {
		0x80000080, 0x80000000, 0x80800080, 0x80000000, 0x80008080,
		0x80000000, 0x80808080
	};
	uint32_t color3[] = {
		0x80000040, 0x80808080, 0x80004080, 0x80000000, 0x800A0A0A
	};
	draw_color_bar_rgba32(buf, color1, color2, color3,0xFF000000);
}

static void
draw_color_bar_bgra(struct buffer *buf)
{
	uint32_t color1[] = {
		0x80808080, 0x00808080, 0x80800080, 0x00800080, 0x80008080,
		0x00008080, 0x80000080
	};
	uint32_t color2[] = {
		0x80000080, 0x00000080, 0x80008080, 0x00000080, 0x80800080,
		0x00000080, 0x80808080
	};
	uint32_t color3[] = {
		0x40000080, 0x80808080, 0x80400080, 0x00000080, 0x0A0A0A80
	};
	draw_color_bar_rgba32(buf, color1, color2, color3,0x000000FF);
}

static void set_packed_yuv(uint8_t *p, uint8_t y, uint8_t u, uint8_t v,
			   uint32_t format)
{
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

static void draw_color_bar_packed_yuv(struct buffer *buf)
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
		{0x10, 0xC6, 0x15},
		{0xEB, 0x80, 0x80},
		{0x10, 0xEB, 0xC6},
		{0x00, 0x80, 0x80},
		{0x10, 0x80, 0x80}
	};
	uint8_t *p = buf->planes[0].mmap;
	int width_align = ((buf->width * 2 + 3) & ~0x03) / 2;
	int width = buf->width;
	int height = buf->height;
	int height_remain = 0;
	int x, y, i;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width_align; x += 2) {
			if(x >= width) {
				p += 2;
			} else {
				i = x * 7 / width;
				set_packed_yuv(p, color1[i][0], color1[i][1],
						   color1[i][2], buf->format);
				p += 4;
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width_align; x += 2) {
			if(x >= width) {
				p += 2;
			} else {
				i = x * 7 / width;
				set_packed_yuv(p, color2[i][0], color2[i][1],
						   color2[i][2], buf->format);
				p += 4;
			}
		}
	}
	height_remain = height - (height * 8 / 12) - (height * 1 / 12) - (height * 3 / 12);
	for (y = 0; y < height * 3 / 12 + height_remain; y++) {
		for (x = 0; x < width_align; x += 2) {
			if(x >= width) {
				p += 2;
			} else {
				if (x < 4 * width / 6)
					i = x * 6 / width;
				else if (x < 4 * width / 6 + width / 12)
					i = 4;
				else
					if (lock_image)
						i = (x + y + bufer_id) % MAX_BUFFERS == 0 ? 1 : 3;
					else
						i = rand() % 2 == 0 ? 1 : 3;
				set_packed_yuv(p, color3[i][0], color3[i][1],
					   color3[i][2], buf->format);
				p += 4;
			}
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % MAX_BUFFERS;
}

static void draw_color_bar_nv12(struct buffer *buf)
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
		{0x10, 0xC6, 0x15},
		{0xEB, 0x80, 0x80},
		{0x10, 0xEB, 0xC6},
		{0x00, 0x80, 0x80},
		{0x10, 0x80, 0x80}
	};
	uint8_t *p = buf->planes[0].mmap;
	uint8_t *uv = buf->planes[1].mmap;
	int idx1 = (buf->format == DRM_FORMAT_NV12) ? 1 : 2;
	int idx2 = (buf->format == DRM_FORMAT_NV12) ? 2 : 1;
	int width_align = (buf->width + 3) & ~0x03;
	int width = buf->width;
	int height = buf->height;
	int height_remain = 0;
	int x, y;

	for (y = 0; y < height * 8 / 12; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
				if (y % 2 == 0 && x % 2 == 0) {
					uv += 2;
				}
			} else {
				*p++ = color1[x * 7 / width][0];
				if (y % 2 == 0 && x % 2 == 0) {
					*uv++ = color1[x * 7 / width][idx1];
					*uv++ = color1[x * 7 / width][idx2];
				}
			}
		}
	}
	for (y = 0; y < height * 1 / 12; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
				if (y % 2 == 0 && x % 2 == 0) {
					uv += 2;
				}
			} else {
				*p++ = color2[x * 7 / width][0];
				if (y % 2 == 0 && x % 2 == 0) {
					*uv++ = color2[x * 7 / width][idx1];
					*uv++ = color2[x * 7 / width][idx2];
				}
			}
		}
	}
	height_remain = height - (height * 8 / 12) - (height * 1 / 12) - (height * 3 / 12);
	for (y = 0; y < height * 3 / 12 + height_remain; y++) {
		for (x = 0; x < width_align; x++) {
			if(x >= width) {
				p++;
				if (y % 2 == 0 && x % 2 == 0) {
					uv += 2;
				}
			} else {
				int i;
				if (x < 4 * width / 6)
					i = x * 6 / width;
				else if (x < 4 * width / 6 + width / 12)
					i = 4;
				else
					if (lock_image)
						i = (x + y + bufer_id) % MAX_BUFFERS == 0 ? 1 : 3;
					else
						i = rand() % 2 == 0 ? 1 : 3;
				*p++ = color3[i][0];
				if (y % 2 == 0 && x % 2 == 0) {
					*uv++ = color3[i][idx1];
					*uv++ = color3[i][idx2];
				}
			}
		}
	}
	if (lock_image)
		bufer_id = (bufer_id + 1) % MAX_BUFFERS;
}

static int
read_image(struct buffer *my_buf)
{
	int fd, size;
	char *fname;
	uint8_t *addr;

	switch (my_buf->format) {
	case DRM_FORMAT_XRGB8888:
		draw_color_bar_xrgb(my_buf);
		return 0;
	case DRM_FORMAT_ARGB8888:
		draw_color_bar_argb(my_buf);
		return 0;
	case DRM_FORMAT_BGRA8888:
		draw_color_bar_bgra(my_buf);
		return 0;
	case DRM_FORMAT_BGRX8888:
		draw_color_bar_bgrx(my_buf);
		return 0;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		draw_color_bar_packed_yuv(my_buf);
		return 0;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
		draw_color_bar_nv12(my_buf);
		return 0;
	case DRM_FORMAT_NV16:
		fname = "./images/img.nv16";
		size = img_width * img_height * 2;
		break;
	case DRM_FORMAT_NV61:
		fname = "./images/img.nv61";
		size = img_width * img_height * 2;
		break;
	case DRM_FORMAT_YUV420:
		fname = "./images/img.yuv420";
		size = img_width * img_height * 3 / 2;
		break;
	case DRM_FORMAT_YVU420:
		fname = "./images/img.yvu420";
		size = img_width * img_height * 3 / 2;
		break;
	case DRM_FORMAT_YUV422:
		fname = "./images/img.yuv422";
		size = img_width * img_height * 2;
		break;
	case DRM_FORMAT_YVU422:
		fname = "./images/img.yvu422";
		size = img_width * img_height * 2;
		break;
	case DRM_FORMAT_YUV444:
		fname = "./images/img.yuv444";
		size = img_width * img_height * 3;
		break;
	case DRM_FORMAT_YVU444:
		fname = "./images/img.yvu444";
		size = img_width * img_height * 3;
		break;
	default:
		return -1;
	}

	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Can't open '%s'\n", fname);
		return -1;
	}

	addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		close(fd);
		fprintf(stderr, "mmap failed\n");
		return -1;
	}

	if (my_buf->width != img_width || my_buf->height != img_height)
		copy_buffer_tiled(addr, my_buf);
	else
		copy_buffer(addr, my_buf);

	munmap(addr, size);
	close(fd);

	return 0;
}

static void
unmap_bo(struct buffer *my_buf)
{
	int i;

	for (i = 0; i < my_buf->num_planes; i++)
		kms_bo_unmap(my_buf->planes[i].bo);
}

static void
free_buffer(struct buffer *my_buf)
{
	int i;

	wl_buffer_destroy(my_buf->buffer);
	free_bo(my_buf);
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

		free_buffer(&window->buffers[i]);
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
param_test_for_linux_dmabuf(struct buffer *buffer, struct zwp_linux_buffer_params_v1 *params)
{
	uint64_t modifier = 0;
	uint32_t flags = 0;
	uint32_t offset = 0;
	uint32_t plane_idx, i;
	int width;
	int height;

	width = buffer->width;
	height = buffer->height;

	flags = yflip ? ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT : 0;

	switch (test_mode) {
	case 3:		/* ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE = 3 */
		close(buffer->planes[0].fd);
		buffer->planes[0].fd = -1;
		break;
	case 5:		/* ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS = 5 */
		width = 0;
		height = 0;
		break;
	case 6:		/* ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS = 6 */
		offset = buffer->width * buffer->height * 4;
		break;
	}

	for (i = 0; i < buffer->num_planes; i++) {
		if (test_mode == 1)	/* ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX = 1 */
			plane_idx = 1000;
		else if (test_mode == 2)/* ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET = 2 */
			plane_idx = 0;
		else
			plane_idx = i;

		zwp_linux_buffer_params_v1_add(params,
					       buffer->planes[i].fd,
					       plane_idx,
					       offset,
					       buffer->planes[i].stride,
					       modifier >> 32,
					       modifier & 0xffffffff);
	}
	zwp_linux_buffer_params_v1_add_listener(params, &params_listener,
						buffer);
	zwp_linux_buffer_params_v1_create(params, width, height,
					  buffer->format, flags);

	return 0;
}

static int
create_dmabuf_buffer(struct display *display, struct buffer *buffer,
		     int width, int height)
{
	struct zwp_linux_buffer_params_v1 *params;
	uint64_t modifier;
	uint32_t handle;
	int err, i;
	uint32_t flags;

	buffer->width = width;
	buffer->height = height;

	flags = yflip ? ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT : 0;

	if (!alloc_bo(display, buffer)) {
		fprintf(stderr, "alloc_bo failed\n");
		goto error1;
	}

	if (!map_bo(buffer)) {
		fprintf(stderr, "map_bo failed\n");
		goto error2;
	}
	if (read_image(buffer)) {
		fprintf(stderr, "read_image failed\n");
		goto error2;
	}
	unmap_bo(buffer);

	for (i = 0; i < buffer->num_planes; i++) {
		err = kms_bo_get_prop(buffer->planes[i].bo, KMS_HANDLE,
				      &handle);
		if (err) {
			fprintf(stderr, "get kms handle failed.\n");
			goto error2;
		}

		err = drmPrimeHandleToFD(display->drm_fd, handle, DRM_CLOEXEC,
					 &buffer->planes[i].fd);
		if (err) {
			fprintf(stderr, "get dmabuf fd failed.\n");
			goto error2;
		}

		if (buffer->planes[i].fd < 0) {
			fprintf(stderr, "error: dmabuf_fd < 0\n");
			goto error2;
		}
	}

	/* We now have a dmabuf! It should contain 2x2 tiles (i.e. each tile
	 * is 256x256) of misc colours, and be mappable, either as ARGB8888, or
	 * XRGB8888. */
	modifier = 0;

	params = zwp_linux_dmabuf_v1_create_params(display->dmabuf);

	if (test_mode) {
		param_test_for_linux_dmabuf(buffer, params);
		return 0;
	}

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
					  buffer->format,
					  flags);

	return 0;

error2:
	free_bo(buffer);
error1:
	return -1;
}

static void
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *xdg_surface,
			 uint32_t serial)
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
		window->width = img_width;
		window->height = img_height;
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
handle_xdg_toplevel_delete(void *data, struct zxdg_toplevel_v6 *xdg_toplevel)
{
	running = 0;
}

static const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
	handle_xdg_toplevel_configure,
	handle_xdg_toplevel_delete,
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

	if (display->xshell) {
		window->xdg_surface = zxdg_shell_v6_get_xdg_surface(display->xshell, window->surface);

		assert(window->xdg_surface);
		zxdg_surface_v6_add_listener(window->xdg_surface, &xdg_surface_listener, window);

		window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "dmabuf-test-fs");

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
		free_buffer(buffer);
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

	if (window->opaque || window->fullscreen) {
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

	if (odd_damage) {
		int width, height;
		static int first = 1;

		if (first) {
			width = window->width;
			height = window->height;
			first = 0;
		} else {
			width = (window->width & 2) ?
				window->width / 2: window->width / 2 + 1;
			height = (window->height & 2) ?
				window->height / 2: window->height / 2 + 1;
		}
		wl_surface_damage(window->surface, 0, 0, width, height);
	} else {
		wl_surface_damage(window->surface, 0, 0, window->width, window->height);
	}

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
xdg_do_fullscreen(struct display *d)
{
	struct wl_output *output = NULL;

	if (d->buffer_color_format != DRM_FORMAT_XRGB8888)
		return;

	if (d->viewport)
		wp_viewport_set_destination(d->viewport, -1, -1);

	if (test_mode == -1)	/* For test */
		output = (struct wl_output *)d;

	if (d->window->fullscreen)
		zxdg_toplevel_v6_unset_fullscreen(d->window->xdg_toplevel);
	else {
		zxdg_toplevel_v6_set_fullscreen(d->window->xdg_toplevel, output);
	}
}

static void do_clipping(struct display *d)
{
	static int count = 0;
	wl_fixed_t x, y, w, h;

	if (!d->viewport)
		return;

	if (count % 2 == 0) {
		x = wl_fixed_from_int(clp_rect.x);
		y = wl_fixed_from_int(clp_rect.y);
		w = wl_fixed_from_int(clp_rect.width);
		h = wl_fixed_from_int(clp_rect.height);
	} else {
		x = y = w = h = wl_fixed_from_int(-1);
	}
	wp_viewport_set_source(d->viewport, x, y, w, h);
	count++;
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
		xdg_do_fullscreen(d);
	} else if (key == KEY_F9 && state) {
		do_clipping(d);
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
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_registry_bind(registry,
					   id, &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->xshell = wl_registry_bind(registry,
					    id, &zxdg_shell_v6_interface, 1);
		zxdg_shell_v6_add_listener(d->xshell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, id,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
		}

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

	if (display->xshell)
		zxdg_shell_v6_destroy(display->xshell);

	if (display->output)
		wl_output_destroy(display->output);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

	if (display->seat)
		wl_seat_destroy(display->seat);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

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

static uint32_t
buffer_color_format(char *arg)
{
	int i;

	fprintf(stderr, "color format. %s\n", arg);
	for (i = 0; i < strlen(arg); i++)
		arg[i] = tolower(arg[i]);
	if (!strcmp(arg, "yuyv"))
		return DRM_FORMAT_YUYV;
	else if (!strcmp(arg, "yvyu"))
		return DRM_FORMAT_YVYU;
	else if (!strcmp(arg, "uyvy"))
		return DRM_FORMAT_UYVY;
	else if (!strcmp(arg, "vyuy"))
		return DRM_FORMAT_VYUY;
	else if (!strcmp(arg, "nv12"))
		return DRM_FORMAT_NV12;
	else if (!strcmp(arg, "nv21"))
		return DRM_FORMAT_NV21;
	else if (!strcmp(arg, "nv16"))
		return DRM_FORMAT_NV16;
	else if (!strcmp(arg, "nv61"))
		return DRM_FORMAT_NV61;
	else if (!strcmp(arg, "yuv420"))
		return DRM_FORMAT_YUV420;
	else if (!strcmp(arg, "yvu420"))
		return DRM_FORMAT_YVU420;
	else if (!strcmp(arg, "yuv422"))
		return DRM_FORMAT_YUV422;
	else if (!strcmp(arg, "yvu422"))
		return DRM_FORMAT_YVU422;
	else if (!strcmp(arg, "yuv444"))
		return DRM_FORMAT_YUV444;
	else if (!strcmp(arg, "yvu444"))
		return DRM_FORMAT_YVU444;
	else if (!strcmp(arg, "argb8888"))
		return DRM_FORMAT_ARGB8888;
	else if (!strcmp(arg, "bgra8888"))
		return DRM_FORMAT_BGRA8888;
	else if (!strcmp(arg, "xrgb8888"))
		return DRM_FORMAT_XRGB8888;
	else if (!strcmp(arg, "bgrx8888"))
		return DRM_FORMAT_BGRX8888;
	else
		fprintf(stderr, "Invalid color format. Select default color format: XRGB\n");

	return DRM_FORMAT_XRGB8888;
}

static int
get_img_size(int *idx, int argc, char **argv)
{
	int i = *idx;

	if (i + 2 >= argc)
		return 1;

	img_width = atol(argv[++i]);
	img_height = atol(argv[++i]);
	if ((img_width < 0) || (img_height < 0))
		return 1;
	*idx = i;
	return 0;
}

static int
get_clipping_area(int *idx, int argc, char **argv)
{
	int i = *idx;

	if (i + 4 >= argc)
		return 1;

	clp_rect.x = atol(argv[++i]);
	clp_rect.y = atol(argv[++i]);
	clp_rect.width = atol(argv[++i]);
	clp_rect.height = atol(argv[++i]);

	*idx = i;
	return 0;
}

static void
check_and_correct(uint32_t *format)
{
	switch (*format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		if (test_mode > 0) {	/* multiple planes are needed for the test */
			*format = DRM_FORMAT_NV12;
			fprintf(stderr, "Color format is changed into NV12 for test.\n");
		}
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		/* image data in these format has fixed size. */
		img_width = IMG_WIDTH;
		img_height = IMG_HEIGHT;
		fprintf(stderr, "Image size is changed into default size.\n");
		break;
	}
}

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret , i, opaque = 0;
	uint32_t format = DRM_FORMAT_XRGB8888;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-f") == 0) {
			if (++i >= argc)
				goto usage;
			format = buffer_color_format(argv[i]);
		} else if (strcmp(argv[i], "-s") == 0) {
			ret = get_img_size(&i, argc, argv);
		} else if (strcmp(argv[i], "-c") == 0) {
			ret = get_clipping_area(&i, argc, argv);
		} else if (strcmp(argv[i], "-y") == 0) {
			yflip = 1;
		} else if (strcmp(argv[i], "-t") == 0) {
			if (++i >= argc)
				goto usage;
			test_mode = atoi(argv[i]);
		} else if (strcmp(argv[i], "-p") == 0) {
			if (sscanf(argv[++i], "%d,%d", &pos_x, &pos_y) == 2) {
				pos_y -= PANEL_HEIGHT;
				step_set_pos = 1;
			} else {
				goto usage;
			}
		} else if (strcmp(argv[i], "-d") == 0) {
			odd_damage = 1;
		} else if (strcmp(argv[i], "-l") == 0) {
			lock_image = 1;
		} else if (strcmp(argv[i], "-o") == 0) {
			opaque = 1;
		} else
			goto usage;
		if (ret)
			goto usage;
	}

	check_and_correct(&format);

	display = create_display();
	if (step_set_pos) {
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(1);
		}
	}
	display->buffer_color_format = format;

	if (!drm_connect(display)) {
		fprintf(stderr, "drm_connect_failed\n");
		return 1;
	}

	window = create_window(display, img_width, img_height);
	if (!window)
		return 1;

	/* Set opaque */
	window->opaque = opaque;

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

	ret = 0;
	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-dmabuf exiting\n");
	destroy_window(window);
	drm_shutdown(display);
	destroy_display(display);

	return 0;

usage:
	fprintf(stderr, "Usage: %s [OPTIONS]\n\n"
		"   -f <format>\tSet color format\n"
		"   -s <w> <h>\tSet window width to <w> and window height to <h>\n"
		"   -c <x> <y> <w> <h>\tSet a clipping rectangle\n"
		"   -t <num> \tSet test mode to <num>\n"
		"   -d \t\tSet a damge rectangle that has an odd width and height\n"
		"   -p <x>,<y> \tSet pos x y for window\n"
		"   -l \t\tSet fix image for random part\n"
		"   -o \t\tCreate opaque surface\n"
		"   -y \t\tyflip invert\n\n", argv[0]);
	return 1;
}
