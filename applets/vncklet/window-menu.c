/* window-menu.c: Window Selector applet
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <string.h>
#include <cafe-panel-applet.h>

#include <glib/gi18n.h>
#include <cdk/cdkkeysyms.h>

#define VNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libvnck/libvnck.h>

#include "vncklet.h"
#include "window-menu.h"

#define WINDOW_MENU_ICON "cafe-panel-window-menu"

typedef struct {
	CtkWidget* applet;
	CtkWidget* selector;
	int size;
	CafePanelAppletOrient orient;
} WindowMenu;

static void window_menu_help(CtkAction* action, WindowMenu* window_menu)
{
	vncklet_display_help(window_menu->applet, "cafe-user-guide", "panel-windowselector", WINDOW_MENU_ICON);
}

static void window_menu_about(CtkAction* action, WindowMenu* window_menu)
{
	static const char* authors[] = {
		"Perberos <perberos@gmail.com>",
		"Steve Zesch <stevezesch2@gmail.com>",
		"Stefano Karapetsas <stefano@karapetsas.com>",
		"Mark McLoughlin <mark@skynet.ie>",
		"George Lebl <jirka@5z.com>",
		"Jacob Berkman <jacob@helixcode.com>",
		NULL
	};

	const char* documenters[] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	ctk_show_about_dialog(CTK_WINDOW(window_menu->applet),
		"program-name", _("Window Selector"),
		"title", _("About Window Selector"),
		"authors", authors,
		"comments", _("The Window Selector shows a list of all windows in a menu and lets you browse them."),
		"copyright", _("Copyright \xc2\xa9 2000 Helix Code, Inc.\n"
		               "Copyright \xc2\xa9 2001 Free Software Foundation, Inc.\n"
		               "Copyright \xc2\xa9 2003 Sun Microsystems, Inc.\n"
		               "Copyright \xc2\xa9 2011 Perberos\n"
		               "Copyright \xc2\xa9 2012-2020 MATE developers\n"
		               "Copyright \xc2\xa9 2022-2024 Pablo Barciela"),
		"documenters", documenters,
		"icon-name", WINDOW_MENU_ICON,
		"logo-icon-name", WINDOW_MENU_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		"website", "http://www.cafe-desktop.org/",
		NULL);
}

static const CtkActionEntry window_menu_actions[] = {
	{
		"WindowMenuHelp",
		"help-browser",
		N_("_Help"),
		NULL,
		NULL,
		G_CALLBACK(window_menu_help)
	},
	{
		"WindowMenuAbout",
		"help-about",
		N_("_About"),
		NULL,
		NULL,
		G_CALLBACK(window_menu_about)
	}
};

static void window_menu_destroy(CtkWidget* widget, WindowMenu* window_menu)
{
	g_free(window_menu);
}

static gboolean window_menu_on_draw (CtkWidget* widget,
				     cairo_t*   cr,
				     gpointer   data)
{
	CtkStyleContext *context;
	CtkStateFlags    state;
	WindowMenu      *window_menu = data;

	if (!ctk_widget_has_focus (window_menu->applet))
		return FALSE;

	state = ctk_widget_get_state_flags (widget);
	context = ctk_widget_get_style_context (widget);
	ctk_style_context_save (context);
	ctk_style_context_set_state (context, state);

	cairo_save (cr);
	ctk_render_focus (context, cr,
			  0., 0.,
			  ctk_widget_get_allocated_width (widget),
			  ctk_widget_get_allocated_height (widget));
			  cairo_restore (cr);

	ctk_style_context_restore (context);

	return FALSE;
}

static void window_menu_size_allocate(CafePanelApplet* applet, CtkAllocation* allocation, WindowMenu* window_menu)
{
	CafePanelAppletOrient orient;
	GList* children;
	CtkWidget* child;

	orient = cafe_panel_applet_get_orient(applet);

	children = ctk_container_get_children(CTK_CONTAINER(window_menu->selector));
	child = CTK_WIDGET(children->data);
	g_list_free(children);

	if (orient == CAFE_PANEL_APPLET_ORIENT_LEFT || orient == CAFE_PANEL_APPLET_ORIENT_RIGHT)
	{
		if (window_menu->size == allocation->width && orient == window_menu->orient)
			return;

		window_menu->size = allocation->width;
		ctk_widget_set_size_request(child, window_menu->size, -1);
	}
	else
	{
		if (window_menu->size == allocation->height && orient == window_menu->orient)
			return;

		window_menu->size = allocation->height;
		ctk_widget_set_size_request(child, -1, window_menu->size);
	}

	window_menu->orient = orient;
}

static gboolean window_menu_key_press_event(CtkWidget* widget, CdkEventKey* event, WindowMenu* window_menu)
{
	CtkMenuShell* menu_shell;
	VnckSelector* selector;

	switch (event->keyval)
	{
		case CDK_KEY_KP_Enter:
		case CDK_KEY_ISO_Enter:
		case CDK_KEY_3270_Enter:
		case CDK_KEY_Return:
		case CDK_KEY_space:
		case CDK_KEY_KP_Space:
			selector = VNCK_SELECTOR(window_menu->selector);
			/*
			 * We need to call _ctk_menu_shell_activate() here as is done in
			 * window_key_press_handler in ctkmenubar.c which pops up menu
			 * when F10 is pressed.
			 *
			 * As that function is private its code is replicated here.
			 */
			menu_shell = CTK_MENU_SHELL(selector);

			ctk_menu_shell_select_first(menu_shell, FALSE);
			return TRUE;
		default:
			break;
	}

	return FALSE;
}

static gboolean filter_button_press(CtkWidget* widget, CdkEventButton* event, gpointer data)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name(widget, "button_press_event");

	return FALSE;
}

gboolean window_menu_applet_fill(CafePanelApplet* applet)
{
	WindowMenu* window_menu;
	CtkActionGroup* action_group;

	window_menu = g_new0(WindowMenu, 1);

	window_menu->applet = CTK_WIDGET(applet);
	ctk_widget_set_name (window_menu->applet, "window-menu-applet-button");
	ctk_widget_set_tooltip_text(window_menu->applet, _("Window Selector"));

	cafe_panel_applet_set_flags(applet, CAFE_PANEL_APPLET_EXPAND_MINOR);
	window_menu->size = cafe_panel_applet_get_size(applet);
	window_menu->orient = cafe_panel_applet_get_orient(applet);

	g_signal_connect(window_menu->applet, "destroy", G_CALLBACK(window_menu_destroy), window_menu);

	action_group = ctk_action_group_new("WindowMenu Applet Actions");
	ctk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions(action_group, window_menu_actions, G_N_ELEMENTS(window_menu_actions), window_menu);
	cafe_panel_applet_setup_menu_from_resource (CAFE_PANEL_APPLET (window_menu->applet),
	                                            VNCKLET_RESOURCE_PATH "window-menu-menu.xml",
	                                            action_group);
	g_object_unref(action_group);

	window_menu->selector = vnck_selector_new();
	ctk_container_add(CTK_CONTAINER(window_menu->applet), window_menu->selector);

	cafe_panel_applet_set_background_widget(CAFE_PANEL_APPLET(window_menu->applet), CTK_WIDGET(window_menu->selector));

	g_signal_connect(window_menu->applet, "key_press_event", G_CALLBACK(window_menu_key_press_event), window_menu);
	g_signal_connect(window_menu->applet, "size-allocate", G_CALLBACK(window_menu_size_allocate), window_menu);

	g_signal_connect_after(G_OBJECT(window_menu->applet), "focus-in-event", G_CALLBACK(ctk_widget_queue_draw), window_menu);
	g_signal_connect_after(G_OBJECT(window_menu->applet), "focus-out-event", G_CALLBACK(ctk_widget_queue_draw), window_menu);
	g_signal_connect_after(G_OBJECT(window_menu->selector), "draw", G_CALLBACK(window_menu_on_draw), window_menu);

	g_signal_connect(G_OBJECT(window_menu->selector), "button_press_event", G_CALLBACK(filter_button_press), window_menu);

	ctk_widget_show_all(CTK_WIDGET(window_menu->applet));

	return TRUE;
}
