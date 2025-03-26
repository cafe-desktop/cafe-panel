/* Symbols needed for libcafe-panel-applet-private-mini, which is used by the test
 * program */

#include <glib.h>

gboolean cafe_panel_applet_frame_dbus_load (const gchar *iid, gpointer frame_act);

gboolean cafe_panel_applet_frame_dbus_load (const    gchar *iid G_GNUC_UNUSED,
					    gpointer frame_act G_GNUC_UNUSED)
{
	return FALSE;
}
