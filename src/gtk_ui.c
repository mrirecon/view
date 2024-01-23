#define _GNU_SOURCE

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cairo.h>
#undef MIN
#undef MAX

#include <libgen.h>
#include <string.h>

#include "gtk_ui.h"
#include "view.h"

#include "misc/misc.h"

#define STRINGIFY(x) # x
const char* viewer_gui =
#include "viewer.inc"
;


struct view_gtk_ui_s {

	// UI
	cairo_surface_t* source;

	// widgets
	GtkComboBox* gtk_mode;
	GtkComboBox* gtk_flip;
	GtkComboBox* gtk_interp;
	GtkWidget* gtk_drawingarea;
	GtkWidget* gtk_viewport;
	GtkAdjustment* gtk_winlow;
	GtkAdjustment* gtk_winhigh;
	GtkSpinButton* gtk_button_winlow;
	GtkSpinButton* gtk_button_winhigh;
	GtkToolItem* toolbar_scale1;
	GtkToolItem* toolbar_scale2;
	GtkToolItem* toolbar_button1;
	GtkToolItem* toolbar_button2;
	GtkAdjustment* gtk_zoom;
	GtkAdjustment* gtk_aniso;
	GtkEntry* gtk_entry;
	GtkToggleToolButton* gtk_transpose;
	GtkToggleToolButton* gtk_fit;
	GtkToggleToolButton* gtk_sync;
	GtkToggleToolButton* gtk_absolutewindowing;

	GtkAdjustment* gtk_posall[DIMS];
	GtkCheckButton* gtk_checkall[DIMS];

	GtkWidget *dialog; // Save dialog
	GtkFileChooser *chooser; // Save dialog
	GtkWindow *window;
};


extern gboolean fit_callback(GtkWidget* /*widget*/, gpointer data)
{
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

extern gboolean configure_callback(GtkWidget *widget, GdkEvent* /*event*/, gpointer data)
{
	return fit_callback(widget, data);
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

extern gboolean motion_notify_event(GtkWidget* /*widget*/, GdkEventMotion *event, gpointer data)
{
	struct view_s* v = data;

	double inc_low = gtk_adjustment_get_step_increment(v->ui->gtk_winlow);
	double inc_high = gtk_adjustment_get_step_increment(v->ui->gtk_winhigh);

	if (event->state & GDK_BUTTON1_MASK)
		view_windowing_move(v, event->x, event->y, inc_low, inc_high);
	else
		view_windowing_release(v);

	return FALSE;
}

extern gboolean show_hide(GtkWidget *widget, GtkCheckButton* button)
{
	gboolean flag = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(button));
	(flag ? gtk_widget_show : gtk_widget_hide)(widget);

	return FALSE;
}

extern gboolean window_close(GtkWidget* /*widget*/, GdkEvent* /*event*/, gpointer data)
{
	struct view_s* v = data;

	view_window_close(v);

	return FALSE;
}

extern gboolean window_clone(GtkWidget* /*widget*/, gpointer data)
{
	struct view_s* v = data;

	view_clone(v, v->settings.pos);

	return FALSE;
}

extern gboolean draw_callback(GtkWidget* /*widget*/, cairo_t *cr, gpointer data)
{
	struct view_s* v = data;
	view_redraw(v);

	cairo_set_source_surface(cr, v->ui->source, 0, 0);
	cairo_paint(cr);
	return FALSE;
}

extern gboolean toggle_sync(GtkToggleButton* /*button*/, gpointer data)
{
	struct view_s* v = data;
	v->sync = gtk_toggle_tool_button_get_active(v->ui->gtk_sync);

	return FALSE;
}

extern gboolean toggle_plot(GtkToggleButton* /*button*/, gpointer data)
{
	struct view_s* v = data;
	v->settings.plot = !v->settings.plot;

	gtk_widget_set_sensitive(GTK_WIDGET(v->ui->gtk_mode),
		v->settings.plot ? FALSE : TRUE);

	v->invalid = true;
	ui_trigger_redraw(v);

	return FALSE;
}

extern gboolean toogle_absolute_windowing(GtkToggleToolButton* button, gpointer data)
{
	struct view_s* v = data;

	if ((TRUE == gtk_toggle_tool_button_get_active(button)) == v->settings.absolute_windowing)
		return FALSE;

	view_toggle_absolute_windowing(v);

	return FALSE;
}

extern gboolean button_press_event(GtkWidget* /*widget*/, GdkEventButton *event, gpointer data)
{
	struct view_s* v = data;

	if (event->button == GDK_BUTTON_PRIMARY)
		view_click(v, event->x, event->y, 1);

	if (event->button == GDK_BUTTON_SECONDARY)
		view_click(v, event->x, event->y, 2);

	return FALSE;
}



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

void ui_pull_geom(struct view_s* v)
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
	gtk_adjustment_set_value(v->ui->gtk_winlow, v->settings.winlow);
	gtk_adjustment_set_value(v->ui->gtk_winhigh, v->settings.winhigh);
}

void ui_set_mode(struct view_s* v)
{
	gtk_combo_box_set_active(v->ui->gtk_mode, v->settings.mode);
}

void ui_set_msg(struct view_s* v, const char* msg)
{
	gtk_entry_set_text(v->ui->gtk_entry, msg);
}

void ui_window_new(struct view_s* v, int N, const long dims[N])
{
	v->ui = xmalloc(sizeof(struct view_gtk_ui_s));
	v->ui->source = NULL;

	GtkBuilder* builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, viewer_gui, -1, NULL);

	v->ui->gtk_drawingarea = GTK_WIDGET(gtk_builder_get_object(builder, "drawingarea1"));
	v->ui->gtk_viewport = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow1"));

	v->ui->gtk_winlow = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "winlow"));
	v->ui->gtk_winhigh = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "winhigh"));

	v->ui->gtk_entry = GTK_ENTRY(gtk_builder_get_object(builder, "entry"));

	v->ui->gtk_zoom = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "zoom"));
	v->ui->gtk_aniso = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "aniso"));

	v->ui->gtk_mode = GTK_COMBO_BOX(gtk_builder_get_object(builder, "mode"));
	gtk_combo_box_set_active(v->ui->gtk_mode, 0);

	v->ui->gtk_flip = GTK_COMBO_BOX(gtk_builder_get_object(builder, "flip"));
	gtk_combo_box_set_active(v->ui->gtk_flip, 0);

	v->ui->gtk_interp = GTK_COMBO_BOX(gtk_builder_get_object(builder, "interp"));
	gtk_combo_box_set_active(v->ui->gtk_interp, 0);

	v->ui->gtk_transpose = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "transpose"));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(v->ui->gtk_transpose), TRUE);
	v->ui->gtk_fit = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "fit"));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(v->ui->gtk_fit), TRUE);

	v->ui->gtk_sync = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "sync"));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(v->ui->gtk_sync), v->sync ? TRUE : FALSE);

	v->ui->gtk_button_winlow = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "scale1_button"));
	v->ui->gtk_button_winhigh = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "scale2_button"));

	v->ui->toolbar_scale1 = GTK_TOOL_ITEM(gtk_builder_get_object(builder, "toolbar_scale1"));
	v->ui->toolbar_scale2 = GTK_TOOL_ITEM(gtk_builder_get_object(builder, "toolbar_scale2"));
	v->ui->toolbar_button1 = GTK_TOOL_ITEM(gtk_builder_get_object(builder, "toolbar_button1"));
	v->ui->toolbar_button2 = GTK_TOOL_ITEM(gtk_builder_get_object(builder, "toolbar_button2"));

	v->ui->gtk_absolutewindowing = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "abswindow"));
	gtk_toggle_tool_button_set_active(v->ui->gtk_absolutewindowing , v->settings.absolute_windowing ? TRUE : FALSE);

	for (int j = 0; j < DIMS; j++) {

		char pname[10];
		snprintf(pname, 10, "pos%02d", j);
		v->ui->gtk_posall[j] = GTK_ADJUSTMENT(gtk_builder_get_object(builder, pname));
		gtk_adjustment_set_upper(v->ui->gtk_posall[j], dims[j] - 1);
		gtk_adjustment_set_value(v->ui->gtk_posall[j], v->settings.pos[j]);

		snprintf(pname, 10, "check%02d", j);
		v->ui->gtk_checkall[j] = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, pname));
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->ui->gtk_checkall[v->settings.xdim]), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v->ui->gtk_checkall[v->settings.ydim]), TRUE);

	gtk_builder_connect_signals(builder, v);

	GtkWindow* window = GTK_WINDOW(gtk_builder_get_object(builder, "window1"));
	g_object_unref(G_OBJECT(builder));
	v->ui->window = window;
	gtk_window_set_title(window, v->name);
	gtk_widget_show(GTK_WIDGET(window));
}

void ui_init(int* argc_p, char** argv_p[])
{
	gtk_disable_setlocale();
	gtk_init(argc_p, argv_p);

}

void ui_main()
{
	gtk_main();
}

void ui_loop_quit()
{
	gtk_main_quit();
}


bool gtk_ui_save_png(struct view_s* v, const char* filename)
{
	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(v->ui->source, filename))
		return true;
	else
		return false;
}

void ui_set_windowing(struct view_s* v, double max, double inc, int digits)
{
	gtk_adjustment_configure(v->ui->gtk_winhigh, v->settings.winhigh, 0, max, inc, gtk_adjustment_get_page_increment(v->ui->gtk_winhigh), gtk_adjustment_get_page_size(v->ui->gtk_winhigh));
	gtk_adjustment_configure(v->ui->gtk_winlow, v->settings.winlow, 0, max, inc, gtk_adjustment_get_page_increment(v->ui->gtk_winlow), gtk_adjustment_get_page_size(v->ui->gtk_winlow));
	gtk_spin_button_set_digits(v->ui->gtk_button_winhigh, digits);
	gtk_spin_button_set_digits(v->ui->gtk_button_winlow, digits);

	gtk_widget_set_visible(GTK_WIDGET(v->ui->toolbar_scale1), v->settings.absolute_windowing ? FALSE : TRUE);
	gtk_widget_set_visible(GTK_WIDGET(v->ui->toolbar_scale2), v->settings.absolute_windowing ? FALSE : TRUE);
	gtk_widget_set_visible(GTK_WIDGET(v->ui->toolbar_button1), v->settings.absolute_windowing ? TRUE : FALSE);
	gtk_widget_set_visible(GTK_WIDGET(v->ui->toolbar_button2), v->settings.absolute_windowing ? TRUE : FALSE);
}

void ui_set_position(struct view_s* v, unsigned int dim, unsigned int p)
{
	v->settings.pos[dim] = p;

	gtk_adjustment_set_value(v->ui->gtk_posall[dim], p);

	for (struct view_s* v2 = v->next; v2 != v; v2 = v2->next)
		if (v->sync && v2->sync)
			gtk_adjustment_set_value(v2->ui->gtk_posall[dim], p);
}
