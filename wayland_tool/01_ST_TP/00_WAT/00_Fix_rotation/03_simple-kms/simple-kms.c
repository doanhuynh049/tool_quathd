/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <xf86drm.h>
#include <libkms.h>
#include <getopt.h>
#include <wayland-client.h>
#include "shared/os-compatibility.h"
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "wayland-kms-client-protocol.h"

#include "mmngr_user_public.h"
#include "mmngr_buf_user_public.h"

#include <sys/types.h>
#include "ivi-application-client-protocol.h"
#define IVI_SURFACE_ID 9000

#define MAX_PLANES		3
#define NUM_BUFFERS		3

#define WINDOW_WIDTH		250
#define WINDOW_HEIGHT		250

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zxdg_shell_v6 *xshell;
	struct wl_output *output;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct ivi_application *ivi_application;
	struct wl_kms *kms;
	struct kms_driver *kmsdrv;
	int fd;
	int authenticated;
};

struct buffer {
	struct wl_buffer *buffer;
	int busy;
	struct kms_bo *bo[MAX_PLANES];
	void *addr[MAX_PLANES];
	MMNGR_ID id[MAX_PLANES];
	int ext_id[MAX_PLANES];
	int width, height;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct ivi_surface *ivi_surface;
	struct buffer buffers[NUM_BUFFERS];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
	enum wl_shell_surface_fullscreen_method method;
	bool wait_for_configure;
};

static int running = 1;
static int use_mmngr = 0;
static int pos_x = 0;
static int pos_y = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;
static int flag_ft = 0;
static uint32_t time_fix = 0;
uint32_t time_count = 0;
int time_step_val = -16;
unsigned int use_wl_loop_time = 0;

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
create_kms_buffer(struct display *display, struct buffer *buffer,
		  int width, int height, uint32_t format,
		  int plane_num, const int *p_stride)
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

	for (i = 0; i < plane_num; i++) {
		kms_bo_create(display->kmsdrv, attr, &buffer->bo[i]);
		kms_bo_map(buffer->bo[i], &buffer->addr[i]);
		stride[i] = width * p_stride[i];
		kms_bo_get_prop(buffer->bo[i], KMS_HANDLE, &handle[i]);
		ret = drmPrimeHandleToFD (display->fd, handle[i], DRM_CLOEXEC, &prime_fd[i]);
		if (ret) {
			fprintf(stderr, "%s: drmPrimeHandleToFD failed. %s\n", __func__, strerror(errno));
			return -1;
		}
	}

	buffer->buffer = wl_kms_create_mp_buffer(display->kms, width, height, format,
						 prime_fd[0], stride[0], prime_fd[1], stride[1],
						 prime_fd[2], stride[2]);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);

	return 0;
}

static int
create_kms_buffer_with_mmngr(struct display *display, struct buffer *buffer,
			     int width, int height, uint32_t format,
			     int plane_num, const int *p_stride)
{
	int		dmabuf_fd[MAX_PLANES] = {0};
	uint32_t	stride[MAX_PLANES] = {0};
	unsigned int	hard_addr;
	int		size;
	int 		i, ret;

	for (i = 0; i < plane_num; i++) {
		stride[i] = width * p_stride[i];
		size = stride[i] * height;
		size = ((size + 4095) / 4096) * 4096;

		ret = mmngr_alloc_in_user_ext(
					&buffer->id[i], size,
					&hard_addr,
					&buffer->addr[i],
					MMNGR_VA_SUPPORT, NULL);
		if (ret != R_MM_OK) {
			fprintf(stderr,
				"%s: mmngr_alloc_in_user_ext failed. %d\n",
				__func__, ret);
			return -1;
		}
		ret = mmngr_export_start_in_user_ext(
					&buffer->ext_id[i], size,
					hard_addr,
					&dmabuf_fd[i], NULL);
		if (ret != R_MM_OK) {
			fprintf(stderr,
				"%s: mmngr_export_start_in_user_ext failed."
				" %d\n", __func__, ret);
			return -1;
		}
	}

	buffer->buffer = wl_kms_create_mp_buffer(display->kms, width, height, format,
						 dmabuf_fd[0], stride[0], dmabuf_fd[1], stride[1],
						 dmabuf_fd[2], stride[2]);
	wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);

	return 0;
}

static void
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *xdg_surface, uint32_t serial)
{
	struct window *window = data;

	zxdg_surface_v6_ack_configure(xdg_surface, serial);

	if (window->wait_for_configure) {
		redraw(window, NULL, 0);
		window->wait_for_configure = false;
	}
}

static void
handle_xdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *xdg_toplevel,
			  int32_t width, int32_t height,
			  struct wl_array *states)
{
	struct window *window = data;
	uint32_t *p;

	wl_array_for_each(p, states) {
		uint32_t state = *p;
		switch (state) {
		case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
			break;
		}
	}

	if (width > 0 && height > 0) {
		window->width = width;
		window->height = height;
	} else {
		window->width = WINDOW_WIDTH;
		window->height = WINDOW_HEIGHT;
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
}

static const struct ivi_surface_listener ivi_surface_listener = {
	handle_ivi_surface_configure,
};

static void
free_buffer(struct buffer *buffer)
{
	int i;

	wl_buffer_destroy(buffer->buffer);
	buffer->buffer = NULL;

	for (i = 0; i < MAX_PLANES; i++) {
		if (buffer->bo[i]) {
			kms_bo_unmap(buffer->bo[i]);
			kms_bo_destroy(&buffer->bo[i]);
			buffer->bo[i] = NULL;
		}
		if (buffer->id[i]) {
			mmngr_export_end_in_user_ext(buffer->ext_id[i]);
			mmngr_free_in_user(buffer->id[i]);
			buffer->ext_id[i] = 0;
			buffer->id[i] = 0;
		}
		buffer->addr[i] = NULL;
	}
}

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;

	window = zalloc(sizeof *window);
	if (!window)
		return NULL;

	window->callback = NULL;
	window->display = display;
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface(display->compositor);

	if (display->xshell) {
		window->xdg_surface =
			zxdg_shell_v6_get_xdg_surface(display->xshell,
						  window->surface);

		assert(window->xdg_surface);

		zxdg_surface_v6_add_listener(window->xdg_surface,
					 &xdg_surface_listener, window);

		window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
				      &xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "simple-kms");
		window->wait_for_configure = true;
		wl_surface_commit(window->surface);
	} else if (display->fshell) {
		zwp_fullscreen_shell_v1_present_surface(display->fshell,
							window->surface,
							ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
							NULL);
	} else if (display->ivi_application ) {
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

	} else {
		assert(0);
	}

	return window;
}

static void
destroy_window(struct window *window)
{
	int i;

	if (window->callback)
		wl_callback_destroy(window->callback);

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (window->buffers[i].buffer)
			free_buffer(&window->buffers[i]);
	}

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
	int ret = 0;
	int p_stride[MAX_PLANES] = {4, 0, 0};

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (!window->buffers[i].busy) {
			buffer = &window->buffers[i];
			break;
		}
	}
	if (!buffer)
		return NULL;

	if ((window->method == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT) &&
	    (buffer->buffer)) {
		if ((window->width != buffer->width) ||
		    (window->height != buffer->height)) {
			free_buffer(buffer);
		}
	}

	if (!buffer->buffer) {
		if (use_mmngr)
			ret = create_kms_buffer_with_mmngr(
					window->display, buffer,
 					window->width, window->height,
					WL_KMS_FORMAT_XRGB8888,
					1,
					p_stride);
		else
			ret = create_kms_buffer(
					window->display, buffer,
					window->width, window->height,
					WL_KMS_FORMAT_XRGB8888,
					1,
					p_stride);
		if (ret < 0)
			return NULL;

		/* paint the padding */
		memset(buffer->addr[0], 0xff,
		       window->width * window->height * 4);

		buffer->width = window->width;
		buffer->height = window->height;
	}

	return buffer;
}

static void
paint_pixels(void *image, int padding, int width, int height, uint32_t time)
{
	const int halfh = padding + (height - padding * 2) / 2;
	const int halfw = padding + (width  - padding * 2) / 2;
	int ir, or;
	uint32_t *pixel = image;
	int y;

	/* squared radii thresholds */
	or = (halfw < halfh ? halfw : halfh) - 8;
	ir = or - 32;
	or *= or;
	ir *= ir;

	pixel += padding * width;
	for (y = padding; y < height - padding; y++) {
		int x;
		int y2 = (y - halfh) * (y - halfh);

		pixel += padding;
		for (x = padding; x < width - padding; x++) {
			uint32_t v;

			/* squared distance from center */
			int r2 = (x - halfw) * (x - halfw) + y2;

			if (r2 < ir)
				v = (r2 / 32 + time_count / 64) * 0x0080401;
			else if (r2 < or)
				v = (y + time_count / 32) * 0x0080401;
			else
				v = (x + time_count / 16) * 0x0080401;
			v &= 0x00ffffff;

			/* cross if compositor uses X from XRGB as alpha */
			if (abs(x - y) > 6 && abs(x + y - height) > 6)
				v |= 0xff000000;

			*pixel++ = v;
		}

		pixel += padding;
	}
	
	if (use_wl_loop_time)
	{
		if ((int)time_count >= 3200 || (int)time_count < 16)
			time_step_val = -time_step_val;

	 	time_count += time_step_val;	
	}
	else
	{
		time_count = time;
	}
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
create_dummy_buffer(struct buffer *buffer, struct display *display, int width, int height)
{
	int p_stride[MAX_PLANES] = {4, 0, 0};


	unsigned attr[] = {
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_TERMINATE_PROP_LIST
	};
	uint32_t	stride[3] = {0, 0, 0};
	int		prime_fd[3] = {0, 0, 0};
	uint32_t	handle[3] = {0, 0, 0};
	int 		i;

	attr[3] = ((width + 31) >> 5) << 5;
	attr[5] = height;

	for (i = 0; i < 3; i++) {
		kms_bo_create(display->kmsdrv, attr, &buffer->bo[i]);
		kms_bo_map(buffer->bo[i], &buffer->addr[i]);
		stride[i] = width * p_stride[i];
		kms_bo_get_prop(buffer->bo[i], KMS_HANDLE, &handle[i]);
		drmPrimeHandleToFD (display->fd, handle[i], DRM_CLOEXEC, &prime_fd[i]);
	}

	buffer->buffer = wl_kms_create_mp_buffer(display->kms, width, height, WL_KMS_FORMAT_XRGB8888,
						 prime_fd[0], stride[0], prime_fd[1], stride[1],
						 prime_fd[2], stride[2]);
	wl_buffer_add_listener(buffer->buffer, &dummy_buffer_listener, buffer);
	return buffer->buffer;
}

static void
set_dummy_buffer(struct window *window)
{
	struct wl_buffer *buffer;
	struct buffer *buffer1 = NULL;
	int i;

	for (i = 0; i < NUM_BUFFERS; i++) {
		if (!window->buffers[i].busy) {
			buffer1 = &window->buffers[i];
			break;
		}
	}
	buffer = create_dummy_buffer(buffer1,window->display, dummy_w, dummy_h);
	if (!buffer) {
		fprintf(stderr, "create_dummy_buffer failed.\n");
		abort();
	}
	wl_surface_attach(window->surface, buffer, 0, 0);
	wl_surface_damage(window->surface, 0, 0, dummy_w, dummy_h);
	wl_surface_commit(window->surface);
	for (i = 0; i < NUM_BUFFERS; i++) {
	if (window->buffers[i].buffer)
		free_buffer(&window->buffers[i]);
	}

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

	if ( flag_ft == 1) {
		time_fix += 17;
		time = time_fix;
	}

	buffer = window_next_buffer(window);
	if (!buffer) {
		fprintf(stderr,
			!callback ? "Failed to create the first buffer.\n" :
			"Both buffers busy at redraw(). Server bug?\n");
		abort();
	}

	paint_pixels(buffer->addr[0], 20, window->width, window->height, time);

	if (window->method == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT) {
		struct wl_region *region;
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0, window->width, window->height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	wl_surface_attach(window->surface, buffer->buffer, 0, 0);

	if (step_set_pos) {
		wl_surface_attach(window->surface, buffer->buffer,
				  pos_x, pos_y);
		step_set_pos--;
	} else {
		wl_surface_attach(window->surface, buffer->buffer, 0, 0);
	}

	wl_surface_damage(window->surface,
			  20, 20, window->width - 40, window->height - 40);

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

static void wayland_kms_handle_device(void *data, struct wl_kms *kms, const char *device)
{
	struct display *display = data;
	drm_magic_t magic;

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
}

static void wayland_kms_handle_authenticated(void *data, struct wl_kms *kms)
{
	struct display *display = data;

	display->authenticated = 1;
}

static const struct wl_kms_listener wayland_kms_listener = {
	wayland_kms_handle_device,
	wayland_kms_handle_format,
	wayland_kms_handle_authenticated,
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
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, id,
						     &wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
		}
	} else if (strcmp(interface, "zxdg_shell_v6") == 0) {
		d->xshell = wl_registry_bind(registry,
					    id, &zxdg_shell_v6_interface, 1);
		zxdg_shell_v6_add_listener(d->xshell, &xdg_shell_listener, d);
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "wl_kms") == 0) {
		d->kms = wl_registry_bind(registry, id, &wl_kms_interface, version);
		wl_kms_add_listener(d->kms, &wayland_kms_listener, d);
	}
	else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application =
			wl_registry_bind(registry, id,
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

static struct display *
create_display(void)
{
	struct display *display;
	int ret;

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
	if (display->kms == NULL) {
		fprintf(stderr, "No wl_kms global\n");
		exit(1);
	}

	wl_display_roundtrip(display->display);
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
	kms_destroy(&display->kmsdrv);
	close(display->fd);

	if (display->kms)
		wl_kms_destroy(display->kms);

	if (display->xshell)
		zxdg_shell_v6_destroy(display->xshell);
	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);
	if (display->output)
		wl_output_destroy(display->output);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

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
usage(int error_code)
{
	fprintf(stderr,
		"Usage: simple-kms [OPTIONS]\n\n"
		"  -s, --size=WxH       Set the window width and height\n"
		"  -m, --mmngr          Use mmngr\n"
		"  -f, --fullscreen     Run in fullscreen mode (default)\n"
		"  -d, --fullscreen-d   Run in fullscreen mode with "
					"driver method\n"
		"  -r, --framerate=mHz  Set the framerate in mHz "
					"i.e., framerate of 60000 is 60Hz\n"
		"  -h, --help           Display this help text and exit\n"
		"  -p, --pos <x>,<y>   	Set the application position\n\n"
		"  -l, --loop 					Loop time cyclic\n\n"
		"  -t, --fix-timer   	Use fixed timer\n\n");
	exit(error_code);
}

static int
get_img_size(char *str, int *width, int *height)
{
	int ret;

	if ((!str) || (!width) || (!height))
		goto err;


	ret = sscanf(str, "%dx%d", width, height);
	if (ret != 2 || *width <=0 || *height <= 0)
		goto err;

	return 0;
err:
	fprintf(stderr, "Unexpected values are specified\n");
	return 1;
}

static int
str2i(char *str, int *val)
{
	long value;
	char *endptr;

	if (!str)
		return 1;

	value = strtol(str, &endptr, 0);

	if ((value == LONG_MIN) || (value == LONG_MAX) ||
	    (value <= (long)INT_MIN) || (value >= (long)INT_MAX)) {
		fprintf(stderr, "A range error was detected.\n");
		return -1;
	}
	if ((value == 0) && (errno != 0)) {
		fprintf(stderr, "Unexpected error was detected in strtol()\n");
		return -1;
	}
	if (*endptr != '\0') {
		fprintf(stderr, "Failed to convert the string to "
			"a long integer.\n");
		return -1;
	}
	*val = (int)value;

	return 0;
}

static const struct option long_options[] = {
	{"size", required_argument, NULL, 's'},
	{"mmngr", no_argument, NULL, 'm'},
	{"fullscreen", no_argument, NULL, 'f'},
	{"fullscreen-d", no_argument, NULL, 'd'},
	{"framerate", required_argument, NULL, 'r'},
	{"help", no_argument, NULL, 'h'},
	{"fix-timer", no_argument, NULL, 't'},
	{"pos", required_argument, NULL, 'p'},
	{"loop", required_argument, NULL, 'l'},
	{NULL, 0, NULL, 0},
};

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret = 0;
	int width  = WINDOW_WIDTH;
	int height = WINDOW_HEIGHT;
	int full = 0;
	enum wl_shell_surface_fullscreen_method method =
		WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	int framerate = 0;

	while (1) {
		int c = getopt_long(argc, argv, "s:mfdr:hx:y:p:t:l", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 's':
			ret = get_img_size(optarg, &width, &height);
			break;
		case 'm':
			use_mmngr = 1;
			break;
		case 'f':
			full = 1;
			break;
		case 'd':
			full = 1;
			method = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER;
			break;
		case 'r':
			ret = str2i(optarg, &framerate);
			if (framerate < 0)
				exit(EXIT_FAILURE);
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'p':
			if (sscanf(optarg, "%d,%d", &pos_x, &pos_y) == 2) {
				pos_y -= 32;
				step_set_pos = 1;
				break;
			} else {
				fprintf(stderr, "Invalid argument.\n");
				exit(EXIT_FAILURE);
			}
		case 't':
			flag_ft = 1;
			break;
		case 'l':
			use_wl_loop_time = 1;
			break;
		default:
			usage(EXIT_FAILURE);
			break;
		}
		if (ret)
			exit(EXIT_FAILURE);
	}

	display = create_display();
	if (step_set_pos) {
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(1);
		}
	}
	window = create_window(display, width, height);
	if (!window)
		return 1;

	window->method = method;

	if (full) {
		zxdg_toplevel_v6_set_fullscreen(window->xdg_toplevel, NULL);
	}

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Initialise damage to full surface, so the padding gets painted */
	wl_surface_damage(window->surface, 0, 0,
			  window->width, window->height);

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	ret = 0;
	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-kms exiting\n");

	if (window->display->ivi_application) {
		ivi_surface_destroy(window->ivi_surface);
		ivi_application_destroy(window->display->ivi_application);
	}

	destroy_window(window);
	destroy_display(display);

	return 0;
}
