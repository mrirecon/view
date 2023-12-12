#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cairo.h>

#include "view.h"

#undef MIN
#undef MAX

#ifndef DIMS
#define DIMS 16
#endif

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


extern void ui_trigger_redraw(struct view_s* v);

extern void ui_rgbbuffer_disconnect(struct view_s* v);
extern void ui_rgbbuffer_connect(struct view_s* v, int rgbw, int rgbh, int rgbstr, unsigned char* buf);

extern void ui_set_selected_dims(struct view_s* v, const bool* selected);
extern void ui_set_limits(struct view_s* v);
extern void ui_set_values(struct view_s* v);

extern void ui_pull_geom(struct view_s* v);
extern void ui_pull_window(struct view_s* v);

extern void ui_set_msg(struct view_s* v, const char* msg);

extern bool gtk_ui_save_png(struct view_s* v, const char* filename);
