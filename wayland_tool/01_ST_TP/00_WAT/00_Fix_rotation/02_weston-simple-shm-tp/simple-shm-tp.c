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

#include <wayland-client.h>
#include "shared/os-compatibility.h"
#include "shared/zalloc.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

#include <sys/types.h>
#include "ivi-application-client-protocol.h"
#define IVI_SURFACE_ID 9000

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *wshell;
	struct zxdg_shell_v6 *xshell;
	struct zwp_fullscreen_shell_v1 *fshell;
	struct wl_shm *shm;
	bool has_xrgb;
	struct ivi_application *ivi_application;
	struct wp_viewporter *viewporter;
	struct wl_output *output;
};

struct buffer {
	struct wl_buffer *buffer;
	void *shm_data;
	int busy;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct zxdg_surface_v6 *xdg_surface;
	struct zxdg_toplevel_v6 *xdg_toplevel;
	struct wl_shell_surface *shell_surface;
	struct ivi_surface *ivi_surface;
	struct buffer buffers[2];
	struct buffer *prev_buffer;
	struct wl_callback *callback;
	struct wp_viewport *viewport;
	bool wait_for_configure;
};

static int running = 1;
static int init_move = 0;
static int dummy_w = 0;
static int dummy_h = 0;
static int step_set_pos = 0;
unsigned char use_wl_shell = 0;
unsigned int use_wl_loop_time = 0;
static int flag_ft = 0;
static int time_fix = 0;
static uint32_t time_count = 0;
int time_step_val = -16;

static int32_t attach_x;
static int32_t attach_y;
static int32_t init_x;
static int32_t init_y;
static int32_t src_x;
static int32_t src_y;
static int32_t src_w;
static int32_t src_h;
static int32_t dst_w;
static int32_t dst_h;
static int scaling_mode;

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

enum mode {
	MODE_NONE=0,
	MODE_SCALING=(1<<0),
	MODE_CROPPING=(1<<1),
};

#define WINDOW_WIDTH	(1000)
#define WINDOW_HEIGHT	(1000)

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
create_shm_buffer(struct display *display, struct buffer *buffer,
		  int width, int height, uint32_t format)
{
	struct wl_shm_pool *pool;
	int fd, size, stride;
	void *data;

	stride = width * 4;
	size = stride * height;

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

	buffer->shm_data = data;

	return 0;
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
	    uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void
handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *xdg_surface,
				uint32_t serial)
{
	struct window *window = data;

	zxdg_surface_v6_ack_configure(xdg_surface, serial);

	if (window->wait_for_configure) {
		redraw(window, NULL, 0);
		window->wait_for_configure = false;
	}
}

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
	handle_xdg_surface_configure,
};

static void
handle_xdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *xdg_toplevel,
				int32_t width, int32_t height,
				struct wl_array *state)
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

static void
handle_ivi_surface_configure(void *data, struct ivi_surface *ivi_surface,
			     int32_t width, int32_t height)
{
	/* Simple-shm is resizable */
}

static const struct ivi_surface_listener ivi_surface_listener = {
	handle_ivi_surface_configure,
};

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
	window->viewport = wp_viewporter_get_viewport(display->viewporter,
							  window->surface);
	assert(window->viewport);

	if (use_wl_shell) {
		window->shell_surface =
			wl_shell_get_shell_surface(display->wshell,
						   window->surface);

		assert(window->shell_surface);

		wl_shell_surface_add_listener(window->shell_surface,
					      &shell_surface_listener, window);
       
		wl_shell_surface_set_title(window->shell_surface, "weston-simple-shm-tp");


		wl_shell_surface_set_toplevel(window->shell_surface);
	} else if (display->xshell) {
		window->xdg_surface =
			zxdg_shell_v6_get_xdg_surface(display->xshell,
						window->surface);
		assert(window->xdg_surface);
		zxdg_surface_v6_add_listener(window->xdg_surface,
						&xdg_surface_listener, window);

		window->xdg_toplevel =
			zxdg_surface_v6_get_toplevel(window->xdg_surface);
		assert(window->xdg_toplevel);
		zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
						&xdg_toplevel_listener, window);

		zxdg_toplevel_v6_set_title(window->xdg_toplevel, "weston-simple-shm-tp");
		wl_surface_commit(window->surface);
		window->wait_for_configure = true;
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
	if (window->viewport)
		wp_viewport_destroy(window->viewport);

	if (window->callback)
		wl_callback_destroy(window->callback);

	if (window->buffers[0].buffer)
		wl_buffer_destroy(window->buffers[0].buffer);
	if (window->buffers[1].buffer)
		wl_buffer_destroy(window->buffers[1].buffer);

	if (window->xdg_toplevel)
		zxdg_toplevel_v6_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		zxdg_surface_v6_destroy(window->xdg_surface);

	if (window->shell_surface)
		wl_shell_surface_destroy(window->shell_surface);

	wl_surface_destroy(window->surface);
	free(window);
}

static struct buffer *
window_next_buffer(struct window *window)
{
	struct buffer *buffer;
	int ret = 0;

	if (!window->buffers[0].busy)
		buffer = &window->buffers[0];
	else if (!window->buffers[1].busy)
		buffer = &window->buffers[1];
	else
		return NULL;

	if (!buffer->buffer) {
		ret = create_shm_buffer(window->display, buffer,
					window->width, window->height,
					WL_SHM_FORMAT_XRGB8888);

		if (ret < 0)
			return NULL;

		/* paint the padding */
		memset(buffer->shm_data, 0xff,
		       window->width * window->height * 4);
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
	int32_t dmg_w, dmg_h;

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

	paint_pixels(buffer->shm_data, 20, window->width, window->height, time);

	dmg_w = window->width;
	dmg_h = window->height;

	if (scaling_mode & MODE_CROPPING) {
		wp_viewport_set_source(window->viewport,
				       wl_fixed_from_int(src_x),
				       wl_fixed_from_int(src_y),
				       wl_fixed_from_int(src_w),
				       wl_fixed_from_int(src_h));
		dmg_w = src_w;
		dmg_h = src_h;
	}
	if (scaling_mode & MODE_SCALING) {
		wp_viewport_set_destination(window->viewport, dst_w, dst_h);
		dmg_w = dst_w;
		dmg_h = dst_h;
	}

	if (init_move) {
		wl_surface_attach(window->surface, buffer->buffer, 0, 0);
		init_move = 0;
	} else if (step_set_pos) {
		wl_surface_attach(window->surface, buffer->buffer, init_x, init_y);
		step_set_pos--;
	} else {
		wl_surface_attach(window->surface, buffer->buffer, attach_x, attach_y);
	}
	wl_surface_damage(window->surface, 0, 0, dmg_w, dmg_h);

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
	struct display *d = data;

	if (format == WL_SHM_FORMAT_XRGB8888)
		d->has_xrgb = true;
}

struct wl_shm_listener shm_listener = {
	shm_format
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
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->wshell = wl_registry_bind(registry,
					      id, &wl_shell_interface, 1);
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "wp_viewporter") == 0) {
		d->viewporter = wl_registry_bind(registry, id,
					     &wp_viewporter_interface, 1);
		fprintf(stderr, "get scaler interface");
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
		wl_shm_add_listener(d->shm, &shm_listener, d);
	}
	else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application =
			wl_registry_bind(registry, id,
					 &ivi_application_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (!d->output) {	/* Only the first output is supported. */
			d->output = wl_registry_bind(registry, id,
							&wl_output_interface, 1);
			wl_output_add_listener(d->output, &output_listener, d);
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

	display->has_xrgb = false;
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_roundtrip(display->display);
	if (display->shm == NULL) {
		fprintf(stderr, "No wl_shm global\n");
		exit(1);
	}

	wl_display_roundtrip(display->display);

	/*
	 * Why do we need two roundtrips here?
	 *
	 * wl_display_get_registry() sends a request to the server, to which
	 * the server replies by emitting the wl_registry.global events.
	 * The first wl_display_roundtrip() sends wl_display.sync. The server
	 * first processes the wl_display.get_registry which includes sending
	 * the global events, and then processes the sync. Therefore when the
	 * sync (roundtrip) returns, we are guaranteed to have received and
	 * processed all the global events.
	 *
	 * While we are inside the first wl_display_roundtrip(), incoming
	 * events are dispatched, which causes registry_handle_global() to
	 * be called for each global. One of these globals is wl_shm.
	 * registry_handle_global() sends wl_registry.bind request for the
	 * wl_shm global. However, wl_registry.bind request is sent after
	 * the first wl_display.sync, so the reply to the sync comes before
	 * the initial events of the wl_shm object.
	 *
	 * The initial events that get sent as a reply to binding to wl_shm
	 * include wl_shm.format. These tell us which pixel formats are
	 * supported, and we need them before we can create buffers. They
	 * don't change at runtime, so we receive them as part of init.
	 *
	 * When the reply to the first sync comes, the server may or may not
	 * have sent the initial wl_shm events. Therefore we need the second
	 * wl_display_roundtrip() call here.
	 *
	 * The server processes the wl_registry.bind for wl_shm first, and
	 * the second wl_display.sync next. During our second call to
	 * wl_display_roundtrip() the initial wl_shm events are received and
	 * processed. Finally, when the reply to the second wl_display.sync
	 * arrives, it guarantees we have processed all wl_shm initial events.
	 *
	 * This sequence contains two examples on how wl_display_roundtrip()
	 * can be used to guarantee, that all reply events to a request
	 * have been received and processed. This is a general Wayland
	 * technique.
	 */

	if (!display->has_xrgb) {
		fprintf(stderr, "WL_SHM_FORMAT_XRGB32 not available\n");
		exit(1);
	}

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->shm)
		wl_shm_destroy(display->shm);

	if (display->wshell)
		wl_shell_destroy(display->wshell);

	if (display->xshell)
		zxdg_shell_v6_destroy(display->xshell);

	if (display->fshell)
		zwp_fullscreen_shell_v1_release(display->fshell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	if (display->output)
		wl_output_destroy(display->output);

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

int
main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display;
	struct window *window;
	int ret = 0;
	int i;

	attach_x = 0;
	attach_y = 0;
	init_x = 0;
	init_y = 0;
	/* Crop rectangle */
	src_x = 400;
	src_y = 400;
	src_w = 320;
	src_h = 240;
	/* Scaled in size */
	dst_w = 240;
	dst_h = 150;
	scaling_mode = MODE_NONE;

 	for (i = 1; i < argc; i++) {
		if (sscanf(argv[i], "-scale=%d", &scaling_mode) == 1) {
			fprintf(stderr, "sacle mode is %d \n", scaling_mode);
		} else if (sscanf(argv[i], "-mov=%d,%d",
				  &attach_x, &attach_y) == 2) {
        		fprintf(stderr, "attach_x,attach_y=%d,%d \n",
			        attach_x, attach_y);
		} else if (strcmp("-t", argv[i]) == 0) {
				flag_ft = 1;
		}else if (sscanf(argv[i], "-xy=%d,%d",
				  &init_x, &init_y) == 2) {
			fprintf(stderr, "init_x,init_y=%d,%d \n",
				init_x, init_y);
			step_set_pos = 1;
		} else if (sscanf(argv[i], "-pos=%d,%d,%d,%d,%d,%d",
				  &src_x, &src_y, &src_w, &src_h,
				  &dst_w, &dst_h) == 6) {
			fprintf(stderr, "src x,y,w,h=%d,%d,%d,%d  "
				"dst w,y=%d,%d \n",
			        src_x, src_y, src_w, src_h, dst_w, dst_h);
			step_set_pos = 1;
		} else if (strcmp(argv[i],"-w") == 0) {
			use_wl_shell = 1;
		}
		else if (strcmp(argv[i],"-loop") == 0) {
			use_wl_loop_time = 1;
		}
	}

	if (step_set_pos)
		init_y -= 32;

	if (scaling_mode & MODE_CROPPING) {
		printf("Cropped from (x,y,w,h) = (%d, %d, %d, %d)\n",
			src_x, src_y, src_w, src_h);
	}
	if (scaling_mode & MODE_SCALING) {
		printf("Scaled into %d x %d\n", dst_w, dst_h);
	}

	display = create_display();
	if (step_set_pos) {
		if ((dummy_w == 0) || (dummy_h == 0)) {
			fprintf(stderr, "output mode is not received.\n");
			exit(1);
		}
	}
	window = create_window(display, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (!window)
		return 1;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* Initialise damage to full surface, so the padding gets painted */
	wl_surface_damage(window->surface, 0, 0,
			  window->width, window->height);

	if (!window->wait_for_configure)
		redraw(window, NULL, 0);

	init_move = 1;

	while (running && ret != -1)
		ret = wl_display_dispatch(display->display);

	fprintf(stderr, "simple-shm exiting\n");

	if (window->display->ivi_application) {
		ivi_surface_destroy(window->ivi_surface);
		ivi_application_destroy(window->display->ivi_application);
	}

	destroy_window(window);
	destroy_display(display);

	return 0;
}
