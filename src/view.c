/* Copyright 2015-2016. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Author:
 *	2015-2016 Martin Uecker <martin.uecker@med.uni-goettinge.de>
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
#include "misc/png.h"
#include "misc/debug.h"

#include "draw.h"

#include "view.h"

#ifndef DIMS
#define DIMS 16
#endif

#define STRINGIFY(x) # x
const char* viewer_gui =
#include "viewer.inc"
;

struct view_s {

	struct view_s* next;
	struct view_s* prev;
	bool sync;

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

	complex float* buf;

	cairo_surface_t* source;

	// rgb buffer
	int rgbh;
	int rgbw;
	int rgbstr;
	unsigned char* rgb;
	bool invalid;
	bool rgb_invalid;

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
	GtkToggleToolButton* gtk_fit;

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

	v->rgbw = v->dims[v->xdim] * v->xzoom;
	v->rgbh = v->dims[v->ydim] * v->yzoom;
	v->rgbstr = 4 * v->rgbw;
	v->rgb = realloc(v->rgb, v->rgbh * v->rgbstr);

	v->buf = realloc(v->buf, v->rgbh * v->rgbw * sizeof(complex float));

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

	gboolean flag = gtk_toggle_tool_button_get_active(v->gtk_fit);

	if (!flag)
		return FALSE;

	double aniso = gtk_adjustment_get_value(v->gtk_aniso);

	GtkAllocation alloc;
	gtk_widget_get_allocation(v->gtk_viewport, &alloc);
	double xz = (double)(alloc.width - 5) / (double)v->dims[v->xdim];
	double yz = (double)(alloc.height - 5) / (double)v->dims[v->ydim];


	if (yz > xz / aniso)
		yz = xz / aniso; // aniso

	gtk_adjustment_set_value(v->gtk_zoom, yz);

	return FALSE;
}


extern gboolean configure_callback(GtkWidget *widget, GdkEvent* event, gpointer data)
{
	return fit_callback(widget, data);
}


extern void view_setpos(struct view_s* v, unsigned int flags, const long pos[DIMS])
{
	for (unsigned int i = 0; i < DIMS; i++) {

		if (MD_IS_SET(flags, i)) {

			gtk_adjustment_set_value(v->gtk_posall[i], pos[i]);

			for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next)
				if (v->sync && v2->sync)
					gtk_adjustment_set_value(v2->gtk_posall[i], pos[i]);
		}
	}
}


extern void view_refresh(struct view_s* v)
{
	v->invalid = true;

	long size = md_calc_size(DIMS, v->dims);
	double max = 0.;
	for (long j = 0; j < size; j++)
		if (max < cabsf(v->data[j]))
			max = cabsf(v->data[j]);

	if (0. == max)
		max = 1.;

	v->max = max;

	update_view(v);
}

extern gboolean refresh_callback(GtkWidget *widget, gpointer data)
{
	view_refresh(data);
	return FALSE;
}


extern gboolean geom_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	for (int j = 0; j < DIMS; j++) {

		v->pos[j] = gtk_adjustment_get_value(v->gtk_posall[j]);
		bool check = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[j]));

		if (!check)
			continue;

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

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->xdim]), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->ydim]), TRUE);

	double zoom = gtk_adjustment_get_value(v->gtk_zoom);
	double aniso = gtk_adjustment_get_value(v->gtk_aniso);
	v->xzoom = zoom * aniso;
	v->yzoom = zoom;

	v->flip = gtk_combo_box_get_active(v->gtk_flip);
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

extern gboolean window_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	v->mode = gtk_combo_box_get_active(v->gtk_mode);
	v->winlow = gtk_adjustment_get_value(v->gtk_winlow);
	v->winhigh = gtk_adjustment_get_value(v->gtk_winhigh);

	for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next) {

		if (v->sync && v2->sync) {

			gtk_adjustment_set_value(v2->gtk_winlow, v->winlow);
			gtk_adjustment_set_value(v2->gtk_winhigh, v->winhigh);
			gtk_combo_box_set_active(v2->gtk_mode, v->mode);
		}
	}

	v->rgb_invalid = true;

	update_view(v);

	return FALSE;
}

void update_buf(struct view_s* v);


extern gboolean movie_callback(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	int wdim = 10;

	for (unsigned int f = 0; f < v->dims[wdim]; f++) {

		v->pos[wdim] = f;
		update_buf(v);

		draw(v->rgbw, v->rgbh, v->rgbstr, v->rgb,
			v->mode, 1. / v->max, v->winlow, v->winhigh, v->phrot,
			v->rgbw, v->buf);

		char name[16];
		snprintf(name, 16, "mov-%04d.png", f);

		if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->source, name))
			gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");
	}

	gtk_entry_set_text(v->gtk_entry, "Movie exported.");

	return FALSE;
}



extern gboolean save_callback(GtkWidget *widget, gpointer data)
{
#if 1
	struct view_s* v = data;

	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->source, "save.png"))
		gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");

	gtk_entry_set_text(v->gtk_entry, "Saved to: save.png");

	UNUSED(movie_callback);
#else
	movie_callback(widget, data);
#endif
	return FALSE;
}


void update_buf(struct view_s* v)
{
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

	resample(v->rgbw, v->rgbh, v->rgbw, v->buf,
		DIMS, dpos, dx, dy, v->dims, v->strs, v->data);
}


extern gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct view_s* v = data;

	if (v->invalid) {

		update_buf(v);

		v->invalid = false;
		v->rgb_invalid = true;
	}

	if (v->rgb_invalid) {

		draw(v->rgbw, v->rgbh, v->rgbstr, v->rgb,
			v->mode, 1. / v->max, v->winlow, v->winhigh, v->phrot,
			v->rgbw, v->buf);


		v->rgb_invalid = false;
	}


	// add_text(v->source, 3, 3, 10, v->name);

	cairo_set_source_surface(cr, v->source, 0, 0);
	cairo_paint(cr);
	return FALSE;
}




struct view_s* create_view(const char* name, const long pos[DIMS], const long dims[DIMS], const complex float* data)
{
	long sq_dims[2] = { 0 };

	int l = 0;

	for (int i = 0; (i < DIMS) && (l < 2); i++)
		if (1 != dims[i])
			sq_dims[l++] = i;

	assert(2 == l);

	struct view_s* v = xmalloc(sizeof(struct view_s));

	v->next = v->prev = v;
	v->sync = true;

	v->name = name;
	v->max = 1.;

	v->pos = xmalloc(DIMS * sizeof(long));

	for (int i = 0; i < DIMS; i++)
		v->pos[i] = (NULL != pos) ? pos[i] : 0;

	v->xdim = sq_dims[0];
	v->ydim = sq_dims[1];

	v->xzoom = 2.;
	v->yzoom = 2.;

	v->source = NULL;
	v->rgb = NULL;
	v->buf = NULL;


	md_copy_dims(DIMS, v->dims, dims);
	md_calc_strides(DIMS, v->strs, dims, sizeof(complex float));
	v->data = data;

	v->winlow = 0.;
	v->winhigh = 1.;
	v->phrot = 0.;

	v->lastx = -1;
	v->lasty = -1;

	v->invalid = true;

	return v;
}


static void delete_view(struct view_s* v)
{
	v->next->prev = v->prev;
	v->prev->next = v->next;

	free(v->buf);
	free(v->rgb);

#if 0
	free(v->pos);
	//free(v);
#endif
}


extern gboolean toggle_sync(GtkWidget *widget, GtkToggleButton* button, gpointer data)
{
	struct view_s* v = data;
	v->sync = !v->sync;

	return FALSE;
}


static void update_status_bar(struct view_s* v, int x2, int y2)
{
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


extern void set_position(struct view_s* v, unsigned int dim, unsigned int p)
{
	v->pos[dim] = p;

	gtk_adjustment_set_value(v->gtk_posall[dim], p);

	for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next)
		if (v->sync && v2->sync)
			gtk_adjustment_set_value(v2->gtk_posall[dim], p);
}


extern gboolean button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct view_s* v = data;

	int y = event->y;
	int x = event->x;

	int x2 = x / v->xzoom;
	int y2 = y / v->yzoom;

	if (event->button == GDK_BUTTON_SECONDARY) {

		set_position(v, v->xdim, x2);
		set_position(v, v->ydim, y2);

		update_status_bar(v, x2, y2);

		for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next)
			if (v->sync && v2->sync)
				update_status_bar(v2, x2, y2);
	}

	return FALSE;
}



extern gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	struct view_s* v = data;

	int y = event->y;
	int x = event->x;

	if (event->state & GDK_BUTTON1_MASK) {

		if (-1 != v->lastx) {

			double low = gtk_adjustment_get_value(v->gtk_winlow);
			double high = gtk_adjustment_get_value(v->gtk_winhigh);

			low -= (x - v->lastx) / 200.;
			high -= (y - v->lasty) / 200.;

			if (high > low) {

				gtk_adjustment_set_value(v->gtk_winlow, low);
				gtk_adjustment_set_value(v->gtk_winhigh, high);

				for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next) {

					if (v->sync && v2->sync) {

						gtk_adjustment_set_value(v2->gtk_winlow, low);
						gtk_adjustment_set_value(v2->gtk_winhigh, high);
					}
				}
			}
		}

		v->lastx = x;
		v->lasty = y;

	} else {

		v->lastx = -1;
		v->lasty = -1;
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

extern gboolean window_close(GtkWidget *widget, GdkEvent* event, gpointer data)
{
	struct view_s* v = data;

	delete_view(v);

	if (0 == --nr_windows)
		gtk_main_quit();

	return FALSE;
}




extern struct view_s* window_new(const char* name, const long pos[DIMS], const long dims[DIMS], const complex float* x)
{
	struct view_s* v = create_view(name, pos, dims, x);

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
	v->gtk_fit = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "fit"));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(v->gtk_fit), TRUE);

	for (int j = 0; j < DIMS; j++) {

		char pname[10];
		snprintf(pname, 10, "pos%02d", j);
		v->gtk_posall[j] = GTK_ADJUSTMENT(gtk_builder_get_object(builder, pname));
		gtk_adjustment_set_upper(v->gtk_posall[j], v->dims[j] - 1);
		gtk_adjustment_set_value(v->gtk_posall[j], v->pos[j]);

		snprintf(pname, 10, "check%02d", j);
		v->gtk_checkall[j] = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, pname));
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->xdim]), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->gtk_checkall[v->ydim]), TRUE);

#if 0
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "toolbar1")));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "toolbar2")));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "entry")));
#endif
	gtk_builder_connect_signals(builder, v);

	GtkWindow* window = GTK_WINDOW(gtk_builder_get_object(builder, "window1"));
	g_object_unref(G_OBJECT(builder));

	gtk_window_set_title(window, name);
	gtk_widget_show(GTK_WIDGET(window));

	nr_windows++;

//	fit_callback(NULL, v);
	refresh_callback(NULL, v);
	geom_callback(NULL, v);
	window_callback(NULL, v);

	return v;
}

void window_connect_sync(struct view_s* v, struct view_s* v2)
{
	// add to linked list for sync
	v2->next = v->next;
	v->next->prev = v2;
	v2->prev = v;
	v->next = v2;

	window_callback(NULL, v);
}

struct view_s* view_clone(struct view_s* v, const long pos[DIMS])
{
	struct view_s* v2 = window_new(v->name, pos, v->dims, v->data);

	window_connect_sync(v, v2);

	return v2;
}

extern gboolean window_clone(GtkWidget *widget, gpointer data)
{
	struct view_s* v = data;

	view_clone(v, v->pos);

	return FALSE;
}




