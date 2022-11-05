/*
 * panel-properties-dialog.c:
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

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-properties-dialog.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-ctk.h>
#include <libpanel-util/panel-icon-chooser.h>
#include <libpanel-util/panel-show.h>

#include "panel-profile.h"
#include "panel-schemas.h"
#include "panel-util.h"
#include "panel-globals.h"
#include "panel-icon-names.h"

typedef struct {
	PanelToplevel *toplevel;
	GSettings     *settings;
	GSettings     *background_settings;

	CtkWidget     *properties_dialog;

	CtkWidget     *general_table;
	CtkWidget     *general_vbox;
	CtkWidget     *orientation_combo;
	CtkWidget     *orientation_label;
	CtkWidget     *size_widgets;
	CtkWidget     *size_spin;
	CtkWidget     *size_label;
	CtkWidget     *size_label_pixels;
	CtkWidget     *icon_align;
	CtkWidget     *icon_chooser;
	CtkWidget     *icon_label;
  	CtkWidget     *expand_toggle;
	CtkWidget     *autohide_toggle;
	CtkWidget     *hidebuttons_toggle;
	CtkWidget     *arrows_toggle;
	CtkWidget     *default_radio;
	CtkWidget     *color_radio;
	CtkWidget     *image_radio;
	CtkWidget     *color_widgets;
	CtkWidget     *image_widgets;
	CtkWidget     *color_button;
	CtkWidget     *color_label;
	CtkWidget     *image_chooser;
	CtkWidget     *opacity_scale;
	CtkWidget     *opacity_label;
	CtkWidget     *opacity_legend;

	CtkWidget     *writability_warn_general;
	CtkWidget     *writability_warn_background;

	/* FIXME: This is a workaround for CTK+ bug #327243 */
	int            selection_emitted;
} PanelPropertiesDialog;

static GQuark panel_properties_dialog_quark = 0;

static void panel_properties_dialog_opacity_changed (PanelPropertiesDialog *dialog);

static void
panel_properties_dialog_free (PanelPropertiesDialog *dialog)
{
	if (dialog->settings)
		g_object_unref (dialog->settings);
	dialog->settings = NULL;

	if (dialog->background_settings)
		g_object_unref (dialog->background_settings);
	dialog->background_settings = NULL;

	if (dialog->properties_dialog)
		ctk_widget_destroy (dialog->properties_dialog);
	dialog->properties_dialog = NULL;

	g_free (dialog);
}

enum {
	COLUMN_TEXT,
	COLUMN_ITEM,
	NUMBER_COLUMNS
};

typedef struct {
	const char       *name;
	PanelOrientation  orientation;
} OrientationComboItem;

static OrientationComboItem orientation_items [] = {
	{ NC_("Orientation", "Top"),    PANEL_ORIENTATION_TOP    },
	{ NC_("Orientation", "Bottom"), PANEL_ORIENTATION_BOTTOM },
	{ NC_("Orientation", "Left"),   PANEL_ORIENTATION_LEFT   },
	{ NC_("Orientation", "Right"),  PANEL_ORIENTATION_RIGHT  }
};

static void
panel_properties_dialog_orientation_changed (PanelPropertiesDialog *dialog,
					     CtkComboBox           *combo_box)
{
	CtkTreeIter           iter;
	CtkTreeModel         *model;
	OrientationComboItem *item;

	g_assert (dialog->orientation_combo == CTK_WIDGET (combo_box));

	if (!ctk_combo_box_get_active_iter (combo_box, &iter))
		return;

	model = ctk_combo_box_get_model (combo_box);
	ctk_tree_model_get (model, &iter, COLUMN_ITEM, &item, -1);
	if (item == NULL)
		return;

	panel_profile_set_toplevel_orientation (dialog->toplevel,
						item->orientation);
}

static void
panel_properties_dialog_setup_orientation_combo_sensitivty (PanelPropertiesDialog *dialog, CtkToggleButton *n)
{
	gboolean expand;
	expand = panel_profile_get_toplevel_expand (dialog->toplevel);

	if (! panel_profile_key_is_writable (dialog->toplevel, PANEL_TOPLEVEL_ORIENTATION_KEY)) {
		ctk_widget_set_sensitive (dialog->orientation_combo, FALSE);
		ctk_widget_set_sensitive (dialog->orientation_label, FALSE);
		ctk_widget_show (dialog->writability_warn_general);
	}
	else {
		/* enable orientation only for non-expanded panels */
		ctk_widget_set_sensitive (dialog->orientation_combo, expand);
		ctk_widget_set_sensitive (dialog->orientation_label, expand);
	}
}

static void
panel_properties_dialog_setup_orientation_combo (PanelPropertiesDialog *dialog,
						 CtkBuilder            *gui)
{
	PanelOrientation  orientation;
	CtkListStore     *model;
	CtkTreeIter       iter;
	CtkCellRenderer  *renderer;
	int               i;

	dialog->orientation_combo = PANEL_CTK_BUILDER_GET (gui, "orientation_combo");
	g_return_if_fail (dialog->orientation_combo != NULL);
	dialog->orientation_label = PANEL_CTK_BUILDER_GET (gui, "orientation_label");
	g_return_if_fail (dialog->orientation_label != NULL);

	orientation = panel_profile_get_toplevel_orientation (dialog->toplevel);

	model = ctk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_POINTER);

	ctk_combo_box_set_model (CTK_COMBO_BOX (dialog->orientation_combo),
				 CTK_TREE_MODEL (model));

	for (i = 0; i < G_N_ELEMENTS (orientation_items); i++) {
		ctk_list_store_append (model, &iter);
		ctk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_dpgettext2 (NULL, "Orientation", orientation_items [i].name),
				    COLUMN_ITEM, &(orientation_items [i]),
				    -1);
		if (orientation == orientation_items [i].orientation)
			ctk_combo_box_set_active_iter (CTK_COMBO_BOX (dialog->orientation_combo),
						       &iter);
	}

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (dialog->orientation_combo),
				    renderer, TRUE);
	ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (dialog->orientation_combo),
					renderer, "text", COLUMN_TEXT, NULL);

	g_signal_connect_swapped (dialog->orientation_combo, "changed",
				  G_CALLBACK (panel_properties_dialog_orientation_changed),
				  dialog);

	panel_properties_dialog_setup_orientation_combo_sensitivty (dialog, NULL);
}

static void
panel_properties_dialog_size_changed (PanelPropertiesDialog *dialog,
				      CtkSpinButton         *spin_button)
{
	panel_profile_set_toplevel_size (dialog->toplevel,
					 ctk_spin_button_get_value_as_int (spin_button));
}

static void
panel_properties_dialog_setup_size_spin (PanelPropertiesDialog *dialog,
					 CtkBuilder            *gui)
{
	dialog->size_widgets = PANEL_CTK_BUILDER_GET (gui, "size_widgets");
	g_return_if_fail (dialog->size_widgets != NULL);
	dialog->size_spin = PANEL_CTK_BUILDER_GET (gui, "size_spin");
	g_return_if_fail (dialog->size_spin != NULL);
	dialog->size_label = PANEL_CTK_BUILDER_GET (gui, "size_label");
	g_return_if_fail (dialog->size_label != NULL);
	dialog->size_label_pixels = PANEL_CTK_BUILDER_GET (gui, "size_label_pixels");
	g_return_if_fail (dialog->size_label_pixels != NULL);

	ctk_spin_button_set_range (CTK_SPIN_BUTTON (dialog->size_spin),
				   panel_toplevel_get_minimum_size (dialog->toplevel),
				   panel_toplevel_get_maximum_size (dialog->toplevel));

	ctk_spin_button_set_value (CTK_SPIN_BUTTON (dialog->size_spin),
				   panel_profile_get_toplevel_size (dialog->toplevel));

	g_signal_connect_swapped (dialog->size_spin, "value_changed",
				  G_CALLBACK (panel_properties_dialog_size_changed),
				  dialog);

	if ( ! panel_profile_key_is_writable (dialog->toplevel, PANEL_TOPLEVEL_SIZE_KEY)) {
		ctk_widget_set_sensitive (dialog->size_spin, FALSE);
		ctk_widget_set_sensitive (dialog->size_label, FALSE);
		ctk_widget_set_sensitive (dialog->size_label_pixels, FALSE);
		ctk_widget_show (dialog->writability_warn_general);
	}
}

static void
panel_properties_dialog_icon_changed (PanelIconChooser      *chooser,
				      const char            *icon,
				      PanelPropertiesDialog *dialog)
{
        panel_profile_set_attached_custom_icon (dialog->toplevel, icon);
}

static void
panel_properties_dialog_setup_icon_chooser (PanelPropertiesDialog *dialog,
					    CtkBuilder            *gui)
{
	char *custom_icon;

	dialog->icon_align = PANEL_CTK_BUILDER_GET (gui, "icon_align");
	g_return_if_fail (dialog->icon_align != NULL);

	dialog->icon_chooser = panel_icon_chooser_new (NULL);
	panel_icon_chooser_set_fallback_icon_name (PANEL_ICON_CHOOSER (dialog->icon_chooser),
						   PANEL_ICON_DRAWER);
	ctk_widget_show (dialog->icon_chooser);
	ctk_container_add (CTK_CONTAINER (dialog->icon_align),
			   dialog->icon_chooser);

	dialog->icon_label = PANEL_CTK_BUILDER_GET (gui, "icon_label");
	g_return_if_fail (dialog->icon_label != NULL);

	custom_icon = panel_profile_get_attached_custom_icon (dialog->toplevel);
	panel_icon_chooser_set_icon (PANEL_ICON_CHOOSER (dialog->icon_chooser),
				     custom_icon);
	g_free (custom_icon);

	g_signal_connect (dialog->icon_chooser, "changed",
			  G_CALLBACK (panel_properties_dialog_icon_changed), dialog);

	if (!panel_profile_is_writable_attached_custom_icon (dialog->toplevel)) {
		ctk_widget_set_sensitive (dialog->icon_chooser, FALSE);
		ctk_widget_set_sensitive (dialog->icon_label, FALSE);
		if (panel_toplevel_get_is_attached (dialog->toplevel))
			ctk_widget_show (dialog->writability_warn_general);
	}
}

/* Note: this is only for toggle buttons on the general page, if needed for togglebuttons
   elsewhere you must make this respect the writability warning thing for the right page */
#define SETUP_TOGGLE_BUTTON(wid, n, p, key)                                                            \
	static void                                                                               \
	panel_properties_dialog_##n (PanelPropertiesDialog *dialog,                               \
				     CtkToggleButton       *n)                                    \
	{                                                                                         \
		panel_profile_set_toplevel_##p (dialog->toplevel,                                 \
						ctk_toggle_button_get_active (n));                \
	}                                                                                         \
	static void                                                                               \
	panel_properties_dialog_setup_##n (PanelPropertiesDialog *dialog,                         \
					   CtkBuilder            *gui)                            \
	{                                                                                         \
		dialog->n = PANEL_CTK_BUILDER_GET (gui, wid);                                      \
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->n),                      \
					      panel_profile_get_toplevel_##p (dialog->toplevel)); \
		g_signal_connect_swapped (dialog->n, "toggled",                                   \
					  G_CALLBACK (panel_properties_dialog_##n), dialog);      \
		if ( ! panel_profile_key_is_writable (dialog->toplevel, key)) {               \
			ctk_widget_set_sensitive (dialog->n, FALSE);                              \
			ctk_widget_show (dialog->writability_warn_general);			  \
		}										  \
	}

SETUP_TOGGLE_BUTTON ("expand_toggle",      expand_toggle,      expand,           PANEL_TOPLEVEL_EXPAND_KEY)
SETUP_TOGGLE_BUTTON ("autohide_toggle",    autohide_toggle,    auto_hide,        PANEL_TOPLEVEL_AUTO_HIDE_KEY)
SETUP_TOGGLE_BUTTON ("hidebuttons_toggle", hidebuttons_toggle, enable_buttons,   PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY)
SETUP_TOGGLE_BUTTON ("arrows_toggle",      arrows_toggle,      enable_arrows,    PANEL_TOPLEVEL_ENABLE_ARROWS_KEY)

static void
panel_properties_dialog_color_changed (PanelPropertiesDialog *dialog,
				       CtkColorChooser       *color_button)
{
	CdkRGBA color;

	g_assert (dialog->color_button == CTK_WIDGET (color_button));

	ctk_color_chooser_get_rgba (color_button, &color);
	panel_profile_set_background_cdk_rgba (dialog->toplevel, &color);
	panel_properties_dialog_opacity_changed (dialog);
}

static void
panel_properties_dialog_setup_color_button (PanelPropertiesDialog *dialog,
					    CtkBuilder            *gui)
{
	CdkRGBA color;

	dialog->color_button = PANEL_CTK_BUILDER_GET (gui, "color_button");
	g_return_if_fail (dialog->color_button != NULL);
	dialog->color_label = PANEL_CTK_BUILDER_GET (gui, "color_label");
	g_return_if_fail (dialog->color_label != NULL);

	panel_profile_get_background_color (dialog->toplevel, &color);

	ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (dialog->color_button),
				     &color);

	g_signal_connect_swapped (dialog->color_button, "color_set",
				  G_CALLBACK (panel_properties_dialog_color_changed),
				  dialog);

	if ( ! panel_profile_background_key_is_writable (dialog->toplevel, "color")) {
		ctk_widget_set_sensitive (dialog->color_button, FALSE);
		ctk_widget_set_sensitive (dialog->color_label, FALSE);
		ctk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_image_changed (PanelPropertiesDialog *dialog)
{
	char *image;

	image = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (dialog->image_chooser));

	/* FIXME: This is an ugly workaround for CTK+ bug #327243.
	 * FIXME: Note that CTK+ 2.12 and file-set signal might help. */
	if (! (dialog->selection_emitted < 2) && !image) {
		dialog->selection_emitted++;
		return;
	}
	panel_profile_set_background_image (dialog->toplevel, image);

	g_free (image);
}

static void
panel_properties_dialog_setup_image_chooser (PanelPropertiesDialog *dialog,
					     CtkBuilder            *gui)
{
	char *image;

	dialog->image_chooser = PANEL_CTK_BUILDER_GET (gui, "image_chooser");
	panel_ctk_file_chooser_add_image_preview (CTK_FILE_CHOOSER (dialog->image_chooser));

	image = panel_profile_get_background_image (dialog->toplevel);

	if (PANEL_GLIB_STR_EMPTY (image))
		ctk_file_chooser_unselect_all (CTK_FILE_CHOOSER (dialog->image_chooser));
	else
		ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (dialog->image_chooser),
					       image);

	if (image)
		g_free (image);

	dialog->selection_emitted = 0;
	g_signal_connect_swapped (dialog->image_chooser, "selection-changed",
				  G_CALLBACK (panel_properties_dialog_image_changed),
				  dialog);

	if ( ! panel_profile_background_key_is_writable (dialog->toplevel, "image")) {
		ctk_widget_set_sensitive (dialog->image_chooser, FALSE);
		ctk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_opacity_changed (PanelPropertiesDialog *dialog)
{
	gdouble percentage;
	guint16 opacity;

	percentage = ctk_range_get_value (CTK_RANGE (dialog->opacity_scale));

	if (percentage >= 98)
		percentage = 100;
	else if (percentage <= 2)
		percentage = 0;

	opacity = (percentage / 100) * 65535;

	panel_profile_set_background_opacity (dialog->toplevel, opacity);
}

static void
panel_properties_dialog_setup_opacity_scale (PanelPropertiesDialog *dialog,
					     CtkBuilder            *gui)
{
	guint16 opacity;
	gdouble percentage;

	dialog->opacity_scale = PANEL_CTK_BUILDER_GET (gui, "opacity_scale");
	g_return_if_fail (dialog->opacity_scale != NULL);
	dialog->opacity_label = PANEL_CTK_BUILDER_GET (gui, "opacity_label");
	g_return_if_fail (dialog->opacity_label != NULL);
	dialog->opacity_legend = PANEL_CTK_BUILDER_GET (gui, "opacity_legend");
	g_return_if_fail (dialog->opacity_legend != NULL);

	opacity = panel_profile_get_background_opacity (dialog->toplevel);

	percentage = (opacity * 100.0) / 65535;

	ctk_range_set_value (CTK_RANGE (dialog->opacity_scale), percentage);

	g_signal_connect_swapped (dialog->opacity_scale, "value_changed",
				  G_CALLBACK (panel_properties_dialog_opacity_changed),
				  dialog);

	if ( ! panel_profile_background_key_is_writable (dialog->toplevel, "opacity")) {
		ctk_widget_set_sensitive (dialog->opacity_scale, FALSE);
		ctk_widget_set_sensitive (dialog->opacity_label, FALSE);
		ctk_widget_set_sensitive (dialog->opacity_legend, FALSE);
		ctk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_upd_sensitivity (PanelPropertiesDialog *dialog,
					 PanelBackgroundType    background_type)
{
	ctk_widget_set_sensitive (dialog->color_widgets,
				  background_type == PANEL_BACK_COLOR);
	ctk_widget_set_sensitive (dialog->image_widgets,
				  background_type == PANEL_BACK_IMAGE);
}

static void
panel_properties_dialog_background_toggled (PanelPropertiesDialog *dialog,
					    CtkWidget             *radio)
{
	PanelBackgroundType background_type = PANEL_BACK_NONE;

	if (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (radio)))
		return;

	if (radio == dialog->default_radio)
		background_type = PANEL_BACK_NONE;

	else if (radio == dialog->color_radio)
		background_type = PANEL_BACK_COLOR;

	else if (radio == dialog->image_radio)
		background_type = PANEL_BACK_IMAGE;

	panel_properties_dialog_upd_sensitivity (dialog, background_type);

	panel_profile_set_background_type (dialog->toplevel, background_type);
}

static void
panel_properties_dialog_setup_background_radios (PanelPropertiesDialog *dialog,
						 CtkBuilder            *gui)
{
	PanelBackgroundType  background_type;
	CtkWidget           *active_radio;

	dialog->default_radio     = PANEL_CTK_BUILDER_GET (gui, "default_radio");
	dialog->color_radio       = PANEL_CTK_BUILDER_GET (gui, "color_radio");
	dialog->image_radio       = PANEL_CTK_BUILDER_GET (gui, "image_radio");
	dialog->color_widgets     = PANEL_CTK_BUILDER_GET (gui, "color_widgets");
	dialog->image_widgets     = PANEL_CTK_BUILDER_GET (gui, "image_widgets");

	background_type = panel_profile_get_background_type (dialog->toplevel);
	switch (background_type) {
	case PANEL_BACK_NONE:
		active_radio = dialog->default_radio;
		break;
	case PANEL_BACK_COLOR:
		active_radio = dialog->color_radio;
		break;
	case PANEL_BACK_IMAGE:
		active_radio = dialog->image_radio;
		break;
	default:
		active_radio = NULL;
		g_assert_not_reached ();
	}

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (active_radio), TRUE);

	panel_properties_dialog_upd_sensitivity (dialog, background_type);

	g_signal_connect_swapped (dialog->default_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->color_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->image_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);

	if ( ! panel_profile_background_key_is_writable (dialog->toplevel, "type")) {
		ctk_widget_set_sensitive (dialog->default_radio, FALSE);
		ctk_widget_set_sensitive (dialog->color_radio, FALSE);
		ctk_widget_set_sensitive (dialog->image_radio, FALSE);
		ctk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_update_arrows_toggle_visible (PanelPropertiesDialog *dialog,
					       CtkToggleButton       *toggle)
{
	if (ctk_toggle_button_get_active (toggle))
		ctk_widget_set_sensitive (dialog->arrows_toggle,
					  panel_profile_key_is_writable (dialog->toplevel, PANEL_TOPLEVEL_ENABLE_ARROWS_KEY));
	else
		ctk_widget_set_sensitive (dialog->arrows_toggle, FALSE);
}

static void
panel_properties_dialog_response (PanelPropertiesDialog *dialog,
				  int                    response,
				  CtkWidget             *properties_dialog)
{
	char *help_id;

	switch (response) {
	case CTK_RESPONSE_CLOSE:
		ctk_widget_destroy (properties_dialog);
		break;
	case CTK_RESPONSE_HELP:
		if (panel_toplevel_get_is_attached (dialog->toplevel)) {
			help_id = "gospanel-550";
		} else {
			help_id = "gospanel-28";
		}
		panel_show_help (ctk_window_get_screen (CTK_WINDOW (properties_dialog)),
				 "cafe-user-guide", help_id, NULL);
		break;
	default:
		break;
	}
}

static void
panel_properties_dialog_destroy (PanelPropertiesDialog *dialog)
{
	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (dialog->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->toplevel),
			    panel_properties_dialog_quark,
			    NULL);
}

static void
panel_properties_dialog_update_orientation (PanelPropertiesDialog *dialog,
					    PanelOrientation       orientation)
{
	CtkTreeModel         *model;
	CtkTreeIter           iter;
	OrientationComboItem *item;
	int                   max_size;
	int                   spin_size;
	int                   profile_size;

	/* change the maximum size of the panel */
	//TODO: we should also do this when the monitor size changes
	max_size = panel_toplevel_get_maximum_size (dialog->toplevel);
	spin_size = ctk_spin_button_get_value_as_int (CTK_SPIN_BUTTON (dialog->size_spin));
	profile_size = panel_profile_get_toplevel_size (dialog->toplevel);

	ctk_spin_button_set_range (CTK_SPIN_BUTTON (dialog->size_spin),
				   panel_toplevel_get_minimum_size (dialog->toplevel),
				   max_size);

	if (spin_size > max_size)
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (dialog->size_spin),
					   max_size);
	else if (spin_size != profile_size)
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (dialog->size_spin),
					   MIN (profile_size, max_size));

	/* update the orientation combo box */
	model = ctk_combo_box_get_model (CTK_COMBO_BOX (dialog->orientation_combo));

	if (!ctk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		ctk_tree_model_get (model, &iter, COLUMN_ITEM, &item, -1);
		if (item != NULL && item->orientation == orientation) {
			ctk_combo_box_set_active_iter (CTK_COMBO_BOX (dialog->orientation_combo),
						       &iter);
			return;
		}
	} while (ctk_tree_model_iter_next (model, &iter));
}

static void
panel_properties_dialog_update_size (PanelPropertiesDialog *dialog,
				     int size)
{
	ctk_spin_button_set_value (CTK_SPIN_BUTTON (dialog->size_spin), size);
}

static void
panel_properties_dialog_toplevel_notify (GSettings             *settings,
					 gchar                 *key,
					 PanelPropertiesDialog *dialog)
{

#define UPDATE_TOGGLE(p, n)                                                                \
	if (!strcmp (key, p)) {                                                            \
		gboolean val = g_settings_get_boolean (settings, key);                     \
		if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->n)) != val)   \
			ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->n), val); \
	}

	if (!strcmp (key, "orientation"))
		panel_properties_dialog_update_orientation (dialog, g_settings_get_enum (settings, key));
	else if (!strcmp (key, "size"))
		panel_properties_dialog_update_size (dialog, g_settings_get_int (settings, key));
	else UPDATE_TOGGLE ("expand",         expand_toggle)
	else UPDATE_TOGGLE ("auto-hide",      autohide_toggle)
	else UPDATE_TOGGLE ("enable-buttons", hidebuttons_toggle)
	else UPDATE_TOGGLE ("enable-arrows",  arrows_toggle)
}

static void
panel_properties_dialog_update_background_type (PanelPropertiesDialog *dialog,
						PanelBackgroundType    background_type)
{
	CtkWidget           *active_radio;

	switch (background_type) {
	case PANEL_BACK_NONE:
		active_radio = dialog->default_radio;
		break;
	case PANEL_BACK_COLOR:
		active_radio = dialog->color_radio;
		break;
	case PANEL_BACK_IMAGE:
		active_radio = dialog->image_radio;
		break;
	default:
		active_radio = NULL;
		g_assert_not_reached ();
		break;
	}

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (active_radio), TRUE);
}

static void
panel_properties_dialog_update_background_color (PanelPropertiesDialog *dialog,
						 gchar                 *str_color)
{
	CdkRGBA new_color;
	CdkRGBA old_color;

	if (!cdk_rgba_parse (&new_color, str_color))
		return;

	ctk_color_chooser_get_rgba (CTK_COLOR_CHOOSER (dialog->color_button),
				    &old_color);

	if (!cdk_rgba_equal (&old_color, &new_color))
		ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (dialog->color_button),
					    &new_color);
}

static void
panel_properties_dialog_update_background_image (PanelPropertiesDialog *dialog,
						 gchar                 *text)
{
	char       *old_text;

	old_text = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (dialog->image_chooser));

	if (PANEL_GLIB_STR_EMPTY (text) && old_text)
		ctk_file_chooser_unselect_all (CTK_FILE_CHOOSER (dialog->image_chooser));
	else if (!PANEL_GLIB_STR_EMPTY (text) &&
		 (!old_text || strcmp (text, old_text)))
		ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (dialog->image_chooser),
					       text);

	if (old_text)
		g_free (old_text);
}

static void
panel_properties_dialog_background_notify (GSettings             *settings,
					   gchar                 *key,
					   PanelPropertiesDialog *dialog)
{
	if (!strcmp (key, "type"))
	{
		PanelBackgroundType type = g_settings_get_enum (settings, key);
		panel_properties_dialog_update_background_type (dialog, type);
	}
	else if (!strcmp (key, "color"))
	{
		char *color = g_settings_get_string (settings, key);
		panel_properties_dialog_update_background_color (dialog, color);
		g_free (color);
	}
	else if (!strcmp (key, "image"))
	{
		char *image = g_settings_get_string (settings, key);
		panel_properties_dialog_update_background_image (dialog, image);
		g_free (image);
	}
}

static void
panel_properties_dialog_remove_orientation_combo (PanelPropertiesDialog *dialog)
{
	CtkContainer *container = CTK_CONTAINER (dialog->general_table);
	CtkGrid      *grid      = CTK_GRID (dialog->general_table);

	g_object_ref (dialog->size_label);
	g_object_ref (dialog->size_widgets);
	g_object_ref (dialog->icon_label);
	g_object_ref (dialog->icon_align);

	ctk_container_remove (container, dialog->orientation_label);
	ctk_container_remove (container, dialog->orientation_combo);
	ctk_container_remove (container, dialog->size_label);
	ctk_container_remove (container, dialog->size_widgets);
	ctk_container_remove (container, dialog->icon_label);
	ctk_container_remove (container, dialog->icon_align);

	ctk_grid_attach (grid, dialog->size_label,   0, 1, 1, 1);
	ctk_grid_attach (grid, dialog->size_widgets, 1, 1, 1, 1);
	ctk_grid_attach (grid, dialog->icon_label,   0, 2, 1, 1);
	ctk_grid_attach (grid, dialog->icon_align,   1, 2, 1, 1);

	dialog->orientation_label = NULL;
	dialog->orientation_combo = NULL;
	g_object_unref (dialog->size_label);
	g_object_unref (dialog->size_widgets);
	g_object_unref (dialog->icon_label);
	g_object_unref (dialog->icon_align);
}

static void
panel_properties_dialog_remove_icon_chooser (PanelPropertiesDialog *dialog)
{
	CtkContainer *container = CTK_CONTAINER (dialog->general_table);

	ctk_container_remove (container, dialog->icon_label);
	ctk_container_remove (container, dialog->icon_align);

	dialog->icon_label = NULL;
	dialog->icon_align = NULL;
	dialog->icon_chooser = NULL;
}

static void
panel_properties_dialog_remove_toggles (PanelPropertiesDialog *dialog)
{
	CtkContainer *container = CTK_CONTAINER (dialog->general_vbox);

	ctk_container_remove (container, dialog->autohide_toggle);
	ctk_container_remove (container, dialog->expand_toggle);

	dialog->autohide_toggle = NULL;
	dialog->expand_toggle   = NULL;
}

static void
panel_properties_dialog_update_for_attached (PanelPropertiesDialog *dialog,
					     gboolean               attached)
{
	if (!attached)
		panel_properties_dialog_remove_icon_chooser (dialog);
	else {
		ctk_window_set_title (CTK_WINDOW (dialog->properties_dialog),
				      _("Drawer Properties"));
		panel_properties_dialog_remove_toggles (dialog);
		panel_properties_dialog_remove_orientation_combo (dialog);
	}
}

static PanelPropertiesDialog *
panel_properties_dialog_new (PanelToplevel *toplevel,
			     CtkBuilder    *gui)
{
	PanelPropertiesDialog *dialog;
	char                  *toplevel_settings_path;

	dialog = g_new0 (PanelPropertiesDialog, 1);

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 panel_properties_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_properties_dialog_free);

	dialog->toplevel = toplevel;

	dialog->properties_dialog = PANEL_CTK_BUILDER_GET (gui, "panel_properties_dialog");
	g_signal_connect_swapped (dialog->properties_dialog, "response",
				  G_CALLBACK (panel_properties_dialog_response), dialog);
	g_signal_connect_swapped (dialog->properties_dialog, "destroy",
				  G_CALLBACK (panel_properties_dialog_destroy), dialog);

	ctk_window_set_screen (CTK_WINDOW (dialog->properties_dialog),
			       ctk_window_get_screen (CTK_WINDOW (toplevel)));

	dialog->writability_warn_general = PANEL_CTK_BUILDER_GET (gui, "writability_warn_general");
	dialog->writability_warn_background = PANEL_CTK_BUILDER_GET (gui, "writability_warn_background");

	dialog->general_vbox  = PANEL_CTK_BUILDER_GET (gui, "general_vbox");
	dialog->general_table = PANEL_CTK_BUILDER_GET (gui, "general_table");

	panel_properties_dialog_setup_orientation_combo  (dialog, gui);
	panel_properties_dialog_setup_size_spin          (dialog, gui);
	panel_properties_dialog_setup_icon_chooser       (dialog, gui);
	panel_properties_dialog_setup_expand_toggle      (dialog, gui);
	panel_properties_dialog_setup_autohide_toggle    (dialog, gui);
	panel_properties_dialog_setup_hidebuttons_toggle (dialog, gui);
	panel_properties_dialog_setup_arrows_toggle      (dialog, gui);

	g_signal_connect_swapped (dialog->expand_toggle, "toggled",
				  G_CALLBACK (panel_properties_dialog_setup_orientation_combo_sensitivty), dialog);

	panel_properties_update_arrows_toggle_visible (
		dialog, CTK_TOGGLE_BUTTON (dialog->hidebuttons_toggle));
	g_signal_connect_swapped (dialog->hidebuttons_toggle, "toggled",
				  G_CALLBACK (panel_properties_update_arrows_toggle_visible),
				  dialog);

	g_object_get (toplevel, "settings-path", &toplevel_settings_path, NULL);
	dialog->settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA,
						     toplevel_settings_path);
	gchar *toplevel_background_path;
	toplevel_background_path = g_strdup_printf ("%sbackground/", toplevel_settings_path);
	dialog->background_settings = g_settings_new_with_path (PANEL_TOPLEVEL_BACKGROUND_SCHEMA,
								toplevel_background_path);
	g_free (toplevel_background_path);
	g_free (toplevel_settings_path);

	g_signal_connect (dialog->settings,
			  "changed",
			  G_CALLBACK (panel_properties_dialog_toplevel_notify),
			  dialog);

	panel_properties_dialog_setup_color_button      (dialog, gui);
	panel_properties_dialog_setup_image_chooser     (dialog, gui);
	panel_properties_dialog_setup_opacity_scale     (dialog, gui);
	panel_properties_dialog_setup_background_radios (dialog, gui);

	g_signal_connect (dialog->background_settings,
			  "changed",
			  G_CALLBACK (panel_properties_dialog_background_notify),
			  dialog);

	panel_properties_dialog_update_for_attached (dialog,
						     panel_toplevel_get_is_attached (dialog->toplevel));

	panel_toplevel_push_autohide_disabler (dialog->toplevel);
	panel_widget_register_open_dialog (panel_toplevel_get_panel_widget (dialog->toplevel),
					   dialog->properties_dialog);

	ctk_widget_show (dialog->properties_dialog);

	return dialog;
}

void
panel_properties_dialog_present (PanelToplevel *toplevel)
{
	PanelPropertiesDialog *dialog;
	CtkBuilder            *gui;

	if (!panel_properties_dialog_quark)
		panel_properties_dialog_quark =
			g_quark_from_static_string ("panel-properties-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel), panel_properties_dialog_quark);
	if (dialog) {
		ctk_window_set_screen (CTK_WINDOW (dialog->properties_dialog),
				       ctk_window_get_screen (CTK_WINDOW (toplevel)));
		ctk_window_present (CTK_WINDOW (dialog->properties_dialog));
		return;
	}

	gui = ctk_builder_new ();
	ctk_builder_set_translation_domain (gui, GETTEXT_PACKAGE);
	ctk_builder_add_from_resource (gui,
	                               PANEL_RESOURCE_PATH "panel-properties-dialog.ui",
	                               NULL);

	dialog = panel_properties_dialog_new (toplevel, gui);

	g_object_unref (gui);
}
