/*
 * wayland-backend.c: Support for running on Wayland compositors
 *
 * Copyright (C) 2019 William Wold
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
 *	William Wold <wm@wmww.sh>
 */

#include <config.h>

#include <ctk-layer-shell.h>

#include "wayland-backend.h"

void
wayland_panel_toplevel_init (PanelToplevel* toplevel)
{
	CtkWindow* window;

	window = CTK_WINDOW (toplevel);
	ctk_layer_init_for_window (window);
	ctk_layer_set_layer (window, CTK_LAYER_SHELL_LAYER_TOP);
	ctk_layer_set_namespace (window, "panel");
	ctk_layer_auto_exclusive_zone_enable (window);
	wayland_panel_toplevel_update_placement (toplevel);
}

void
wayland_panel_toplevel_update_placement (PanelToplevel* toplevel)
{
	CtkWindow* window;
	gboolean expand;
	PanelOrientation orientation;
	gboolean anchor[CTK_LAYER_SHELL_EDGE_ENTRY_NUMBER];

	window = CTK_WINDOW (toplevel);
	expand = panel_toplevel_get_expand (toplevel);
	orientation = panel_toplevel_get_orientation (toplevel);
	for (int i = 0; i < CTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++)
		anchor[i] = expand;

	switch (orientation) {
	case PANEL_ORIENTATION_LEFT:
		anchor[CTK_LAYER_SHELL_EDGE_LEFT] = TRUE;
		anchor[CTK_LAYER_SHELL_EDGE_RIGHT] = FALSE;
		break;
	case PANEL_ORIENTATION_RIGHT:
		anchor[CTK_LAYER_SHELL_EDGE_RIGHT] = TRUE;
		anchor[CTK_LAYER_SHELL_EDGE_LEFT] = FALSE;
		break;
	case PANEL_ORIENTATION_TOP:
		anchor[CTK_LAYER_SHELL_EDGE_TOP] = TRUE;
		anchor[CTK_LAYER_SHELL_EDGE_BOTTOM] = FALSE;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		anchor[CTK_LAYER_SHELL_EDGE_BOTTOM] = TRUE;
		anchor[CTK_LAYER_SHELL_EDGE_TOP] = FALSE;
		break;
	default:
		g_warning ("Invalid panel orientation %d", orientation);
	}

	for (int i = 0; i < CTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++)
		ctk_layer_set_anchor (window, i, anchor[i]);
}
