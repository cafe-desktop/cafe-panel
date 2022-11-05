/*
 * panel-action-protocol.h: _CAFE_PANEL_ACTION protocol impl.
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include "panel-action-protocol.h"

#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <X11/Xlib.h>

#include "menu.h"
#include "applet.h"
#include "panel-globals.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-force-quit.h"
#include "panel-run-dialog.h"
#include "panel-menu-button.h"
#include "panel-menu-bar.h"

static Atom atom_cafe_panel_action            = None;
static Atom atom_gnome_panel_action           = None;
static Atom atom_cafe_panel_action_main_menu  = None;
static Atom atom_cafe_panel_action_run_dialog = None;
static Atom atom_gnome_panel_action_main_menu  = None;
static Atom atom_gnome_panel_action_run_dialog = None;
static Atom atom_cafe_panel_action_kill_dialog = None;

static void
panel_action_protocol_main_menu (CdkScreen *screen,
				 guint32    activate_time, CdkEvent  *event)
{
	PanelWidget *panel_widget;
	CtkWidget   *menu;
	AppletInfo  *info;
	CdkVisual *visual;
	CtkWidget *toplevel;
	CtkStyleContext *context;
	CdkSeat *seat;
	CdkDevice *device;

	info = cafe_panel_applet_get_by_type (PANEL_OBJECT_MENU_BAR, screen);
	if (info) {
		panel_menu_bar_popup_menu (PANEL_MENU_BAR (info->widget),
					   activate_time);
		return;
	}

	info = cafe_panel_applet_get_by_type (PANEL_OBJECT_MENU, screen);
	if (info && !panel_menu_button_get_use_menu_path (PANEL_MENU_BUTTON (info->widget))) {
		panel_menu_button_popup_menu (PANEL_MENU_BUTTON (info->widget),
					      1, activate_time);
		return;
	}

	panel_widget = panels->data;
	menu = create_main_menu (panel_widget);

	panel_toplevel_push_autohide_disabler (panel_widget->toplevel);

	ctk_menu_set_screen (CTK_MENU (menu), screen);
/* Set up theme and transparency support */
	toplevel = ctk_widget_get_toplevel (menu);
/* Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	visual = cdk_screen_get_rgba_visual(screen);
	ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);
/* Set menu and it's toplevel window to follow panel theme */
	context = ctk_widget_get_style_context (CTK_WIDGET(toplevel));
	ctk_style_context_add_class(context,"gnome-panel-menu-bar");
	ctk_style_context_add_class(context,"cafe-panel-menu-bar");

	seat = cdk_display_get_default_seat (cdk_display_get_default());
	device = cdk_seat_get_pointer (seat);
	cdk_event_set_device (event, device);

	ctk_menu_popup_at_pointer (CTK_MENU (menu),event);
}

static void
panel_action_protocol_run_dialog (CdkScreen *screen,
				  guint32    activate_time)
{
	panel_run_dialog_present (screen, activate_time);
}

static void
panel_action_protocol_kill_dialog (CdkScreen *screen,
				   guint32    activate_time)
{
	panel_force_quit (screen, activate_time);
}

static CdkFilterReturn
panel_action_protocol_filter (CdkXEvent *cdk_xevent,
			      CdkEvent  *event,
			      gpointer   data)
{
	CdkWindow *window;
	CdkScreen *screen;
	CdkDisplay *display;
	XEvent    *xevent = (XEvent *) cdk_xevent;

	if (xevent->type != ClientMessage)
		return CDK_FILTER_CONTINUE;

	if ((xevent->xclient.message_type != atom_cafe_panel_action) &&
	   (xevent->xclient.message_type != atom_gnome_panel_action))
		return CDK_FILTER_CONTINUE;

	screen = cdk_event_get_screen (event);
	display = cdk_screen_get_display (screen);
	if (!CDK_IS_X11_DISPLAY (display))
		return CDK_FILTER_CONTINUE;
	window = cdk_x11_window_lookup_for_display (display, xevent->xclient.window);
	if (!window)
		return CDK_FILTER_CONTINUE;

	if (window != cdk_screen_get_root_window (screen))
		return CDK_FILTER_CONTINUE;

	if (xevent->xclient.data.l [0] == atom_cafe_panel_action_main_menu)
		panel_action_protocol_main_menu (screen, xevent->xclient.data.l [1], event);
	else if (xevent->xclient.data.l [0] == atom_cafe_panel_action_run_dialog)
		panel_action_protocol_run_dialog (screen, xevent->xclient.data.l [1]);
	else if (xevent->xclient.data.l [0] == atom_gnome_panel_action_main_menu)
		panel_action_protocol_main_menu (screen, xevent->xclient.data.l [1], event);
	else if (xevent->xclient.data.l [0] == atom_gnome_panel_action_run_dialog)
		panel_action_protocol_run_dialog (screen, xevent->xclient.data.l [1]);
	else if (xevent->xclient.data.l [0] == atom_cafe_panel_action_kill_dialog)
		panel_action_protocol_kill_dialog (screen, xevent->xclient.data.l [1]);
	else
		return CDK_FILTER_CONTINUE;

	return CDK_FILTER_REMOVE;
}

void
panel_action_protocol_init (void)
{
	CdkDisplay *display;

	display = cdk_display_get_default ();
	g_assert(CDK_IS_X11_DISPLAY (display));

	atom_cafe_panel_action =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_CAFE_PANEL_ACTION",
			     FALSE);
	atom_gnome_panel_action =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION",
			     FALSE);
	atom_cafe_panel_action_main_menu =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_CAFE_PANEL_ACTION_MAIN_MENU",
			     FALSE);
	atom_cafe_panel_action_run_dialog =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_CAFE_PANEL_ACTION_RUN_DIALOG",
			     FALSE);
	atom_gnome_panel_action_main_menu =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION_MAIN_MENU",
			     FALSE);
	atom_gnome_panel_action_run_dialog =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION_RUN_DIALOG",
			     FALSE);
	atom_cafe_panel_action_kill_dialog =
		XInternAtom (CDK_DISPLAY_XDISPLAY (display),
			     "_CAFE_PANEL_ACTION_KILL_DIALOG",
			     FALSE);

	/* We'll filter event sent on non-root windows later */
	cdk_window_add_filter (NULL, panel_action_protocol_filter, NULL);
}
