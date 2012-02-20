// Per pixel translucent of gtk window
//  To build
//    g++ `pkg-config --cflags --libs gtk+-2.0` gtk_translucent.cc

#include <gtk/gtk.h>

static gboolean OnWindowExpose(GtkWidget *widget, GdkEventExpose *event,
                  gpointer data) {

  cairo_t *cr;
  cr = gdk_cairo_create(widget->window);
  // set (r, g, b, a) value of the cairo source
  //  eg, alpha = 0.8, half transparency
  cairo_set_source_rgba(cr, 0.5, 0.5, 0.0, 0.8);
  cairo_rectangle(cr, 0, 0, 600, 400);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_fill(cr);
  cairo_paint(cr);
  cairo_destroy(cr);

  return FALSE;
}

int main(int argc, char **argv)
{
  GtkWidget *window;
  GdkScreen *screen;
  GdkColormap *colormap;
  
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable(window, TRUE);
  gtk_window_resize(GTK_WINDOW(window), 600, 400);

  g_signal_connect(G_OBJECT(window), "expose-event",
      G_CALLBACK(OnWindowExpose), NULL);

  //set 32bit depth colormap (rgba)
  screen = gtk_widget_get_screen(window);
  colormap = gdk_screen_get_rgba_colormap(screen);
  gtk_widget_set_colormap(window, colormap);

  gtk_widget_show_all(window);

  gtk_main();
  return 0L;
}

