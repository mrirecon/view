#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <cairo.h>

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
