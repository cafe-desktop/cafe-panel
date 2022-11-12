/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * libvnck based pager applet.
 * (C) 2001 Alexander Larsson
 * (C) 2001 Red Hat, Inc
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

#include <stdlib.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#define VNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libvnck/libvnck.h>
#include <gio/gio.h>

#include <libcafe-desktop/cafe-gsettings.h>

#include "workspace-switcher.h"

#include "vncklet.h"

/* even 16 is pretty darn dubious. */
#define MAX_REASONABLE_ROWS 16
#define DEFAULT_ROWS 1

#define WORKSPACE_SWITCHER_SCHEMA "org.cafe.panel.applet.workspace-switcher"

#define NEVER_SENSITIVE "never_sensitive"
#define MARCO_GENERAL_SCHEMA "org.cafe.Marco.general"
#define NUM_WORKSPACES "num-workspaces"
#define MARCO_WORKSPACES_SCHEMA "org.cafe.Marco.workspace-names"
#define WORKSPACE_NAME "name-1"

#define WORKSPACE_SWITCHER_ICON "cafe-panel-workspace-switcher"

typedef enum {
	PAGER_WM_MARCO,
	PAGER_WM_METACITY,
	PAGER_WM_COMPIZ,
	PAGER_WM_I3,
	PAGER_WM_UNKNOWN
} PagerWM;

typedef struct {
	CtkWidget* applet;

	CtkWidget* pager;

	WnckScreen* screen;
	PagerWM wm;

	/* Properties: */
	CtkWidget* properties_dialog;
	CtkWidget* workspaces_frame;
	CtkWidget* workspace_names_label;
	CtkWidget* workspace_names_scroll;
	CtkWidget* display_workspaces_toggle;
	CtkWidget* wrap_workspaces_toggle;
	CtkWidget* all_workspaces_radio;
	CtkWidget* current_only_radio;
	CtkWidget* num_rows_spin;	       /* for vertical layout this is cols */
	CtkWidget* label_row_col;
	CtkWidget* num_workspaces_spin;
	CtkWidget* workspaces_tree;
	CtkListStore* workspaces_store;
	CtkCellRenderer* cell;

	CtkOrientation orientation;
	int n_rows;				/* for vertical layout this is cols */
	WnckPagerDisplayMode display_mode;
	gboolean display_all;
	gboolean wrap_workspaces;

	GSettings* settings;
} PagerData;

static void display_properties_dialog(CtkAction* action, PagerData* pager);
static void display_help_dialog(CtkAction* action, PagerData* pager);
static void display_about_dialog(CtkAction* action, PagerData* pager);
static void destroy_pager(CtkWidget* widget, PagerData* pager);

static void pager_update(PagerData* pager)
{
	vnck_pager_set_orientation(VNCK_PAGER(pager->pager), pager->orientation);
	vnck_pager_set_n_rows(VNCK_PAGER(pager->pager), pager->n_rows);
	vnck_pager_set_show_all(VNCK_PAGER(pager->pager), pager->display_all);

	if (pager->wm == PAGER_WM_MARCO)
		vnck_pager_set_display_mode(VNCK_PAGER(pager->pager), pager->display_mode);
	else if (pager->wm == PAGER_WM_METACITY)
		vnck_pager_set_display_mode(VNCK_PAGER(pager->pager), pager->display_mode);
	else if (pager->wm == PAGER_WM_I3)
		vnck_pager_set_display_mode(VNCK_PAGER(pager->pager), pager->display_mode);
	else
		vnck_pager_set_display_mode(VNCK_PAGER(pager->pager), VNCK_PAGER_DISPLAY_CONTENT);
}

static void update_properties_for_wm(PagerData* pager)
{
	switch (pager->wm)
	{
		case PAGER_WM_MARCO:
			if (pager->workspaces_frame)
				ctk_widget_show(pager->workspaces_frame);
			if (pager->workspace_names_label)
				ctk_widget_show(pager->workspace_names_label);
			if (pager->workspace_names_scroll)
				ctk_widget_show(pager->workspace_names_scroll);
			if (pager->display_workspaces_toggle)
				ctk_widget_show(pager->display_workspaces_toggle);
			if (pager->cell)
				g_object_set (pager->cell, "editable", TRUE, NULL);
			break;
		case PAGER_WM_METACITY:
			if (pager->workspaces_frame)
				ctk_widget_show(pager->workspaces_frame);
			if (pager->workspace_names_label)
				ctk_widget_show(pager->workspace_names_label);
			if (pager->workspace_names_scroll)
				ctk_widget_show(pager->workspace_names_scroll);
			if (pager->display_workspaces_toggle)
				ctk_widget_show(pager->display_workspaces_toggle);
			if (pager->cell)
				g_object_set (pager->cell, "editable", TRUE, NULL);
			break;
		case PAGER_WM_I3:
			if (pager->workspaces_frame)
				ctk_widget_show(pager->workspaces_frame);
			if (pager->num_workspaces_spin)
				ctk_widget_set_sensitive(pager->num_workspaces_spin, FALSE);
			if (pager->workspace_names_label)
				ctk_widget_hide(pager->workspace_names_label);
			if (pager->workspace_names_scroll)
				ctk_widget_hide(pager->workspace_names_scroll);
			if (pager->display_workspaces_toggle)
				ctk_widget_show(pager->display_workspaces_toggle);
			if (pager->cell)
				g_object_set (pager->cell, "editable", FALSE, NULL);
			break;
		case PAGER_WM_COMPIZ:
			if (pager->workspaces_frame)
				ctk_widget_show(pager->workspaces_frame);
			if (pager->workspace_names_label)
				ctk_widget_hide(pager->workspace_names_label);
			if (pager->workspace_names_scroll)
				ctk_widget_hide(pager->workspace_names_scroll);
			if (pager->display_workspaces_toggle)
				ctk_widget_hide(pager->display_workspaces_toggle);
			if (pager->cell)
				g_object_set (pager->cell, "editable", FALSE, NULL);
			break;
		case PAGER_WM_UNKNOWN:
			if (pager->workspaces_frame)
				ctk_widget_hide(pager->workspaces_frame);
			break;
		default:
			g_assert_not_reached();
	}

	if (pager->properties_dialog) {
	        ctk_widget_hide (pager->properties_dialog);
	        ctk_widget_unrealize (pager->properties_dialog);
	        ctk_widget_show (pager->properties_dialog);
	}
}

static void window_manager_changed(WnckScreen* screen, PagerData* pager)
{
	const char *wm_name;

	wm_name = vnck_screen_get_window_manager_name(screen);

	if (!wm_name)
		pager->wm = PAGER_WM_UNKNOWN;
	else if (strcmp(wm_name, "Metacity (Marco)") == 0)
		pager->wm = PAGER_WM_MARCO;
	else if (strcmp(wm_name, "Metacity") == 0)
		pager->wm = PAGER_WM_METACITY;
	else if (strcmp(wm_name, "i3") == 0)
		pager->wm = PAGER_WM_I3;
	else if (strcmp(wm_name, "Compiz") == 0)
		pager->wm = PAGER_WM_COMPIZ;
	else
		pager->wm = PAGER_WM_UNKNOWN;

	update_properties_for_wm(pager);
	pager_update(pager);
}

static void applet_realized(CafePanelApplet* applet, PagerData* pager)
{
	pager->screen = vncklet_get_screen(CTK_WIDGET(applet));

	window_manager_changed(pager->screen, pager);
	vncklet_connect_while_alive(pager->screen, "window_manager_changed", G_CALLBACK(window_manager_changed), pager, pager->applet);
}

static void applet_unrealized(CafePanelApplet* applet, PagerData* pager)
{
	pager->screen = NULL;
	pager->wm = PAGER_WM_UNKNOWN;
}

static void applet_change_orient(CafePanelApplet* applet, CafePanelAppletOrient orient, PagerData* pager)
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

	if (new_orient == pager->orientation)
		return;

	pager->orientation = new_orient;
	pager_update(pager);

	if (pager->label_row_col)
		ctk_label_set_text(CTK_LABEL(pager->label_row_col), pager->orientation == CTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));
}

static void applet_change_background(CafePanelApplet* applet, CafePanelAppletBackgroundType type, CdkColor* color, cairo_pattern_t *pattern, PagerData* pager)
{
        CtkStyleContext *new_context;
        ctk_widget_reset_style (CTK_WIDGET (pager->pager));
        new_context = ctk_style_context_new ();
        ctk_style_context_set_path (new_context, ctk_widget_get_path (CTK_WIDGET (pager->pager)));
        g_object_unref (new_context);

        vnck_pager_set_shadow_type (VNCK_PAGER (pager->pager),
                type == PANEL_NO_BACKGROUND ? CTK_SHADOW_NONE : CTK_SHADOW_IN);
}

static void applet_style_updated (CafePanelApplet *applet, CtkStyleContext *context)
{
	CtkCssProvider *provider;
	CdkRGBA color;
	gchar *color_str;
	gchar *bg_css;

	provider = ctk_css_provider_new ();

	/* Provide a fallback color for the highlighted workspace based on the current theme */
	ctk_style_context_lookup_color (context, "theme_selected_bg_color", &color);
	color_str = cdk_rgba_to_string (&color);
	bg_css = g_strconcat (".vnck-pager:selected {\n"
		              "	background-color:", color_str, ";\n"
		              "}", NULL);
	ctk_css_provider_load_from_data (provider, bg_css, -1, NULL);
	g_free (bg_css);
	g_free (color_str);

	ctk_style_context_add_provider (context,
					CTK_STYLE_PROVIDER (provider),
					CTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
	g_object_unref (provider);
}

/* Replacement for the default scroll handler that also cares about the wrapping property.
 * Alternative: Add behaviour to libvnck (to the WnckPager widget).
 */
static gboolean applet_scroll(CafePanelApplet* applet, CdkEventScroll* event, PagerData* pager)
{
	CdkScrollDirection absolute_direction;
	int index;
	int n_workspaces;
	int n_columns;
	int in_last_row;

	if (event->type != CDK_SCROLL)
		return FALSE;

	if (event->direction == CDK_SCROLL_SMOOTH)
		return FALSE;

	index = vnck_workspace_get_number(vnck_screen_get_active_workspace(pager->screen));
	n_workspaces = vnck_screen_get_workspace_count(pager->screen);
	n_columns = n_workspaces / pager->n_rows;

	if (n_workspaces % pager->n_rows != 0)
		n_columns++;

	in_last_row    = n_workspaces % n_columns;

	absolute_direction = event->direction;

	if (ctk_widget_get_direction(CTK_WIDGET(applet)) == CTK_TEXT_DIR_RTL)
	{
		switch (event->direction)
		{
			case CDK_SCROLL_RIGHT:
				absolute_direction = CDK_SCROLL_LEFT;
				break;
			case CDK_SCROLL_LEFT:
				absolute_direction = CDK_SCROLL_RIGHT;
				break;
			default:
				break;
		}
	}

	switch (absolute_direction)
	{
		case CDK_SCROLL_DOWN:
			if (index + n_columns < n_workspaces)
			{
				index += n_columns;
			}
			else if (pager->wrap_workspaces && index == n_workspaces - 1)
			{
				index = 0;
			}
			else if ((index < n_workspaces - 1 && index + in_last_row != n_workspaces - 1) || (index == n_workspaces - 1 && in_last_row != 0))
			{
				index = (index % n_columns) + 1;
			}
			break;

		case CDK_SCROLL_RIGHT:
			if (index < n_workspaces - 1)
			{
				index++;
			}
			else if (pager->wrap_workspaces)
			{
			        index = 0;
			}
			break;

		case CDK_SCROLL_UP:
			if (index - n_columns >= 0)
			{
				index -= n_columns;
			}
			else if (index > 0)
			{
				index = ((pager->n_rows - 1) * n_columns) + (index % n_columns) - 1;
			}
			else if (pager->wrap_workspaces)
			{
				index = n_workspaces - 1;
			}

			if (index >= n_workspaces)
				index -= n_columns;
			break;

		case CDK_SCROLL_LEFT:
			if (index > 0)
			{
				index--;
			}
			else if (pager->wrap_workspaces)
			{
				index = n_workspaces - 1;
			}
			break;
		default:
			g_assert_not_reached();
			break;
	}

	vnck_workspace_activate(vnck_screen_get_workspace(pager->screen, index), event->time);

	return TRUE;
}

static const CtkActionEntry pager_menu_actions[] = {
	{
		"PagerPreferences",
		"document-properties",
		N_("_Preferences"),
		NULL,
		NULL,
		G_CALLBACK(display_properties_dialog)
	},
	{
		"PagerHelp",
		"help-browser",
		N_("_Help"),
		NULL,
		NULL,
		G_CALLBACK(display_help_dialog)
	},
	{
		"PagerAbout",
		"help-about",
		N_("_About"),
		NULL,
		NULL,
		G_CALLBACK(display_about_dialog)
	}
};

static void num_rows_changed(GSettings* settings, gchar* key, PagerData* pager)
{
	int n_rows = DEFAULT_ROWS;

	n_rows = g_settings_get_int (settings, key);

	n_rows = CLAMP(n_rows, 1, MAX_REASONABLE_ROWS);

	pager->n_rows = n_rows;
	pager_update(pager);

	if (pager->num_rows_spin && ctk_spin_button_get_value_as_int(CTK_SPIN_BUTTON(pager->num_rows_spin)) != n_rows)
		ctk_spin_button_set_value(CTK_SPIN_BUTTON(pager->num_rows_spin), pager->n_rows);
}

static void display_workspace_names_changed(GSettings* settings, gchar* key, PagerData* pager)
{
	gboolean value = FALSE; /* Default value */

	value = g_settings_get_boolean (settings, key);

	if (value)
	{
		pager->display_mode = VNCK_PAGER_DISPLAY_NAME;
	}
	else
	{
		pager->display_mode = VNCK_PAGER_DISPLAY_CONTENT;
	}

	pager_update(pager);

	if (pager->display_workspaces_toggle && ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(pager->display_workspaces_toggle)) != value)
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->display_workspaces_toggle), value);
	}
}


static void all_workspaces_changed(GSettings* settings, gchar* key, PagerData* pager)
{
	gboolean value = TRUE; /* Default value */

	value = g_settings_get_boolean (settings, key);

	pager->display_all = value;
	pager_update(pager);

	if (pager->all_workspaces_radio)
	{
		if (ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(pager->all_workspaces_radio)) != value)
		{
			if (value)
			{
				ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->all_workspaces_radio), TRUE);
			}
			else
			{
				ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->current_only_radio), TRUE);
			}
		}

		if (!g_object_get_data(G_OBJECT(pager->num_rows_spin), NEVER_SENSITIVE))
			ctk_widget_set_sensitive(pager->num_rows_spin, value);
	}
}

static void wrap_workspaces_changed(GSettings* settings, gchar* key, PagerData* pager)
{
	gboolean value = FALSE; /* Default value */

	value = g_settings_get_boolean (settings, key);

	pager->wrap_workspaces = value;

	if (pager->wrap_workspaces_toggle && ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(pager->wrap_workspaces_toggle)) != value)
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->wrap_workspaces_toggle), value);
	}
}

static void setup_gsettings(PagerData* pager)
{
	pager->settings = cafe_panel_applet_settings_new (CAFE_PANEL_APPLET (pager->applet), WORKSPACE_SWITCHER_SCHEMA);

	g_signal_connect (pager->settings,
					  "changed::num-rows",
					  G_CALLBACK (num_rows_changed),
					  pager);
	g_signal_connect (pager->settings,
					  "changed::display-workspace-names",
					  G_CALLBACK (display_workspace_names_changed),
					  pager);
	g_signal_connect (pager->settings,
					  "changed::display-all-workspaces",
					  G_CALLBACK (all_workspaces_changed),
					  pager);
	g_signal_connect (pager->settings,
					  "changed::wrap-workspaces",
					  G_CALLBACK (wrap_workspaces_changed),
					  pager);

}

gboolean workspace_switcher_applet_fill(CafePanelApplet* applet)
{
	PagerData* pager;
	CtkActionGroup* action_group;
	gboolean display_names;

	pager = g_new0(PagerData, 1);

	pager->applet = CTK_WIDGET(applet);

	cafe_panel_applet_set_flags(CAFE_PANEL_APPLET(pager->applet), CAFE_PANEL_APPLET_EXPAND_MINOR);

	setup_gsettings(pager);

	pager->n_rows = g_settings_get_int(pager->settings, "num-rows");

	pager->n_rows = CLAMP(pager->n_rows, 1, MAX_REASONABLE_ROWS);

	display_names = g_settings_get_boolean(pager->settings, "display-workspace-names");

	pager->wrap_workspaces = g_settings_get_boolean(pager->settings, "wrap-workspaces");

	if (display_names)
	{
		pager->display_mode = VNCK_PAGER_DISPLAY_NAME;
	}
	else
	{
		pager->display_mode = VNCK_PAGER_DISPLAY_CONTENT;
	}

	pager->display_all = g_settings_get_boolean(pager->settings, "display-all-workspaces");

	switch (cafe_panel_applet_get_orient(applet))
	{
		case CAFE_PANEL_APPLET_ORIENT_LEFT:
		case CAFE_PANEL_APPLET_ORIENT_RIGHT:
			pager->orientation = CTK_ORIENTATION_VERTICAL;
			break;
		case CAFE_PANEL_APPLET_ORIENT_UP:
		case CAFE_PANEL_APPLET_ORIENT_DOWN:
		default:
			pager->orientation = CTK_ORIENTATION_HORIZONTAL;
			break;
	}

	pager->pager = vnck_pager_new();
	pager->screen = NULL;
	pager->wm = PAGER_WM_UNKNOWN;
	vnck_pager_set_shadow_type(VNCK_PAGER(pager->pager), CTK_SHADOW_IN);

	CtkStyleContext *context;
	context = ctk_widget_get_style_context (CTK_WIDGET (applet));
	ctk_style_context_add_class (context, "vnck-applet");
	context = ctk_widget_get_style_context (pager->pager);
	ctk_style_context_add_class (context, "vnck-pager");

	g_signal_connect(G_OBJECT(pager->pager), "destroy", G_CALLBACK(destroy_pager), pager);

	/* overwrite default WnckPager widget scroll-event */
	g_signal_connect(G_OBJECT(pager->pager), "scroll-event", G_CALLBACK(applet_scroll), pager);

	ctk_container_add(CTK_CONTAINER(pager->applet), pager->pager);

	g_signal_connect(G_OBJECT(pager->applet), "realize", G_CALLBACK(applet_realized), pager);
	g_signal_connect(G_OBJECT(pager->applet), "unrealize", G_CALLBACK(applet_unrealized), pager);
	g_signal_connect(G_OBJECT(pager->applet), "change_orient", G_CALLBACK(applet_change_orient), pager);
	g_signal_connect(G_OBJECT(pager->applet), "change_background", G_CALLBACK(applet_change_background), pager);
	g_signal_connect(G_OBJECT(pager->applet), "style-updated", G_CALLBACK(applet_style_updated), context);

	ctk_widget_show(pager->pager);
	ctk_widget_show(pager->applet);

	cafe_panel_applet_set_background_widget(CAFE_PANEL_APPLET(pager->applet), CTK_WIDGET(pager->applet));

	action_group = ctk_action_group_new("WorkspaceSwitcher Applet Actions");
	ctk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions(action_group, pager_menu_actions, G_N_ELEMENTS(pager_menu_actions), pager);
	cafe_panel_applet_setup_menu_from_resource (CAFE_PANEL_APPLET (pager->applet),
	                                            VNCKLET_RESOURCE_PATH "workspace-switcher-menu.xml",
	                                            action_group);

	if (cafe_panel_applet_get_locked_down(CAFE_PANEL_APPLET(pager->applet)))
	{
		CtkAction *action;

		action = ctk_action_group_get_action(action_group, "PagerPreferences");
		ctk_action_set_visible(action, FALSE);
	}

	g_object_unref(action_group);

	return TRUE;
}


static void display_help_dialog(CtkAction* action, PagerData* pager)
{
	vncklet_display_help(pager->applet, "cafe-user-guide", "overview-workspaces", WORKSPACE_SWITCHER_ICON);
}

static void display_about_dialog(CtkAction* action, PagerData* pager)
{
	static const gchar* authors[] = {
		"Perberos <perberos@gmail.com>",
		"Steve Zesch <stevezesch2@gmail.com>",
		"Stefano Karapetsas <stefano@karapetsas.com>",
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};

	const char* documenters[] = {
		"John Fleck <jfleck@inkstain.net>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	ctk_show_about_dialog(CTK_WINDOW(pager->applet),
		"program-name", _("Workspace Switcher"),
		"title", _("About Workspace Switcher"),
		"authors", authors,
		"comments", _("The Workspace Switcher shows you a small version of your workspaces that lets you manage your windows."),
		"copyright", _("Copyright \xc2\xa9 2002 Red Hat, Inc.\n"
                               "Copyright \xc2\xa9 2011 Perberos\n"
                               "Copyright \xc2\xa9 2012-2020 CAFE developers"),
		"documenters", documenters,
		"icon-name", WORKSPACE_SWITCHER_ICON,
		"logo-icon-name", WORKSPACE_SWITCHER_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		"website", "http://www.cafe-desktop.org/",
		NULL);
}

static void wrap_workspaces_toggled(CtkToggleButton* button, PagerData* pager)
{
	g_settings_set_boolean(pager->settings, "wrap-workspaces", ctk_toggle_button_get_active(button));
}

static void display_workspace_names_toggled(CtkToggleButton* button, PagerData* pager)
{
	g_settings_set_boolean(pager->settings, "display-workspace-names", ctk_toggle_button_get_active(button));
}

static void all_workspaces_toggled(CtkToggleButton* button, PagerData* pager)
{
	g_settings_set_boolean(pager->settings, "display-all-workspaces", ctk_toggle_button_get_active(button));
}

static void num_rows_value_changed(CtkSpinButton* button, PagerData* pager)
{
	g_settings_set_int(pager->settings, "num-rows", ctk_spin_button_get_value_as_int(button));
}

static void update_workspaces_model(PagerData* pager)
{
	int nr_ws, i;
	WnckWorkspace* workspace;
	CtkTreeIter iter;

	nr_ws = vnck_screen_get_workspace_count(pager->screen);

	if (pager->properties_dialog)
	{
		if (nr_ws != ctk_spin_button_get_value_as_int(CTK_SPIN_BUTTON(pager->num_workspaces_spin)))
			ctk_spin_button_set_value(CTK_SPIN_BUTTON(pager->num_workspaces_spin), nr_ws);

		ctk_list_store_clear(pager->workspaces_store);

		for (i = 0; i < nr_ws; i++)
		{
			workspace = vnck_screen_get_workspace(pager->screen, i);
			ctk_list_store_append(pager->workspaces_store, &iter);
			ctk_list_store_set(pager->workspaces_store, &iter, 0, vnck_workspace_get_name(workspace), -1);
		}
	}
}

static void workspace_renamed(WnckWorkspace* space, PagerData* pager)
{
	int i;
	CtkTreeIter iter;

	i = vnck_workspace_get_number(space);

	if (ctk_tree_model_iter_nth_child(CTK_TREE_MODEL(pager->workspaces_store), &iter, NULL, i))
		ctk_list_store_set(pager->workspaces_store, &iter, 0, vnck_workspace_get_name(space), -1);
}

static void workspace_created(WnckScreen* screen, WnckWorkspace* space, PagerData* pager)
{
	g_return_if_fail(VNCK_IS_SCREEN(screen));

	update_workspaces_model(pager);

	vncklet_connect_while_alive(space, "name_changed", G_CALLBACK(workspace_renamed), pager, pager->properties_dialog);
}

static void workspace_destroyed(WnckScreen* screen, WnckWorkspace* space, PagerData* pager)
{
	g_return_if_fail(VNCK_IS_SCREEN(screen));
	update_workspaces_model(pager);
}

static void num_workspaces_value_changed(CtkSpinButton* button, PagerData* pager)
{
	vnck_screen_change_workspace_count(pager->screen, ctk_spin_button_get_value_as_int(CTK_SPIN_BUTTON(pager->num_workspaces_spin)));
}

static gboolean workspaces_tree_focused_out(CtkTreeView* treeview, CdkEventFocus* event, PagerData* pager)
{
	CtkTreeSelection* selection;

	selection = ctk_tree_view_get_selection(treeview);
	ctk_tree_selection_unselect_all(selection);
	return TRUE;
}

static void workspace_name_edited(CtkCellRendererText* cell_renderer_text, const gchar* path, const gchar* new_text, PagerData* pager)
{
	const gint* indices;
	WnckWorkspace* workspace;
	CtkTreePath* p;

	p = ctk_tree_path_new_from_string(path);
	indices = ctk_tree_path_get_indices(p);
	workspace = vnck_screen_get_workspace(pager->screen, indices[0]);

	if (workspace != NULL)
	{
		gchar* temp_name = g_strdup(new_text);

		vnck_workspace_change_name(workspace, g_strstrip(temp_name));

		g_free(temp_name);
	}
	else
	{
		g_warning("Edited name of workspace %d which no longer exists", indices[0]);
	}

	ctk_tree_path_free(p);
}

static void properties_dialog_destroyed(CtkWidget* widget, PagerData* pager)
{
	pager->properties_dialog = NULL;
	pager->workspaces_frame = NULL;
	pager->workspace_names_label = NULL;
	pager->workspace_names_scroll = NULL;
	pager->display_workspaces_toggle = NULL;
	pager->wrap_workspaces_toggle = NULL;
	pager->all_workspaces_radio = NULL;
	pager->current_only_radio = NULL;
	pager->num_rows_spin = NULL;
	pager->label_row_col = NULL;
	pager->num_workspaces_spin = NULL;
	pager->workspaces_tree = NULL;
	pager->workspaces_store = NULL;
}

static gboolean delete_event(CtkWidget* widget, gpointer data)
{
	ctk_widget_destroy(widget);
	return TRUE;
}

static void response_cb(CtkWidget* widget, int id, PagerData* pager)
{
	if (id == CTK_RESPONSE_HELP)
		vncklet_display_help(widget, "cafe-user-guide", "overview-workspaces", WORKSPACE_SWITCHER_ICON);
	else
		ctk_widget_destroy(widget);
}

static void close_dialog(CtkWidget* button, gpointer data)
{
	PagerData* pager = data;
	CtkTreeViewColumn* col;
	CtkCellArea *area;
	CtkCellEditable *edit_widget;

	/* This is a hack. The "editable" signal for CtkCellRenderer is emitted
	only on button press or focus cycle. Hence when the user changes the
	name and closes the preferences dialog without a button-press he would
	lose the name changes. So, we call the ctk_cell_editable_editing_done
	to stop the editing. Thanks to Paolo for a better crack than the one I had.
	*/

	col = ctk_tree_view_get_column(CTK_TREE_VIEW(pager->workspaces_tree), 0);

	area = ctk_cell_layout_get_area (CTK_CELL_LAYOUT (col));
	edit_widget = ctk_cell_area_get_edit_widget (area);
	if (edit_widget)
		ctk_cell_editable_editing_done (edit_widget);

	ctk_widget_destroy(pager->properties_dialog);
}

#define WID(s) CTK_WIDGET(ctk_builder_get_object(builder, s))

static void
setup_sensitivity(PagerData* pager, CtkBuilder* builder, const char* wid1, const char* wid2, const char* wid3, GSettings* settings, const char* key)
{
	CtkWidget* w;

	if ((settings != NULL) && g_settings_is_writable(settings, key))
	{
		return;
	}

	w = WID(wid1);
	g_assert(w != NULL);
	g_object_set_data(G_OBJECT(w), NEVER_SENSITIVE, GINT_TO_POINTER(1));
	ctk_widget_set_sensitive(w, FALSE);

	if (wid2 != NULL)
	{
		w = WID(wid2);
		g_assert(w != NULL);
		g_object_set_data(G_OBJECT(w), NEVER_SENSITIVE, GINT_TO_POINTER(1));
		ctk_widget_set_sensitive(w, FALSE);
	}

	if (wid3 != NULL)
	{
		w = WID(wid3);
		g_assert(w != NULL);
		g_object_set_data(G_OBJECT(w), NEVER_SENSITIVE, GINT_TO_POINTER(1));
		ctk_widget_set_sensitive(w, FALSE);
	}
}

static void setup_dialog(CtkBuilder* builder, PagerData* pager)
{
	gboolean value;
	CtkTreeViewColumn* column;
	CtkCellRenderer* cell;
	int nr_ws, i;
	GSettings *marco_general_settings = NULL;
	GSettings *marco_workspaces_settings = NULL;

	if (cafe_gsettings_schema_exists(MARCO_GENERAL_SCHEMA))
		marco_general_settings = g_settings_new (MARCO_GENERAL_SCHEMA);
	if (cafe_gsettings_schema_exists(MARCO_WORKSPACES_SCHEMA))
		marco_workspaces_settings = g_settings_new (MARCO_WORKSPACES_SCHEMA);

	pager->workspaces_frame = WID("workspaces_frame");
	pager->workspace_names_label = WID("workspace_names_label");
	pager->workspace_names_scroll = WID("workspace_names_scroll");

	pager->display_workspaces_toggle = WID("workspace_name_toggle");
	setup_sensitivity(pager, builder, "workspace_name_toggle", NULL, NULL, pager->settings, "display-workspace-names" /* key */);

	pager->wrap_workspaces_toggle = WID("workspace_wrap_toggle");
	setup_sensitivity(pager, builder, "workspace_wrap_toggle", NULL, NULL, pager->settings, "wrap-workspaces" /* key */);

	pager->all_workspaces_radio = WID("all_workspaces_radio");
	pager->current_only_radio = WID("current_only_radio");
	setup_sensitivity(pager, builder, "all_workspaces_radio", "current_only_radio", "label_row_col", pager->settings, "display-all-workspaces" /* key */);

	pager->num_rows_spin = WID("num_rows_spin");
	pager->label_row_col = WID("label_row_col");
	setup_sensitivity(pager, builder, "num_rows_spin", NULL, NULL, pager->settings, "num-rows" /* key */);

	pager->num_workspaces_spin = WID("num_workspaces_spin");
	setup_sensitivity(pager, builder, "num_workspaces_spin", NULL, NULL, marco_general_settings, NUM_WORKSPACES /* key */);

	pager->workspaces_tree = WID("workspaces_tree_view");
	setup_sensitivity(pager, builder, "workspaces_tree_view", NULL, NULL, marco_workspaces_settings, WORKSPACE_NAME /* key */);

	if (marco_general_settings != NULL)
		g_object_unref (marco_general_settings);
	if (marco_workspaces_settings != NULL)
		g_object_unref (marco_workspaces_settings);


	/* Wrap workspaces: */
	if (pager->wrap_workspaces_toggle)
	{
		/* make sure the toggle button resembles the value of wrap_workspaces */
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->wrap_workspaces_toggle), pager->wrap_workspaces);
	}

	g_signal_connect(G_OBJECT(pager->wrap_workspaces_toggle), "toggled", (GCallback) wrap_workspaces_toggled, pager);

	/* Display workspace names: */

	g_signal_connect(G_OBJECT(pager->display_workspaces_toggle), "toggled", (GCallback) display_workspace_names_toggled, pager);

	if (pager->display_mode == VNCK_PAGER_DISPLAY_NAME)
	{
		value = TRUE;
	}
	else
	{
		value = FALSE;
	}

	ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->display_workspaces_toggle), value);

	/* Display all workspaces: */
	g_signal_connect(G_OBJECT(pager->all_workspaces_radio), "toggled", (GCallback) all_workspaces_toggled, pager);

	if (pager->display_all)
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->all_workspaces_radio), TRUE);

		if (!g_object_get_data(G_OBJECT(pager->num_rows_spin), NEVER_SENSITIVE))
		{
			ctk_widget_set_sensitive(pager->num_rows_spin, TRUE);
		}
	}
	else
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(pager->current_only_radio), TRUE);
		ctk_widget_set_sensitive(pager->num_rows_spin, FALSE);
	}

	/* Num rows: */
	g_signal_connect(G_OBJECT(pager->num_rows_spin), "value_changed", (GCallback) num_rows_value_changed, pager);

	ctk_spin_button_set_value(CTK_SPIN_BUTTON(pager->num_rows_spin), pager->n_rows);
	ctk_label_set_text(CTK_LABEL(pager->label_row_col), pager->orientation == CTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));

	g_signal_connect(pager->properties_dialog, "destroy", G_CALLBACK(properties_dialog_destroyed), pager);
	g_signal_connect(pager->properties_dialog, "delete_event", G_CALLBACK(delete_event), pager);
	g_signal_connect(pager->properties_dialog, "response", G_CALLBACK(response_cb), pager);

	g_signal_connect(WID("done_button"), "clicked", (GCallback) close_dialog, pager);

	ctk_spin_button_set_value(CTK_SPIN_BUTTON(pager->num_workspaces_spin), vnck_screen_get_workspace_count(pager->screen));
	g_signal_connect(G_OBJECT(pager->num_workspaces_spin), "value_changed", (GCallback) num_workspaces_value_changed, pager);

	vncklet_connect_while_alive(pager->screen, "workspace_created", G_CALLBACK(workspace_created), pager, pager->properties_dialog);

	vncklet_connect_while_alive(pager->screen, "workspace_destroyed", G_CALLBACK(workspace_destroyed), pager, pager->properties_dialog);

	g_signal_connect(G_OBJECT(pager->workspaces_tree), "focus_out_event", (GCallback) workspaces_tree_focused_out, pager);

	pager->workspaces_store = ctk_list_store_new(1, G_TYPE_STRING, NULL);
	update_workspaces_model(pager);
	ctk_tree_view_set_model(CTK_TREE_VIEW(pager->workspaces_tree), CTK_TREE_MODEL(pager->workspaces_store));

	g_object_unref(pager->workspaces_store);

	cell = g_object_new(CTK_TYPE_CELL_RENDERER_TEXT, "editable", TRUE, NULL);
	pager->cell = cell;
	column = ctk_tree_view_column_new_with_attributes("workspace", cell, "text", 0, NULL);
	ctk_tree_view_append_column(CTK_TREE_VIEW(pager->workspaces_tree), column);
	g_signal_connect(cell, "edited", (GCallback) workspace_name_edited, pager);

	nr_ws = vnck_screen_get_workspace_count(pager->screen);

	for (i = 0; i < nr_ws; i++)
	{
		vncklet_connect_while_alive(G_OBJECT(vnck_screen_get_workspace(pager->screen, i)), "name_changed", G_CALLBACK(workspace_renamed), pager, pager->properties_dialog);
	}

	update_properties_for_wm(pager);
}

static void display_properties_dialog(CtkAction* action, PagerData* pager)
{
	if (pager->properties_dialog == NULL)
	{
		CtkBuilder* builder;

		builder = ctk_builder_new();
		ctk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
		ctk_builder_add_from_resource (builder, VNCKLET_RESOURCE_PATH "workspace-switcher.ui", NULL);

		pager->properties_dialog = WID("pager_properties_dialog");

		g_object_add_weak_pointer(G_OBJECT(pager->properties_dialog), (gpointer*) &pager->properties_dialog);

		setup_dialog(builder, pager);

		g_object_unref(builder);
	}

	ctk_window_set_icon_name(CTK_WINDOW(pager->properties_dialog), WORKSPACE_SWITCHER_ICON);
	ctk_window_set_screen(CTK_WINDOW(pager->properties_dialog), ctk_widget_get_screen(pager->applet));
	ctk_window_present(CTK_WINDOW(pager->properties_dialog));
}

static void destroy_pager(CtkWidget* widget, PagerData* pager)
{
	g_signal_handlers_disconnect_by_data (pager->settings, pager);

	g_object_unref (pager->settings);

	if (pager->properties_dialog)
		ctk_widget_destroy(pager->properties_dialog);
	g_free(pager);
}
