/*
* Copyright 息 2008 Kristian H淡gsberg
* Copyright 息 2013 Collabora, Ltd.
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

//#include "config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cairo.h>

#include <linux/input.h>

#include "window.h"
#include "viewporter-client-protocol.h"
#include "cairo-util.h"

struct image {
	char *filename;
	cairo_surface_t *image;
	int width;
	int height;
};

struct box {
	struct display *display;
	struct window *window;
	struct widget *widget;
	int width, height;

	struct wp_viewporter *viewporter;
	struct wp_viewport *viewport;

	struct image image;

	int crop_x;
	int crop_y;
	int crop_width;
	int crop_height;
	int out_width;
	int out_height;
};

static void
set_my_viewport(struct box *box)
{
	wl_fixed_t src_x, src_y, src_width, src_height;
	int32_t dst_width = box->out_width;
	int32_t dst_height = box->out_height;

	src_x = wl_fixed_from_double(box->crop_x);
	src_y = wl_fixed_from_double(box->crop_y);
	src_width = wl_fixed_from_double(box->crop_width);
	src_height = wl_fixed_from_double(box->crop_height);

	//#ifdef DEBUG_RSCALER
	fprintf(stderr, "src_x \t%d\n", src_x);
	fprintf(stderr, "src_y \t%d\n", src_y);
	fprintf(stderr, "src_width \t%d\n", src_width);
	fprintf(stderr, "src_height \t%d\n", src_height);
	fprintf(stderr, "dst_width \t%d\n", dst_width);
	fprintf(stderr, "dst_height \t%d\n", dst_height);
	//#endif //DEBUG_RSCALER


	wp_viewport_set_source(box->viewport, src_x, src_y,
	src_width, src_height);
	wp_viewport_set_destination(box->viewport,
	dst_width, dst_height);
}

static void
resize_handler(struct widget *widget,
int32_t width, int32_t height, void *data)
{
	struct box *box = data;

	/* Dont resize me */
	widget_set_size(box->widget, box->width, box->height);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct box *box = data;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = window_get_surface(box->window);
	if (surface == NULL ||
		cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to create cairo egl surface\n");
		return;
	}

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, box->image.image, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	set_my_viewport(box);
}



static void
global_handler(struct display *display, uint32_t name,
const char *interface, uint32_t version, void *data)
{
	struct box *box = data;

	if (strcmp(interface, "wp_viewporter") == 0) {

		box->viewporter = display_bind(display, name,
								   &wp_viewporter_interface, 1);

		box->viewport = wp_viewporter_get_viewport(box->viewporter,
		widget_get_wl_surface(box->widget));
	}
}

static void
button_handler(struct widget *widget,
struct input *input, uint32_t time,
uint32_t button, enum wl_pointer_button_state state, void *data)
{
	struct box *box = data;

	if (button != BTN_LEFT)
		return;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		window_move(box->window, input,
		display_get_serial(box->display));
	}
}

int
main(int argc, char *argv[])
{
	struct box box;
	struct display *d;
	struct timeval tv;

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	if (argc != 8) {
		fprintf(stderr, "failed to num of argument!! %d\n", argc);
		fprintf(stderr, "ex) weston-rscale image.png cut_x cut_y cut_width cut_height out_width out_height \n");
		return -1;
	}

	// load image & get size
	box.image.filename = strdup(argv[1]);
	box.image.image = load_cairo_surface(argv[1]);
	box.image.width = cairo_image_surface_get_width(box.image.image);
	box.image.height = cairo_image_surface_get_height(box.image.image);

	box.crop_x = atoi(argv[2]);
	box.crop_y = atoi(argv[3]);
	box.crop_width = atoi(argv[4]);
	box.crop_height = atoi(argv[5]);
	box.out_width = atoi(argv[6]);
	box.out_height = atoi(argv[7]);

	//#ifdef DEBUG_RSCALER
	fprintf(stderr, "---------- MAIN ----------\n");
	fprintf(stderr, "box.image.width \t%d\n", box.image.width);
	fprintf(stderr, "box.image.height \t%d\n", box.image.height);
	fprintf(stderr, "box.crop_x \t%d\n", box.crop_x);
	fprintf(stderr, "box.crop_y \t%d\n", box.crop_y);
	fprintf(stderr, "box.crop_width \t%d\n", box.crop_width);
	fprintf(stderr, "box.crop_height \t%d\n", box.crop_height);
	fprintf(stderr, "box.out_width \t%d\n", box.out_width);
	fprintf(stderr, "box.out_height \t%d\n", box.out_height);
	fprintf(stderr, "---------- MAIN ----------\n");
	//#endif //DEBUG_RSCALER

	// check crop position
	if ((box.crop_x > box.image.width) || (box.crop_y > box.image.height) ||
		(box.crop_x < 0) || (box.crop_y < 0)) {
		fprintf(stderr, "failed to crop position!!\n");
		return -1;
	}

	// check crop size
	if ((box.crop_width > box.image.width - box.crop_x) ||
	(box.crop_height > box.image.height - box.crop_y) ||
	(box.crop_width < 0) || (box.crop_height < 0)) {
		fprintf(stderr, "failed to crop size!!\n");
		return -1;
	}

	gettimeofday(&tv, NULL);
	srandom(tv.tv_usec);

	box.width = box.image.width;
	box.height = box.image.height;
	box.display = d;

	box.window = window_create(d);
	box.widget = window_add_widget(box.window, &box);
	window_set_title(box.window, "Scaler Test Demo");

	widget_set_resize_handler(box.widget, resize_handler);
	widget_set_redraw_handler(box.widget, redraw_handler);

	widget_set_button_handler(box.widget, button_handler);
	widget_set_default_cursor(box.widget, CURSOR_HAND1);

	window_schedule_resize(box.window, box.width, box.height);

	display_set_user_data(box.display, &box);
	display_set_global_handler(box.display, global_handler);

	display_run(d);

	window_destroy(box.window);
	return 0;
}

