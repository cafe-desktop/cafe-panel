/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel-applet-frame.c: panel side container for applets
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
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
#include <string.h>

#include <glib/gi18n.h>

#include <gio/gio.h>
#include <cdk/cdk.h>

#include <libpanel-util/panel-ctk.h>

#include "panel-applets-manager.h"
#include "panel-profile.h"
#include "panel.h"
#include "applet.h"
#include "panel-marshal.h"
#include "panel-background.h"
#include "panel-lockdown.h"
#include "panel-stock-icons.h"
#ifdef HAVE_X11
#include "xstuff.h"
#endif
#include "panel-schemas.h"

#include "panel-applet-frame.h"

#define PANEL_RESPONSE_DELETE       0
#define PANEL_RESPONSE_DONT_RELOAD  1
#define PANEL_RESPONSE_RELOAD       2

static void cafe_panel_applet_frame_activating_free (CafePanelAppletFrameActivating *frame_act);

static void cafe_panel_applet_frame_loading_failed  (const char  *iid,
					        PanelWidget *panel,
					        const char  *id);

static void cafe_panel_applet_frame_load            (const gchar *iid,
						PanelWidget *panel,
						gboolean     locked,
						int          position,
						gboolean     exactpos,
						const char  *id);

struct _CafePanelAppletFrameActivating {
	gboolean     locked;
	PanelWidget *panel;
	int          position;
	gboolean     exactpos;
	char        *id;
};

/* CafePanelAppletFrame implementation */

#define HANDLE_SIZE 10
#define CAFE_PANEL_APPLET_PREFS_PATH "/org/cafe/panel/objects/%s/prefs/"

struct _CafePanelAppletFramePrivate {
	PanelWidget     *panel;
	AppletInfo      *applet_info;

	PanelOrientation orientation;

	gchar           *iid;

	CtkAllocation    child_allocation;
	CdkRectangle     handle_rect;

	guint            has_handle : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (CafePanelAppletFrame, cafe_panel_applet_frame, CTK_TYPE_EVENT_BOX)

static gboolean
cafe_panel_applet_frame_draw (CtkWidget *widget,
                         cairo_t   *cr)
{
	CafePanelAppletFrame *frame = CAFE_PANEL_APPLET_FRAME (widget);
	CtkStyleContext *context;
	CtkStateFlags     state;
	PanelBackground  *background;

	if (CTK_WIDGET_CLASS (cafe_panel_applet_frame_parent_class)->draw)
		CTK_WIDGET_CLASS (cafe_panel_applet_frame_parent_class)->draw (widget, cr);

	if (!frame->priv->has_handle)
		return FALSE;

	context = ctk_widget_get_style_context (widget);
	state = ctk_widget_get_state_flags (widget);
	ctk_style_context_save (context);
	ctk_style_context_set_state (context, state);

	cairo_save (cr);

	background = &frame->priv->panel->toplevel->background;
	if (background->type == PANEL_BACK_IMAGE ||
	    (background->type == PANEL_BACK_COLOR && background->has_alpha)) {
		cairo_pattern_t *bg_pattern;

		/* Set the pattern transform so as to correctly render a patterned
		 * background with the handle */
		ctk_style_context_get (context, state,
				       "background-image", &bg_pattern,
				       NULL);
		if (bg_pattern) {
			cairo_matrix_t ptm;

			cairo_matrix_init_translate (&ptm,
						     frame->priv->handle_rect.x,
						     frame->priv->handle_rect.y);
			cairo_matrix_scale (&ptm,
					    frame->priv->handle_rect.width,
					    frame->priv->handle_rect.height);
			cairo_pattern_set_matrix (bg_pattern, &ptm);
			cairo_pattern_destroy (bg_pattern);
		}
	}

	cairo_rectangle (cr,
		frame->priv->handle_rect.x,
		frame->priv->handle_rect.y,
		frame->priv->handle_rect.width,
		frame->priv->handle_rect.height);
	cairo_clip (cr);
	ctk_render_handle (context, cr,
			   0, 0,
			   ctk_widget_get_allocated_width (widget),
			   ctk_widget_get_allocated_height (widget));

	cairo_restore (cr);

	ctk_style_context_restore (context);

	return FALSE;
}

static void
cafe_panel_applet_frame_update_background_size (CafePanelAppletFrame *frame,
					   CtkAllocation    *old_allocation,
					   CtkAllocation    *new_allocation)
{
	PanelBackground *background;

	if (old_allocation->x      == new_allocation->x &&
	    old_allocation->y      == new_allocation->y &&
	    old_allocation->width  == new_allocation->width &&
	    old_allocation->height == new_allocation->height)
		return;

	background = &frame->priv->panel->toplevel->background;
	if (background->type == PANEL_BACK_NONE ||
	   (background->type == PANEL_BACK_COLOR && !background->has_alpha))
		return;

	cafe_panel_applet_frame_change_background (frame, background->type);
}

static void
cafe_panel_applet_frame_get_preferred_width(CtkWidget *widget, gint *minimal_width, gint *natural_width)
{
	CafePanelAppletFrame *frame;
	CtkBin           *bin;
	CtkWidget        *child;
	guint             border_width;

	frame = CAFE_PANEL_APPLET_FRAME (widget);
	bin = CTK_BIN (widget);

	if (!frame->priv->has_handle) {
		CTK_WIDGET_CLASS (cafe_panel_applet_frame_parent_class)->get_preferred_width (widget, minimal_width, natural_width);
		return;
	}

	child = ctk_bin_get_child (bin);
	if (child && ctk_widget_get_visible (child))
		ctk_widget_get_preferred_width (child, minimal_width, natural_width);

	border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));
	*minimal_width += border_width;
	*natural_width += border_width;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		*minimal_width += HANDLE_SIZE;
		*natural_width += HANDLE_SIZE;
		break;
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
cafe_panel_applet_frame_get_preferred_height(CtkWidget *widget, gint *minimal_height, gint *natural_height)
{
	CafePanelAppletFrame *frame;
	CtkBin           *bin;
	CtkWidget        *child;
	guint             border_width;

	frame = CAFE_PANEL_APPLET_FRAME (widget);
	bin = CTK_BIN (widget);

	if (!frame->priv->has_handle) {
		CTK_WIDGET_CLASS (cafe_panel_applet_frame_parent_class)->get_preferred_height (widget, minimal_height, natural_height);
		return;
	}

	child = ctk_bin_get_child (bin);
	if (child && ctk_widget_get_visible (child))
		ctk_widget_get_preferred_height (child, minimal_height, natural_height);

	border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));
	*minimal_height += border_width;
	*natural_height += border_width;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		*minimal_height += HANDLE_SIZE;
		*natural_height += HANDLE_SIZE;
		break;
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
cafe_panel_applet_frame_size_allocate (CtkWidget     *widget,
				  CtkAllocation *allocation)
{
	CafePanelAppletFrame *frame;
	CtkBin           *bin;
	CtkWidget        *child;
	CdkWindow        *window;
	CtkAllocation     new_allocation;
	CtkAllocation     old_allocation;
	CtkAllocation     widget_allocation;

	ctk_widget_get_allocation (widget, &widget_allocation);

	old_allocation.x      = widget_allocation.x;
	old_allocation.y      = widget_allocation.y;
	old_allocation.width  = widget_allocation.width;
	old_allocation.height = widget_allocation.height;

	frame = CAFE_PANEL_APPLET_FRAME (widget);
	bin = CTK_BIN (widget);

	if (!frame->priv->has_handle) {
		CTK_WIDGET_CLASS (cafe_panel_applet_frame_parent_class)->size_allocate (widget,  allocation);
		cafe_panel_applet_frame_update_background_size (frame, &old_allocation, allocation);
		return;
	}

	window = ctk_widget_get_window (widget);
	child = ctk_bin_get_child (bin);
	ctk_widget_set_allocation (widget, allocation);

	frame->priv->handle_rect.x = 0;
	frame->priv->handle_rect.y = 0;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		frame->priv->handle_rect.width  = HANDLE_SIZE;
		frame->priv->handle_rect.height = allocation->height;

		if (ctk_widget_get_direction (CTK_WIDGET (frame)) !=
		    CTK_TEXT_DIR_RTL) {
			frame->priv->handle_rect.x = 0;
			new_allocation.x = HANDLE_SIZE;
		} else {
			frame->priv->handle_rect.x = allocation->width - HANDLE_SIZE;
			new_allocation.x = 0;
		}

		new_allocation.y      = 0;
		new_allocation.width  = allocation->width - HANDLE_SIZE;
		new_allocation.height = allocation->height;
		break;
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		frame->priv->handle_rect.width  = allocation->width;
		frame->priv->handle_rect.height = HANDLE_SIZE;

		new_allocation.x      = 0;
		new_allocation.y      = HANDLE_SIZE;
		new_allocation.width  = allocation->width;
		new_allocation.height = allocation->height - HANDLE_SIZE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	new_allocation.width  = MAX (1, new_allocation.width);
	new_allocation.height = MAX (1, new_allocation.height);

	/* If the child allocation changed, that means that the frame is drawn
	 * in a new place, so we must redraw the entire widget.
	 */
	if (ctk_widget_get_mapped (widget) &&
	    (new_allocation.x != frame->priv->child_allocation.x ||
	     new_allocation.y != frame->priv->child_allocation.y ||
	     new_allocation.width != frame->priv->child_allocation.width ||
	     new_allocation.height != frame->priv->child_allocation.height))
		cdk_window_invalidate_rect (window, &widget_allocation, FALSE);

	if (ctk_widget_get_realized (widget)) {
		guint border_width;

		border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));
		cdk_window_move_resize (window,
			allocation->x + border_width,
			allocation->y + border_width,
			MAX (allocation->width - border_width * 2, 0),
			MAX (allocation->height - border_width * 2, 0));
	}

	if (child && ctk_widget_get_visible (child))
		ctk_widget_size_allocate (child, &new_allocation);

	frame->priv->child_allocation = new_allocation;

	cafe_panel_applet_frame_update_background_size (frame,
						   &old_allocation,
						   allocation);
}

static inline gboolean
button_event_in_rect (CdkEventButton *event,
		      CdkRectangle   *rect)
{
	if (event->x >= rect->x &&
	    event->x <= (rect->x + rect->width) &&
	    event->y >= rect->y &&
	    event->y <= (rect->y + rect->height))
		return TRUE;

	return FALSE;
}

static gboolean
cafe_panel_applet_frame_button_changed (CtkWidget      *widget,
					CdkEventButton *event)
{
	CafePanelAppletFrame *frame;
	gboolean              handled = FALSE;
	CdkDisplay *display;
	CdkSeat *seat;

	frame = CAFE_PANEL_APPLET_FRAME (widget);

	if (!frame->priv->has_handle)
		return handled;

	if (event->window != ctk_widget_get_window (widget))
		return FALSE;

	switch (event->button) {
	case 1:
	case 2:
		if (button_event_in_rect (event, &frame->priv->handle_rect)) {
			if (event->type == CDK_BUTTON_PRESS ||
			    event->type == CDK_2BUTTON_PRESS) {
				panel_widget_applet_drag_start (
					frame->priv->panel, CTK_WIDGET (frame),
					PW_DRAG_OFF_CURSOR, event->time);
				handled = TRUE;
			} else if (event->type == CDK_BUTTON_RELEASE) {
				panel_widget_applet_drag_end (frame->priv->panel);
				handled = TRUE;
			}
		}
		break;
	case 3:
		if (event->type == CDK_BUTTON_PRESS ||
		    event->type == CDK_2BUTTON_PRESS) {
			display = ctk_widget_get_display (widget);
			seat = cdk_display_get_default_seat (display);
			cdk_seat_ungrab (seat);

			CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->popup_menu (frame,
									  event->button,
									  event->time);

			handled = TRUE;
		} else if (event->type == CDK_BUTTON_RELEASE)
			handled = TRUE;
		break;
	default:
		break;
	}

	return handled;
}

static void
cafe_panel_applet_frame_finalize (GObject *object)
{
	CafePanelAppletFrame *frame = CAFE_PANEL_APPLET_FRAME (object);

	cafe_panel_applets_manager_factory_deactivate (frame->priv->iid);

	panel_lockdown_notify_remove (G_CALLBACK (cafe_panel_applet_frame_sync_menu_state),
				      frame);

	g_free (frame->priv->iid);
	frame->priv->iid = NULL;

	G_OBJECT_CLASS (cafe_panel_applet_frame_parent_class)->finalize (object);
}

static void
cafe_panel_applet_frame_class_init (CafePanelAppletFrameClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	CtkWidgetClass *widget_class = (CtkWidgetClass *) klass;

	gobject_class->finalize = cafe_panel_applet_frame_finalize;

	widget_class->draw                 = cafe_panel_applet_frame_draw;
	widget_class->get_preferred_width  = cafe_panel_applet_frame_get_preferred_width;
	widget_class->get_preferred_height = cafe_panel_applet_frame_get_preferred_height;
	widget_class->size_allocate        = cafe_panel_applet_frame_size_allocate;
	widget_class->button_press_event   = cafe_panel_applet_frame_button_changed;
	widget_class->button_release_event = cafe_panel_applet_frame_button_changed;
}

static void
cafe_panel_applet_frame_init (CafePanelAppletFrame *frame)
{
	frame->priv = cafe_panel_applet_frame_get_instance_private (frame);

	frame->priv->panel       = NULL;
	frame->priv->orientation = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info = NULL;
	frame->priv->has_handle  = FALSE;
}

static void
cafe_panel_applet_frame_init_properties (CafePanelAppletFrame *frame)
{
	CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->init_properties (frame);
}

void
cafe_panel_applet_frame_sync_menu_state (CafePanelAppletFrame *frame)
{
	PanelWidget *panel_widget;
	gboolean     locked_down;
	gboolean     locked;
	gboolean     lockable;
	gboolean     movable;
	gboolean     removable;

	panel_widget = PANEL_WIDGET (ctk_widget_get_parent (CTK_WIDGET (frame)));

	movable = cafe_panel_applet_can_freely_move (frame->priv->applet_info);
	removable = panel_profile_id_lists_are_writable ();
	lockable = cafe_panel_applet_lockable (frame->priv->applet_info);

	locked = panel_widget_get_applet_locked (panel_widget, CTK_WIDGET (frame));
	locked_down = panel_lockdown_get_locked_down ();

	CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->sync_menu_state (frame, movable, removable, lockable, locked, locked_down);
}

void
cafe_panel_applet_frame_change_orientation (CafePanelAppletFrame *frame,
				       PanelOrientation  orientation)
{
	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;
	CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->change_orientation (frame, orientation);
}

void
cafe_panel_applet_frame_change_size (CafePanelAppletFrame *frame,
				guint             size)
{
	CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->change_size (frame, size);
}

void
cafe_panel_applet_frame_change_background (CafePanelAppletFrame    *frame,
				      PanelBackgroundType  type)
{
	CtkWidget *parent;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	parent = ctk_widget_get_parent (CTK_WIDGET (frame));

	g_return_if_fail (PANEL_IS_WIDGET (parent));

	if (frame->priv->has_handle) {
		PanelBackground *background;
		background = &PANEL_WIDGET (parent)->toplevel->background;
		panel_background_apply_css (background, CTK_WIDGET (frame));
	}

	CAFE_PANEL_APPLET_FRAME_GET_CLASS (frame)->change_background (frame, type);
}

void
cafe_panel_applet_frame_set_panel (CafePanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
}

void
_cafe_panel_applet_frame_set_iid (CafePanelAppletFrame *frame,
			     const gchar      *iid)
{
	if (frame->priv->iid)
		g_free (frame->priv->iid);
	frame->priv->iid = g_strdup (iid);
}

void
_cafe_panel_applet_frame_activated (CafePanelAppletFrame           *frame,
			       CafePanelAppletFrameActivating *frame_act,
			       GError                     *error)
{
	AppletInfo *info;

	g_assert (frame->priv->iid != NULL);

	if (error != NULL) {
		g_warning ("Failed to load applet %s:\n%s",
			   frame->priv->iid, error->message);
		g_error_free (error);

		cafe_panel_applet_frame_loading_failed (frame->priv->iid,
						   frame_act->panel,
						   frame_act->id);
		cafe_panel_applet_frame_activating_free (frame_act);
		ctk_widget_destroy (CTK_WIDGET (frame));

		return;
	}

	frame->priv->panel = frame_act->panel;
	ctk_widget_show_all (CTK_WIDGET (frame));

	info = cafe_panel_applet_register (CTK_WIDGET (frame), CTK_WIDGET (frame),
				      NULL, frame->priv->panel,
				      frame_act->locked, frame_act->position,
				      frame_act->exactpos, PANEL_OBJECT_APPLET,
				      frame_act->id);
	frame->priv->applet_info = info;

	panel_widget_set_applet_size_constrained (frame->priv->panel,
						  CTK_WIDGET (frame), TRUE);

	cafe_panel_applet_frame_sync_menu_state (frame);
	cafe_panel_applet_frame_init_properties (frame);

	panel_lockdown_notify_add (G_CALLBACK (cafe_panel_applet_frame_sync_menu_state),
				   frame);

	cafe_panel_applet_stop_loading (frame_act->id);
	cafe_panel_applet_frame_activating_free (frame_act);
}

void
_cafe_panel_applet_frame_update_flags (CafePanelAppletFrame *frame,
				  gboolean          major,
				  gboolean          minor,
				  gboolean          has_handle)
{
	gboolean old_has_handle;

	panel_widget_set_applet_expandable (
		frame->priv->panel, CTK_WIDGET (frame), major, minor);

	old_has_handle = frame->priv->has_handle;
	frame->priv->has_handle = has_handle;

	if (!old_has_handle && frame->priv->has_handle) {
		/* we've added an handle, so we need to get the background for
		 * it */
		PanelBackground *background;

		background = &frame->priv->panel->toplevel->background;
		cafe_panel_applet_frame_change_background (frame, background->type);
	}
}

void
_cafe_panel_applet_frame_update_size_hints (CafePanelAppletFrame *frame,
				       gint             *size_hints,
				       guint             n_elements)
{
	if (frame->priv->has_handle) {
		gint extra_size = HANDLE_SIZE + 1;
		gint i;

		for (i = 0; i < n_elements; i++)
			size_hints[i] += extra_size;
	}

	/* It takes the ownership of size-hints array */
	panel_widget_set_applet_size_hints (frame->priv->panel,
					    CTK_WIDGET (frame),
					    size_hints,
					    n_elements);
}

char *
_cafe_panel_applet_frame_get_background_string (CafePanelAppletFrame *frame,
						PanelWidget          *panel,
						PanelBackgroundType   type G_GNUC_UNUSED)
{
	CtkAllocation allocation;
	int x;
	int y;

	ctk_widget_get_allocation (CTK_WIDGET (frame), &allocation);

	x = allocation.x;
	y = allocation.y;

	if (frame->priv->has_handle) {
		switch (frame->priv->orientation) {
		case PANEL_ORIENTATION_TOP:
		case PANEL_ORIENTATION_BOTTOM:
			if (ctk_widget_get_direction (CTK_WIDGET (frame)) !=
			    CTK_TEXT_DIR_RTL)
				x += frame->priv->handle_rect.width;
			break;
		case PANEL_ORIENTATION_LEFT:
		case PANEL_ORIENTATION_RIGHT:
			y += frame->priv->handle_rect.height;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	return panel_background_make_string (&panel->toplevel->background, x, y);
}

static void
cafe_panel_applet_frame_reload_response (CtkWidget        *dialog,
				    int               response,
				    CafePanelAppletFrame *frame)
{
	AppletInfo *info;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	if (!frame->priv->iid || !frame->priv->panel) {
		g_object_unref (frame);
		ctk_widget_destroy (dialog);
		return;
	}

	info = frame->priv->applet_info;

	if (response == PANEL_RESPONSE_RELOAD) {
		PanelWidget *panel;
		char        *iid;
		char        *id = NULL;
		int          position = -1;
		gboolean     locked = FALSE;

		panel = frame->priv->panel;
		iid   = g_strdup (frame->priv->iid);

		if (info) {
			id = g_strdup (info->id);
			position  = cafe_panel_applet_get_position (info);
			locked = panel_widget_get_applet_locked (panel, info->widget);
			cafe_panel_applet_clean (info);
		}

		cafe_panel_applet_frame_load (iid, panel, locked,
					 position, TRUE, id);

		g_free (iid);
		g_free (id);

	} else if (response == PANEL_RESPONSE_DELETE) {
		/* if we can't write to applets list we can't really delete
		   it, so we'll just ignore this.  FIXME: handle this
		   more correctly I suppose. */
		if (panel_profile_id_lists_are_writable () && info)
			panel_profile_delete_object (info);
	}

	g_object_unref (frame);
	ctk_widget_destroy (dialog);
}

void
_cafe_panel_applet_frame_applet_broken (CafePanelAppletFrame *frame)
{
	CtkWidget  *dialog;
	CdkScreen  *screen;
	const char *applet_name = NULL;
	char       *dialog_txt;

	screen = ctk_widget_get_screen (CTK_WIDGET (frame));

#ifdef HAVE_X11
	if (is_using_x11 () && xstuff_is_display_dead ())
		return;
#endif

	if (frame->priv->iid) {
		CafePanelAppletInfo *info;

		info = (CafePanelAppletInfo *)cafe_panel_applets_manager_get_applet_info (frame->priv->iid);
		applet_name = cafe_panel_applet_info_get_name (info);
	}

	if (applet_name)
		dialog_txt = g_strdup_printf (_("\"%s\" has quit unexpectedly"), applet_name);
	else
		dialog_txt = g_strdup (_("Panel object has quit unexpectedly"));

	dialog = ctk_message_dialog_new (NULL, CTK_DIALOG_DESTROY_WITH_PARENT,
					 CTK_MESSAGE_WARNING, CTK_BUTTONS_NONE,
					 dialog_txt, applet_name ? applet_name : NULL);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  _("If you reload a panel object, it will automatically "
						    "be added back to the panel."));

	ctk_container_set_border_width (CTK_CONTAINER (dialog), 6);

	if (panel_profile_id_lists_are_writable ()) {
		ctk_dialog_add_buttons (CTK_DIALOG (dialog),
					_("D_elete"), PANEL_RESPONSE_DELETE,
					_("_Don't Reload"), PANEL_RESPONSE_DONT_RELOAD,
					_("_Reload"), PANEL_RESPONSE_RELOAD,
					NULL);
	} else {
		ctk_dialog_add_buttons (CTK_DIALOG (dialog),
					_("_Don't Reload"), PANEL_RESPONSE_DONT_RELOAD,
					_("_Reload"), PANEL_RESPONSE_RELOAD,
					NULL);
	}

	ctk_dialog_set_default_response (CTK_DIALOG (dialog),
					 PANEL_RESPONSE_RELOAD);

	ctk_window_set_screen (CTK_WINDOW (dialog), screen);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (cafe_panel_applet_frame_reload_response),
			  g_object_ref (frame));

	panel_widget_register_open_dialog (frame->priv->panel, dialog);
	ctk_window_set_urgency_hint (CTK_WINDOW (dialog), TRUE);
	/* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
	ctk_window_set_title (CTK_WINDOW (dialog), _("Error"));

	ctk_widget_show (dialog);

#ifdef HAVE_X11
	if (CDK_IS_X11_DISPLAY (ctk_widget_get_display (dialog)))
		ctk_window_present_with_time (CTK_WINDOW (dialog),
					      cdk_x11_get_server_time (ctk_widget_get_window (CTK_WIDGET (dialog))));
	else
#endif
	{ // Not using X11
		ctk_window_present(CTK_WINDOW (dialog));
	}

	g_free (dialog_txt);
}

void
_cafe_panel_applet_frame_applet_remove (CafePanelAppletFrame *frame)
{
	AppletInfo *info;

	if (!frame->priv->applet_info)
		return;

	info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;

	panel_profile_delete_object (info);
}

void
_cafe_panel_applet_frame_applet_move (CafePanelAppletFrame *frame)
{
	CtkWidget *widget = CTK_WIDGET (frame);
	CtkWidget *parent = ctk_widget_get_parent (widget);

	if (!PANEL_IS_WIDGET (parent))
		return;

	panel_widget_applet_drag_start (PANEL_WIDGET (parent),
					widget,
					PW_DRAG_OFF_CENTER,
					CDK_CURRENT_TIME);
}

void
_cafe_panel_applet_frame_applet_lock (CafePanelAppletFrame *frame,
				 gboolean          locked)
{
	PanelWidget *panel_widget = PANEL_WIDGET (ctk_widget_get_parent (CTK_WIDGET (frame)));

	if (panel_widget_get_applet_locked (panel_widget, CTK_WIDGET (frame)) == locked)
		return;

	cafe_panel_applet_toggle_locked (frame->priv->applet_info);
}

/* Generic methods */

static GSList *no_reload_applets = NULL;

enum {
	LOADING_FAILED_RESPONSE_DONT_DELETE,
	LOADING_FAILED_RESPONSE_DELETE
};

static void
cafe_panel_applet_frame_activating_free (CafePanelAppletFrameActivating *frame_act)
{
	g_free (frame_act->id);
	g_slice_free (CafePanelAppletFrameActivating, frame_act);
}

CdkScreen *
panel_applet_frame_activating_get_screen (CafePanelAppletFrameActivating *frame_act)
{
    return ctk_widget_get_screen (CTK_WIDGET(frame_act->panel));
}

PanelOrientation
cafe_panel_applet_frame_activating_get_orientation(CafePanelAppletFrameActivating *frame_act)
{
	return panel_widget_get_applet_orientation(frame_act->panel);
}

guint32
cafe_panel_applet_frame_activating_get_size (CafePanelAppletFrameActivating *frame_act)
{
	return frame_act->panel->sz;
}

gboolean
cafe_panel_applet_frame_activating_get_locked (CafePanelAppletFrameActivating *frame_act)
{
	return frame_act->locked;
}

gboolean
cafe_panel_applet_frame_activating_get_locked_down (CafePanelAppletFrameActivating *frame_act G_GNUC_UNUSED)
{
	return panel_lockdown_get_locked_down ();
}

gchar *
cafe_panel_applet_frame_activating_get_conf_path (CafePanelAppletFrameActivating *frame_act)
{
	return g_strdup_printf (CAFE_PANEL_APPLET_PREFS_PATH, frame_act->id);
}

static void
cafe_panel_applet_frame_loading_failed_response (CtkWidget *dialog,
					    guint      response,
					    char      *id)
{
	ctk_widget_destroy (dialog);

	if (response == LOADING_FAILED_RESPONSE_DELETE &&
	    !panel_lockdown_get_locked_down () &&
	    panel_profile_id_lists_are_writable ()) {
		GSList *item;

		item = g_slist_find_custom (no_reload_applets, id,
					    (GCompareFunc) strcmp);
		if (item) {
			g_free (item->data);
			no_reload_applets = g_slist_delete_link (no_reload_applets,
								 item);
		}

		panel_profile_remove_from_list (PANEL_GSETTINGS_OBJECTS, id);
	}

	g_free (id);
}

static void
cafe_panel_applet_frame_loading_failed (const char  *iid,
				   PanelWidget *panel,
				   const char  *id)
{
	CtkWidget *dialog;
	char      *problem_txt;
	gboolean   locked_down;

	no_reload_applets = g_slist_prepend (no_reload_applets,
					     g_strdup (id));

	locked_down = panel_lockdown_get_locked_down ();

	problem_txt = g_strdup_printf (_("The panel encountered a problem "
					 "while loading \"%s\"."),
				       iid);

	dialog = ctk_message_dialog_new (NULL, 0,
					 locked_down ? CTK_MESSAGE_INFO : CTK_MESSAGE_WARNING,
					 CTK_BUTTONS_NONE,
					 "%s", problem_txt);
	g_free (problem_txt);

	if (locked_down) {
		panel_dialog_add_button (CTK_DIALOG (dialog),
					 _("_OK"), "ctk-ok", LOADING_FAILED_RESPONSE_DONT_DELETE);
	} else {
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
					_("Do you want to delete the applet "
					  "from your configuration?"));

		ctk_dialog_add_button (CTK_DIALOG (dialog),
				       PANEL_STOCK_DONT_DELETE, LOADING_FAILED_RESPONSE_DONT_DELETE);

		panel_dialog_add_button (CTK_DIALOG (dialog),
					 _("_Delete"), "edit-delete", LOADING_FAILED_RESPONSE_DELETE);
	}

	ctk_dialog_set_default_response (CTK_DIALOG (dialog),
					 LOADING_FAILED_RESPONSE_DONT_DELETE);

	ctk_window_set_screen (CTK_WINDOW (dialog),
			       ctk_window_get_screen (CTK_WINDOW (panel->toplevel)));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (cafe_panel_applet_frame_loading_failed_response),
			  g_strdup (id));

	panel_widget_register_open_dialog (panel, dialog);
	ctk_window_set_urgency_hint (CTK_WINDOW (dialog), TRUE);
	/* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
	ctk_window_set_title (CTK_WINDOW (dialog), _("Error"));

	ctk_widget_show_all (dialog);

	/* Note: this call will free the memory for id, so the variable should
	 * not get accessed afterwards. */
	cafe_panel_applet_stop_loading (id);
}

static void
cafe_panel_applet_frame_load (const gchar *iid,
			 PanelWidget *panel,
			 gboolean     locked,
			 int          position,
			 gboolean     exactpos,
			 const char  *id)
{
	CafePanelAppletFrameActivating *frame_act;

	g_return_if_fail (iid != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (id != NULL);

	if (g_slist_find_custom (no_reload_applets, id,
				 (GCompareFunc) strcmp)) {
		cafe_panel_applet_stop_loading (id);
		return;
	}

	if (panel_lockdown_is_applet_disabled (iid)) {
		cafe_panel_applet_stop_loading (id);
		return;
	}

	frame_act = g_slice_new0 (CafePanelAppletFrameActivating);
	frame_act->locked   = locked;
	frame_act->panel    = panel;
	frame_act->position = position;
	frame_act->exactpos = exactpos;
	frame_act->id       = g_strdup (id);

	if (!cafe_panel_applets_manager_load_applet (iid, frame_act)) {
		cafe_panel_applet_frame_loading_failed (iid, panel, id);
		cafe_panel_applet_frame_activating_free (frame_act);
	}
}

void
cafe_panel_applet_frame_load_from_gsettings (PanelWidget *panel_widget,
				    gboolean     locked,
				    int          position,
				    const char  *id)
{
	GSettings *settings;
	gchar *path;
	gchar *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	path = g_strdup_printf (PANEL_OBJECT_PATH "%s/", id);
	settings = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
	applet_iid = g_settings_get_string (settings, PANEL_OBJECT_APPLET_IID_KEY);
	g_object_unref (settings);
	g_free (path);

	if (!applet_iid) {
		cafe_panel_applet_stop_loading (id);
		return;
	}

	cafe_panel_applet_frame_load (applet_iid, panel_widget,
				 locked, position, TRUE, id);

	g_free (applet_iid);
}

void
cafe_panel_applet_frame_create (PanelToplevel *toplevel,
			   int            position,
			   const char    *iid)
{
	GSettings   *settings;
	gchar       *path;
	char        *id;

	g_return_if_fail (iid != NULL);

	id = panel_profile_prepare_object (PANEL_OBJECT_APPLET, toplevel, position, FALSE);

	path = g_strdup_printf (PANEL_OBJECT_PATH "%s/", id);
	settings = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
	g_settings_set_string (settings, PANEL_OBJECT_APPLET_IID_KEY, iid);

	panel_profile_add_to_list (PANEL_GSETTINGS_OBJECTS, id);

	g_free (id);
	g_free (path);
	g_object_unref (settings);
}
