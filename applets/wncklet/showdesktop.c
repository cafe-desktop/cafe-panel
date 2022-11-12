/* -*- mode: C; c-file-style: "linux" -*- */
/* "Show desktop" panel applet */

/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Developed by Havoc Pennington
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

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include <glib/gi18n.h>

#include <ctk/ctk.h>
#include <cdk/cdkx.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libvnck/libvnck.h>

#include "vncklet.h"
#include "showdesktop.h"

#include <string.h>

#define TIMEOUT_ACTIVATE_SECONDS 1
#define SHOW_DESKTOP_ICON "user-desktop"


typedef struct {
	/* widgets */
	CtkWidget* applet;
	CtkWidget* button;
	CtkWidget* image;

	CtkOrientation orient;
	int size;

	WnckScreen* vnck_screen;

	guint showing_desktop: 1;
	guint button_activate;

	CtkIconTheme* icon_theme;
} ShowDesktopData;

static void display_help_dialog(CtkAction* action, ShowDesktopData* sdd);
static void display_about_dialog(CtkAction* action, ShowDesktopData* sdd);

static void update_icon(ShowDesktopData* sdd);
static void update_button_state(ShowDesktopData* sdd);
static void update_button_display(ShowDesktopData* sdd);

static void theme_changed_callback(CtkIconTheme* icon_theme, ShowDesktopData* sdd);

static void button_toggled_callback(CtkWidget* button, ShowDesktopData* sdd);
static void show_desktop_changed_callback(WnckScreen* screen, ShowDesktopData* sdd);

/* this is when the panel orientation changes */

static void applet_change_orient(CafePanelApplet* applet, CafePanelAppletOrient orient, ShowDesktopData* sdd)
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

	if (new_orient == sdd->orient)
		return;

	sdd->orient = new_orient;

	update_icon (sdd);
}

/* this is when the panel size changes */
static void button_size_allocated(CtkWidget* button, CtkAllocation* allocation, ShowDesktopData* sdd)
{
	/*
	if ((sdd->orient == CTK_ORIENTATION_HORIZONTAL) && (sdd->size == allocation->height))
	{
		return;
	}
	else if ((sdd->orient == CTK_ORIENTATION_VERTICAL) && (sdd->size == allocation->width))
	{
		return;
	}
	*/

	if (((sdd->orient == CTK_ORIENTATION_HORIZONTAL) && (sdd->size == allocation->height)) || ((sdd->orient == CTK_ORIENTATION_VERTICAL) && (sdd->size == allocation->width)))
		return;

	switch (sdd->orient)
	{
		case CTK_ORIENTATION_HORIZONTAL:
			sdd->size = allocation->height;
			break;
		case CTK_ORIENTATION_VERTICAL:
			sdd->size = allocation->width;
			break;
	}

	update_icon(sdd);
}

static void update_icon(ShowDesktopData* sdd)
{
	CtkStyleContext *context;
	CtkStateFlags    state;
	CtkBorder        padding;
	int width, height;
	cairo_surface_t* icon;
	cairo_surface_t* scaled;
	int icon_size, icon_scale;
	GError* error;
	int thickness = 0;

	if (!sdd->icon_theme)
		return;

	state = ctk_widget_get_state_flags (sdd->button);
	context = ctk_widget_get_style_context (sdd->button);
	ctk_style_context_get_padding (context, state, &padding);

	switch (sdd->orient) {
	case CTK_ORIENTATION_HORIZONTAL:
		thickness = padding.top + padding.bottom;
		break;
	case CTK_ORIENTATION_VERTICAL:
		thickness = padding.left + padding.right;
		break;
	}

	icon_scale = ctk_widget_get_scale_factor (sdd->button);
	icon_size = sdd->size * icon_scale - thickness;

	if (icon_size < 22)
		icon_size = 16;
	else if (icon_size < 24)
		icon_size = 22;
	else if (icon_size < 32)
		icon_size = 24;
	else if (icon_size < 48)
		icon_size = 32;
	else if (icon_size < 64)
		icon_size = 48;
	else if (icon_size < 128)
		icon_size = 64;

	error = NULL;
	icon = ctk_icon_theme_load_surface (sdd->icon_theme, SHOW_DESKTOP_ICON, icon_size, icon_scale, NULL, 0, &error);

	if (icon == NULL)
	{
		g_printerr(_("Failed to load %s: %s\n"), SHOW_DESKTOP_ICON, error ? error->message : _("Icon not found"));

		if (error)
		{
			g_error_free(error);
			error = NULL;
		}

		ctk_image_set_from_icon_name (CTK_IMAGE (sdd->image), "image-missing", CTK_ICON_SIZE_SMALL_TOOLBAR);
		return;
	}

	width = cairo_image_surface_get_width (icon);
	height = cairo_image_surface_get_height (icon);

	scaled = NULL;

	/* Make it fit on the given panel */
	switch (sdd->orient)
	{
		case CTK_ORIENTATION_HORIZONTAL:
			width = (icon_size / icon_scale * width) / height;
			height = icon_size / icon_scale;
			break;
		case CTK_ORIENTATION_VERTICAL:
			height = (icon_size / icon_scale * height) / width;
			width = icon_size / icon_scale;
			break;
	}

	scaled = cairo_surface_create_similar (icon,
		                               cairo_surface_get_content (icon),
		                               width,
		                               height);

	if (scaled != NULL)
	{
		cairo_t *cr;
		cr = cairo_create (scaled);
		cairo_scale (cr, (double) width / icon_size, (double) height / icon_size);
		cairo_set_source_surface (cr, icon, 0, 0);
		cairo_paint (cr);
		ctk_image_set_from_surface (CTK_IMAGE(sdd->image), scaled);
		cairo_destroy (cr);
		cairo_surface_destroy (scaled);
	}
	else
	{
		ctk_image_set_from_surface (CTK_IMAGE (sdd->image), icon);
	}

	cairo_surface_destroy (icon);
}

static const CtkActionEntry show_desktop_menu_actions[] = {
	{
		"ShowDesktopHelp",
		"help-browser",
		N_("_Help"),
		NULL,
		NULL,
		G_CALLBACK(display_help_dialog)
	},
	{
		"ShowDesktopAbout",
		"help-about",
		N_("_About"),
		NULL,
		NULL,
		G_CALLBACK(display_about_dialog)
	}
};

/* This updates things that should be consistent with the button's appearance,
 * and update_button_state updates the button appearance itself
 */
static void update_button_display(ShowDesktopData* sdd)
{
	const char* tip;

	if (ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(sdd->button)))
	{
		tip = _("Click here to restore hidden windows.");
	}
	else
	{
		tip = _("Click here to hide all windows and show the desktop.");
	}

	ctk_widget_set_tooltip_text(sdd->button, tip);
}

static void update_button_state(ShowDesktopData* sdd)
{
	if (sdd->showing_desktop)
	{
		g_signal_handlers_block_by_func(G_OBJECT(sdd->button), G_CALLBACK(button_toggled_callback), sdd);
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(sdd->button), TRUE);
		g_signal_handlers_unblock_by_func(G_OBJECT(sdd->button), G_CALLBACK(button_toggled_callback), sdd);
	}
	else
	{
		g_signal_handlers_block_by_func(G_OBJECT(sdd->button), G_CALLBACK(button_toggled_callback), sdd);
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON(sdd->button), FALSE);
		g_signal_handlers_unblock_by_func(G_OBJECT(sdd->button), G_CALLBACK(button_toggled_callback), sdd);
	}

	update_button_display (sdd);
}

static void applet_destroyed(CtkWidget* applet, ShowDesktopData* sdd)
{
	if (sdd->button_activate != 0)
	{
		g_source_remove(sdd->button_activate);
		sdd->button_activate = 0;
	}

	if (sdd->vnck_screen != NULL)
	{
		g_signal_handlers_disconnect_by_func(sdd->vnck_screen, show_desktop_changed_callback, sdd);
		sdd->vnck_screen = NULL;
	}

	if (sdd->icon_theme != NULL)
	{
		g_signal_handlers_disconnect_by_func(sdd->icon_theme, theme_changed_callback, sdd);
		sdd->icon_theme = NULL;
	}

	g_free (sdd);
}

static gboolean do_not_eat_button_press(CtkWidget* widget, CdkEventButton* event)
{
	if (event->button != 1)
	{
		g_signal_stop_emission_by_name(widget, "button_press_event");
	}

	return FALSE;
}

static gboolean button_motion_timeout(gpointer data)
{
	ShowDesktopData* sdd = (ShowDesktopData*) data;

	sdd->button_activate = 0;

	g_signal_emit_by_name(G_OBJECT(sdd->button), "clicked", sdd);

	return FALSE;
}

static void button_drag_leave(CtkWidget* widget, CdkDragContext* context, guint time, ShowDesktopData* sdd)
{
	if (sdd->button_activate != 0)
	{
		g_source_remove(sdd->button_activate);
		sdd->button_activate = 0;
	}
}

static gboolean button_drag_motion(CtkWidget* widget, CdkDragContext* context, gint x, gint y, guint time, ShowDesktopData* sdd)
{
	if (sdd->button_activate == 0)
		sdd->button_activate = g_timeout_add_seconds (TIMEOUT_ACTIVATE_SECONDS, button_motion_timeout, sdd);

	cdk_drag_status(context, 0, time);

	return TRUE;
}

static void show_desktop_applet_realized(CafePanelApplet* applet, gpointer data)
{
	ShowDesktopData* sdd;
	CdkScreen* screen;

	sdd = (ShowDesktopData*) data;

	if (sdd->vnck_screen != NULL)
		g_signal_handlers_disconnect_by_func(sdd->vnck_screen, show_desktop_changed_callback, sdd);

	if (sdd->icon_theme != NULL)
		g_signal_handlers_disconnect_by_func(sdd->icon_theme, theme_changed_callback, sdd);

	screen = ctk_widget_get_screen(sdd->applet);
	sdd->vnck_screen = vnck_screen_get(cdk_x11_screen_get_screen_number (screen));

	if (sdd->vnck_screen != NULL)
		vncklet_connect_while_alive(sdd->vnck_screen, "showing_desktop_changed", G_CALLBACK(show_desktop_changed_callback), sdd, sdd->applet);
	else
		g_warning("Could not get WnckScreen!");

	show_desktop_changed_callback(sdd->vnck_screen, sdd);

	sdd->icon_theme = ctk_icon_theme_get_for_screen (screen);
	vncklet_connect_while_alive(sdd->icon_theme, "changed", G_CALLBACK(theme_changed_callback), sdd, sdd->applet);

	update_icon (sdd);
}

static void theme_changed_callback(CtkIconTheme* icon_theme, ShowDesktopData* sdd)
{
	update_icon (sdd);
}

gboolean show_desktop_applet_fill(CafePanelApplet* applet)
{
	ShowDesktopData* sdd;
	CtkActionGroup* action_group;
	AtkObject* atk_obj;
	CtkCssProvider *provider;

	cafe_panel_applet_set_flags(applet, CAFE_PANEL_APPLET_EXPAND_MINOR);

	sdd = g_new0(ShowDesktopData, 1);

	sdd->applet = CTK_WIDGET(applet);

	sdd->image = ctk_image_new();

	switch (cafe_panel_applet_get_orient(applet))
	{
		case CAFE_PANEL_APPLET_ORIENT_LEFT:
		case CAFE_PANEL_APPLET_ORIENT_RIGHT:
			sdd->orient = CTK_ORIENTATION_VERTICAL;
			break;
		case CAFE_PANEL_APPLET_ORIENT_UP:
		case CAFE_PANEL_APPLET_ORIENT_DOWN:
		default:
			sdd->orient = CTK_ORIENTATION_HORIZONTAL;
			break;
	}

	sdd->size = cafe_panel_applet_get_size(CAFE_PANEL_APPLET(sdd->applet));

	g_signal_connect(G_OBJECT(sdd->applet), "realize", G_CALLBACK(show_desktop_applet_realized), sdd);

	sdd->button = ctk_toggle_button_new ();

	ctk_widget_set_name (sdd->button, "showdesktop-button");
    provider = ctk_css_provider_new ();
	ctk_css_provider_load_from_data (provider,
					 "#showdesktop-button {\n"
                     "border-width: 0px; \n" /*a border here causes CTK warnings */
					 " padding: 0px;\n"
					 " margin: 0px; }",
					 -1, NULL);

	ctk_style_context_add_provider (ctk_widget_get_style_context (sdd->button),
					CTK_STYLE_PROVIDER (provider),
					CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
					g_object_unref (provider);

	atk_obj = ctk_widget_get_accessible(sdd->button);
	atk_object_set_name (atk_obj, _("Show Desktop Button"));
	g_signal_connect(G_OBJECT(sdd->button), "button_press_event", G_CALLBACK(do_not_eat_button_press), NULL);

	g_signal_connect(G_OBJECT(sdd->button), "toggled", G_CALLBACK(button_toggled_callback), sdd);

	ctk_container_set_border_width(CTK_CONTAINER(sdd->button), 0);
	ctk_container_add(CTK_CONTAINER(sdd->button), sdd->image);
	ctk_container_add(CTK_CONTAINER(sdd->applet), sdd->button);

	g_signal_connect (G_OBJECT(sdd->button), "size_allocate", G_CALLBACK(button_size_allocated), sdd);

	/* FIXME: Update this comment. */
	/* we have to bind change_orient before we do applet_widget_add
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	g_signal_connect(G_OBJECT (sdd->applet), "change_orient", G_CALLBACK (applet_change_orient), sdd);

	cafe_panel_applet_set_background_widget(CAFE_PANEL_APPLET (sdd->applet), CTK_WIDGET(sdd->applet));

	action_group = ctk_action_group_new("ShowDesktop Applet Actions");
	ctk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions(action_group, show_desktop_menu_actions, G_N_ELEMENTS (show_desktop_menu_actions), sdd);
	cafe_panel_applet_setup_menu_from_resource (CAFE_PANEL_APPLET (sdd->applet),
	                                            WNCKLET_RESOURCE_PATH "showdesktop-menu.xml",
	                                            action_group);
	g_object_unref(action_group);

	g_signal_connect(G_OBJECT(sdd->applet), "destroy", G_CALLBACK(applet_destroyed), sdd);

	ctk_drag_dest_set(CTK_WIDGET(sdd->button), 0, NULL, 0, 0);

	g_signal_connect(G_OBJECT(sdd->button), "drag_motion", G_CALLBACK (button_drag_motion), sdd);
	g_signal_connect(G_OBJECT(sdd->button), "drag_leave", G_CALLBACK (button_drag_leave), sdd);

	ctk_widget_show_all(sdd->applet);

	return TRUE;
}

static void display_help_dialog(CtkAction* action, ShowDesktopData* sdd)
{
	vncklet_display_help(sdd->applet, "cafe-user-guide", "gospanel-564", SHOW_DESKTOP_ICON);
}

static void display_about_dialog(CtkAction* action, ShowDesktopData* sdd)
{
	static const gchar* authors[] = {
		"Perberos <perberos@gmail.com>",
		"Steve Zesch <stevezesch2@gmail.com>",
		"Stefano Karapetsas <stefano@karapetsas.com>",
		"Havoc Pennington <hp@redhat.com>",
		NULL
	};
	static const char* documenters[] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	ctk_show_about_dialog(CTK_WINDOW(sdd->applet),
		"program-name", _("Show Desktop Button"),
		"title", _("About Show Desktop Button"),
		"authors", authors,
		"comments", _("This button lets you hide all windows and show the desktop."),
		"copyright", _("Copyright \xc2\xa9 2002 Red Hat, Inc.\n"
		               "Copyright \xc2\xa9 2011 Perberos\n"
                               "Copyright \xc2\xa9 2012-2020 CAFE developers"),
		"documenters", documenters,
		"icon-name", SHOW_DESKTOP_ICON,
		"logo-icon-name", SHOW_DESKTOP_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		"website", "http://www.cafe-desktop.org/",
		NULL);
}

static void button_toggled_callback(CtkWidget* button, ShowDesktopData* sdd)
{
	if (!cdk_x11_screen_supports_net_wm_hint(ctk_widget_get_screen(button), cdk_atom_intern("_NET_SHOWING_DESKTOP", FALSE)))
	{
		static CtkWidget* dialog = NULL;

		if (dialog && ctk_widget_get_screen(dialog) != ctk_widget_get_screen(button))
			ctk_widget_destroy (dialog);

		if (dialog)
		{
			ctk_window_present(CTK_WINDOW(dialog));
			return;
		}

		dialog = ctk_message_dialog_new(NULL, CTK_DIALOG_MODAL, CTK_MESSAGE_ERROR, CTK_BUTTONS_CLOSE, _("Your window manager does not support the show desktop button, or you are not running a window manager."));

		g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer) &dialog);

		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(ctk_widget_destroy), NULL);

		ctk_window_set_resizable(CTK_WINDOW(dialog), FALSE);
		ctk_window_set_screen(CTK_WINDOW(dialog), ctk_widget_get_screen(button));
		ctk_widget_show(dialog);

		return;
	}

	if (sdd->vnck_screen != NULL)
		vnck_screen_toggle_showing_desktop(sdd->vnck_screen, ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON(button)));

	update_button_display (sdd);
}

static void show_desktop_changed_callback(WnckScreen* screen, ShowDesktopData* sdd)
{
	if (sdd->vnck_screen != NULL)
		sdd->showing_desktop = vnck_screen_get_showing_desktop(sdd->vnck_screen);

	update_button_state (sdd);
}
