/* Copyright 2015-2019. Martin Uecker.
 * Copyright 2023-2024. TU Graz. Institute of Biomedical Imaging.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 */

#define _GNU_SOURCE
#include <complex.h>
#include <stdbool.h>
#include <math.h>

#include <stdio.h>

#include "gtk_ui.h"

#include "num/multind.h"

#include "misc/misc.h"
#include "misc/png.h"
#include "misc/debug.h"

#include "draw.h"

#include "view.h"

#include <stdatomic.h>
#include <threads.h>

#ifndef DIMS
#define DIMS 16
#endif

struct view_control_s {

	// change-management
	bool invalid;
	bool rgb_invalid;

	//data
	long dims[DIMS];
	long strs[DIMS];
	const complex float* data;

	// interpolation buffer
	complex float* buf;

	// rgb buffer
	int rgbh;
	int rgbw;
	int rgbstr;
	unsigned char* rgb;

	// geometry
	unsigned long geom_flags;
	const float (*geom)[3][3];
	const float (*geom_current)[3][3];

	// windowing
	int lastx;
	int lasty;

	//misc
	bool status_bar;
	double max;

	mtx_t mx;

	bool transpose;
	double aniso;
};

static void view_window_nosync(struct view_s* v, enum mode_t mode, double winlow, double winhigh);
static void view_geom2(struct view_s* v);

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

bool view_acquire(struct view_s* v, bool wait)
{
	if (wait)
		mtx_lock(&v->control->mx);
	else if (0 != mtx_trylock(&v->control->mx))
			return false;

	return true;
}

void view_release(struct view_s* v)
{
	mtx_unlock(&v->control->mx);
}


void update_geom(struct view_s* v)
{
	if (NULL == v->control->geom)
		return;

	long dims[DIMS];
	md_select_dims(DIMS, v->control->geom_flags, dims, v->control->dims);

	long strs[DIMS];
	md_calc_strides(DIMS, strs, dims, 1);

	v->control->geom_current = &v->control->geom[md_calc_offset(DIMS, strs, v->settings.pos)];

	printf("%f %f %f %f %f %f %f %f %f\n",
		(*v->control->geom_current)[0][0], (*v->control->geom_current)[0][1], (*v->control->geom_current)[0][2],
		(*v->control->geom_current)[1][0], (*v->control->geom_current)[1][1], (*v->control->geom_current)[1][2],
		(*v->control->geom_current)[2][0], (*v->control->geom_current)[2][1], (*v->control->geom_current)[2][2]);
}

void view_sync(struct view_s* v)
{
	for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next) {

		if (v->sync && v2->sync) {

			view_acquire(v2, true);

			v2->settings.pos[v->settings.xdim] = v->settings.pos[v->settings.xdim];
			v2->settings.pos[v->settings.ydim] = v->settings.pos[v->settings.ydim];

			// if we have changed anything outside of the current x/y dims of v2, we need to reinterpolate
			// this is common when changing slices in 3D datasets
			if ((v2->settings.xdim != v->settings.xdim) || (v2->settings.ydim != v->settings.ydim))
				v2->control->invalid = true;

			view_window_nosync(v2, v->settings.mode, v->settings.winlow, v->settings.winhigh);
			ui_set_params(v2, v2->ui_params, v2->settings);

			view_release(v2);
		}
	}
}

void view_refresh(struct view_s* v)
{
	if (v->settings.absolute_windowing) {

		long idims[DIMS];
		md_select_dims(DIMS, MD_BIT(v->settings.xdim) | MD_BIT(v->settings.ydim), idims, v->control->dims);

		complex float* tmp = md_alloc(DIMS, idims, sizeof(complex float));

		long pos[DIMS];
		md_copy_dims(DIMS, pos, v->settings.pos);
		pos[v->settings.xdim] = 0;
		pos[v->settings.ydim] = 0;

		md_slice(DIMS, ~(MD_BIT(v->settings.xdim) | MD_BIT(v->settings.ydim)), pos, v->control->dims, tmp, v->control->data, sizeof(complex float));

		long size = md_calc_size(DIMS, idims);

		double max = 0.;
		for (long j = 0; j < size; j++)
			if (max < cabsf(tmp[j]))
				max = cabsf(tmp[j]);

		max = MIN(1.e10, max);

		md_free(tmp);

		if (0 == v->control->max) {

			v->settings.winhigh = max;
			v->control->max = max;
		}

		if (v->control->max < max)
			v->control->max = max;
	} else {

		long size = md_calc_size(DIMS, v->control->dims);
		double max = 0.;

		for (long j = 0; j < size; j++)
			if (max < cabsf(v->control->data[j]))
				max = cabsf(v->control->data[j]);

		max = MIN(1.e10, max);

		if (0. == max)
			max = 1.;

		v->control->max = max;
	}

	v->ui_params.windowing_max = v->control->max;

	ui_set_params(v, v->ui_params, v->settings);
	ui_trigger_redraw(v);
}


void view_add_geometry(struct view_s* v, unsigned long flags, const float (*geom)[3][3])
{
	v->control->geom_flags = flags;
	v->control->geom = geom;
	v->control->geom_current = NULL;

	update_geom(v);
}


void view_geom(struct view_s* v, const bool* selected, const long* pos, double zoom, double aniso, _Bool transpose, enum flip_t flip, enum interp_t interp)
{
	for (int j = 0; j < DIMS; j++) {

		v->ui_params.selected[j] = selected[j];
		v->settings.pos[j] = pos[j];
	}

	v->ui_params.zoom = zoom;
	v->control->aniso = aniso;
	v->control->transpose = transpose;

	v->settings.flip = flip;
	v->settings.interpolation = interp;

	view_geom2(v);
}

static void view_geom2(struct view_s* v)
{
	for (int j = 0; j < DIMS; j++) {

		if (!v->ui_params.selected[j])
			continue;

		if (1 == v->control->dims[j]) {

			v->ui_params.selected[j] = false;

		} else if ((j != v->settings.xdim) && (j != v->settings.ydim)) {

			for (int i = 0; i < DIMS; i++) {

				if (v->settings.xdim == (DIMS + j - i) % DIMS) {

					v->ui_params.selected[v->settings.xdim] = false;
					v->settings.xdim = j;
					break;
				}

				if (v->settings.ydim == (DIMS + j - i) % DIMS) {

					v->ui_params.selected[v->settings.ydim] = false;
					v->settings.ydim = j;
					break;
				}

			}
		}
	}

	v->ui_params.selected[v->settings.xdim] = true;
	v->ui_params.selected[v->settings.ydim] = true;

	v->settings.xzoom = v->ui_params.zoom * v->control->aniso;
	v->settings.yzoom = v->ui_params.zoom;

	if (v->control->transpose) {

		if (v->settings.xdim < v->settings.ydim) {

			int swp = v->settings.xdim;
			v->settings.xdim = v->settings.ydim;
			v->settings.ydim = swp;
		}

	} else {

		if (v->settings.xdim > v->settings.ydim) {

			int swp = v->settings.xdim;
			v->settings.xdim = v->settings.ydim;
			v->settings.ydim = swp;
		}
	}

	v->control->lastx = -1;
	v->control->lasty = -1;

	v->control->invalid = true;

	update_geom(v);

	ui_set_params(v, v->ui_params, v->settings);
	ui_trigger_redraw(v);
}

static void view_window_nosync(struct view_s* v, enum mode_t mode, double winlow, double winhigh)
{
	v->settings.mode = mode;
	v->settings.winlow = winlow;
	v->settings.winhigh = winhigh;
	v->control->rgb_invalid = true;
	ui_trigger_redraw(v);
}


void view_window(struct view_s* v, enum mode_t mode, double winlow, double winhigh)
{
	view_window_nosync(v, mode, winlow, winhigh);
	view_sync(v);
}

static void update_buf_view(struct view_s* v)
{
	update_buf(v->settings.xdim, v->settings.ydim, DIMS, v->control->dims, v->control->strs, v->settings.pos,
		v->settings.flip, v->settings.interpolation, v->settings.xzoom, v->settings.yzoom, v->settings.plot,
		v->control->rgbw, v->control->rgbh, v->control->data, v->control->buf);
}


char *construct_filename_view2(struct view_s* v)
{
	long loopdims[DIMS];
	md_select_dims(DIMS, ~(MD_BIT(v->settings.xdim) | MD_BIT(v->settings.ydim)), loopdims, v->control->dims);

	return construct_filename_view(DIMS, loopdims, v->settings.pos, v->name, "png");
}

bool view_save_png(struct view_s* v, const char *filename)
{
	return gtk_ui_save_png(v, filename);
}


bool view_save_pngmovie(struct view_s* v, const char *folder)
{
	int frame_dim = 10;

	for (int f = 0; f < v->control->dims[frame_dim]; f++) {

		v->settings.pos[frame_dim] = f;
		update_buf_view(v);

		draw(v->control->rgbw, v->control->rgbh, v->control->rgbstr, (unsigned char(*)[v->control->rgbw][v->control->rgbstr / 4][4])v->control->rgb,
			v->settings.mode, v->settings.colortable, 1. / v->control->max, v->settings.winlow, v->settings.winhigh, v->settings.phrot,
			v->control->rgbw, v->control->buf);

		char output_name[256];
		int len = snprintf(output_name, 256, "%s/mov-%04d.png", folder, f);

		if (len + 1 >= sizeof(output_name)) {

			ui_set_msg(v, "Error: writing image file.\n");
			goto fail;
		}

		if (gtk_ui_save_png(v, output_name)) {

			ui_set_msg(v, "Error: writing image file.\n");
			goto fail;
		}
	}

	return true;

fail:
	return false;
}


struct xy_s { float x; float y; };

static struct xy_s pos2screen(const struct view_s* v, const float (*pos)[DIMS])
{
	float x = (*pos)[v->settings.xdim];
	float y = (*pos)[v->settings.ydim];

	if ((XY == v->settings.flip) || (XO == v->settings.flip))
		x = v->control->dims[v->settings.xdim] - 1 - x;

	if ((XY == v->settings.flip) || (OY == v->settings.flip))
		y = v->control->dims[v->settings.ydim] - 1 - y;

	// shift to the center of pixels
	x += 0.5;
	y += 0.5;

	x *= v->settings.xzoom;
	y *= v->settings.yzoom;

	if (v->settings.plot)
		y = v->control->rgbh / 2;

	return (struct xy_s){ x, y };
}

static void screen2pos(const struct view_s* v, float (*pos)[DIMS], struct xy_s xy)
{
	for (int i = 0; i < DIMS; i++)
		(*pos)[i] = v->settings.pos[i];

	float x = xy.x / v->settings.xzoom - 0.5;
	float y = xy.y / v->settings.yzoom - 0.5;

	if ((XY == v->settings.flip) || (XO == v->settings.flip))
		x = v->control->dims[v->settings.xdim] - 1 - x;

	if ((XY == v->settings.flip) || (OY == v->settings.flip))
		y = v->control->dims[v->settings.ydim] - 1 - y;

	(*pos)[v->settings.xdim] = roundf(x);

	if (!v->settings.plot)
		(*pos)[v->settings.ydim] = roundf(y);
}



static void clear_status_bar(struct view_s* v)
{
	char buf = '\0';
	ui_set_msg(v, &buf);
}


static void update_status_bar(struct view_s* v)
{
	int x2 = v->settings.pos[v->settings.xdim];
	int y2 = v->settings.pos[v->settings.ydim];

	float posf[DIMS];
	for (int i = 0; i < DIMS; i++)
		posf[i] = v->settings.pos[i];


	complex float val = sample(DIMS, posf, v->control->dims, v->control->strs, v->settings.interpolation, v->control->data);

	// FIXME: make sure this matches exactly the pixel
	char buf[100];
	snprintf(buf, 100, "Pos: %03d %03d Magn: %.3e Val: %+.3e%+.3ei Arg: %+.2f", x2, y2,
			cabsf(val), crealf(val), cimagf(val), cargf(val));

	ui_set_msg(v, buf);
}


void view_draw(struct view_s* v)
{
	v->control->rgbw = v->control->dims[v->settings.xdim] * v->settings.xzoom;
	v->control->rgbh = v->control->dims[v->settings.ydim] * v->settings.yzoom;
	v->control->rgbstr = 4 * v->control->rgbw;

	if (v->control->invalid) {

		v->control->buf = realloc(v->control->buf, v->control->rgbh * v->control->rgbw * sizeof(complex float));

		update_buf_view(v);

		v->control->invalid = false;
		v->control->rgb_invalid = true;
	}

	if (v->control->rgb_invalid) {

		ui_rgbbuffer_disconnect(v);
		v->control->rgb = realloc(v->control->rgb, v->control->rgbh * v->control->rgbstr);
		ui_rgbbuffer_connect(v, v->control->rgbw, v->control->rgbh, v->control->rgbstr, v->control->rgb);

		(v->settings.plot ? draw_plot : draw)(v->control->rgbw, v->control->rgbh, v->control->rgbstr,
			(unsigned char(*)[v->control->rgbw][v->control->rgbstr / 4][4])v->control->rgb,
			v->settings.mode, v->settings.colortable, v->settings.absolute_windowing ? 1. : 1. / v->control->max, v->settings.winlow, v->settings.winhigh, v->settings.phrot,
			v->control->rgbw, v->control->buf);

		v->control->rgb_invalid = false;
	}


	// add_text(v->ui->source, 3, 3, 10, v->name);

	if (v->settings.cross_hair) {

		float posi[DIMS];
		for (int i = 0; i < DIMS; i++)
			posi[i] = v->settings.pos[i];

		struct xy_s xy = pos2screen(v, &posi);

		draw_line(v->control->rgbw, v->control->rgbh, v->control->rgbstr, (unsigned char (*)[v->control->rgbw][v->control->rgbstr / 4][4])v->control->rgb,
				0, (int)xy.y, v->control->rgbw - 1, (int)xy.y, (v->settings.xdim > v->settings.ydim) ? &color_red : &color_blue);

		draw_line(v->control->rgbw, v->control->rgbh, v->control->rgbstr, (unsigned char (*)[v->control->rgbw][v->control->rgbstr / 4][4])v->control->rgb,
				(int)xy.x, 0, (int)xy.x, v->control->rgbh - 1, (v->settings.xdim < v->settings.ydim) ? &color_red : &color_blue);

//		float coords[4][2] = { { 0, 0 }, { 100, 0 }, { 0, 100 }, { 100, 100 } };
//		draw_grid(v->control->rgbw, v->control->rgbh, v->control->rgbstr, (unsigned char (*)[v->control->rgbw][v->control->rgbstr / 4][4])v->control->rgb, &coords, 4, &color_white);
	}

	if (v->control->status_bar)
		update_status_bar(v);
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
	v->control = xmalloc(sizeof(struct view_control_s));
	v->ui_params.selected = xmalloc(sizeof(bool[DIMS]));
	v->settings.pos = xmalloc(DIMS * sizeof(long));

	v->next = v->prev = v;
	v->sync = true;
	v->name = name;
	v->ui = NULL;

	v->settings.mode = MAGN;
	v->settings.interpolation = NLINEAR;
	v->settings.colortable = NONE;
	v->settings.flip = OO;
	v->settings.cross_hair = false;
	v->settings.plot = false;
	v->settings.xzoom = 2.;
	v->settings.yzoom = 2.;
	v->settings.xdim = sq_dims[0];
	v->settings.ydim = sq_dims[1];

	v->settings.winlow = 0.;
	v->settings.winhigh = 1.;
	v->settings.phrot = 0.;

	for (int i = 0; i < DIMS; i++) {

		v->settings.pos[i] = (NULL == pos) ? 0 : pos[i];
		v->ui_params.selected[i] = false;
	}

	v->ui_params.selected[v->settings.xdim] = true;
	v->ui_params.selected[v->settings.ydim] = true;
	v->ui_params.zoom = 2;

	md_copy_dims(DIMS, v->control->dims, dims);
	md_calc_strides(DIMS, v->control->strs, dims, sizeof(complex float));

	v->control->data = data;
	v->control->rgb = NULL;
	v->control->buf = NULL;
	v->control->status_bar = false;
	v->control->max = 0.;

	v->control->geom_flags = 0ul;
	v->control->geom = NULL;
	v->control->geom_current = NULL;

	v->control->lastx = -1;
	v->control->lasty = -1;

	v->control->aniso = 1;
	v->control->transpose = true;

	v->control->invalid = true;

	mtx_init(&v->control->mx, mtx_plain);

	return v;
}


static void delete_view(struct view_s* v)
{
	v->next->prev = v->prev;
	v->prev->next = v->next;

	free(v->control->buf);
	free(v->control->rgb);

	free(v->ui_params.selected);

	mtx_destroy(&v->control->mx);

#if 0
	free(v->settings.pos);
	//free(v);
#endif
}

static void view_set_windowing(struct view_s* v)
{
	double max = v->settings.absolute_windowing ? v->control->max : 1.;

	max = MIN(1.e10, max);

	v->ui_params.windowing_max = max;
	v->ui_params.windowing_inc = exp(log(10) * round(log(max) / log(10.))) * 0.001;
	v->ui_params.windowing_digits =  MAX(3, 3 - (int)round(log(max) / log(10.)));
}

void view_toggle_absolute_windowing(struct view_s* v, bool state)
{
	if (state && v->settings.absolute_windowing)
		return;

	if (v->settings.absolute_windowing) {

		long size = md_calc_size(DIMS, v->control->dims);
		double max = 0.;

		for (long j = 0; j < size; j++)
			if (max < cabsf(v->control->data[j]))
				max = cabsf(v->control->data[j]);

		max = MIN(1.e10, max);

		if (0. == max)
			max = 1.;

		v->control->max = max;
		v->settings.winhigh = MIN(v->settings.winhigh / v->control->max, 1);
		v->settings.winlow = MIN(v->settings.winlow / v->control->max, 1);

		v->settings.absolute_windowing = false;
	} else {

		v->settings.winhigh *= v->control->max;
		v->settings.winlow *= v->control->max;
		v->settings.absolute_windowing = true;
	}

	view_set_windowing(v);

	ui_set_params(v, v->ui_params, v->settings);
}

static void view_set_position(struct view_s* v, float pos[DIMS])
{
	v->settings.pos[v->settings.xdim] = pos[v->settings.xdim];
	v->settings.pos[v->settings.ydim] = pos[v->settings.ydim];

	ui_set_params(v, v->ui_params, v->settings);
	view_sync(v);
}

void view_click(struct view_s* v, int x, int y, int button)
{
	struct xy_s xy = { x, y };

	float pos[DIMS];
	screen2pos(v, &pos, xy);

	if (1 == button) {

		v->settings.cross_hair = false;
		v->control->status_bar = false;
		clear_status_bar(v);
	}

	if (2 == button) {

		v->settings.cross_hair = true;
		v->control->status_bar = true;
		view_set_position(v, pos);
	}

	v->control->rgb_invalid = true;
	ui_trigger_redraw(v);
}

void view_motion(struct view_s* v, int x, int y, double inc_low, double inc_high, int button)
{
	bool changed = false;

	if (0 == button) {

		v->control->lastx = -1;
		v->control->lasty = -1;
		return;
	}

	if (1 == button) {

		if (-1 != v->control->lastx) {

			double low = v->settings.winlow;
			double high = v->settings.winhigh;

			low -= (x - v->control->lastx) * inc_low * 5;
			high -= (y - v->control->lasty) * inc_high * 5;

			if (high > low) {

				v->settings.winlow = MAX(0, low);
				v->settings.winhigh = MIN(v->ui_params.windowing_max, high);

				changed = true;
			}
		}

		v->control->lastx = x;
		v->control->lasty = y;
	}

	if (changed) {

		ui_set_params(v, v->ui_params, v->settings);
		view_window(v, v->settings.mode, v->settings.winlow, v->settings.winhigh);
	}
}

static int nr_windows = 0;

void view_window_close(struct view_s* v)
{
	delete_view(v);

	if (0 == --nr_windows)
		ui_loop_quit();
}




struct view_s* window_new(const char* name, const long pos[DIMS], const long dims[DIMS], const complex float* x,
		bool absolute_windowing, enum color_t ctab)
{
	struct view_s* v = create_view(name, pos, dims, x);

	view_acquire(v, true);

	v->settings.absolute_windowing = absolute_windowing;
	v->settings.colortable = ctab;

	ui_window_new(v, DIMS, dims, v->settings);
	nr_windows++;

	ui_configure(v);

	view_refresh(v);
	view_geom2(v);
	view_set_windowing(v);
	view_window(v, v->settings.mode, v->settings.winlow, v->settings.winhigh);

	ui_set_params(v, v->ui_params, v->settings);

	view_release(v);

	return v;
}

void window_connect_sync(struct view_s* v, struct view_s* v2)
{
	// add to linked list for sync
	v2->next = v->next;
	v->next->prev = v2;
	v2->prev = v;
	v->next = v2;

	// set windowing
	view_window(v, v->settings.mode, v->settings.winlow, v->settings.winhigh);
}

struct view_s* view_window_clone(struct view_s* v)
{
	struct view_s* v2 = window_new(v->name, v->settings.pos, v->control->dims, v->control->data,
			v->settings.absolute_windowing, v->settings.colortable);

	window_connect_sync(v, v2);

	return v2;
}

void view_fit(struct view_s* v, int width, int height)
{
	double xz = (double)(width - 5) / (double)v->control->dims[v->settings.xdim];
	double yz = (double)(height - 5) / (double)v->control->dims[v->settings.ydim];

	if (yz > xz / v->control->aniso)
		yz = xz / v->control->aniso; // aniso

	v->ui_params.zoom = yz;

	view_geom2(v);
}

void view_toggle_plot(struct view_s* v)
{
	v->settings.plot = !v->settings.plot;
	v->control->invalid = true;

	ui_set_params(v, v->ui_params, v->settings);
	ui_trigger_redraw(v);
}

