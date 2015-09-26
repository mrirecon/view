/*
 * gcc `pkg-config --cflags gtk+-3.0` main.c `pkg-config --libs gtk+-3.0`
 */

#include <gtk/gtk.h>



int main(int argc, char* argv[])
{
	gtk_init(&argc, &argv);

	GtkBuilder* builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "viewer.ui", NULL);
	gtk_builder_connect_signals(builder, NULL);

	GtkWidget* window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));

	g_object_unref(G_OBJECT(builder));
	gtk_widget_show(window);
	gtk_main();

	return 0;
}

