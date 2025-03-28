/*
 * panel-menu-bar.c: panel Applications/Places/Desktop menu bar
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Vincent Untz
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
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>

#include "panel-menu-bar.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-show.h>

#include "panel-util.h"
#include "panel-background.h"
#include "panel-action-button.h"
#include "applet.h"
#include "menu.h"
#include "panel-menu-items.h"
#include "panel-globals.h"
#include "panel-profile.h"
#include "panel-lockdown.h"
#include "panel-stock-icons.h"
#include "panel-typebuiltins.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

struct _PanelMenuBarPrivate {
	AppletInfo* info;
	PanelWidget* panel;

	CtkWidget* applications_menu;
	CtkWidget* applications_item;
	CtkWidget* places_item;
	CtkWidget* desktop_item;

	GSettings* settings;

	PanelOrientation orientation;
};

enum {
	PROP_0,
	PROP_ORIENTATION,
};

G_DEFINE_TYPE_WITH_PRIVATE (PanelMenuBar, panel_menu_bar, CTK_TYPE_MENU_BAR)

static void panel_menu_bar_update_text_gravity(PanelMenuBar* menubar);

static gboolean panel_menu_bar_reinit_tooltip (CtkWidget    *widget G_GNUC_UNUSED,
					       PanelMenuBar *menubar)
{
	g_object_set(menubar->priv->applications_item, "has-tooltip", TRUE, NULL);
	g_object_set(menubar->priv->places_item, "has-tooltip", TRUE, NULL);
	g_object_set(menubar->priv->desktop_item, "has-tooltip", TRUE, NULL);

	return FALSE;
}

static gboolean panel_menu_bar_hide_tooltip_and_focus(CtkWidget* widget, PanelMenuBar* menubar)
{
	/* remove focus that would be drawn on the currently focused child of
	 * the toplevel. See bug#308632. */
	ctk_window_set_focus(CTK_WINDOW(menubar->priv->panel->toplevel), NULL);

	g_object_set(widget, "has-tooltip", FALSE, NULL);

	return FALSE;
}

static void panel_menu_bar_setup_tooltip(PanelMenuBar* menubar)
{
	panel_util_set_tooltip_text(menubar->priv->applications_item, _("Browse and run installed applications"));
	panel_util_set_tooltip_text(menubar->priv->places_item, _("Access documents, folders and network places"));
	panel_util_set_tooltip_text(menubar->priv->desktop_item, _("Change desktop appearance and behavior, get help, or log out"));

	//FIXME: this doesn't handle the right-click case. Sigh.
	/* Hide tooltip if a menu is activated */
	g_signal_connect(menubar->priv->applications_item, "activate", G_CALLBACK (panel_menu_bar_hide_tooltip_and_focus), menubar);
	g_signal_connect(menubar->priv->places_item, "activate", G_CALLBACK (panel_menu_bar_hide_tooltip_and_focus), menubar);
	g_signal_connect(menubar->priv->desktop_item, "activate", G_CALLBACK (panel_menu_bar_hide_tooltip_and_focus), menubar);

	/* Reset tooltip when the menu bar is not used */
	g_signal_connect(CTK_MENU_SHELL (menubar), "deactivate", G_CALLBACK (panel_menu_bar_reinit_tooltip), menubar);
}

static void panel_menu_bar_update_visibility (GSettings    *settings,
					      gchar        *key G_GNUC_UNUSED,
					      PanelMenuBar *menubar)
{
	CtkWidget* image;
	gchar *str;
	CtkIconSize icon_size;
	gint icon_height;

	if (!CTK_IS_WIDGET (menubar))
		return;

	ctk_widget_set_visible (CTK_WIDGET (menubar->priv->applications_item), g_settings_get_boolean (settings, PANEL_MENU_BAR_SHOW_APPLICATIONS_KEY));
	ctk_widget_set_visible (CTK_WIDGET (menubar->priv->places_item), g_settings_get_boolean (settings, PANEL_MENU_BAR_SHOW_PLACES_KEY));
	ctk_widget_set_visible (CTK_WIDGET (menubar->priv->desktop_item), g_settings_get_boolean (settings, PANEL_MENU_BAR_SHOW_DESKTOP_KEY));

	if (g_settings_get_boolean (settings, PANEL_MENU_BAR_SHOW_ICON_KEY))
	{
		str = g_settings_get_string (settings, PANEL_MENU_BAR_ICON_NAME_KEY);
		icon_size = panel_menu_bar_icon_get_size ();
		ctk_icon_size_lookup (icon_size, NULL, &icon_height);
		if (str != NULL && str[0] != 0)
			image = ctk_image_new_from_icon_name(str, icon_size);
		else
			image = ctk_image_new_from_icon_name(PANEL_ICON_MAIN_MENU, icon_size);
		ctk_image_menu_item_set_image (CTK_IMAGE_MENU_ITEM (menubar->priv->applications_item), image);
		ctk_image_set_pixel_size (CTK_IMAGE (image), icon_height);
		g_free (str);
	}
	else
		ctk_image_menu_item_set_image (CTK_IMAGE_MENU_ITEM (menubar->priv->applications_item), NULL);
}

static void panel_menu_bar_init(PanelMenuBar* menubar)
{
	CtkCssProvider *provider;

	menubar->priv = panel_menu_bar_get_instance_private(menubar);

	provider = ctk_css_provider_new ();
	ctk_css_provider_load_from_data (provider,
		"PanelMenuBar {\n"
		" border-width: 0px;\n"
		"}",
		-1, NULL);
	ctk_style_context_add_provider (ctk_widget_get_style_context (CTK_WIDGET (menubar)),
		CTK_STYLE_PROVIDER (provider),
		CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	menubar->priv->info = NULL;

	menubar->priv->settings = g_settings_new (PANEL_MENU_BAR_SCHEMA);

	menubar->priv->applications_menu = create_applications_menu("cafe-applications.menu", NULL, TRUE);

	menubar->priv->applications_item = panel_image_menu_item_new();
	ctk_menu_item_set_label(CTK_MENU_ITEM(menubar->priv->applications_item), _("Applications"));

	ctk_menu_item_set_submenu(CTK_MENU_ITEM(menubar->priv->applications_item), menubar->priv->applications_menu);
	ctk_menu_shell_append(CTK_MENU_SHELL(menubar), menubar->priv->applications_item);

	menubar->priv->places_item = panel_place_menu_item_new(FALSE);
	ctk_menu_shell_append(CTK_MENU_SHELL(menubar), menubar->priv->places_item);

	menubar->priv->desktop_item = panel_desktop_menu_item_new(FALSE, TRUE);
	ctk_menu_shell_append(CTK_MENU_SHELL(menubar), menubar->priv->desktop_item);

	panel_menu_bar_setup_tooltip(menubar);

	panel_menu_bar_update_visibility(menubar->priv->settings, NULL, menubar);
	g_signal_connect(menubar->priv->settings, "changed", G_CALLBACK (panel_menu_bar_update_visibility), menubar);

	panel_menu_bar_update_text_gravity(menubar);
	g_signal_connect(menubar, "screen-changed", G_CALLBACK(panel_menu_bar_update_text_gravity), NULL);
}

static void panel_menu_bar_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
	PanelMenuBar* menubar;

	g_return_if_fail(PANEL_IS_MENU_BAR(object));

	menubar = PANEL_MENU_BAR (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			g_value_set_enum(value, menubar->priv->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void panel_menu_bar_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
	PanelMenuBar* menubar;

	g_return_if_fail (PANEL_IS_MENU_BAR (object));

	menubar = PANEL_MENU_BAR(object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			panel_menu_bar_set_orientation(menubar, g_value_get_enum(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void panel_menu_bar_parent_set (CtkWidget *widget,
				       CtkWidget *previous_parent G_GNUC_UNUSED)
{
	PanelMenuBar* menubar = PANEL_MENU_BAR(widget);
	CtkWidget* parent;

	parent = ctk_widget_get_parent(widget);

	g_assert(!parent || PANEL_IS_WIDGET(parent));

	menubar->priv->panel = (PanelWidget*) parent;

	if (menubar->priv->applications_menu)
	{
		cafe_panel_applet_menu_set_recurse(CTK_MENU(menubar->priv->applications_menu), "menu_panel", menubar->priv->panel);
	}

	if (menubar->priv->places_item)
	{
		panel_place_menu_item_set_panel(menubar->priv->places_item, menubar->priv->panel);
	}

	if (menubar->priv->desktop_item)
	{
		panel_desktop_menu_item_set_panel(menubar->priv->desktop_item, menubar->priv->panel);
	}
}

static void panel_menu_bar_size_allocate(CtkWidget* widget, CtkAllocation* allocation)
{
	CtkAllocation old_allocation;
	CtkAllocation widget_allocation;
	PanelBackground* background;

	ctk_widget_get_allocation(widget, &widget_allocation);

	old_allocation.x = widget_allocation.x;
	old_allocation.y = widget_allocation.y;
	old_allocation.width = widget_allocation.width;
	old_allocation.height = widget_allocation.height;

	CTK_WIDGET_CLASS(panel_menu_bar_parent_class)->size_allocate (widget, allocation);

	if (old_allocation.x == allocation->x && old_allocation.y == allocation->y && old_allocation.width == allocation->width && old_allocation.height == allocation->height)
	{
		return;
	}

	background = &PANEL_MENU_BAR(widget)->priv->panel->toplevel->background;
	if (background->type == PANEL_BACK_NONE || (background->type == PANEL_BACK_COLOR && !background->has_alpha))
	{
		return;
	}

	panel_menu_bar_change_background(PANEL_MENU_BAR(widget));
}

static void panel_menu_bar_finalize (GObject* object)
{
	PanelMenuBar* menubar;

	menubar = PANEL_MENU_BAR (object);

	if (menubar->priv->settings != NULL)
	{
		g_object_unref (menubar->priv->settings);
		menubar->priv->settings = NULL;
	}
}

static void panel_menu_bar_class_init(PanelMenuBarClass* klass)
{
	GObjectClass* gobject_class = (GObjectClass*) klass;
	CtkWidgetClass* widget_class  = (CtkWidgetClass*) klass;

	gobject_class->get_property = panel_menu_bar_get_property;
	gobject_class->set_property = panel_menu_bar_set_property;
	gobject_class->finalize = panel_menu_bar_finalize;

	widget_class->parent_set = panel_menu_bar_parent_set;
	widget_class->size_allocate = panel_menu_bar_size_allocate;

	g_object_class_install_property(gobject_class, PROP_ORIENTATION, g_param_spec_enum("orientation", "Orientation", "The PanelMenuBar orientation", PANEL_TYPE_ORIENTATION, PANEL_ORIENTATION_TOP, G_PARAM_READWRITE));
}

static gboolean panel_menu_bar_on_draw (CtkWidget* widget, cairo_t* cr, gpointer data)
{
	PanelMenuBar* menubar = data;

	if (ctk_widget_has_focus(CTK_WIDGET(menubar))) {
		CtkStyleContext *context;

		context = ctk_widget_get_style_context (widget);
		ctk_style_context_save (context);
		ctk_style_context_set_state (context, ctk_widget_get_state_flags (widget));

		cairo_save (cr);
		ctk_render_focus (context, cr,
				  0, 0,
				  ctk_widget_get_allocated_width (widget),
				  ctk_widget_get_allocated_height (widget));
		cairo_restore (cr);

		ctk_style_context_restore (context);
        }
	return FALSE;
}

static void panel_menu_bar_load(PanelWidget* panel, gboolean locked, int position, gboolean exactpos, const char* id)
{
	PanelMenuBar* menubar;
	CtkSettings* settings;

	g_return_if_fail (panel != NULL);

	menubar = g_object_new(PANEL_TYPE_MENU_BAR, NULL);

	menubar->priv->info = cafe_panel_applet_register(CTK_WIDGET(menubar), NULL, NULL, panel, locked, position, exactpos, PANEL_OBJECT_MENU_BAR, id);

	if (!menubar->priv->info)
	{
		ctk_widget_destroy(CTK_WIDGET(menubar));
		return;
	}

	settings = ctk_settings_get_for_screen (ctk_widget_get_screen (CTK_WIDGET (panel)));
	g_object_set (settings, "ctk-shell-shows-app-menu", FALSE, "ctk-shell-shows-menubar", FALSE, NULL);

	cafe_panel_applet_add_callback(menubar->priv->info, "help", "help-browser", _("_Help"), NULL);

	/* Menu editors */
	if (!panel_lockdown_get_locked_down () && (panel_is_program_in_path("mozo") || panel_is_program_in_path("menulibre")))
	{
		cafe_panel_applet_add_callback (menubar->priv->info, "edit", "document-properties", _("_Edit Menus"), NULL);
	}

	g_signal_connect_after(menubar, "focus-in-event", G_CALLBACK(ctk_widget_queue_draw), menubar);
	g_signal_connect_after(menubar, "focus-out-event", G_CALLBACK(ctk_widget_queue_draw), menubar);
	g_signal_connect_after(menubar, "draw", G_CALLBACK(panel_menu_bar_on_draw), menubar);

	ctk_widget_set_can_focus(CTK_WIDGET(menubar), TRUE);

	panel_widget_set_applet_expandable(panel, CTK_WIDGET(menubar), FALSE, TRUE);
	panel_menu_bar_update_visibility(menubar->priv->settings, NULL, menubar);
}

void panel_menu_bar_load_from_gsettings (PanelWidget* panel, gboolean locked, int position, gboolean exactpos, const char* id)
{
	panel_menu_bar_load(panel, locked, position, exactpos, id);
}

void panel_menu_bar_create(PanelToplevel* toplevel, int position)
{
	char* id;

	id = panel_profile_prepare_object(PANEL_OBJECT_MENU_BAR, toplevel, position, FALSE);
	panel_profile_add_to_list(PANEL_GSETTINGS_OBJECTS, id);
	g_free(id);
}

void panel_menu_bar_invoke_menu(PanelMenuBar* menubar, const char* callback_name)
{
	CdkScreen* screen;

	g_return_if_fail(PANEL_IS_MENU_BAR(menubar));
	g_return_if_fail(callback_name != NULL);

	screen = ctk_widget_get_screen(CTK_WIDGET(menubar));

	if (!strcmp(callback_name, "help"))
	{
		panel_show_help(screen, "cafe-user-guide", "menubar", NULL);

	}
	else if (!strcmp(callback_name, "edit"))
	{
		if (panel_is_program_in_path("menulibre"))
			panel_launch_desktop_file_with_fallback("menulibre.desktop", "menulibre", screen, NULL);
		else
			panel_launch_desktop_file_with_fallback("mozo.desktop", "mozo", screen, NULL);
	}
}

void panel_menu_bar_popup_menu (PanelMenuBar *menubar,
				guint32       activate_time G_GNUC_UNUSED)
{
	CtkMenu* menu;
	CtkMenuShell* menu_shell;

	g_return_if_fail(PANEL_IS_MENU_BAR(menubar));

	menu = CTK_MENU(menubar->priv->applications_menu);

	/*
	 * We need to call _ctk_menu_shell_activate() here as is done in
	 * window_key_press_handler in ctkmenubar.c which pops up menu
	 * when F10 is pressed.
	 *
	 * As that function is private its code is replicated here.
	 */
	menu_shell = CTK_MENU_SHELL(menubar);

	ctk_menu_shell_select_item(menu_shell, ctk_menu_get_attach_widget(menu));
}

void panel_menu_bar_change_background(PanelMenuBar* menubar)
{
	panel_background_apply_css(&menubar->priv->panel->toplevel->background, CTK_WIDGET(menubar));
}

static void set_item_text_gravity(CtkWidget* item)
{
	CtkWidget* label;
	PangoLayout* layout;
	PangoContext* context;

	label = ctk_bin_get_child(CTK_BIN(item));
	layout = ctk_label_get_layout(CTK_LABEL(label));
	context = pango_layout_get_context(layout);
	pango_context_set_base_gravity(context, PANGO_GRAVITY_AUTO);
}

static void panel_menu_bar_update_text_gravity(PanelMenuBar* menubar)
{
	set_item_text_gravity(menubar->priv->applications_item);
	set_item_text_gravity(menubar->priv->places_item);
	set_item_text_gravity(menubar->priv->desktop_item);
}

static void set_item_text_angle_and_alignment(CtkWidget* item, double text_angle, float xalign, float yalign)
{
	CtkWidget *label;

	label = ctk_bin_get_child (CTK_BIN (item));

	ctk_label_set_angle (CTK_LABEL (label), text_angle);

	ctk_label_set_xalign (CTK_LABEL (label), xalign);
	ctk_label_set_yalign (CTK_LABEL (label), yalign);
}

static void panel_menu_bar_update_orientation(PanelMenuBar* menubar)
{
	CtkPackDirection pack_direction;
	double text_angle;
	float text_xalign;
	float text_yalign;

	pack_direction = CTK_PACK_DIRECTION_LTR;
	text_angle = 0.0;
	text_xalign = 0.0;
	text_yalign = 0.5;

	switch (menubar->priv->orientation)
	{
		case PANEL_ORIENTATION_TOP:
		case PANEL_ORIENTATION_BOTTOM:
			break;
		case PANEL_ORIENTATION_LEFT:
			pack_direction = CTK_PACK_DIRECTION_BTT;
			text_angle = 90.0;
			text_xalign = 0.5;
			text_yalign = 0.0;
			break;
		case PANEL_ORIENTATION_RIGHT:
			pack_direction = CTK_PACK_DIRECTION_TTB;
			text_angle = 270.0;
			text_xalign = 0.5;
			text_yalign = 0.0;
			break;
		default:
			g_assert_not_reached();
			break;
	}

	ctk_menu_bar_set_pack_direction(CTK_MENU_BAR(menubar), pack_direction);
	ctk_menu_bar_set_child_pack_direction(CTK_MENU_BAR(menubar), pack_direction);

	set_item_text_angle_and_alignment(menubar->priv->applications_item, text_angle, text_xalign, text_yalign);
	set_item_text_angle_and_alignment(menubar->priv->places_item, text_angle, text_xalign, text_yalign);
	set_item_text_angle_and_alignment(menubar->priv->desktop_item, text_angle, text_xalign, text_yalign);
}

void panel_menu_bar_set_orientation(PanelMenuBar* menubar, PanelOrientation orientation)
{
	g_return_if_fail(PANEL_IS_MENU_BAR(menubar));

	if (menubar->priv->orientation == orientation)
	{
		return;
	}

	menubar->priv->orientation = orientation;

	panel_menu_bar_update_orientation(menubar);

	g_object_notify(G_OBJECT(menubar), "orientation");
}

PanelOrientation panel_menu_bar_get_orientation(PanelMenuBar* menubar)
{
	g_return_val_if_fail(PANEL_IS_MENU_BAR(menubar), 0);

	return menubar->priv->orientation;
}
