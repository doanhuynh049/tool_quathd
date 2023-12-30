
/*
 * Copyright ‘§ 2011 Benjamin Franzke
 * Copyright ‘§ 2010 Intel Corporation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>

#include <xf86drm.h>
#include <libkms.h>

#include <wayland-client.h>
#include "shared/os-compatibility.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "wayland-kms-client-protocol.h"

#define	SUPPORT_SHM_NUMBER	4
#define	SUPPORT_KMS_NUMBER	15
#define	MAX_PLANES		3
#define PANEL_HEIGHT	32

#define	RGBTOY(r,g,b)	((r) *  0.299 + (g) * 0.587 + (b) * 0.114)
#define	RGBTOU(r,g,b)	((r) * -0.169 - (g) * 0.332 + (b) * 0.500 + 128.0)
#define	RGBTOV(r,g,b)	((r) *  0.500 - (g) * 0.419 - (b) * 0.081 + 128.0)

#define	DEBUG_PRINT	0
#if DEBUG_PRINT
#define	PRINT(...)	printf(__VA_ARGS__)
#else
#define	PRINT(...)
#endif

#define	DEBUG_BUFFER	0
#if DEBUG_BUFFER
#define	PRINT_BUFFER(addr,size)	( {	uint8_t *top = (addr); int i = 0; \
					PRINT("print_buffer\n"); \
					for (i = 0; i < (size); i++) { \
						PRINT("[%02x]", *top); \
						top++; \
					} PRINT("\n");	} )
#else
#define	PRINT_BUFFER(addr,size)
#endif

enum color {
	COLOR_INIT = 0,
	COLOR_BULE,
	COLOR_GREEN,
	COLOR_RED
};

enum buff_type {
	TYPE_NONE = 0,
	TYPE_SHM,
	TYPE_KMS
};

struct display {
	struct wl_display	*display;
	struct wl_registry	*registry;
	struct wl_compositor	*compositor;
	struct zxdg_shell_v6 *xshell;
	struct wl_output *output;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct wl_shm		*shm;
	struct wl_kms		*kms;
	struct kms_driver	*kmsdrv;
	int			fd;
	int			authenticated;
};

struct buffer {
	struct wl_buffer	*buffer;
	enum buff_type		buffer_type;
	void			*addr[MAX_PLANES];
	int			busy;
	enum color		color_type;
	int			buffer_change;
	int			shm_buffer_size;
	struct kms_bo		*bo[MAX_PLANES];
};

struct window {
	struct display		*display;
	int			width, height;
	struct wl_surface	*surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct buffer		buffers[2];
	struct wl_callback	*callback;
	int			mode;
	int			num;
	int			count;
	int			max_exec[2];
	bool wait_for_configure;
	bool initialized;
};

typedef void (*FUNCPTR)(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);

struct buff_format {
	uint32_t	shm_format;
	uint32_t	kms_format;
	int		need_plane_num;
	int		pix_stride[MAX_PLANES];
	FUNCPTR		pfunc;
	char		prt_format[16];
};


static void redraw(void *data, struct wl_callback *callback, uint32_t time);

void set8888(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, int size);
void set888(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, int size);
void setYUV(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, int size);
void setNV(void *ydata, void *uvdata, uint8_t byte1, uint8_t byte2, uint8_t byte3, int ysize, int uvsize);
void setYV(void *ydata, void *snd_data, void *trd_data, uint8_t byte1, uint8_t byte2, uint8_t byte3,
		int ysize, int snd_size, int trd_size);

void drawARGB8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawXRGB8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawRGB565(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawYUYV(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawRGB888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawUYVY(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawNV12(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawNV16(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawNV21(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawNV61(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawRGB332(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawBGR888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawXBGR8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawYVYU(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawYUV420(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void drawNOP(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b);

const struct buff_format fmt_tbl[] = {
	{ WL_SHM_FORMAT_ARGB8888	, WL_KMS_FORMAT_ARGB8888	, 1	, {4, 0, 0}	, &drawARGB8888	, "ARGB8888"	},
	{ WL_SHM_FORMAT_XRGB8888	, WL_KMS_FORMAT_XRGB8888	, 1	, {4, 0, 0}	, &drawXRGB8888	, "XRGB8888"	},
	{ WL_SHM_FORMAT_RGB565		, WL_KMS_FORMAT_RGB565		, 1	, {2, 0, 0}	, &drawRGB565	, "RGB565"	},
	{ WL_SHM_FORMAT_YUYV		, WL_KMS_FORMAT_YUYV		, 1	, {2, 0, 0}	, &drawYUYV	, "YUYV"	},
	{ WL_SHM_FORMAT_RGB888		, WL_KMS_FORMAT_RGB888		, 1	, {3, 0, 0}	, &drawRGB888	, "RGB888"	},
	{ WL_SHM_FORMAT_UYVY		, WL_KMS_FORMAT_UYVY		, 1	, {2, 0, 0}	, &drawUYVY	, "UYVY"	},
	{ WL_SHM_FORMAT_NV12		, WL_KMS_FORMAT_NV12		, 2	, {1, 1, 0}	, &drawNV12	, "NV12"	},
	{ WL_SHM_FORMAT_NV16		, WL_KMS_FORMAT_NV16		, 2	, {1, 1, 0}	, &drawNV16	, "NV16"	},
	{ WL_SHM_FORMAT_NV21		, WL_KMS_FORMAT_NV21		, 2	, {1, 1, 0}	, &drawNV21	, "NV21"	},
	{ WL_SHM_FORMAT_NV61		, WL_KMS_FORMAT_NV61		, 2	, {1, 1, 0}	, &drawNV61	, "NV61"	},
	{ WL_SHM_FORMAT_RGB332		, WL_KMS_FORMAT_RGB332		, 1	, {1, 0, 0}	, &drawRGB332	, "RGB332"	},
	{ WL_SHM_FORMAT_BGR888		, WL_KMS_FORMAT_BGR888		, 1	, {3, 0, 0}	, &drawBGR888	, "BGR888"	},
	{ WL_SHM_FORMAT_XBGR8888	, WL_KMS_FORMAT_XBGR8888	, 1	, {4, 0, 0}	, &drawXBGR8888	, "XBGR8888"	},
	{ WL_SHM_FORMAT_YVYU		, WL_KMS_FORMAT_YVYU		, 1	, {2, 0, 0}	, &drawYVYU	, "YVYU"	},
	{ WL_SHM_FORMAT_YUV420		, WL_KMS_FORMAT_YUV420		, 3	, {1, 1, 1}	, &drawYUV420	, "YUV420"	},
	{ WL_SHM_FORMAT_C8		, WL_KMS_FORMAT_C8		, 1	, {1, 0, 0}	, &drawNOP	, "C8"		},
	{ WL_SHM_FORMAT_BGR233		, WL_KMS_FORMAT_BGR233		, 1	, {1, 0, 0}	, &drawNOP	, "BGR233"	},
	{ WL_SHM_FORMAT_XRGB4444	, WL_KMS_FORMAT_XRGB4444	, 1	, {2, 0, 0}	, &drawNOP	, "XRGB4444"	},
	{ WL_SHM_FORMAT_XBGR4444	, WL_KMS_FORMAT_XBGR4444	, 1	, {2, 0, 0}	, &drawNOP	, "XBGR4444"	},
	{ WL_SHM_FORMAT_RGBX4444	, WL_KMS_FORMAT_RGBX4444	, 1	, {2, 0, 0}	, &drawNOP	, "RGBX4444"	},
	{ WL_SHM_FORMAT_BGRX4444	, WL_KMS_FORMAT_BGRX4444	, 1	, {2, 0, 0}	, &drawNOP	, "BGRX4444"	},
	{ WL_SHM_FORMAT_ARGB4444	, WL_KMS_FORMAT_ARGB4444	, 1	, {2, 0, 0}	, &drawNOP	, "ARGB4444"	},
	{ WL_SHM_FORMAT_ABGR4444	, WL_KMS_FORMAT_ABGR4444	, 1	, {2, 0, 0}	, &drawNOP	, "ABGR4444"	},
	{ WL_SHM_FORMAT_RGBA4444	, WL_KMS_FORMAT_RGBA4444	, 1	, {2, 0, 0}	, &drawNOP	, "RGBA4444"	},
	{ WL_SHM_FORMAT_BGRA4444	, WL_KMS_FORMAT_BGRA4444	, 1	, {2, 0, 0}	, &drawNOP	, "BGRA4444"	},
	{ WL_SHM_FORMAT_XRGB1555	, WL_KMS_FORMAT_XRGB1555	, 1	, {2, 0, 0}	, &drawNOP	, "XRGB1555"	},
	{ WL_SHM_FORMAT_XBGR1555	, WL_KMS_FORMAT_XBGR1555	, 1	, {2, 0, 0}	, &drawNOP	, "XBGR1555"	},
	{ WL_SHM_FORMAT_RGBX5551	, WL_KMS_FORMAT_RGBX5551	, 1	, {2, 0, 0}	, &drawNOP	, "RGBX5551"	},
	{ WL_SHM_FORMAT_BGRX5551	, WL_KMS_FORMAT_BGRX5551	, 1	, {2, 0, 0}	, &drawNOP	, "BGRX5551"	},
	{ WL_SHM_FORMAT_ARGB1555	, WL_KMS_FORMAT_ARGB1555	, 1	, {2, 0, 0}	, &drawNOP	, "ARGB1555"	},
	{ WL_SHM_FORMAT_ABGR1555	, WL_KMS_FORMAT_ABGR1555	, 1	, {2, 0, 0}	, &drawNOP	, "ABGR1555"	},
	{ WL_SHM_FORMAT_RGBA5551	, WL_KMS_FORMAT_RGBA5551	, 1	, {2, 0, 0}	, &drawNOP	, "RGBA5551"	},
	{ WL_SHM_FORMAT_BGRA5551	, WL_KMS_FORMAT_BGRA5551	, 1	, {2, 0, 0}	, &drawNOP	, "BGRA5551"	},
	{ WL_SHM_FORMAT_BGR565		, WL_KMS_FORMAT_BGR565		, 1	, {2, 0, 0}	, &drawNOP	, "BGR565"	},
	{ WL_SHM_FORMAT_RGBX8888	, WL_KMS_FORMAT_RGBX8888	, 1	, {4, 0, 0}	, &drawNOP	, "RGBX8888"	},
	{ WL_SHM_FORMAT_BGRX8888	, WL_KMS_FORMAT_BGRX8888	, 1	, {4, 0, 0}	, &drawNOP	, "BGRX8888"	},
	{ WL_SHM_FORMAT_ABGR8888	, WL_KMS_FORMAT_ABGR8888	, 1	, {4, 0, 0}	, &drawNOP	, "ABGR8888"	},
	{ WL_SHM_FORMAT_RGBA8888	, WL_KMS_FORMAT_RGBA8888	, 1	, {4, 0, 0}	, &drawNOP	, "RGBA8888"	},
	{ WL_SHM_FORMAT_BGRA8888	, WL_KMS_FORMAT_BGRA8888	, 1	, {4, 0, 0}	, &drawNOP	, "BGRA8888"	},
	{ WL_SHM_FORMAT_XRGB2101010	, WL_KMS_FORMAT_XRGB2101010	, 1	, {4, 0, 0}	, &drawNOP	, "XRGB2101010"	},
	{ WL_SHM_FORMAT_XBGR2101010	, WL_KMS_FORMAT_XBGR2101010	, 1	, {4, 0, 0}	, &drawNOP	, "XBGR2101010"	},
	{ WL_SHM_FORMAT_RGBX1010102	, WL_KMS_FORMAT_RGBX1010102	, 1	, {4, 0, 0}	, &drawNOP	, "RGBX1010102"	},
	{ WL_SHM_FORMAT_BGRX1010102	, WL_KMS_FORMAT_BGRX1010102	, 1	, {4, 0, 0}	, &drawNOP	, "BGRX1010102"	},
	{ WL_SHM_FORMAT_ARGB2101010	, WL_KMS_FORMAT_ARGB2101010	, 1	, {4, 0, 0}	, &drawNOP	, "ARGB2101010"	},
	{ WL_SHM_FORMAT_ABGR2101010	, WL_KMS_FORMAT_ABGR2101010	, 1	, {4, 0, 0}	, &drawNOP	, "ABGR2101010"	},
	{ WL_SHM_FORMAT_RGBA1010102	, WL_KMS_FORMAT_RGBA1010102	, 1	, {4, 0, 0}	, &drawNOP	, "RGBA1010102"	},
	{ WL_SHM_FORMAT_BGRA1010102	, WL_KMS_FORMAT_BGRA1010102	, 1	, {4, 0, 0}	, &drawNOP	, "BGRA1010102"	},
	{ WL_SHM_FORMAT_VYUY		, WL_KMS_FORMAT_VYUY		, 1	, {2, 0, 0}	, &drawNOP	, "VYUY"	},
	{ WL_SHM_FORMAT_AYUV		, WL_KMS_FORMAT_AYUV		, 1	, {2, 0, 0}	, &drawNOP	, "AYUV"	},
	{ WL_SHM_FORMAT_YUV410		, WL_KMS_FORMAT_YUV410		, 1	, {2, 0, 0}	, &drawNOP	, "YUV410"	},
	{ WL_SHM_FORMAT_YVU410		, WL_KMS_FORMAT_YVU410		, 1	, {2, 0, 0}	, &drawNOP	, "YVU410"	},
	{ WL_SHM_FORMAT_YUV411		, WL_KMS_FORMAT_YUV411		, 1	, {2, 0, 0}	, &drawNOP	, "YUV411"	},
	{ WL_SHM_FORMAT_YVU411		, WL_KMS_FORMAT_YVU411		, 1	, {2, 0, 0}	, &drawNOP	, "YVU411"	},
	{ WL_SHM_FORMAT_YVU420		, WL_KMS_FORMAT_YVU420		, 3	, {1, 1, 1}	, &drawNOP	, "YVU420"	},
	{ WL_SHM_FORMAT_YUV422		, WL_KMS_FORMAT_YUV422		, 1	, {2, 0, 0}	, &drawNOP	, "YUV422"	},
	{ WL_SHM_FORMAT_YVU422		, WL_KMS_FORMAT_YVU422		, 1	, {2, 0, 0}	, &drawNOP	, "YVU422"	},
	{ WL_SHM_FORMAT_YUV444		, WL_KMS_FORMAT_YUV444		, 1	, {3, 0, 0}	, &drawNOP	, "YUV444"	},
	{ WL_SHM_FORMAT_YVU444		, WL_KMS_FORMAT_YVU444		, 1	, {3, 0, 0}	, &drawNOP	, "YVU444"	}
};

static int running = 1;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct buffer *buff = data;

	buff->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static int
create_shm_buffer(struct display *display, struct buffer *buffer,
		  int width, int height, uint32_t format, int need_plane_num, const int *pix_stride )
{
	struct wl_shm_pool	*pool;
	int			i, fd, size, stride = 0;
	void			*data;

	for (i = 0; i < need_plane_num; i++) {
		stride += pix_stride[i];
	}
	stride = width * stride;
	size = stride * height;
	stride = width * pix_stride[0];

	fd = os_create_anonymous_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
			size);
		return -1;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return -1;
	}

	pool = wl_shm_create_pool(display->shm, fd, size);
	buffer->buffer = wl_shm_pool_create_buffer(pool, 0,
						   width, height,
						   stride, format);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (format == WL_SHM_FORMAT_YUV420) {
		buffer->addr[0] = data;
		buffer->addr[1] = data + width * height;
		buffer->addr[2] = data + width * height * 5 / 4;
	} else {
		for (i = 0; i < need_plane_num; i++) {
			buffer->addr[i] = data;
			data += (width * pix_stride[i] * height);
		}
	}
	buffer->buffer_type = TYPE_SHM;
	buffer->shm_buffer_size = size;

	return 0;
}

static int
create_kms_buffer(struct display *display, struct buffer *buffer,
		  int width, int height, uint32_t format, int need_plane_num, const int *pix_stride )
{
	unsigned attr[] = {
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_TERMINATE_PROP_LIST
	};
	uint32_t	stride[3] = {0, 0, 0};
	int		prime_fd[3] = {0, 0, 0};
	uint32_t	handle[3] = {0, 0, 0};
	int 		i, ret;

	attr[3] = ((width + 31) >> 5) << 5;
	attr[5] = height;

	for (i = 0; i < need_plane_num; i++) {
		kms_bo_create(display->kmsdrv, attr, &buffer->bo[i]);
		kms_bo_map(buffer->bo[i], &buffer->addr[i]);
		stride[i] = width * pix_stride[i];
		kms_bo_get_prop(buffer->bo[i], KMS_HANDLE, &handle[i]);
		PRINT("bo[%d]=0x%08x addr[%d]=0x%08x handle[%d]=0x%08x\n",
			i, ((unsigned int)buffer->bo[i]), i, ((unsigned int)buffer->addr[i]), i, handle[i]);
		ret = drmPrimeHandleToFD (display->fd, handle[i], DRM_CLOEXEC, &prime_fd[i]);
		if (ret) {
			fprintf(stderr, "%s: drmPrimeHandleToFD failed. %s\n", __func__, strerror(errno));
			return -1;
		}
	}

	PRINT("prime_fd[0]=%d prime_fd[1]=%d prime_fd[2]=%d stride[0]=%d stride[1]=%d stride[2]=%d\n",
		prime_fd[0], prime_fd[1], prime_fd[2], stride[0], stride[1], stride[2]);

	buffer->buffer = wl_kms_create_mp_buffer(display->kms, width, height, format,
						 prime_fd[0], stride[0], prime_fd[1], stride[1],
						 prime_fd[2], stride[2]);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
	buffer->buffer_type = TYPE_KMS;

	return 0;
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
free_buffer(struct buffer *buffer)
{
	int i;

	wl_buffer_destroy(buffer->buffer);
	buffer->buffer = NULL;

	if (buffer->buffer_type == TYPE_SHM) {
		if (buffer->addr[0]) {
			munmap(buffer->addr[0], buffer->shm_buffer_size);
			buffer->addr[0] = NULL;
		}
	} else if (buffer->buffer_type == TYPE_KMS) {
		for (i = 0; i < MAX_PLANES; i++) {
			if (buffer->bo[i]) {
				kms_bo_unmap(buffer->bo[i]);
				kms_bo_destroy(&buffer->bo[i]);
				buffer->bo[i] = NULL;
				buffer->addr[i] = NULL;
			}
		}
	}
	buffer->buffer_type = TYPE_NONE;
}

static struct window *
create_window(struct display *display, int width, int height, int mode, int num, int exec_shm, int exec_kms)
{
	struct window *window;

	window = calloc(1, sizeof *window);
	if (!window)
		return NULL;

	window->callback	= NULL;
	window->display		= display;
	window->width		= width;
	window->height		= height;
	window->surface		= wl_compositor_create_surface(display->compositor);
	window->mode		= mode;
	window->num		= num;
	window->count		= 0;
	window->max_exec[0]	= exec_shm;
	window->max_exec[1]	= exec_kms;
	memset( &(window->buffers[0]), 0, sizeof(struct buffer) );
	memset( &(window->buffers[1]), 0, sizeof(struct buffer) );

	if (display->xshell) {
		window->xdg_surface = zxdg_shell_v6_get_xdg_surface(display->xshell,
								window->surface);

		zxdg_surface_v6_add_listener(window->xdg_surface,
					 &xdg_surface_listener, window);
		window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
					 &xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "confirm-format");

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

	return window;
}

static void
destroy_window(struct window *window)
{
	if (window->callback)
		wl_callback_destroy(window->callback);

	if (window->buffers[0].buffer)
		free_buffer(&window->buffers[0]);
	if (window->buffers[1].buffer)
		free_buffer(&window->buffers[1]);
	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);
	wl_surface_destroy(window->surface);
	free(window);
}

void
set8888(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, int size)
{
	int i = 0;

	for ( i = 0; i < size; i++ ) {
		*((uint32_t *)data + i) = ((byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4);
	}
}

void
set888(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, int size)
{
	int i = 0;
	uint8_t *rgb = data;

	for ( i = 0; i < size; i++ ) {
#if 0
		*rgb = byte3;
		rgb++;
		*rgb = byte2;
		rgb++;
		*rgb = byte1;
		rgb++;
#else
		*rgb = byte1;
		rgb++;
		*rgb = byte2;
		rgb++;
		*rgb = byte3;
		rgb++;
#endif
	}
}

void
setYUV(void *data, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, int size)
{
	int i = 0;

	for ( i = 0; i < size / 2; i++ ) {
		*((uint32_t *)data + i) = ((byte4 << 24) | (byte3 << 16) | (byte2 << 8) | byte1);
	}
}

void
setNV(void *ydata, void *uvdata, uint8_t byte1, uint8_t byte2, uint8_t byte3, int ysize, int uvsize)
{
	int i = 0;
	uint8_t *yuv = uvdata;

	memset( ydata, byte1, ysize );

	for ( i = 0; i < uvsize / 2; i++ ) {
#if 0
		*yuv = byte3;
		yuv++;
		*yuv = byte2;
		yuv++;
#else
		*yuv = byte2;
		yuv++;
		*yuv = byte3;
		yuv++;
#endif
	}
}

void
setYV(void *ydata, void *snd_data, void *trd_data, uint8_t byte1, uint8_t byte2, uint8_t byte3, int ysize, int snd_size, int trd_size)
{
	memset( ydata, byte1, ysize );
	memset( snd_data, byte2, snd_size );
	memset( trd_data, byte3, trd_size );
}

void
drawARGB8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t a = 0x7F;
	int size = width * height;

	set8888(buffer->addr[0], a, r, g, b, size);
}

void
drawXRGB8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t a = 0xff;
	int size = width * height;

	set8888(buffer->addr[0], a, r, g, b, size);
}

void
drawRGB565(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	int i = 0;

	for ( i = 0; i < size; i++ ) {
		*((uint16_t *)buffer->addr[0] + i) = (((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
	}
	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawYUYV(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

#if 0
	setYUV(buffer->addr[0], y, v, y, u, size);
#else
	setYUV(buffer->addr[0], y, u, y, v, size);
#endif

	PRINT("y=%d=0x%02x, u=%d=0x%02x, v=%d=0x%02x\n", y, y, u, u, v, v);
	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawRGB888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;

	set888(buffer->addr[0], r, g, b, size);

	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawUYVY(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

#if 0
	setYUV(buffer->addr[0], v, y, u, y, size);
#else
	setYUV(buffer->addr[0], u, y, v, y, size);
#endif

	PRINT("y=%d=0x%02x, u=%d=0x%02x, v=%d=0x%02x\n", y, y, u, u, v, v);
	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawNV12(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

	setNV(buffer->addr[0], buffer->addr[1], y, u, v, size, size / 2);

	PRINT("y=%d=0x%02x, u=%d=0x%02x, v=%d=0x%02x\n", y, y, u, u, v, v);
	PRINT_BUFFER(buffer->addr[0], 8);
	PRINT_BUFFER(buffer->addr[1], 8);
}

void
drawNV16(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

	setNV(buffer->addr[0], buffer->addr[1], y, u, v, size, size);
}

void
drawNV21(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

	setNV(buffer->addr[0], buffer->addr[1], y, v, u, size, size / 2);

	PRINT("y=%d=0x%02x, u=%d=0x%02x, v=%d=0x%02x\n", y, y, u, u, v, v);
	PRINT_BUFFER(buffer->addr[0], 8);
	PRINT_BUFFER(buffer->addr[1], 8);
}

void
drawNV61(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

	setNV(buffer->addr[0], buffer->addr[1], y, v, u, size, size);
}

void
drawRGB332(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	int i = 0;

	for ( i = 0; i < size; i++ ) {
		*((uint8_t *)buffer->addr[0] + i) = (((r & 0x07) << 5) | ((g & 0x07) << 2) | (b & 0x03));
	}

	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawBGR888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;

	set888(buffer->addr[0], b, g, r, size);
}

void
drawXBGR8888(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t a = 0xff;
	int size = width * height;

#if 0
	set8888(buffer->addr[0], a, b, g, r, size);
#else
	set8888(buffer->addr[0], b, g, r, a, size);
#endif

	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawYVYU(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

#if 0
	setYUV(buffer->addr[0], y, u, y, v, size);
#else
	setYUV(buffer->addr[0], y, v, y, u, size);
#endif

	PRINT_BUFFER(buffer->addr[0], 16);
}

void
drawYUV420(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int size = width * height;
	uint8_t y = RGBTOY(r, g, b);
	uint8_t u = RGBTOU(r, g, b);
	uint8_t v = RGBTOV(r, g, b);

	setYV(buffer->addr[0], buffer->addr[1], buffer->addr[2], y, u, v, size, size, size);

	PRINT("y=%d=0x%02x, u=%d=0x%02x, v=%d=0x%02x\n", y, y, u, u, v, v);
	PRINT_BUFFER(buffer->addr[0], 8);
	PRINT_BUFFER(buffer->addr[1], 4);
	PRINT_BUFFER(buffer->addr[2], 4);
}

void
drawNOP(struct buffer *buffer, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
}

static void
check_buffer_create(struct window *window)
{
	if ( window->count >= 90 ) {
		window->count = 0;
		if ( window->mode > 0 ) {
			window->buffers[0].color_type = COLOR_INIT;
			window->buffers[1].color_type = COLOR_INIT;
			window->buffers[0].buffer_change = 1;
			window->buffers[1].buffer_change = 1;
			window->num++;
			if ( window->num >= window->max_exec[window->mode - 1] ) {
				window->mode = ((window->mode == 1)? 2 : 1);
				window->num = 0;
			}
		}
	}
	window->count++;
}

static enum color
check_change_color_ptn(struct window *window, uint8_t *r, uint8_t *g, uint8_t *b, char *color_type, enum color current)
{
	enum color	colortype;

	switch ( (window->count-1)/30 ) {
	case 0:
		*b = 0xff;
		strcpy( color_type, "blue" );
		colortype = COLOR_BULE;
		break;
	case 1:
		*g = 0xff;
		strcpy( color_type, "green" );
		colortype = COLOR_GREEN;
		break;
	case 2:
		*r = 0xff;
		strcpy( color_type, "red" );
		colortype = COLOR_RED;
		break;
	default:
		fprintf(stderr, "%s counter error\n", __func__ );
		colortype = current;
		break;
	}

	return colortype;
}

static int
create_buffer(struct window *window, struct buffer *buffer)
{
	int		ret = 0;

	if (!buffer->buffer || buffer->buffer_change) {
		if ( buffer->buffer ) {
			free_buffer(buffer);
		}

		if ( abs(window->mode) == 1 ) {
			printf("create shm buffer\n");
			ret = create_shm_buffer(window->display, buffer,
						window->width, window->height,
						fmt_tbl[window->num].shm_format,
						fmt_tbl[window->num].need_plane_num,
						fmt_tbl[window->num].pix_stride);

			if (ret < 0)
				return -1;

			buffer->buffer_change = 0;
		}
		else if ( abs(window->mode) == 2 ) {
			printf("create kms buffer\n");
			ret = create_kms_buffer(window->display, buffer,
						window->width, window->height,
						fmt_tbl[window->num].kms_format,
						fmt_tbl[window->num].need_plane_num,
						fmt_tbl[window->num].pix_stride);

			if (ret < 0)
				return -1;

			/* paint the padding */
			buffer->buffer_change = 0;
		}
	}

	return 0;
}

static struct buffer *
window_next_buffer(struct window *window)
{
	struct buffer	*buffer;
	int		ret = 0;
	uint8_t		r = 0x00;
	uint8_t		g = 0x00;
	uint8_t		b = 0x00;
	char		color_type[8];
	enum color	colortype;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return NULL;

	check_buffer_create(window);

	colortype = check_change_color_ptn(window, &r, &g, &b, color_type, buffer->color_type);

	ret = create_buffer(window, buffer);
	if (ret < 0)
		return NULL;

	if (buffer->color_type != colortype ) {
		printf("next buffer[%s] format[%s] color[%s]\n", ((abs(window->mode) == 1)? "SHM" : "KMS"), fmt_tbl[window->num].prt_format, color_type);
		fmt_tbl[window->num].pfunc(buffer, window->width, window->height, r, g, b);
		buffer->color_type = colortype;
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
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	PRINT("%s format=0x%08x\n", __func__, format);
}

struct wl_shm_listener shm_listener = {
	shm_format
};

static void wayland_kms_handle_device(void *data, struct wl_kms *kms, const char *device)
{
	struct display *display = data;
	drm_magic_t magic;

	PRINT("%s: (device=%s)\n", __func__, device);

	if ((display->fd = open(device, O_RDWR | O_CLOEXEC)) < 0) {
		fprintf(stderr, "%s: Can't open %s (%s)\n",
			    __func__, device, strerror(errno));
		return;
	}

	drmGetMagic(display->fd, &magic);
	wl_kms_authenticate(kms, magic);
}

static void wayland_kms_handle_format(void *data, struct wl_kms *kms, uint32_t format)
{
	PRINT("%s format=0x%08x\n", __func__, format);
}

static void wayland_kms_handle_authenticated(void *data, struct wl_kms *kms)
{
	struct display *display = data;

	PRINT("%s: authenticated.\n", __func__);

	display->authenticated = 1;
}

static const struct wl_kms_listener wayland_kms_listener = {
	.device = wayland_kms_handle_device,
	.format = wayland_kms_handle_format,
	.authenticated = wayland_kms_handle_authenticated,
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
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
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
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
		wl_shm_add_listener(d->shm, &shm_listener, d);
	} else if (strcmp(interface, "wl_kms") == 0) {
		d->kms = wl_registry_bind(registry, id, &wl_kms_interface, version);
		wl_kms_add_listener(d->kms, &wayland_kms_listener, d);
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

static void wayland_sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
	int *done = data;
	*done = 1;
	wl_callback_destroy(callback);
}

static const struct wl_callback_listener wayland_sync_listener = {
	.done = wayland_sync_callback
};

static struct display *
create_display(void)
{
	struct display		*display;
	struct wl_callback	*callback;
	int			ret = 0, done = 0;

	display = malloc(sizeof *display);
	if (display == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->fd = -1;
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);

	wl_display_roundtrip(display->display);
	if (display->shm == NULL) {
		fprintf(stderr, "No wl_shm global\n");
		exit(1);
	}

	ret = 0;
	callback = wl_display_sync(display->display);
	wl_callback_add_listener(callback, &wayland_sync_listener, &done);
	while (ret >= 0 && !done) {
		ret = wl_display_dispatch(display->display);
	}
	if (!done) {
		wl_callback_destroy(callback);
	}
	if (display->fd == -1) {
		fprintf(stderr, "no DRM device given\n");
		exit(1);
	}

	ret = kms_create(display->fd, &display->kmsdrv);
	if (ret < 0 ) {
		fprintf(stderr, "%s: kms_create failed. %s\n", __func__, strerror(errno));
		exit(1);
	}

	wl_display_get_fd(display->display);

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->kms)
		wl_kms_destroy(display->kms);

	if (display->shm)
		wl_shm_destroy(display->shm);

	if (display->xshell)
		zxdg_shell_v6_destroy(display->xshell);

	if (display->output)
		wl_output_destroy(display->output);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	wl_registry_destroy(display->registry);
	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	kms_destroy(&display->kmsdrv);
	close(display->fd);
	free(display);
}

static void
signal_int(int signum)
{
	running = 0;
}

static void
usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [type] ([format(only -s & -k)] or [shm_number(only -m)] or [kms_number(only -n)]) [-p <x> <y>]\n\n"
		"where 'type' is one of\n"
		"  -b\tuse all format to support(shm until 0-3 & kms until 0-14)\n"
		"  -s\tuse only one format for shm buffer\n"
		"  -k\tuse only one format for kms buffer\n"
		"  -m\tuse multiple formats until shm_number\n"
		"  -n\tuse multiple formats until kms_number\n"
		"  (-m and -n is possible to specify at the same time)\n\n"
		"where 'format' is one of\n"
		"  0\tformat is ARGB8888\n"
		"  1\tformat is XRGB8888\n"
		"  2\tformat is RGB565\n"
		"  3\tformat is YUYV\n"
		"  4\tformat is RGB888\n"
		"  5\tformat is UYVY\n"
		"  6\tformat is NV12\n"
		"  7\tformat is NV16\n"
		"  8\tformat is NV21\n"
		"  9\tformat is NV61\n"
		" 10\tformat is RGB332\n"
		" 11\tformat is BGR888\n"
		" 12\tformat is XBGR8888\n"
		" 13\tformat is YVYU\n"
		" 14\tformat is YUV420\n\n"
		"  -p <x>,<y>\tset application position\n\n",
		progname);
}

int
main(int argc, char **argv)
{
	struct sigaction	sigint;
	struct display		*display;
	struct window		*window;
	int			ret = 0, opt = 0, mode = 0, num = 0;
	int			max_exec[2] = {SUPPORT_SHM_NUMBER, SUPPORT_KMS_NUMBER};

	do {
		opt = getopt(argc, argv, "bs:k:m:n:p:");
		if ( opt != -1 ) {
			switch ( opt ) {
			case 'b':
				mode = 1;
				num = 0;
				break;
			case 's':
				mode = -1;
				num = atoi(optarg) - 1;
				break;
			case 'k':
				mode = -2;
				num = atoi(optarg) - 1;
				break;
			case 'm':
				mode = 1;
				num = 0;
				max_exec[0] = atoi(optarg);
				break;
			case 'n':
				mode = 2;
				num = 0;
				max_exec[1] = atoi(optarg);
				break;
			case 'p':
				if (sscanf(optarg, "%d,%d", &pos_x, &pos_y) == 2) {
					pos_y -= PANEL_HEIGHT;
					step_set_pos = 1;
					break;
				} else {
					usage(argv[0]);
					exit(1);
				}
			default:
				usage(argv[0]);
				exit(1);
			}
		}
	} while ( opt != -1 );

	if (!mode) {
		usage(argv[0]);
		exit(1);
	}
	display = create_display();

	if (step_set_pos) {
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(1);
		}
	}
	window = create_window(display, 256, 256, mode, num, max_exec[0], max_exec[1]);
	if (!window)
		return 1;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Initialise damage to full surface, so the padding gets painted */
	wl_surface_damage(window->surface, 0, 0,
			  window->width, window->height);

	window->initialized = true;

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "confirm-format exiting\n");
	destroy_window(window);
	destroy_display(display);

	return 0;
}
