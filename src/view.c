/* Copyright 2015-2016. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <complex.h>
#include <stdbool.h>
#include <math.h>

#include <libgen.h>

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

	bool cross_hair;
	bool status_bar;

	const char* name;

	// geometry
	long* pos; //[DIMS];
	int xdim;
	int ydim;
	double xzoom;
	double yzoom;
	enum flip_t flip;
	bool transpose;

	// representation
	bool plot;
	enum mode_t mode;
	double winhigh;
	double winlow;
	double phrot;
	double max;
	enum interp_t interpolation;

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

	// geometry
	unsigned long geom_flags;
	const float (*geom)[3][3];
	const float (*geom_current)[3][3];

	// widgets
	GtkComboBox* gtk_mode;
	GtkComboBox* gtk_flip;
	GtkComboBox* gtk_interp;
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
	
	GtkWidget *dialog; // Save dialog
	GtkFileChooser *chooser; // Save dialog
	GtkWindow *window; 

	// windowing
	int lastx;
	int lasty;
};


#if 0
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
#endif

extern void update_geom(struct view_s* v)
{
	if (NULL == v->geom)
		return;

	long dims[DIMS];
	md_select_dims(DIMS, v->geom_flags, dims, v->dims);

	long strs[DIMS];
	md_calc_strides(DIMS, strs, dims, 1);

	v->geom_current = &v->geom[md_calc_offset(DIMS, strs, v->pos)];

	printf("%f %f %f %f %f %f %f %f %f\n",
		(*v->geom_current)[0][0], (*v->geom_current)[0][1], (*v->geom_current)[0][2],
		(*v->geom_current)[1][0], (*v->geom_current)[1][1], (*v->geom_current)[1][2],
		(*v->geom_current)[2][0], (*v->geom_current)[2][1], (*v->geom_current)[2][2]);
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
	UNUSED(widget);
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
	UNUSED(event);
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


extern void view_add_geometry(struct view_s* v, unsigned long flags, const float (*geom)[3][3])
{
	v->geom_flags = flags;
	v->geom = geom;
	v->geom_current = NULL;

	update_geom(v);
}


extern gboolean refresh_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
	view_refresh(data);
	return FALSE;
}


extern gboolean geom_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
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
	v->interpolation = gtk_combo_box_get_active(v->gtk_interp);
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

	update_geom(v);
	update_view(v);

	return FALSE;
}

extern gboolean window_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
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

static void update_buf_view(struct view_s* v)
{
	update_buf(v->xdim, v->ydim, DIMS, v->dims, v->strs, v->pos,
		v->flip, v->interpolation, v->xzoom, v->yzoom, v->plot,
		v->rgbw, v->rgbh, v->data, v->buf);
}


static const char* spec = "xyzcmnopqsfrtuvw";

extern gboolean save_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
	struct view_s* v = data;
	
	int len = 0;

	len += snprintf(NULL, 0, "%s_", v->name);

	for (int i = 0; i < DIMS; i++)
		if ((v->dims[i] != 1) && (i != v->xdim) && (i != v->ydim))
			len += snprintf(NULL, 0, "%c%04ld", spec[i], v->pos[i]);

	len += snprintf(NULL, 0, ".png");

	len++;

	char* name = xmalloc(len);

	int off = 0;

	off += snprintf(name + off, len - off, "%s_", v->name);

	for (int i = 0; i < DIMS; i++)
		if ((v->dims[i] != 1) && (i != v->xdim) && (i != v->ydim))
			off += snprintf(name + off, len - off, "%c%04ld", spec[i], v->pos[i]);

	off += snprintf(name + off, len - off, ".png");



	v->dialog = gtk_file_chooser_dialog_new("Save File",
                                      v->window,
				      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

	v->chooser = GTK_FILE_CHOOSER(v->dialog);

	gtk_file_chooser_set_current_name(v->chooser, basename(name));
	
	char* dname = strdup(v->name);

	// Outputfolder = Inputfolder
	gtk_file_chooser_set_current_folder(v->chooser, dirname(dname));
	gtk_file_chooser_set_do_overwrite_confirmation(v->chooser, TRUE);

	gint res = gtk_dialog_run(GTK_DIALOG(v->dialog));

	if (GTK_RESPONSE_ACCEPT == res) {

		// export single image
		char *filename = gtk_file_chooser_get_filename(v->chooser);

		if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->source, filename))
			gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");

		gtk_entry_set_text(v->gtk_entry, "Saved!");

		g_free(filename);
	}

	gtk_widget_destroy (v->dialog);

	xfree(name);
	xfree(dname);

	return FALSE;
}



extern gboolean save_movie_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
	struct view_s* v = data;

	int frame_dim = 10;

	v->dialog = gtk_file_chooser_dialog_new("Export movie to folder",
						v->window,
						GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						"Cancel",
						GTK_RESPONSE_CANCEL,
						"Export",
						GTK_RESPONSE_ACCEPT,
						NULL);

	v->chooser = GTK_FILE_CHOOSER(v->dialog);


	char* dname = strdup(v->name);

	// Outputfolder = Inputfolder
	gtk_file_chooser_set_current_folder(v->chooser, dirname(dname));

	gint res = gtk_dialog_run(GTK_DIALOG (v->dialog));

	if (GTK_RESPONSE_ACCEPT == res) {

		char *chosen_dir = gtk_file_chooser_get_filename(v->chooser);

		for (unsigned int f = 0; f < v->dims[frame_dim]; f++) {

			v->pos[frame_dim] = f;
			update_buf_view(v);

			draw(v->rgbw, v->rgbh, v->rgbstr, (unsigned char(*)[v->rgbw][v->rgbstr / 4][4])v->rgb,
				v->mode, 1. / v->max, v->winlow, v->winhigh, v->phrot,
				v->rgbw, v->buf);

			char output_name[256];
			int len = snprintf(output_name, 256, "%s/mov-%04d.png", chosen_dir, f);

			if (len + 1 >= sizeof(output_name)) {

				gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");
				break;
			}

			if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->source, output_name))
				gtk_entry_set_text(v->gtk_entry, "Error: writing image file.\n");
		}

		g_free(chosen_dir);
		gtk_entry_set_text(v->gtk_entry, "Movie exported.");
	}

	gtk_widget_destroy (v->dialog);

	xfree(dname);

	return FALSE;
}


struct xy_s { float x; float y; };

static struct xy_s pos2screen(const struct view_s* v, const float (*pos)[DIMS])
{
	float x = (*pos)[v->xdim];
	float y = (*pos)[v->ydim];

	if ((XY == v->flip) || (XO == v->flip))
		x = v->dims[v->xdim] - 1 - x;

	if ((XY == v->flip) || (OY == v->flip))
		y = v->dims[v->ydim] - 1 - y;

	// shift to the center of pixels
	x += 0.5;
	y += 0.5;

	x *= v->xzoom;
	y *= v->yzoom;

	if (v->plot)
		y = v->rgbh / 2;

	return (struct xy_s){ x, y };
}

static void screen2pos(const struct view_s* v, float (*pos)[DIMS], struct xy_s xy)
{
	for (unsigned int i = 0; i < DIMS; i++)
		(*pos)[i] = v->pos[i];

	float x = xy.x / v->xzoom - 0.5;
	float y = xy.y / v->yzoom - 0.5;

	if ((XY == v->flip) || (XO == v->flip))
		x = v->dims[v->xdim] - 1 - x;

	if ((XY == v->flip) || (OY == v->flip))
		y = v->dims[v->ydim] - 1 - y;

	(*pos)[v->xdim] = roundf(x);

	if (!v->plot)
		(*pos)[v->ydim] = roundf(y);
}



static void clear_status_bar(struct view_s* v)
{
	char buf = '\0';
	gtk_entry_set_text(v->gtk_entry, &buf);
}


static void update_status_bar(struct view_s* v, const float (*pos)[DIMS])
{
	int x2 = (*pos)[v->xdim];
	int y2 = (*pos)[v->ydim];

	complex float val = sample(DIMS, *pos, v->dims, v->strs, v->interpolation, v->data);

	// FIXME: make sure this matches exactly the pixel
	char buf[100];
	snprintf(buf, 100, "Pos: %03d %03d Magn: %.3e Val: %+.3e%+.3ei Arg: %+.2f", x2, y2,
			cabsf(val), crealf(val), cimagf(val), cargf(val));

	gtk_entry_set_text(v->gtk_entry, buf);
}


extern gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	UNUSED(widget);
	struct view_s* v = data;

	if (v->invalid) {

		update_buf_view(v);

		v->invalid = false;
		v->rgb_invalid = true;
	}

	if (v->rgb_invalid) {

		(v->plot ? draw_plot : draw)(v->rgbw, v->rgbh, v->rgbstr,
			(unsigned char(*)[v->rgbw][v->rgbstr / 4][4])v->rgb,
			v->mode, 1. / v->max, v->winlow, v->winhigh, v->phrot,
			v->rgbw, v->buf);

		v->rgb_invalid = false;
	}


	// add_text(v->source, 3, 3, 10, v->name);

	if (v->cross_hair) {

		float posi[DIMS];
		for (unsigned int i = 0; i < DIMS; i++)
			posi[i] = v->pos[i];

		struct xy_s xy = pos2screen(v, &posi);

		draw_line(v->rgbw, v->rgbh, v->rgbstr, (unsigned char (*)[v->rgbw][v->rgbstr / 4][4])v->rgb,
				0, (int)xy.y, v->rgbw - 1, (int)xy.y, (v->xdim > v->ydim) ? &color_red : &color_blue);

		draw_line(v->rgbw, v->rgbh, v->rgbstr, (unsigned char (*)[v->rgbw][v->rgbstr / 4][4])v->rgb,
				(int)xy.x, 0, (int)xy.x, v->rgbh - 1, (v->xdim < v->ydim) ? &color_red : &color_blue);

//		float coords[4][2] = { { 0, 0 }, { 100, 0 }, { 0, 100 }, { 100, 100 } };
//		draw_grid(v->rgbw, v->rgbh, v->rgbstr, (unsigned char (*)[v->rgbw][v->rgbstr / 4][4])v->rgb, &coords, 4, &color_white);
	}

	if (v->status_bar) {

		float posi[DIMS];
		for (unsigned int i = 0; i < DIMS; i++)
			posi[i] = v->pos[i];

		update_status_bar(v, &posi);

		for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next)
			if (v->sync && v2->sync)
				update_status_bar(v2, &posi);
	}

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

	v->cross_hair = false;
	v->status_bar = false;

	v->name = name;
	v->max = 1.;

	v->pos = xmalloc(DIMS * sizeof(long));

	for (int i = 0; i < DIMS; i++)
		v->pos[i] = (NULL != pos) ? pos[i] : 0;

	v->xdim = sq_dims[0];
	v->ydim = sq_dims[1];

	v->plot = false;

	v->xzoom = 2.;
	v->yzoom = 2.;

	v->source = NULL;
	v->rgb = NULL;
	v->buf = NULL;


	md_copy_dims(DIMS, v->dims, dims);
	md_calc_strides(DIMS, v->strs, dims, sizeof(complex float));
	v->data = data;

	v->geom_flags = 0ul;
	v->geom = NULL;
	v->geom_current = NULL;

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


extern gboolean toggle_sync(GtkToggleButton* button, gpointer data)
{
	UNUSED(button);
	struct view_s* v = data;
	v->sync = !v->sync;

	return FALSE;
}


extern gboolean toggle_plot(GtkToggleButton* button, gpointer data)
{
	UNUSED(button);
	struct view_s* v = data;
	v->plot = !v->plot;

	gtk_widget_set_sensitive(GTK_WIDGET(v->gtk_mode),
		v->plot ? FALSE : TRUE);

	v->invalid = true;
	update_view(v);

	return FALSE;
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
	UNUSED(widget);
	struct view_s* v = data;

	struct xy_s xy = { event->x, event->y };

	float pos[DIMS];
	screen2pos(v, &pos, xy);

	if (event->button == GDK_BUTTON_PRIMARY) {

		v->cross_hair = false;
		v->status_bar = false;
		clear_status_bar(v);

		v->rgb_invalid = true;
		update_view(v);
	}

	if (event->button == GDK_BUTTON_SECONDARY) {

		v->cross_hair = true;
		v->status_bar = true;

		set_position(v, v->xdim, pos[v->xdim]);
		set_position(v, v->ydim, pos[v->ydim]);
	}

	return FALSE;
}



extern gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	UNUSED(widget);
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
	UNUSED(widget);
	UNUSED(event);
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
#if 0
	PangoFontDescription* desc = pango_font_description_new();
	pango_font_description_set_family(desc, "mono");
	pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
	gtk_widget_override_font(GTK_WIDGET(v->gtk_entry), desc);
	pango_font_description_free(desc);
#endif
	v->gtk_zoom = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "zoom"));
	v->gtk_aniso = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "aniso"));

	v->gtk_mode = GTK_COMBO_BOX(gtk_builder_get_object(builder, "mode"));
	gtk_combo_box_set_active(v->gtk_mode, 0);

	v->gtk_flip = GTK_COMBO_BOX(gtk_builder_get_object(builder, "flip"));
	gtk_combo_box_set_active(v->gtk_flip, 0);

	v->gtk_interp = GTK_COMBO_BOX(gtk_builder_get_object(builder, "interp"));
	gtk_combo_box_set_active(v->gtk_interp, 0);
    
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
	v->window = window;
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
	UNUSED(widget);
	struct view_s* v = data;

	view_clone(v, v->pos);

	return FALSE;
}


