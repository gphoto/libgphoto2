#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#ifdef WITHOUT_GNOME
#  include <gtk/gtk.h>
#else
#  include <gnome.h>
#  include <glade/glade.h>
#endif
#include <gphoto2.h>
#include "interface.h"
#include "support.h"


int
main (int argc, char *argv[])
{
  GtkWidget *main_win;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  gtk_set_locale ();
#ifdef WITHOUT_GNOME
  /* Init gtk. */
  gtk_init (&argc, &argv);
#else
  /* Init gnome and glade. */
  gnome_init (PACKAGE, VERSION, argc, argv);
  glade_gnome_init ();
#endif
  /* Init gphoto backend. */
  gp_init (0);

  add_pixmap_directory (PACKAGE_DATA_DIR "/pixmaps");
  add_pixmap_directory (PACKAGE_SOURCE_DIR "/pixmaps");

  /* Create the gui. */
  main_win = create_main_win ();
  gtk_widget_show (main_win);
  gtk_main ();

  return (0);
}

