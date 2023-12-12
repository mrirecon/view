#include "view.h"
#define _GNU_SOURCE

#include "gtk_ui.h"

#include "misc/misc.h"
#define UNUSED(x) (void)(x)
#include <libgen.h>
#include <string.h>

void ui_trigger_redraw(struct view_s* v)
{
	gtk_widget_queue_draw(v->ui->gtk_drawingarea);
}

void ui_rgbbuffer_disconnect(struct view_s* v)
{
	if (NULL != v->ui->source)
		cairo_surface_destroy(v->ui->source);
}

void ui_rgbbuffer_connect(struct view_s* v, int rgbw, int rgbh, int rgbstr, unsigned char *buf)
{
	gtk_widget_set_size_request(v->ui->gtk_drawingarea, rgbw, rgbh);
	v->ui->source = cairo_image_surface_create_for_data(buf, CAIRO_FORMAT_RGB24, rgbw, rgbh, rgbstr);
}

extern gboolean fit_callback(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
	struct view_s* v = data;

	gboolean flag = gtk_toggle_tool_button_get_active(v->ui->gtk_fit);

	if (!flag)
		return FALSE;

	double aniso = gtk_adjustment_get_value(v->ui->gtk_aniso);

	GtkAllocation alloc;
	gtk_widget_get_allocation(v->ui->gtk_viewport, &alloc);
	double xz = (double)(alloc.width - 5) / (double)view_get_dims(v)[v->settings.xdim];
	double yz = (double)(alloc.height - 5) / (double)view_get_dims(v)[v->settings.ydim];


	if (yz > xz / aniso)
		yz = xz / aniso; // aniso

	gtk_adjustment_set_value(v->ui->gtk_zoom, yz);

	return FALSE;
}


extern gboolean configure_callback(GtkWidget *widget, GdkEvent* event, gpointer data)
{
	UNUSED(event);
	return fit_callback(widget, data);
}

void ui_set_selected_dims(struct view_s* v, const bool* selected)
{
	// Avoid calling this function from itself.
	// Toggling the GTK_TOOGLE_BUTTONs below would normally lead to another call of this function.
	static bool in_callback = false;
	if (in_callback)
		return;

	in_callback = true;

	for (int i = 0; i < DIMS; i++) {

		if (selected[i])
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->ui->gtk_checkall[i]), TRUE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->ui->gtk_checkall[i]), FALSE);
	}

	in_callback = false;
}

extern gboolean geom_callback(GtkWidget* /*widget*/, gpointer data)
{
	struct view_s* v = data;

	struct view_ui_geom_params_s gp;

	gp.N = DIMS;
	gp.selected = malloc(sizeof(bool[DIMS]));

	for (int j = 0; j < DIMS; j++) {

		v->settings.pos[j] = gtk_adjustment_get_value(v->ui->gtk_posall[j]);
		gp.selected[j] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(v->ui->gtk_checkall[j]));
	}

	gp.zoom = gtk_adjustment_get_value(v->ui->gtk_zoom);
	gp.aniso = gtk_adjustment_get_value(v->ui->gtk_aniso);

	v->settings.flip = gtk_combo_box_get_active(v->ui->gtk_flip);
	v->settings.interpolation = gtk_combo_box_get_active(v->ui->gtk_interp);
	gp.transpose = gtk_toggle_tool_button_get_active(v->ui->gtk_transpose);

	view_set_geom(v, gp);

	free(gp.selected);

	return FALSE;
}

extern gboolean refresh_callback(GtkWidget* /*widget*/, gpointer data)
{
	view_refresh(data);
	return FALSE;
}

extern gboolean window_callback(GtkWidget* /*widget*/, gpointer data)
{
	struct view_s* v = data;

	v->settings.mode = gtk_combo_box_get_active(v->ui->gtk_mode);
	v->settings.winlow = gtk_adjustment_get_value(v->ui->gtk_winlow);
	v->settings.winhigh = gtk_adjustment_get_value(v->ui->gtk_winhigh);

	view_touch_rgb_settings(v);

	return FALSE;
}

extern gboolean save_callback(GtkWidget* /*widget*/, gpointer data)
{
	struct view_s* v = data;

	char* name = construct_filename_view2(v);

	v->ui->dialog = gtk_file_chooser_dialog_new("Save File",
                                      v->ui->window,
				      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

	v->ui->chooser = GTK_FILE_CHOOSER(v->ui->dialog);

	gtk_file_chooser_set_current_name(v->ui->chooser, basename(name));

	char* dname = strdup(v->name);

	// Outputfolder = Inputfolder
	gtk_file_chooser_set_current_folder(v->ui->chooser, dirname(dname));
	gtk_file_chooser_set_do_overwrite_confirmation(v->ui->chooser, TRUE);

	gint res = gtk_dialog_run(GTK_DIALOG(v->ui->dialog));

	if (GTK_RESPONSE_ACCEPT == res) {

		// export single image
		char *filename = gtk_file_chooser_get_filename(v->ui->chooser);

		if (view_save_png(v, filename))
			gtk_entry_set_text(v->ui->gtk_entry, "Error: writing image file.\n");

		gtk_entry_set_text(v->ui->gtk_entry, "Saved!");

		g_free(filename);
	}

	gtk_widget_destroy (v->ui->dialog);

	xfree(name);
	xfree(dname);

	return FALSE;
}

extern gboolean save_movie_callback(GtkWidget* /*widget*/, gpointer data)
{
	struct view_s* v = data;

	v->ui->dialog = gtk_file_chooser_dialog_new("Export movie to folder",
						v->ui->window,
						GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						"Cancel",
						GTK_RESPONSE_CANCEL,
						"Export",
						GTK_RESPONSE_ACCEPT,
						NULL);

	v->ui->chooser = GTK_FILE_CHOOSER(v->ui->dialog);


	char* dname = strdup(v->name);

	// Outputfolder = Inputfolder
	gtk_file_chooser_set_current_folder(v->ui->chooser, dirname(dname));

	gint res = gtk_dialog_run(GTK_DIALOG (v->ui->dialog));

	if (GTK_RESPONSE_ACCEPT == res) {

		char *chosen_dir = gtk_file_chooser_get_filename(v->ui->chooser);

		if (view_save_pngmovie(v, chosen_dir))
			gtk_entry_set_text(v->ui->gtk_entry, "Movie exported.");
		else
			gtk_entry_set_text(v->ui->gtk_entry, "Movie export FAILED.");

		g_free(chosen_dir);
	}

	gtk_widget_destroy (v->ui->dialog);

	xfree(dname);

	return FALSE;
}



extern void ui_pull_geom(struct view_s* v)
{
	geom_callback(NULL, v);
}

void ui_pull_window(struct view_s* v)
{
	window_callback(NULL, v);
}

void ui_set_limits(struct view_s* v)
{
	gtk_adjustment_set_upper(v->ui->gtk_winhigh, v->win_high_max);
	gtk_adjustment_set_upper(v->ui->gtk_winlow, v->win_low_max);
}

void ui_set_values(struct view_s* v)
{
	gtk_adjustment_set_value(v->ui->gtk_winhigh, v->settings.winhigh);
	gtk_adjustment_set_value(v->ui->gtk_winlow, v->settings.winlow);
	gtk_combo_box_set_active(v->ui->gtk_mode, v->settings.mode);
}

extern void ui_set_msg(struct view_s* v, const char* msg)
{
	gtk_entry_set_text(v->ui->gtk_entry, msg);
}



bool gtk_ui_save_png(struct view_s* v, const char* filename)
{
	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->ui->source, filename))
		return true;
	else
		return false;
}
