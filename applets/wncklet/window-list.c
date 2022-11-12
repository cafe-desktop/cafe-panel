/* -*- mode: C; c-file-style: "linux" -*- */
/*
 * libvnck based tasklist applet.
 * (C) 2001 Red Hat, Inc
 * (C) 2001 Alexander Larsson
 *
 * Authors: Alexander Larsson
 *
 */

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <string.h>

#include <cafe-panel-applet.h>
#include <cafe-panel-applet-gsettings.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libvnck/libvnck.h>
#include <gio/gio.h>
#ifdef HAVE_WINDOW_PREVIEWS
#include <cdk/cdkx.h>
#endif

#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-desktop-utils.h>

#include "vncklet.h"
#include "window-list.h"

#define WINDOW_LIST_ICON "cafe-panel-window-list"
#define WINDOW_LIST_SCHEMA "org.cafe.panel.applet.window-list"
#ifdef HAVE_WINDOW_PREVIEWS
#define WINDOW_LIST_PREVIEW_SCHEMA "org.cafe.panel.applet.window-list-previews"
#endif

typedef struct {
	CtkWidget* applet;
	CtkWidget* tasklist;
#ifdef HAVE_WINDOW_PREVIEWS
	CtkWidget* preview;

	gboolean show_window_thumbnails;
	gint thumbnail_size;
#endif
	gboolean include_all_workspaces;
	WnckTasklistGroupingType grouping;
	gboolean move_unminimized_windows;

	CtkOrientation orientation;
	int size;
#if !defined(WNCKLET_INPROCESS) && !CTK_CHECK_VERSION (3, 23, 0)
	gboolean needs_hints;
#endif

	CtkIconTheme* icon_theme;

	/* Properties: */
	CtkWidget* properties_dialog;
	CtkWidget* show_current_radio;
	CtkWidget* show_all_radio;
#ifdef HAVE_WINDOW_PREVIEWS
	CtkWidget* show_thumbnails_radio;
	CtkWidget* hide_thumbnails_radio;
	CtkWidget* thumbnail_size_spin;
#endif
	CtkWidget* never_group_radio;
	CtkWidget* auto_group_radio;
	CtkWidget* always_group_radio;
	CtkWidget* minimized_windows_label;
	CtkWidget* move_minimized_radio;
	CtkWidget* change_workspace_radio;

	GSettings* settings;
#ifdef HAVE_WINDOW_PREVIEWS
	GSettings* preview_settings;
#endif
} TasklistData;

static void call_system_monitor(CtkAction* action, TasklistData* tasklist);
static void display_properties_dialog(CtkAction* action, TasklistData* tasklist);
static void display_help_dialog(CtkAction* action, TasklistData* tasklist);
static void display_about_dialog(CtkAction* action, TasklistData* tasklist);
static void destroy_tasklist(CtkWidget* widget, TasklistData* tasklist);

static void tasklist_update(TasklistData* tasklist)
{
	if (tasklist->orientation == CTK_ORIENTATION_HORIZONTAL)
	{
		ctk_widget_set_size_request(CTK_WIDGET(tasklist->tasklist), -1, tasklist->size);
	}
	else
	{
		ctk_widget_set_size_request(CTK_WIDGET(tasklist->tasklist), tasklist->size, -1);
	}

	vnck_tasklist_set_grouping(WNCK_TASKLIST(tasklist->tasklist), tasklist->grouping);
	vnck_tasklist_set_include_all_workspaces(WNCK_TASKLIST(tasklist->tasklist), tasklist->include_all_workspaces);
	vnck_tasklist_set_switch_workspace_on_unminimize(WNCK_TASKLIST(tasklist->tasklist), tasklist->move_unminimized_windows);
}

static void response_cb(CtkWidget* widget, int id, TasklistData* tasklist)
{
	if (id == CTK_RESPONSE_HELP)
	{
		vncklet_display_help(widget, "cafe-user-guide", "windowlist-prefs", WINDOW_LIST_ICON);
	}
	else
	{
		ctk_widget_hide(widget);
	}
}

static void applet_realized(CafePanelApplet* applet, TasklistData* tasklist)
{
	tasklist->icon_theme = ctk_icon_theme_get_for_screen(ctk_widget_get_screen(tasklist->applet));
}

static void applet_change_orient(CafePanelApplet* applet, CafePanelAppletOrient orient, TasklistData* tasklist)
{
	CtkOrientation new_orient;

	switch (orient)
	{
		case CAFE_PANEL_APPLET_ORIENT_LEFT:
		case CAFE_PANEL_APPLET_ORIENT_RIGHT:
			new_orient = CTK_ORIENTATION_VERTICAL;
			break;
		case CAFE_PANEL_APPLET_ORIENT_UP:
		case CAFE_PANEL_APPLET_ORIENT_DOWN:
		default:
			new_orient = CTK_ORIENTATION_HORIZONTAL;
			break;
	}

	if (new_orient == tasklist->orientation)
		return;

	tasklist->orientation = new_orient;
	vnck_tasklist_set_orientation (WNCK_TASKLIST (tasklist->tasklist), new_orient);

	tasklist_update(tasklist);
}

static void applet_change_background(CafePanelApplet* applet, CafePanelAppletBackgroundType type, CdkColor* color, cairo_pattern_t* pattern, TasklistData* tasklist)
{
	switch (type)
	{
		case PANEL_NO_BACKGROUND:
		case PANEL_COLOR_BACKGROUND:
		case PANEL_PIXMAP_BACKGROUND:
			vnck_tasklist_set_button_relief(WNCK_TASKLIST(tasklist->tasklist), CTK_RELIEF_NONE);
			break;
	}
}

#ifdef HAVE_WINDOW_PREVIEWS
static GdkPixbuf *preview_window_thumbnail (WnckWindow *vnck_window, TasklistData *tasklist)
{
	CdkWindow *window;
	GdkPixbuf *screenshot;
	GdkPixbuf *thumbnail;
	guchar *pixels;
	double ratio;
	int width, height;
	int scale;

	window = cdk_x11_window_foreign_new_for_display (cdk_display_get_default (), vnck_window_get_xid (vnck_window));

	if (window == NULL)
		return NULL;

	scale = cdk_window_get_scale_factor (window);
	width = cdk_window_get_width (window) * scale;
	height = cdk_window_get_height (window) * scale;

	/* Generate window screenshot for preview */
	screenshot = gdk_pixbuf_get_from_window (window, 0, 0, width / scale, height / scale);
	g_object_unref (window);

	if (screenshot == NULL)
		return NULL;

	/* Determine whether the contents of the screenshot are empty */
	pixels = gdk_pixbuf_get_pixels (screenshot);
	if (!g_strcmp0 ((const char *)pixels, ""))
	{
		g_object_unref (screenshot);
		return NULL;
	}

	/* Scale to configured size while maintaining aspect ratio */
	if (width > height)
	{
		ratio = (double) height / (double) width;
		width = MIN(width, tasklist->thumbnail_size);
		height = width * ratio;
	}
	else
	{
		ratio = (double) width / (double) height;
		height = MIN(height, tasklist->thumbnail_size);
		width = height * ratio;
	}

	thumbnail = gdk_pixbuf_scale_simple (screenshot, width, height, GDK_INTERP_BILINEAR);
	g_object_unref (screenshot);

	return thumbnail;
}

#define PREVIEW_PADDING 5
static void preview_window_reposition (TasklistData *tasklist, GdkPixbuf *thumbnail)
{
	CdkMonitor *monitor;
	CdkRectangle monitor_geom;
	int x_pos, y_pos;
	int width, height;

	width = gdk_pixbuf_get_width (thumbnail);
	height = gdk_pixbuf_get_height (thumbnail);

	/* Resize window to fit thumbnail */
	ctk_window_resize (CTK_WINDOW (tasklist->preview), width, height);

	/* Set position at pointer, then re-adjust from there to just outside of the pointer */
	ctk_window_set_position (CTK_WINDOW (tasklist->preview), CTK_WIN_POS_MOUSE);
	ctk_window_get_position (CTK_WINDOW (tasklist->preview), &x_pos, &y_pos);

	/* Get geometry of monitor where tasklist is located to calculate correct position of preview */
	monitor = cdk_display_get_monitor_at_point (cdk_display_get_default (), x_pos, y_pos);
	cdk_monitor_get_geometry (monitor, &monitor_geom);

	/* Add padding to clear the panel */
	switch (cafe_panel_applet_get_orient (CAFE_PANEL_APPLET (tasklist->applet)))
	{
		case CAFE_PANEL_APPLET_ORIENT_LEFT:
			x_pos = monitor_geom.width + monitor_geom.x - (width + tasklist->size) - PREVIEW_PADDING;
			break;
		case CAFE_PANEL_APPLET_ORIENT_RIGHT:
			x_pos = tasklist->size + PREVIEW_PADDING;
			break;
		case CAFE_PANEL_APPLET_ORIENT_UP:
			y_pos = monitor_geom.height + monitor_geom.y - (height + tasklist->size) - PREVIEW_PADDING;
			break;
		case CAFE_PANEL_APPLET_ORIENT_DOWN:
		default:
			y_pos = tasklist->size + PREVIEW_PADDING;
			break;
	}

	ctk_window_move (CTK_WINDOW (tasklist->preview), x_pos, y_pos);
}

static gboolean preview_window_draw (CtkWidget *widget, cairo_t *cr, GdkPixbuf *thumbnail)
{
	CtkStyleContext *context;

	context = ctk_widget_get_style_context (widget);
	ctk_render_icon (context, cr, thumbnail, 0, 0);

	return FALSE;
}

static gboolean applet_enter_notify_event (WnckTasklist *tl, GList *vnck_windows, TasklistData *tasklist)
{
	GdkPixbuf *thumbnail;
	WnckWindow *vnck_window = NULL;
	int n_windows;

	if (tasklist->preview != NULL)
	{
		ctk_widget_destroy (tasklist->preview);
		tasklist->preview = NULL;
	}

	if (!tasklist->show_window_thumbnails || vnck_windows == NULL)
		return FALSE;

	n_windows = g_list_length (vnck_windows);
	/* TODO: Display a list of stacked thumbnails for grouped windows. */
	if (n_windows == 1)
	{
		GList* l = vnck_windows;
		if (l != NULL)
			vnck_window = (WnckWindow*)l->data;
	}

	if (vnck_window == NULL)
		return FALSE;

	/* Do not show preview if window is not visible nor in current workspace */
	if (!vnck_window_is_visible_on_workspace (vnck_window,
						  vnck_screen_get_active_workspace (vnck_screen_get_default ())))
		return FALSE;

	thumbnail = preview_window_thumbnail (vnck_window, tasklist);

	if (thumbnail == NULL)
		return FALSE;

	/* Create window to display preview */
	tasklist->preview = ctk_window_new (CTK_WINDOW_POPUP);

	ctk_widget_set_app_paintable (tasklist->preview, TRUE);
	ctk_window_set_resizable (CTK_WINDOW (tasklist->preview), TRUE);

	preview_window_reposition (tasklist, thumbnail);

	ctk_widget_show (tasklist->preview);

	g_signal_connect_data (G_OBJECT (tasklist->preview), "draw", G_CALLBACK (preview_window_draw), thumbnail, (GClosureNotify) g_object_unref, 0);

	return FALSE;
}

static gboolean applet_leave_notify_event (WnckTasklist *tl, GList *vnck_windows, TasklistData *tasklist)
{
	if (tasklist->preview != NULL)
	{
		ctk_widget_destroy (tasklist->preview);
		tasklist->preview = NULL;
	}

	return FALSE;
}
#endif

static void applet_change_pixel_size(CafePanelApplet* applet, gint size, TasklistData* tasklist)
{
	if (tasklist->size == size)
		return;

	tasklist->size = size;

	tasklist_update(tasklist);
}

/* TODO: this is sad, should be used a function to retrieve  applications from
 *  .desktop or some like that. */
static const char* system_monitors[] = {
	"cafe-system-monitor",
	"gnome-system-monitor",
};

static const CtkActionEntry tasklist_menu_actions[] = {
	{
		"TasklistSystemMonitor",
		"utilities-system-monitor",
		N_("_System Monitor"),
		NULL,
		NULL,
		G_CALLBACK(call_system_monitor)
	},
	{
		"TasklistPreferences",
		"document-properties",
		N_("_Preferences"),
		NULL,
		NULL,
		G_CALLBACK(display_properties_dialog)
	},
	{
		"TasklistHelp",
		"help-browser",
		N_("_Help"),
		NULL,
		NULL,
		G_CALLBACK(display_help_dialog)
	},
	{
		"TasklistAbout",
		"help-about",
		N_("_About"),
		NULL,
		NULL,
		G_CALLBACK(display_about_dialog)
	}
};

static void tasklist_properties_update_content_radio(TasklistData* tasklist)
{
	CtkWidget* button;

	if (tasklist->show_current_radio == NULL)
		return;

	if (tasklist->include_all_workspaces)
	{
		button = tasklist->show_all_radio;
	}
	else
	{
		button = tasklist->show_current_radio;
	}

	if (!ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(button)))
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(button), TRUE);

	ctk_widget_set_sensitive(tasklist->minimized_windows_label, tasklist->include_all_workspaces);
	ctk_widget_set_sensitive(tasklist->move_minimized_radio, tasklist->include_all_workspaces);
	ctk_widget_set_sensitive(tasklist->change_workspace_radio, tasklist->include_all_workspaces);
}

static void display_all_workspaces_changed(GSettings* settings, gchar* key, TasklistData* tasklist)
{
	gboolean value;

	value = g_settings_get_boolean(settings, key);

	tasklist->include_all_workspaces = (value != 0);
	tasklist_update(tasklist);

	tasklist_properties_update_content_radio(tasklist);
}

#ifdef HAVE_WINDOW_PREVIEWS
static void tasklist_update_thumbnails_radio(TasklistData* tasklist)
{
	CtkWidget* button;

	if (tasklist->show_thumbnails_radio == NULL || tasklist->hide_thumbnails_radio == NULL)
		return;

	if (tasklist->show_window_thumbnails)
	{
		button = tasklist->show_thumbnails_radio;
	}
	else
	{
		button = tasklist->hide_thumbnails_radio;
	}

	if (!ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(button)))
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(button), TRUE);
}

static void window_thumbnails_changed(GSettings *settings, gchar* key, TasklistData* tasklist)
{
	gboolean value;

	value = g_settings_get_boolean(settings, key);

	tasklist->show_window_thumbnails = (value != 0);

	tasklist_update_thumbnails_radio(tasklist);
}

static void tasklist_update_thumbnail_size_spin(TasklistData* tasklist)
{
	CtkWidget* button;

	if (!tasklist->thumbnail_size)
		return;

	button = tasklist->thumbnail_size_spin;

	ctk_spin_button_set_value(CTK_SPIN_BUTTON(button), (gdouble)tasklist->thumbnail_size);
}

static void thumbnail_size_changed(GSettings *settings, gchar* key, TasklistData* tasklist)
{
	tasklist->thumbnail_size = g_settings_get_int(settings, key);
	tasklist_update_thumbnail_size_spin(tasklist);
}
#endif

static CtkWidget* get_grouping_button(TasklistData* tasklist, WnckTasklistGroupingType type)
{
	switch (type)
	{
		default:
		case WNCK_TASKLIST_NEVER_GROUP:
			return tasklist->never_group_radio;
			break;
		case WNCK_TASKLIST_AUTO_GROUP:
			return tasklist->auto_group_radio;
			break;
		case WNCK_TASKLIST_ALWAYS_GROUP:
			return tasklist->always_group_radio;
			break;
	}
}

static void group_windows_changed(GSettings* settings, gchar* key, TasklistData* tasklist)
{
	WnckTasklistGroupingType type;
	CtkWidget* button;

	type = g_settings_get_enum (settings, key);

	tasklist->grouping = type;
	tasklist_update(tasklist);

	button = get_grouping_button(tasklist, type);

	if (button && !ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(button)))
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(button), TRUE);
	}
}

static void tasklist_update_unminimization_radio(TasklistData* tasklist)
{
	CtkWidget* button;

	if (tasklist->move_minimized_radio == NULL)
		return;

	if (tasklist->move_unminimized_windows)
	{
		button = tasklist->move_minimized_radio;
	}
	else
	{
		button = tasklist->change_workspace_radio;
	}

	if (!ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(button)))
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(button), TRUE);
}


static void move_unminimized_windows_changed(GSettings* settings, gchar* key, TasklistData* tasklist)
{
	gboolean value;

	value = g_settings_get_boolean(settings, key);

	tasklist->move_unminimized_windows = (value != 0);
	tasklist_update(tasklist);

	tasklist_update_unminimization_radio(tasklist);
}

static void setup_gsettings(TasklistData* tasklist)
{
	tasklist->settings = cafe_panel_applet_settings_new (CAFE_PANEL_APPLET (tasklist->applet), WINDOW_LIST_SCHEMA);

	g_signal_connect (tasklist->settings,
					  "changed::display-all-workspaces",
					  G_CALLBACK (display_all_workspaces_changed),
					  tasklist);

#ifdef HAVE_WINDOW_PREVIEWS
	tasklist->preview_settings = cafe_panel_applet_settings_new (CAFE_PANEL_APPLET (tasklist->applet), WINDOW_LIST_PREVIEW_SCHEMA);

	g_signal_connect (tasklist->preview_settings,
					  "changed::show-window-thumbnails",
					  G_CALLBACK (window_thumbnails_changed),
					  tasklist);
	g_signal_connect (tasklist->preview_settings,
					  "changed::thumbnail-window-size",
					  G_CALLBACK (thumbnail_size_changed),
					  tasklist);
#endif
	g_signal_connect (tasklist->settings,
					  "changed::group-windows",
					  G_CALLBACK (group_windows_changed),
					  tasklist);
	g_signal_connect (tasklist->settings,
					  "changed::move-unminimized-windows",
					  G_CALLBACK (move_unminimized_windows_changed),
					  tasklist);
}

static void applet_size_allocate(CtkWidget *widget, CtkAllocation *allocation, TasklistData *tasklist)
{
	int len;
	const int* size_hints;

	size_hints = vnck_tasklist_get_size_hint_list (WNCK_TASKLIST (tasklist->tasklist), &len);

	g_assert(len % 2 == 0);

#if !defined(WNCKLET_INPROCESS) && !CTK_CHECK_VERSION (3, 23, 0)
	/* HACK: When loading the WnckTasklist initially, it reports size hints as though there were
	 * no elements in the Tasklist. This causes a rendering issue when built out-of-process in
	 * HiDPI displays. We keep a flag to skip size hinting until WnckTasklist has something to
	 * show. */
	if (!tasklist->needs_hints)
	{
		int i;
		for (i = 0; i < len; i++)
		{
			if (size_hints[i])
			{
				tasklist->needs_hints = TRUE;
				break;
			}
		}
	}

	if (tasklist->needs_hints)
#endif
		cafe_panel_applet_set_size_hints(CAFE_PANEL_APPLET(tasklist->applet), size_hints, len, 0);
}

static GdkPixbuf* icon_loader_func(const char* icon, int size, unsigned int flags, void* data)
{
	TasklistData* tasklist;
	GdkPixbuf* retval;
	char* icon_no_extension;
	char* p;

	tasklist = data;

	if (icon == NULL || strcmp(icon, "") == 0)
		return NULL;

	if (g_path_is_absolute(icon))
	{
		if (g_file_test(icon, G_FILE_TEST_EXISTS))
		{
			return gdk_pixbuf_new_from_file_at_size(icon, size, size, NULL);
		}
		else
		{
			char* basename;

			basename = g_path_get_basename(icon);
			retval = icon_loader_func(basename, size, flags, data);
			g_free(basename);

			return retval;
		}
	}

	/* This is needed because some .desktop files have an icon name *and*
	* an extension as icon */
	icon_no_extension = g_strdup(icon);
	p = strrchr(icon_no_extension, '.');

	if (p && (strcmp(p, ".png") == 0 || strcmp(p, ".xpm") == 0 || strcmp(p, ".svg") == 0))
	{
		*p = 0;
	}

	retval = ctk_icon_theme_load_icon(tasklist->icon_theme, icon_no_extension, size, 0, NULL);
	g_free(icon_no_extension);

	return retval;
}

gboolean window_list_applet_fill(CafePanelApplet* applet)
{
	TasklistData* tasklist;
	CtkActionGroup* action_group;
	CtkCssProvider  *provider;
	CdkScreen *screen;

	tasklist = g_new0(TasklistData, 1);

	tasklist->applet = CTK_WIDGET(applet);

	provider = ctk_css_provider_new ();
	screen = cdk_screen_get_default ();
	ctk_css_provider_load_from_data (provider,
										".cafe-panel-menu-bar button,\n"
										" #tasklist-button {\n"
										" padding: 0px;\n"
										" margin: 0px;\n }",
										-1, NULL);
	ctk_style_context_add_provider_for_screen (screen,
										CTK_STYLE_PROVIDER (provider),
										CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	cafe_panel_applet_set_flags(CAFE_PANEL_APPLET(tasklist->applet), CAFE_PANEL_APPLET_EXPAND_MAJOR | CAFE_PANEL_APPLET_EXPAND_MINOR | CAFE_PANEL_APPLET_HAS_HANDLE);

	setup_gsettings(tasklist);

	tasklist->include_all_workspaces = g_settings_get_boolean (tasklist->settings, "display-all-workspaces");

#ifdef HAVE_WINDOW_PREVIEWS
	tasklist->show_window_thumbnails = g_settings_get_boolean (tasklist->preview_settings, "show-window-thumbnails");

	tasklist->thumbnail_size = g_settings_get_int (tasklist->preview_settings, "thumbnail-window-size");
#endif

	tasklist->grouping = g_settings_get_enum (tasklist->settings, "group-windows");

	tasklist->move_unminimized_windows = g_settings_get_boolean (tasklist->settings, "move-unminimized-windows");

	tasklist->size = cafe_panel_applet_get_size(applet);

#if !defined(WNCKLET_INPROCESS) && !CTK_CHECK_VERSION (3, 23, 0)
	tasklist->needs_hints = FALSE;
#endif

	switch (cafe_panel_applet_get_orient(applet))
	{
		case CAFE_PANEL_APPLET_ORIENT_LEFT:
		case CAFE_PANEL_APPLET_ORIENT_RIGHT:
			tasklist->orientation = CTK_ORIENTATION_VERTICAL;
			break;
		case CAFE_PANEL_APPLET_ORIENT_UP:
		case CAFE_PANEL_APPLET_ORIENT_DOWN:
		default:
			tasklist->orientation = CTK_ORIENTATION_HORIZONTAL;
			break;
	}

	tasklist->tasklist = vnck_tasklist_new();

	vnck_tasklist_set_orientation (WNCK_TASKLIST (tasklist->tasklist), tasklist->orientation);
	vnck_tasklist_set_middle_click_close (WNCK_TASKLIST (tasklist->tasklist), TRUE);
	vnck_tasklist_set_icon_loader(WNCK_TASKLIST(tasklist->tasklist), icon_loader_func, tasklist, NULL);

	g_signal_connect(G_OBJECT(tasklist->tasklist), "destroy", G_CALLBACK(destroy_tasklist), tasklist);
#ifdef HAVE_WINDOW_PREVIEWS
	g_signal_connect(G_OBJECT(tasklist->tasklist), "task_enter_notify", G_CALLBACK(applet_enter_notify_event), tasklist);
	g_signal_connect(G_OBJECT(tasklist->tasklist), "task_leave_notify", G_CALLBACK(applet_leave_notify_event), tasklist);
#endif

	g_signal_connect(G_OBJECT(tasklist->applet), "size_allocate", G_CALLBACK(applet_size_allocate), tasklist);

	ctk_container_add(CTK_CONTAINER(tasklist->applet), tasklist->tasklist);

	g_signal_connect(G_OBJECT(tasklist->applet), "realize", G_CALLBACK(applet_realized), tasklist);
	g_signal_connect(G_OBJECT(tasklist->applet), "change_orient", G_CALLBACK(applet_change_orient), tasklist);
	g_signal_connect(G_OBJECT(tasklist->applet), "change_size", G_CALLBACK(applet_change_pixel_size), tasklist);
	g_signal_connect(G_OBJECT(tasklist->applet), "change_background", G_CALLBACK(applet_change_background), tasklist);

	cafe_panel_applet_set_background_widget(CAFE_PANEL_APPLET(tasklist->applet), CTK_WIDGET(tasklist->applet));

	action_group = ctk_action_group_new("Tasklist Applet Actions");
	ctk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions(action_group, tasklist_menu_actions, G_N_ELEMENTS(tasklist_menu_actions), tasklist);


	/* disable the item of system monitor, if not exists.
	 * example, cafe-system-monitor, o gnome-system-monitor */
	char* programpath;
	int i;

	for (i = 0; i < G_N_ELEMENTS(system_monitors); i += 1)
	{
		programpath = g_find_program_in_path(system_monitors[i]);

		if (programpath != NULL)
		{
			g_free(programpath);
			/* we give up */
			goto _system_monitor_found;
		}

		/* search another */
	}

	/* system monitor not found */
	ctk_action_set_visible(ctk_action_group_get_action(action_group, "TasklistSystemMonitor"), FALSE);

	_system_monitor_found:;
	/* end of system monitor item */


	cafe_panel_applet_setup_menu_from_resource (CAFE_PANEL_APPLET (tasklist->applet),
	                                            WNCKLET_RESOURCE_PATH "window-list-menu.xml",
	                                            action_group);

	if (cafe_panel_applet_get_locked_down(CAFE_PANEL_APPLET(tasklist->applet)))
	{
		CtkAction* action;

		action = ctk_action_group_get_action(action_group, "TasklistPreferences");
		ctk_action_set_visible(action, FALSE);
	}

	g_object_unref(action_group);

	tasklist_update(tasklist);
	ctk_widget_show(tasklist->tasklist);
	ctk_widget_show(tasklist->applet);

	return TRUE;
}

static void call_system_monitor(CtkAction* action, TasklistData* tasklist)
{
	char *programpath;
	int i;

	for (i = 0; i < G_N_ELEMENTS(system_monitors); i += 1)
	{
		programpath = g_find_program_in_path(system_monitors[i]);

		if (programpath != NULL)
		{
			g_free(programpath);

			cafe_cdk_spawn_command_line_on_screen(ctk_widget_get_screen(tasklist->applet),
				      system_monitors[i],
				      NULL);
			return;
		}
	}
}


static void display_help_dialog(CtkAction* action, TasklistData* tasklist)
{
	vncklet_display_help(tasklist->applet, "cafe-user-guide", "windowlist", WINDOW_LIST_ICON);
}

static void display_about_dialog(CtkAction* action, TasklistData* tasklist)
{
	static const gchar* authors[] = {
		"Perberos <perberos@gmail.com>",
		"Steve Zesch <stevezesch2@gmail.com>",
		"Stefano Karapetsas <stefano@karapetsas.com>",
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};

	const char* documenters [] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	ctk_show_about_dialog(CTK_WINDOW(tasklist->applet),
		"program-name", _("Window List"),
		"title", _("About Window List"),
		"authors", authors,
		"comments", _("The Window List shows a list of all windows in a set of buttons and lets you browse them."),
		"copyright", _("Copyright \xc2\xa9 2002 Red Hat, Inc.\n"
		               "Copyright \xc2\xa9 2011 Perberos\n"
		               "Copyright \xc2\xa9 2012-2020 CAFE developers"),
		"documenters", documenters,
		"icon-name", WINDOW_LIST_ICON,
		"logo-icon-name", WINDOW_LIST_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		"website", "http://www.cafe-desktop.org/",
		NULL);
}

static void group_windows_toggled(CtkToggleButton* button, TasklistData* tasklist)
{
	if (ctk_toggle_button_get_active(button))
	{
		gchar *value;
		value = g_object_get_data (G_OBJECT (button), "group_value");
		g_settings_set_string (tasklist->settings, "group-windows", value);
	}
}

#ifdef HAVE_WINDOW_PREVIEWS
static void show_thumbnails_toggled(CtkToggleButton* button, TasklistData* tasklist)
{
	g_settings_set_boolean(tasklist->preview_settings, "show-window-thumbnails", ctk_toggle_button_get_active(button));
}

static void thumbnail_size_spin_changed(CtkSpinButton* button, TasklistData* tasklist)
{
	g_settings_set_int(tasklist->preview_settings, "thumbnail-window-size", ctk_spin_button_get_value_as_int(button));
}
#endif

static void move_minimized_toggled(CtkToggleButton* button, TasklistData* tasklist)
{
	g_settings_set_boolean(tasklist->settings, "move-unminimized-windows", ctk_toggle_button_get_active(button));
}

static void display_all_workspaces_toggled(CtkToggleButton* button, TasklistData* tasklist)
{
	g_settings_set_boolean(tasklist->settings, "display-all-workspaces", ctk_toggle_button_get_active(button));
}

#define WID(s) CTK_WIDGET(ctk_builder_get_object(builder, s))

static void setup_sensitivity(TasklistData* tasklist, CtkBuilder* builder, const char* wid1, const char* wid2, const char* wid3, const char* key)
{
	CtkWidget* w;

	if (g_settings_is_writable(tasklist->settings, key))
	{
		return;
	}

	w = WID(wid1);
	g_assert(w != NULL);
	ctk_widget_set_sensitive(w, FALSE);

	if (wid2 != NULL)
	{
		w = WID(wid2);
		g_assert(w != NULL);
		ctk_widget_set_sensitive(w, FALSE);
	}

	if (wid3 != NULL)
	{
		w = WID(wid3);
		g_assert(w != NULL);
		ctk_widget_set_sensitive(w, FALSE);
	}
}

static void setup_dialog(CtkBuilder* builder, TasklistData* tasklist)
{
	CtkWidget* button;
#ifdef HAVE_WINDOW_PREVIEWS
	CtkAdjustment *adjustment;
#endif

	tasklist->show_current_radio = WID("show_current_radio");
	tasklist->show_all_radio = WID("show_all_radio");

	setup_sensitivity(tasklist, builder, "show_current_radio", "show_all_radio", NULL, "display-all-workspaces" /* key */);

	tasklist->never_group_radio = WID("never_group_radio");
	tasklist->auto_group_radio = WID("auto_group_radio");
	tasklist->always_group_radio = WID("always_group_radio");

	setup_sensitivity(tasklist, builder, "never_group_radio", "auto_group_radio", "always_group_radio", "group-windows" /* key */);

#ifdef HAVE_WINDOW_PREVIEWS
	tasklist->show_thumbnails_radio = WID("show_thumbnails_radio");
	tasklist->hide_thumbnails_radio = WID("hide_thumbnails_radio");
	tasklist->thumbnail_size_spin = WID("thumbnail_size_spin");

	setup_sensitivity(tasklist, builder, "show_thumbnails_radio", "hide_thumbnails_radio", NULL, "show-window-thumbnails" /* key */);
	ctk_widget_set_sensitive(tasklist->thumbnail_size_spin, TRUE);
	adjustment = ctk_spin_button_get_adjustment (CTK_SPIN_BUTTON(tasklist->thumbnail_size_spin));
	ctk_adjustment_set_lower (adjustment, 0);
	ctk_adjustment_set_upper (adjustment, 999);
	ctk_adjustment_set_step_increment (adjustment, 1);
#else
	ctk_widget_hide(WID("window_thumbnails"));
#endif

	tasklist->minimized_windows_label = WID("minimized_windows_label");
	tasklist->move_minimized_radio = WID("move_minimized_radio");
	tasklist->change_workspace_radio = WID("change_workspace_radio");

	setup_sensitivity(tasklist, builder, "move_minimized_radio", "change_workspace_radio", NULL, "move-unminimized-windows" /* key */);

	/* Window grouping: */
	button = get_grouping_button(tasklist, tasklist->grouping);
	ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(button), TRUE);
	g_object_set_data(G_OBJECT(tasklist->never_group_radio), "group_value", "never");
	g_object_set_data(G_OBJECT(tasklist->auto_group_radio), "group_value", "auto");
	g_object_set_data(G_OBJECT(tasklist->always_group_radio), "group_value", "always");

	g_signal_connect(G_OBJECT(tasklist->never_group_radio), "toggled", (GCallback) group_windows_toggled, tasklist);
	g_signal_connect(G_OBJECT(tasklist->auto_group_radio), "toggled", (GCallback) group_windows_toggled, tasklist);
	g_signal_connect(G_OBJECT(tasklist->always_group_radio), "toggled", (GCallback) group_windows_toggled, tasklist);

#ifdef HAVE_WINDOW_PREVIEWS
	/* show thumbnails on hover: */
	tasklist_update_thumbnails_radio(tasklist);
	g_signal_connect(G_OBJECT(tasklist->show_thumbnails_radio), "toggled", (GCallback) show_thumbnails_toggled, tasklist);
	/* change thumbnail size: */
	tasklist_update_thumbnail_size_spin(tasklist);
	g_signal_connect(G_OBJECT(tasklist->thumbnail_size_spin), "value-changed", (GCallback) thumbnail_size_spin_changed, tasklist);
#endif

	/* move window when unminimizing: */
	tasklist_update_unminimization_radio(tasklist);
	g_signal_connect(G_OBJECT(tasklist->move_minimized_radio), "toggled", (GCallback) move_minimized_toggled, tasklist);

	/* Tasklist content: */
	tasklist_properties_update_content_radio (tasklist);
	g_signal_connect(G_OBJECT(tasklist->show_all_radio), "toggled", (GCallback) display_all_workspaces_toggled, tasklist);

	g_signal_connect_swapped(WID("done_button"), "clicked", (GCallback) ctk_widget_hide, tasklist->properties_dialog);
	g_signal_connect(tasklist->properties_dialog, "response", G_CALLBACK(response_cb), tasklist);
}

static void display_properties_dialog(CtkAction* action, TasklistData* tasklist)
{
	if (tasklist->properties_dialog == NULL)
	{
		CtkBuilder* builder;

		builder = ctk_builder_new();
		ctk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
		ctk_builder_add_from_resource (builder, WNCKLET_RESOURCE_PATH "window-list.ui", NULL);

		tasklist->properties_dialog = WID("tasklist_properties_dialog");

		g_object_add_weak_pointer(G_OBJECT(tasklist->properties_dialog), (void**) &tasklist->properties_dialog);

		setup_dialog(builder, tasklist);

		g_object_unref(builder);
	}

	ctk_window_set_icon_name(CTK_WINDOW(tasklist->properties_dialog), WINDOW_LIST_ICON);

	ctk_window_set_resizable(CTK_WINDOW(tasklist->properties_dialog), FALSE);
	ctk_window_set_screen(CTK_WINDOW(tasklist->properties_dialog), ctk_widget_get_screen(tasklist->applet));
	ctk_window_present(CTK_WINDOW(tasklist->properties_dialog));
}

static void destroy_tasklist(CtkWidget* widget, TasklistData* tasklist)
{
	g_signal_handlers_disconnect_by_data (G_OBJECT (tasklist->applet), tasklist);

#ifdef HAVE_WINDOW_PREVIEWS
	g_signal_handlers_disconnect_by_data (G_OBJECT (tasklist->tasklist), tasklist);
	g_signal_handlers_disconnect_by_data (tasklist->preview_settings, tasklist);
	g_object_unref(tasklist->preview_settings);
#endif

	g_signal_handlers_disconnect_by_data (tasklist->settings, tasklist);

	g_object_unref(tasklist->settings);

	if (tasklist->properties_dialog)
		ctk_widget_destroy(tasklist->properties_dialog);

#ifdef HAVE_WINDOW_PREVIEWS
	if (tasklist->preview)
		ctk_widget_destroy(tasklist->preview);
#endif

	g_free(tasklist);
}
