/* vncklet.c: A collection of window navigation applets
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 */

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include <string.h>
#include <cafe-panel-applet.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>
#define VNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libvnck/libvnck.h>

#include "vncklet.h"
#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

void vncklet_display_help(CtkWidget* widget, const char* doc_id, const char* link_id, const char* icon_name)
{
	GError* error = NULL;
	char* uri;

	if (link_id)
		uri = g_strdup_printf("help:%s/%s", doc_id, link_id);
	else
		uri = g_strdup_printf("help:%s", doc_id);

	ctk_show_uri_on_window (NULL, uri, ctk_get_current_event_time (), &error);
	g_free(uri);

	if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	{
		g_error_free(error);
	}
	else if (error)
	{
		CtkWidget* parent;
		CtkWidget* dialog;
		char* primary;

		if (CTK_IS_WINDOW(widget))
			parent = widget;
		else
			parent = NULL;

		primary = g_markup_printf_escaped(_("Could not display help document '%s'"), doc_id);
		dialog = ctk_message_dialog_new(parent ? CTK_WINDOW(parent) : NULL, CTK_DIALOG_MODAL|CTK_DIALOG_DESTROY_WITH_PARENT, CTK_MESSAGE_ERROR, CTK_BUTTONS_CLOSE, "%s", primary);

		ctk_message_dialog_format_secondary_text(CTK_MESSAGE_DIALOG(dialog), "%s", error->message);

		g_error_free(error);
		g_free(primary);

		g_signal_connect(dialog, "response", G_CALLBACK(ctk_widget_destroy), NULL);

		ctk_window_set_icon_name(CTK_WINDOW(dialog), icon_name);
		ctk_window_set_screen(CTK_WINDOW(dialog), ctk_widget_get_screen(widget));

		if (parent == NULL)
		{
			/* we have no parent window */
			ctk_window_set_skip_taskbar_hint(CTK_WINDOW(dialog), FALSE);
			ctk_window_set_title(CTK_WINDOW(dialog), _("Error displaying help document"));
		}

		ctk_widget_show(dialog);
	}
}

VnckScreen* vncklet_get_screen(CtkWidget* applet)
{
	int screen_num;

	if (!ctk_widget_has_screen(applet))
		return vnck_screen_get_default();

	screen_num = cdk_x11_screen_get_screen_number(ctk_widget_get_screen(applet));

	return vnck_screen_get(screen_num);
}

void vncklet_connect_while_alive(gpointer object, const char* signal, GCallback func, gpointer func_data, gpointer alive_object)
{
	GClosure* closure;

	closure = g_cclosure_new(func, func_data, NULL);
	g_object_watch_closure(G_OBJECT(alive_object), closure);
	g_signal_connect_closure_by_id(object, g_signal_lookup(signal, G_OBJECT_TYPE(object)), 0, closure, FALSE);
}

static gboolean vncklet_factory(CafePanelApplet* applet, const char* iid, gpointer data)
{
	gboolean retval = FALSE;
	static gboolean type_registered = FALSE;

	if (!type_registered)
	{
		vnck_set_client_type(VNCK_CLIENT_TYPE_PAGER);
		type_registered = TRUE;
	}

	if (!strcmp(iid, "WindowMenuApplet"))
		retval = window_menu_applet_fill(applet);
	else if (!strcmp(iid, "WorkspaceSwitcherApplet") || !strcmp(iid, "PagerApplet"))
		retval = workspace_switcher_applet_fill(applet);
	else if (!strcmp(iid, "WindowListApplet") || !strcmp(iid, "TasklistApplet"))
		retval = window_list_applet_fill(applet);
	else if (!strcmp(iid, "ShowDesktopApplet"))
		retval = show_desktop_applet_fill(applet);

	return retval;
}


#ifdef VNCKLET_INPROCESS
	CAFE_PANEL_APPLET_IN_PROCESS_FACTORY("VnckletFactory", PANEL_TYPE_APPLET, "WindowNavigationApplets", vncklet_factory, NULL)
#else
	CAFE_PANEL_APPLET_OUT_PROCESS_FACTORY("VnckletFactory", PANEL_TYPE_APPLET, "WindowNavigationApplets", vncklet_factory, NULL)
#endif
