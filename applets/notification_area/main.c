/* System tray main() */

/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2011 Perberos
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include <string.h>

#include <cafe-panel-applet.h>
#include <cafe-panel-applet-gsettings.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <gio/gio.h>

#include "main.h"
#include "na-grid.h"

#ifdef PROVIDE_WATCHER_SERVICE
# include "libstatus-notifier-watcher/gf-status-notifier-watcher.h"
#endif

#define NOTIFICATION_AREA_ICON "cafe-panel-notification-area"

typedef struct
{
  GtkWidget *preferences_dialog;
  GtkWidget *min_icon_size_spin;
} NAPreferencesDialog;

struct _NaTrayAppletPrivate
{
  GtkWidget               *grid;

  NAPreferencesDialog     *dialog;
  GtkBuilder              *builder;

  GSettings               *settings;
  gint                     min_icon_size;

#ifdef PROVIDE_WATCHER_SERVICE
  GfStatusNotifierWatcher *sn_watcher;
#endif
};

G_DEFINE_TYPE_WITH_PRIVATE (NaTrayApplet, na_tray_applet, PANEL_TYPE_APPLET)

static void (*parent_class_realize) (GtkWidget *widget);
static void (*parent_class_style_updated) (GtkWidget *widget);
static void (*parent_class_change_background)(CafePanelApplet* panel_applet, CafePanelAppletBackgroundType type, GdkRGBA* color, cairo_pattern_t* pattern);
static void (*parent_class_change_orient)(CafePanelApplet       *panel_applet, CafePanelAppletOrient  orient);


#ifdef PROVIDE_WATCHER_SERVICE
/* Quite dirty way of providing the org.kde.StatusNotifierWatcher service
 * ourselves, in case the session doesn't already */

static GfStatusNotifierWatcher *sn_watcher_service = NULL;

static GfStatusNotifierWatcher *
sn_watcher_service_ref (void)
{
  GSettings *settings;
  settings = g_settings_new ("org.cafe.panel");

  if (g_settings_get_boolean (settings, "enable-sni-support") == TRUE)
    {
      if (sn_watcher_service != NULL)
        g_object_ref (sn_watcher_service);
      else
        {
          sn_watcher_service = gf_status_notifier_watcher_new ();
          g_object_add_weak_pointer ((GObject *) sn_watcher_service,
                                     (gpointer *) &sn_watcher_service);
        }
    }

  g_object_unref (settings);
  return sn_watcher_service;
}
#endif


static GtkOrientation
get_ctk_orientation_from_applet_orient (CafePanelAppletOrient orient)
{
  switch (orient)
    {
    case CAFE_PANEL_APPLET_ORIENT_LEFT:
    case CAFE_PANEL_APPLET_ORIENT_RIGHT:
      return CTK_ORIENTATION_VERTICAL;
    case CAFE_PANEL_APPLET_ORIENT_UP:
    case CAFE_PANEL_APPLET_ORIENT_DOWN:
    default:
      return CTK_ORIENTATION_HORIZONTAL;
    }

  g_assert_not_reached ();

  return CTK_ORIENTATION_HORIZONTAL;
}

static void
gsettings_changed_min_icon_size (GSettings    *settings,
                                 gchar        *key,
                                 NaTrayApplet *applet)
{
  applet->priv->min_icon_size = g_settings_get_int (settings, key);

  if (applet->priv->dialog)
    ctk_spin_button_set_value (CTK_SPIN_BUTTON (applet->priv->dialog->min_icon_size_spin),
                               applet->priv->min_icon_size);

  na_grid_set_min_icon_size (NA_GRID (applet->priv->grid), applet->priv->min_icon_size);
}

static void
setup_gsettings (NaTrayApplet *applet)
{
  applet->priv->settings = cafe_panel_applet_settings_new (CAFE_PANEL_APPLET (applet), NA_TRAY_SCHEMA);
  g_signal_connect (applet->priv->settings, "changed::" KEY_MIN_ICON_SIZE, G_CALLBACK (gsettings_changed_min_icon_size), applet);
}

static void
na_preferences_dialog_min_icon_size_changed (NaTrayApplet  *applet,
                                             GtkSpinButton *spin_button)
{
  applet->priv->min_icon_size = ctk_spin_button_get_value_as_int (spin_button);
  g_settings_set_int (applet->priv->settings, KEY_MIN_ICON_SIZE, applet->priv->min_icon_size);
}

static gboolean
na_preferences_dialog_hide_event (GtkWidget    *widget,
                                  GdkEvent     *event,
                                  NaTrayApplet *applet)
{
  ctk_widget_hide (applet->priv->dialog->preferences_dialog);
  return TRUE;
}

static void
na_preferences_dialog_response (NaTrayApplet *applet,
                                int           response,
                                GtkWidget    *preferences_dialog)
{
  switch (response)
    {
    case CTK_RESPONSE_CLOSE:
      ctk_widget_hide (preferences_dialog);
      break;
    default:
      break;
    }
}

static void
ensure_prefs_window_is_created (NaTrayApplet *applet)
{
  if (applet->priv->dialog)
    return;

  applet->priv->dialog = g_new0 (NAPreferencesDialog, 1);

  applet->priv->dialog->preferences_dialog = CTK_WIDGET (ctk_builder_get_object (applet->priv->builder, "notification_area_preferences_dialog"));

  ctk_window_set_icon_name (CTK_WINDOW (applet->priv->dialog->preferences_dialog), NOTIFICATION_AREA_ICON);

  applet->priv->dialog->min_icon_size_spin = CTK_WIDGET (ctk_builder_get_object (applet->priv->builder, "min_icon_size_spin"));
  g_return_if_fail (applet->priv->dialog->min_icon_size_spin != NULL);

  ctk_spin_button_set_range (CTK_SPIN_BUTTON (applet->priv->dialog->min_icon_size_spin), 7, 130);
  ctk_spin_button_set_value (CTK_SPIN_BUTTON (applet->priv->dialog->min_icon_size_spin), applet->priv->min_icon_size);

  g_signal_connect_swapped (applet->priv->dialog->min_icon_size_spin, "value_changed",
                            G_CALLBACK (na_preferences_dialog_min_icon_size_changed),
                            applet);

  g_signal_connect_swapped (applet->priv->dialog->preferences_dialog, "response",
                            G_CALLBACK (na_preferences_dialog_response), applet);

  g_signal_connect (G_OBJECT (applet->priv->dialog->preferences_dialog), "delete_event",
                    G_CALLBACK (na_preferences_dialog_hide_event), applet);
}

static void
properties_dialog (GtkAction    *action,
                   NaTrayApplet *applet)
{
  ensure_prefs_window_is_created (applet);

  ctk_window_set_screen (CTK_WINDOW (applet->priv->dialog->preferences_dialog),
                         ctk_widget_get_screen (CTK_WIDGET (applet)));
  ctk_window_present (CTK_WINDOW (applet->priv->dialog->preferences_dialog));
}

static void help_cb(GtkAction* action, NaTrayApplet* applet)
{
	GError* error = NULL;
	char* uri;
	#define NA_HELP_DOC "cafe-user-guide"

	uri = g_strdup_printf("help:%s/%s", NA_HELP_DOC, "panels-notification-area");
	ctk_show_uri_on_window (NULL, uri, ctk_get_current_event_time (), &error);
	g_free(uri);

	if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	{
		g_error_free(error);
	}
	else if(error)
	{
		GtkWidget* dialog;
		char* primary;

		primary = g_markup_printf_escaped (_("Could not display help document '%s'"), NA_HELP_DOC);
		dialog = ctk_message_dialog_new (NULL, CTK_DIALOG_DESTROY_WITH_PARENT, CTK_MESSAGE_ERROR, CTK_BUTTONS_CLOSE, "%s", primary);

		ctk_message_dialog_format_secondary_text(CTK_MESSAGE_DIALOG(dialog), "%s", error->message);

		g_error_free(error);
		g_free(primary);

		g_signal_connect(dialog, "response", G_CALLBACK (ctk_widget_destroy), NULL);

		ctk_window_set_icon_name (CTK_WINDOW (dialog), NOTIFICATION_AREA_ICON);
		ctk_window_set_screen (CTK_WINDOW (dialog), ctk_widget_get_screen (CTK_WIDGET (applet)));
		/* we have no parent window */
		ctk_window_set_skip_taskbar_hint (CTK_WINDOW (dialog), FALSE);
		ctk_window_set_title (CTK_WINDOW (dialog), _("Error displaying help document"));

		ctk_widget_show (dialog);
	}
}

static void about_cb(GtkAction* action, NaTrayApplet* applet)
{
	const gchar* authors[] = {
		"Havoc Pennington <hp@redhat.com>",
		"Anders Carlsson <andersca@gnu.org>",
		"Vincent Untz <vuntz@gnome.org>",
		"Alberts Muktupāvels",
		"Colomban Wendling <cwendling@hypra.fr>",
		"Fabien Broquard <braikar@gmail.com>",
		NULL
	};

	const char* documenters[] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	ctk_show_about_dialog(NULL,
		"program-name", _("Notification Area"),
		"title", _("About Notification Area"),
		"authors", authors,
		//"comments", _(comments),
		"copyright", _("Copyright \xc2\xa9 2002 Red Hat, Inc.\n"
		               "Copyright \xc2\xa9 2003-2006 Vincent Untz\n"
		               "Copyright \xc2\xa9 2011 Perberos\n"
		               "Copyright \xc2\xa9 2012-2020 CAFE developers"),
		"documenters", documenters,
		"logo-icon-name", NOTIFICATION_AREA_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		NULL);
}

static const GtkActionEntry menu_actions [] = {
	{ "SystemTrayPreferences", "document-properties", N_("_Preferences"),
	  NULL, NULL,
	  G_CALLBACK (properties_dialog) },
	{ "SystemTrayHelp", "help-browser", N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (help_cb) },
	{ "SystemTrayAbout", "help-about", N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (about_cb) }
};


static void
na_tray_applet_realize (GtkWidget *widget)
{
  NaTrayApplet      *applet = NA_TRAY_APPLET (widget);

  if (parent_class_realize)
    parent_class_realize (widget);

  GtkActionGroup* action_group;
  action_group = ctk_action_group_new("NA Applet Menu Actions");
  ctk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
  ctk_action_group_add_actions(action_group, menu_actions, G_N_ELEMENTS(menu_actions), applet);
  cafe_panel_applet_setup_menu_from_resource (CAFE_PANEL_APPLET (applet),
                                              NA_RESOURCE_PATH "notification-area-menu.xml",
                                              action_group);
  g_object_unref(action_group);

  setup_gsettings (applet);

  // load min icon size
  gsettings_changed_min_icon_size (applet->priv->settings, KEY_MIN_ICON_SIZE, applet);

  applet->priv->builder = ctk_builder_new ();
  ctk_builder_set_translation_domain (applet->priv->builder, GETTEXT_PACKAGE);
  ctk_builder_add_from_resource (applet->priv->builder, NA_RESOURCE_PATH "notification-area-preferences-dialog.ui", NULL);
}

static void
na_tray_applet_dispose (GObject *object)
{
  g_clear_object (&NA_TRAY_APPLET (object)->priv->settings);
#ifdef PROVIDE_WATCHER_SERVICE
  g_clear_object (&NA_TRAY_APPLET (object)->priv->sn_watcher);
#endif

  g_clear_object (&NA_TRAY_APPLET (object)->priv->builder);

  G_OBJECT_CLASS (na_tray_applet_parent_class)->dispose (object);
}

static void
na_tray_applet_style_updated (GtkWidget *widget)
{
  NaTrayApplet    *applet = NA_TRAY_APPLET (widget);
  gint             padding;
  gint             icon_size;

  if (parent_class_style_updated)
    parent_class_style_updated (widget);

  if (!applet->priv->grid)
    return;

  ctk_widget_style_get (widget,
                        "icon-padding", &padding,
                        "icon-size", &icon_size,
                        NULL);
  g_object_set (applet->priv->grid,
                "icon-padding", padding,
                "icon-size", icon_size,
                NULL);
}

static void
na_tray_applet_change_background(CafePanelApplet* panel_applet, CafePanelAppletBackgroundType type, GdkRGBA* color, cairo_pattern_t* pattern)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (panel_applet);

  if (parent_class_change_background) {
    parent_class_change_background (panel_applet, type, color, pattern);
  }

  if (applet->priv->grid)
    na_grid_force_redraw (NA_GRID (applet->priv->grid));
}

static void
na_tray_applet_change_orient (CafePanelApplet       *panel_applet,
                              CafePanelAppletOrient  orient)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (panel_applet);

  if (parent_class_change_orient)
    parent_class_change_orient (panel_applet, orient);

  if (!applet->priv->grid)
    return;

  ctk_orientable_set_orientation (CTK_ORIENTABLE (applet->priv->grid),
                                  get_ctk_orientation_from_applet_orient (orient));
}

static gboolean
na_tray_applet_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  /* Prevent the panel from poping up the applet's popup on the the items,
   * which may also popup a menu which then conflicts.
   * This doesn't prevent the menu from poping up on the applet handle. */
  if (event->button == 3)
    return TRUE;

  return CTK_WIDGET_CLASS (na_tray_applet_parent_class)->button_press_event (widget, event);
}

static gboolean
na_tray_applet_focus (GtkWidget        *widget,
                      GtkDirectionType  direction)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (widget);

  /* We let the grid handle the focus movement because we behave more like a
   * container than a single applet.  But if focus didn't move, we let the
   * applet do its thing. */
  if (ctk_widget_child_focus (applet->priv->grid, direction))
    return TRUE;

  return CTK_WIDGET_CLASS (na_tray_applet_parent_class)->focus (widget, direction);
}

static void
na_tray_applet_class_init (NaTrayAppletClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass   *widget_class = CTK_WIDGET_CLASS (class);
  CafePanelAppletClass *applet_class = CAFE_PANEL_APPLET_CLASS (class);

  object_class->dispose = na_tray_applet_dispose;

  parent_class_realize = widget_class->realize;
  widget_class->realize = na_tray_applet_realize;

  parent_class_style_updated = widget_class->style_updated;
  widget_class->style_updated = na_tray_applet_style_updated;
  parent_class_change_background = applet_class->change_background;
  applet_class->change_background = na_tray_applet_change_background;

  widget_class->button_press_event = na_tray_applet_button_press_event;
  widget_class->focus = na_tray_applet_focus;

  parent_class_change_orient = applet_class->change_orient;
  applet_class->change_orient = na_tray_applet_change_orient;

  ctk_widget_class_install_style_property (
          widget_class,
          g_param_spec_int ("icon-padding",
                            "Padding around icons",
                            "Padding that should be put around icons, in pixels",
                            0, G_MAXINT, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  ctk_widget_class_install_style_property (
          widget_class,
          g_param_spec_int ("icon-size",
                            "Icon size",
                            "If non-zero, hardcodes the size of the icons in pixels",
                            0, G_MAXINT, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  ctk_widget_class_set_css_name (widget_class, "na-tray-applet");
}

static void
na_tray_applet_init (NaTrayApplet *applet)
{
  CafePanelAppletOrient orient;
  AtkObject *atko;

  applet->priv = na_tray_applet_get_instance_private (applet);

#ifdef PROVIDE_WATCHER_SERVICE
  applet->priv->sn_watcher = sn_watcher_service_ref ();
#endif

  orient = cafe_panel_applet_get_orient (CAFE_PANEL_APPLET (applet));
  applet->priv->grid = na_grid_new (get_ctk_orientation_from_applet_orient (orient));

  ctk_container_add (CTK_CONTAINER (applet), CTK_WIDGET (applet->priv->grid));
  ctk_widget_show (CTK_WIDGET (applet->priv->grid));

  atko = ctk_widget_get_accessible (CTK_WIDGET (applet));
  atk_object_set_name (atko, _("Panel Notification Area"));

  cafe_panel_applet_set_flags (CAFE_PANEL_APPLET (applet),
                          CAFE_PANEL_APPLET_HAS_HANDLE|CAFE_PANEL_APPLET_EXPAND_MINOR);
}

static gboolean
applet_factory (CafePanelApplet *applet,
                const gchar *iid,
                gpointer     user_data)
{
  if (!(strcmp (iid, "NotificationArea") == 0 ||
        strcmp (iid, "SystemTrayApplet") == 0))
    return FALSE;

  if (!GDK_IS_X11_DISPLAY (ctk_widget_get_display (CTK_WIDGET (applet)))) {
    g_warning ("Notification area only works on X");
    return FALSE;
  }

#ifndef NOTIFICATION_AREA_INPROCESS
  ctk_window_set_default_icon_name (NOTIFICATION_AREA_ICON);
#endif

  ctk_widget_show_all (CTK_WIDGET (applet));

  return TRUE;
}

#ifdef NOTIFICATION_AREA_INPROCESS
	CAFE_PANEL_APPLET_IN_PROCESS_FACTORY ("NotificationAreaAppletFactory",
				 NA_TYPE_TRAY_APPLET,
				 "NotificationArea",
				 applet_factory,
				 NULL)
#else
	CAFE_PANEL_APPLET_OUT_PROCESS_FACTORY ("NotificationAreaAppletFactory",
				  NA_TYPE_TRAY_APPLET,
				  "NotificationArea",
				  applet_factory,
				  NULL)
#endif
