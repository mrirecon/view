/* Copyright 2015. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Author:
 *	2015 Martin Uecker <martin.uecker@med.uni-goettinge.de>
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <complex.h>
#include <stdbool.h>

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cairo.h>

#undef MAX
#undef MIN

#include "num/multind.h"

#include "misc/misc.h"
#include "misc/mmio.h"
#include "misc/png.h"
#include "misc/debug.h"

#include "draw.h"

#ifndef DIMS
#define DIMS 16
#endif

#define STRINGIFY(x) # x
const char* viewer_gui =
#include "viewer.inc"
;

struct view_s {

	const char* name;

	// geometry
	long* pos; //[DIMS];
	int xdim;
	int ydim;
	double xzoom;
	double yzoom;
	enum flip_t { OO, XO, OY, XY } flip;
	bool transpose;

	// representation
	enum mode_t mode;
	double winhigh;
	double winlow;
	double phrot;
	double max;

	cairo_surface_t* source;

	// rgb buffer
	int rgbh;
	int rgbw;
	int rgbstr;
	unsigned char* rgb;
	bool invalid;
	
	// data	
	long dims[DIMS];
	long strs[DIMS];
	const complex float* data;

	// widgets
	GtkComboBox* gtk_mode;
	GtkComboBox* gtk_flip;
	GtkWidget* gtk_drawingarea;
	GtkWidget* gtk_viewport;
	GtkAdjustment* gtk_winlow;
	GtkAdjustment* gtk_winhigh;
	GtkAdjustment* gtk_zoom;
	GtkAdjustment* gtk_aniso;
	GtkEntry* gtk_entry;
	GtkToggleToolButton* gtk_transpose;

	GtkAdjustment* gtk_posall[DIMS];
	GtkCheckButton* gtk_checkall[DIMS];

	// windowing
	int lastx;
	int lasty;
};



static void add_text(cairo_surface_t* surface, int x, int y, int size, const char* text)
{
	cairo_t* cr = cairo_create(surface);
	cairo_set_source_rgb(cr, 1., 1., 1.);

	PangoLayout* layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, text, -1);
 	PangoFontDescription* desc = pango_font_description_new();
	pango_font_description_set_family(desc, "sans");
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
 	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	int w = 0;
	int h = 0;
	pango_layout_get_pixel_size(layout, &w, &h);

	cairo_move_to(cr, (x >= 0) ? x : -(x + (double)w),
			  (y >= 0) ? y : -(y + (double)h));

	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	cairo_destroy(cr);
}




extern gboolean update_view(struct view_s* v)
{
	if (NULL != v->source)
		cairo_surface_destroy(v->source);

	v->rgbh = v->dims[v->xdim] * v->xzoom;
	v->rgbw = v->dims[v->ydim] * v->yzoom;
	v->rgbstr = 4 * v->rgbw;
	v->rgb = realloc(v->rgb, v->rgbh * v->rgbstr);

	v->source = cairo_image_surface_create_for_data(v->rgb,
                             CAIRO_FORMAT_RGB24, v->rgbw, v->rgbh, v->rgbstr);

	// trigger redraw

	gtk_widget_set_size_request(v->gtk_drawingarea, v->rgbw, v->rgbh);
	gtk_widget_queue_draw(v->gtk_drawingarea);

	return FALSE;
}


extern gboolean fit_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	double aniso = gtk_adjustment_get_value(v->gtk_aniso);

	GtkAllocation alloc;
	gtk_widget_get_allocation(v->gtk_viewport, &alloc);
	double xz = (double)(alloc.height - 5) / (double)v->dims[v->xdim];
	double yz = (double)(alloc.width - 5) / (double)v->dims[v->ydim];


	if (xz > yz / aniso)
		xz = yz / aniso; // aniso

	gtk_adjustment_set_value(v->gtk_zoom, xz);

	return FALSE;
}

extern gboolean gui_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	for (int j = 0; j < DIMS; j++) {

		v->pos[j] = gtk_adjustment_get_value(v->gtk_posall[j]);
		bool check = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[j]));

		if (check) {

			if (1 == v->dims[j])
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[j]), FALSE);
			else
			if ((j != v->xdim) && (j != v->ydim)) {

				for (int i = 0; i < DIMS; i++) {

					if (v->xdim == (DIMS + j - i) % DIMS) {

						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->xdim]), FALSE);
						v->xdim = j;
						break;
					}

					if (v->ydim == (DIMS + j - i) % DIMS) {

						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->ydim]), FALSE);
						v->ydim = j;
						break;
					}

				}
			}
		}
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->xdim]), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->ydim]), TRUE);

	double zoom = gtk_adjustment_get_value(v->gtk_zoom);
	double aniso = gtk_adjustment_get_value(v->gtk_aniso);
	v->xzoom = zoom;
	v->yzoom = zoom * aniso;

	v->mode = gtk_combo_box_get_active(v->gtk_mode);
	v->flip = gtk_combo_box_get_active(v->gtk_flip);
	v->winlow = gtk_adjustment_get_value(v->gtk_winlow);
	v->winhigh = gtk_adjustment_get_value(v->gtk_winhigh);
	v->transpose = gtk_toggle_tool_button_get_active(v->gtk_transpose);

	if (v->transpose) {

		if (v->xdim < v->ydim) {

			int swp = v->xdim;
			v->xdim = v->ydim;
			v->ydim = swp;
		}

	} else {

		if (v->xdim > v->ydim) {

			int swp = v->xdim;
			v->xdim = v->ydim;
			v->ydim = swp;
		}
	}

	v->lastx = -1;
	v->lasty = -1;

	v->invalid = true;

	update_view(v);

	return FALSE;
}


extern gboolean save_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->source, "save.png"))
		gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");

	gtk_entry_set_text(v->gtk_entry, "Saved to: save.png");

	return FALSE;
}


extern gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct view_s* v = data;

	if (v->invalid) {

		double dpos[DIMS];
		for (int i = 0; i < DIMS; i++)
			dpos[i] = v->pos[i];

		dpos[v->xdim] = 0.;
		dpos[v->ydim] = 0.;

		double dx[DIMS];
		for (int i = 0; i < DIMS; i++)
			dx[i] = 0.;

		double dy[DIMS];
		for (int i = 0; i < DIMS; i++)
			dy[i] = 0.;

		dx[v->xdim] = 1.;
		dy[v->ydim] = 1.;

		if ((XY == v->flip) || (XO == v->flip)) {

			dpos[v->xdim] = v->dims[v->xdim] - 1;
			dx[v->xdim] *= -1.;
		}

		if ((XY == v->flip) || (OY == v->flip)) {

			dpos[v->ydim] = v->dims[v->ydim] - 1;
			dy[v->ydim] *= -1.;
		}

		dx[v->xdim] = dx[v->xdim] / v->xzoom;
		dy[v->ydim] = dy[v->ydim] / v->yzoom;

		draw(v->rgbh, v->rgbw, v->rgbstr, v->rgb,
			v->mode, 1. / v->max, v->winlow, v->winhigh, v->phrot,
			DIMS, dpos, dx, dy, v->dims, v->strs, v->data);
	
		v->invalid = false;
	}

	// add_text(v->source, 3, 3, 10, v->name);

	cairo_set_source_surface(cr, v->source, 0, 0);
	cairo_paint(cr);
	return FALSE;
}




struct view_s* create_view(const char* name, long* pos, double max, const long dims[DIMS], const complex float* data)
{
	long sq_dims[2] = { 0 };

	int l = 0;

	for (int i = 0; (i < DIMS) && (l < 2); i++)
		if (1 != dims[i])
			sq_dims[l++] = i;

	assert(2 == l);

	struct view_s* v = xmalloc(sizeof(struct view_s));

	v->name = name;
	v->pos = pos;
	v->max = max;

	if (NULL == v->pos)
		v->pos = xmalloc(DIMS * sizeof(long));

	v->xdim = sq_dims[0];
	v->ydim = sq_dims[1];

	v->xzoom = 2.;
	v->yzoom = 2.;

	v->source = NULL;
	v->rgb = NULL;


	md_copy_dims(DIMS, v->dims, dims);
	md_calc_strides(DIMS, v->strs, dims, sizeof(complex float));
	v->data = data;

	for (int i = 0; i < DIMS; i++)
		v->pos[i] = 0;


	v->winlow = 0.;
	v->winhigh = 1.;
	v->phrot = 0.;

	v->lastx = -1;
	v->lasty = -1;

	v->invalid = true;

	return v;
}


gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	struct view_s* v = data;

	int x = event->x;
	int y = event->y;


	if (event->state & GDK_BUTTON1_MASK) {

		if (-1 != v->lastx) {

			double low = gtk_adjustment_get_value(v->gtk_winlow);
			double high = gtk_adjustment_get_value(v->gtk_winhigh);

			low -= (x - v->lastx) / 200.;
			high -= (y - v->lasty) / 200.;

			if (high > low) {

				gtk_adjustment_set_value(v->gtk_winlow, low);
				gtk_adjustment_set_value(v->gtk_winhigh, high);
			}
		}

		v->lastx = x;	
		v->lasty = y;

	} else {

		v->lastx = -1;
		v->lasty = -1;
	}
  
	int x2 = x / v->xzoom;
	int y2 = y / v->yzoom;

    	if (event->state & GDK_BUTTON2_MASK) {

		v->pos[v->xdim] = x2;
		v->pos[v->ydim] = y2;

		gtk_adjustment_set_value(v->gtk_posall[v->xdim], v->pos[v->xdim]);
		gtk_adjustment_set_value(v->gtk_posall[v->ydim], v->pos[v->ydim]);
	}

	if ((x2 < v->dims[v->xdim]) && (y2 < v->dims[v->ydim])) {

		float pos[DIMS];
		
		for (int i = 0; i < DIMS; i++)
			pos[i] = v->pos[i];

		pos[v->xdim] = x2;
		pos[v->ydim] = y2;

		complex float val = sample(DIMS, pos, v->dims, v->strs, v->data);
		// FIXME: make sure this matches exactly the pixel
		char buf[100];
		snprintf(buf, 100, "Pos: %03d %03d Magn: %.3e Val: %+.3e%+.3ei Arg: %+.2f", x2, y2, 
				cabsf(val), crealf(val), cimagf(val), cargf(val));
		
		gtk_entry_set_text(v->gtk_entry, buf);
	}

	return FALSE;
}



extern gboolean show_hide(GtkWidget *widget, GtkCheckButton* button)
{
	gboolean flag = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(button));
	(flag ? gtk_widget_show : gtk_widget_hide)(widget);

	return FALSE;
}

static int nr_windows = 0;

extern gboolean window_close(GtkWidget *widget, gpointer data)
{
	if (0 == --nr_windows)
		gtk_main_quit();

	return FALSE;
}




static void window_new(const char* name, long* pos, double max, const long dims[DIMS], const complex float* x)
{
	struct view_s* v = create_view(name, pos, max, dims, x);

	GtkBuilder* builder = gtk_builder_new();
	// gtk_builder_add_from_file(builder, "viewer.ui", NULL);
	gtk_builder_add_from_string(builder, viewer_gui, -1, NULL);

	v->gtk_drawingarea = GTK_WIDGET(gtk_builder_get_object(builder, "drawingarea1"));
	v->gtk_viewport = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow1"));

	v->gtk_winlow = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "winlow"));
	v->gtk_winhigh = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "winhigh"));

	v->gtk_entry = GTK_ENTRY(gtk_builder_get_object(builder, "entry"));
	PangoFontDescription* desc = pango_font_description_new();
	pango_font_description_set_family(desc, "mono");
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
	gtk_widget_override_font(GTK_WIDGET(v->gtk_entry), desc);
	pango_font_description_free(desc);

	v->gtk_zoom = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "zoom"));
	v->gtk_aniso = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "aniso"));

	v->gtk_mode = GTK_COMBO_BOX(gtk_builder_get_object(builder, "mode"));
	gtk_combo_box_set_active(v->gtk_mode, 0);

	v->gtk_flip = GTK_COMBO_BOX(gtk_builder_get_object(builder, "flip"));
	gtk_combo_box_set_active(v->gtk_flip, 0);

	v->gtk_transpose = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "transpose"));

	for (int j = 0; j < DIMS; j++) {

		char pname[10];
		snprintf(pname, 10, "pos%02d", j);
		v->gtk_posall[j] = GTK_ADJUSTMENT(gtk_builder_get_object(builder, pname));
		gtk_adjustment_set_upper(v->gtk_posall[j], v->dims[j] - 1);
		gtk_adjustment_set_value(v->gtk_posall[j], 0);

		snprintf(pname, 10, "check%02d", j);
		v->gtk_checkall[j] = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, pname));
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->xdim]), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->ydim]), TRUE);

	gtk_builder_connect_signals(builder, v);

	GtkWindow* window = GTK_WINDOW(gtk_builder_get_object(builder, "window1"));
	g_object_unref(G_OBJECT(builder));

	gtk_window_set_title(window, name);

	gtk_widget_show(GTK_WIDGET(window));

	nr_windows++;

	gui_callback(NULL, v);
}

extern gboolean window_clone(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	window_new(v->name, v->pos, v->max, v->dims, v->data);

	return FALSE;
}


int main(int argc, char* argv[])
{
	int c;
	gtk_init(&argc, &argv);

	while (-1 != (c = getopt(argc, argv, "h"))) {

		switch (c) {
		case 'h':
		default:
			abort();
		}
	}

	if (argc - optind < 1)
		abort();


	for (int i = optind; i < argc; i++) {

		long dims[DIMS];
		complex float* x = load_cfl(argv[i], DIMS, dims);

		long size = md_calc_size(DIMS, dims);
		double max = 0.;
		for (long j = 0; j < size; j++)
			if (max < cabsf(x[j]))
				max = cabsf(x[j]);

		if (0. == max)
			max = 1.;
	
		// FIXME: we never delete them
		window_new(argv[i], NULL, max, dims, x);
	}

	gtk_main();

	return 0;
}

