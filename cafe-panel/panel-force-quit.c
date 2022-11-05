/*
 * panel-force-quit.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#ifndef HAVE_X11
#error file should only be built when HAVE_X11 is enabled
#endif

#include "panel-force-quit.h"

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <X11/extensions/XInput2.h>

#include <libpanel-util/panel-ctk.h>

#include "panel-icon-names.h"
#include "panel-stock-icons.h"

static GdkFilterReturn popup_filter (GdkXEvent *cdk_xevent,
				     GdkEvent  *event,
				     CtkWidget *popup);

static Atom wm_state_atom = None;

static CtkWidget *
display_popup_window (GdkScreen *screen)
{
	CtkWidget     *retval;
	CtkWidget     *vbox;
	CtkWidget     *image;
	CtkWidget     *frame;
	CtkWidget     *label;
	int            screen_width, screen_height;
	CtkAllocation  allocation;

	retval = ctk_window_new (CTK_WINDOW_POPUP);
	atk_object_set_role (ctk_widget_get_accessible (retval), ATK_ROLE_ALERT);
	ctk_window_set_screen (CTK_WINDOW (retval), screen);
	ctk_window_stick (CTK_WINDOW (retval));
	ctk_widget_add_events (retval, CDK_BUTTON_PRESS_MASK | CDK_KEY_PRESS_MASK);

	frame = ctk_frame_new (NULL);
	ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_ETCHED_IN);
	ctk_container_add (CTK_CONTAINER (retval), frame);
	ctk_widget_show (frame);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_container_set_border_width (CTK_CONTAINER (vbox), 8);
	ctk_container_add (CTK_CONTAINER (frame), vbox);
	ctk_widget_show (vbox);

	image = ctk_image_new_from_icon_name (PANEL_ICON_FORCE_QUIT,
					      CTK_ICON_SIZE_DIALOG);
	ctk_widget_set_halign (image, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (image, CTK_ALIGN_CENTER);
	ctk_box_pack_start (CTK_BOX (vbox), image, TRUE, TRUE, 4);
	ctk_widget_show (image);

	label = ctk_label_new (_("Click on a window to force the application to quit. "
				 "To cancel press <ESC>."));
	ctk_label_set_line_wrap (CTK_LABEL (label), TRUE);
	ctk_label_set_justify (CTK_LABEL (label), CTK_JUSTIFY_CENTER);
	ctk_box_pack_start (CTK_BOX (vbox), label, FALSE, FALSE, 4);
	ctk_widget_show (label);

	ctk_widget_realize (retval);

	screen_width  = WidthOfScreen (cdk_x11_screen_get_xscreen (screen));
	screen_height = HeightOfScreen (cdk_x11_screen_get_xscreen (screen));

	ctk_widget_get_allocation (retval, &allocation);

	ctk_window_move (CTK_WINDOW (retval),
			 (screen_width  - allocation.width) / 2,
			 (screen_height - allocation.height) / 2);

	ctk_widget_show (CTK_WIDGET (retval));

	return retval;
}

static void
remove_popup (CtkWidget *popup)
{
	GdkWindow        *root;
	GdkDisplay       *display;
	GdkSeat          *seat;

	root = cdk_screen_get_root_window (
			ctk_window_get_screen (CTK_WINDOW (popup)));
	cdk_window_remove_filter (root, (GdkFilterFunc) popup_filter, popup);

	ctk_widget_destroy (popup);

	display = cdk_window_get_display (root);
	seat = cdk_display_get_default_seat (display);

	cdk_seat_ungrab (seat);
}

static gboolean
wm_state_set (Display *xdisplay,
	      Window   window)
{
	GdkDisplay *display;
	gulong  nitems;
	gulong  bytes_after;
	gulong *prop;
	Atom    ret_type = None;
	int     ret_format;
	int     result;

	display = cdk_display_get_default ();
	cdk_x11_display_error_trap_push (display);
	result = XGetWindowProperty (xdisplay, window, wm_state_atom,
				     0, G_MAXLONG, False, wm_state_atom,
				     &ret_type, &ret_format, &nitems,
				     &bytes_after, (gpointer) &prop);

	if (cdk_x11_display_error_trap_pop (display))
		return FALSE;

	if (result != Success)
		return FALSE;

	XFree (prop);

	if (ret_type != wm_state_atom)
		return FALSE;

	return TRUE;
}

static Window
find_managed_window (Display *xdisplay,
		     Window   window)
{
	GdkDisplay *display;
	Window  root;
	Window  parent;
	Window *kids = NULL;
	Window  retval;
	guint   nkids;
	int     i, result;

	if (wm_state_set (xdisplay, window))
		return window;

	display = cdk_display_get_default ();
	cdk_x11_display_error_trap_push (display);
	result = XQueryTree (xdisplay, window, &root, &parent, &kids, &nkids);
	if (cdk_x11_display_error_trap_pop (display) || !result)
		return None;

	retval = None;

	for (i = 0; i < nkids; i++) {
		if (wm_state_set (xdisplay, kids [i])) {
			retval = kids [i];
			break;
		}

		retval = find_managed_window (xdisplay, kids [i]);
		if (retval != None)
			break;
	}

	if (kids)
		XFree (kids);

	return retval;
}

static void
kill_window_response (CtkDialog *dialog,
		      gint       response_id,
		      gpointer   user_data)
{
	if (response_id == CTK_RESPONSE_ACCEPT) {
		GdkDisplay *display;
		Display *xdisplay;
		Window window = (Window) user_data;

		display = ctk_widget_get_display (CTK_WIDGET (dialog));
		xdisplay = CDK_DISPLAY_XDISPLAY (display);

		cdk_x11_display_error_trap_push (display);
		XKillClient (xdisplay, window);
		cdk_display_flush (display);
		cdk_x11_display_error_trap_pop_ignored (display);
	}

	ctk_widget_destroy (CTK_WIDGET (dialog));
}

/* From marco */
static void
kill_window_question (gpointer window)
{
	CtkWidget *dialog;

	dialog = ctk_message_dialog_new (NULL, 0,
					 CTK_MESSAGE_WARNING,
					 CTK_BUTTONS_NONE,
					 _("Force this application to exit?"));

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  _("If you choose to force an application "
						  "to exit, unsaved changes in any open documents "
						  "in it might get lost."));

	panel_dialog_add_button (CTK_DIALOG (dialog),
				 _("_Cancel"), "process-stop",
				 CTK_RESPONSE_CANCEL);

	ctk_dialog_add_button (CTK_DIALOG (dialog),
			       PANEL_STOCK_FORCE_QUIT,
			       CTK_RESPONSE_ACCEPT);

	ctk_dialog_set_default_response (CTK_DIALOG (dialog),
					 CTK_RESPONSE_CANCEL);
	ctk_window_set_skip_taskbar_hint (CTK_WINDOW (dialog), FALSE);
	ctk_window_set_title (CTK_WINDOW (dialog), _("Force Quit"));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (kill_window_response), window);

	ctk_widget_show (dialog);
}

static void
handle_button_press_event (CtkWidget *popup,
			   Display *display,
			   Window subwindow)
{
	Window window;

	remove_popup (popup);

	if (subwindow == None)
		return;

	if (wm_state_atom == None)
		wm_state_atom = XInternAtom (display, "WM_STATE", FALSE);

	window = find_managed_window (display, subwindow);

	if (window != None) {
		if (!cdk_x11_window_lookup_for_display (cdk_x11_lookup_xdisplay (display), window))
			kill_window_question ((gpointer) window);
	}
}

static GdkFilterReturn
popup_filter (GdkXEvent *cdk_xevent,
	      GdkEvent  *event,
	      CtkWidget *popup)
{
	XEvent *xevent = (XEvent *) cdk_xevent;
	XIEvent *xiev;
	XIDeviceEvent *xidev;

	switch (xevent->type) {
	case ButtonPress:
		handle_button_press_event (popup, xevent->xbutton.display, xevent->xbutton.subwindow);
		return CDK_FILTER_REMOVE;
	case KeyPress:
		if (xevent->xkey.keycode == XKeysymToKeycode (xevent->xany.display, XK_Escape)) {
			remove_popup (popup);
			return CDK_FILTER_REMOVE;
		}
		break;
	case GenericEvent:
		xiev = (XIEvent *) xevent->xcookie.data;
		xidev = (XIDeviceEvent *) xiev;
		switch (xiev->evtype) {
		case XI_KeyPress:
			if (xidev->detail == XKeysymToKeycode (xevent->xany.display, XK_Escape)) {
				remove_popup (popup);
				return CDK_FILTER_REMOVE;
			}
			break;
		case XI_ButtonPress:
			handle_button_press_event (popup, xidev->display, xidev->child);
			return CDK_FILTER_REMOVE;
		}
		break;
	default:
		break;
	}

	return CDK_FILTER_CONTINUE;
}

static void
prepare_root_window (GdkSeat   *seat,
                     GdkWindow *window,
                     gpointer   user_data)
{
	cdk_window_show_unraised (window);
}

void
panel_force_quit (GdkScreen *screen,
		  guint      time)
{
	GdkGrabStatus  status;
	GdkCursor     *cross;
	GdkSeatCapabilities caps;
	CtkWidget     *popup;
	GdkWindow     *root;
	GdkDisplay    *display;
	GdkSeat       *seat;

	g_return_if_fail (CDK_IS_X11_DISPLAY (cdk_screen_get_display (screen)));

	popup = display_popup_window (screen);

	root = cdk_screen_get_root_window (screen);

	cdk_window_add_filter (root, (GdkFilterFunc) popup_filter, popup);
	cross = cdk_cursor_new_for_display (cdk_display_get_default (),
	                                    CDK_CROSS);
	caps = CDK_SEAT_CAPABILITY_POINTER | CDK_SEAT_CAPABILITY_KEYBOARD;
	display = cdk_window_get_display (root);
	seat = cdk_display_get_default_seat (display);

	status = cdk_seat_grab (seat, root,
	                        caps, FALSE,
	                        cross, NULL,
	                        prepare_root_window,
	                        NULL);

	g_object_unref (cross);

	if (status != CDK_GRAB_SUCCESS) {
		g_warning ("Pointer grab failed\n");
		remove_popup (popup);
		return;
	}

	cdk_display_flush (display);
}
