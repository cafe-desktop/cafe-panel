/*
 * panel-ctk.c: various small extensions to ctk+
 *
 * Copyright (C) 2010 Novell, Inc.
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <ctk/ctk.h>
#include <glib/gi18n.h>

#include "panel-ctk.h"
#include "panel-cleanup.h"

/*
 * Originally based on code from panel-properties-dialog.c. This part of the
 * code was:
 * Copyright (C) 2005 Vincent Untz <vuntz@gnome.org>
 */

/*There should be only one icon_settings object for the whole panel
 *So we need a global variable here
 */
static GSettings *icon_settings = NULL;

static void
panel_ctk_file_chooser_preview_update (CtkFileChooser *chooser,
				       gpointer data)
{
	CtkWidget *preview;
	char      *filename;
	GdkPixbuf *pixbuf;
	gboolean   have_preview;

	preview = CTK_WIDGET (data);
	filename = ctk_file_chooser_get_preview_filename (chooser);

	if (filename == NULL)
		return;

	pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
	have_preview = (pixbuf != NULL);
	g_free (filename);

	ctk_image_set_from_pixbuf (CTK_IMAGE (preview), pixbuf);
	if (pixbuf)
		g_object_unref (pixbuf);

	ctk_file_chooser_set_preview_widget_active (chooser,
						    have_preview);
}

void
panel_ctk_file_chooser_add_image_preview (CtkFileChooser *chooser)
{
	CtkFileFilter *filter;
	CtkWidget     *chooser_preview;

	g_return_if_fail (CTK_IS_FILE_CHOOSER (chooser));

	filter = ctk_file_filter_new ();
	ctk_file_filter_add_pixbuf_formats (filter);
	ctk_file_chooser_set_filter (chooser, filter);

	chooser_preview = ctk_image_new ();
	ctk_file_chooser_set_preview_widget (chooser, chooser_preview);
	g_signal_connect (chooser, "update-preview",
			  G_CALLBACK (panel_ctk_file_chooser_preview_update),
			  chooser_preview);
}

/*
 * End of code coming from panel-properties-dialog.c
 */

CtkWidget*
panel_dialog_add_button (CtkDialog   *dialog,
			 const gchar *button_text,
			 const gchar *icon_name,
			       gint   response_id)
{
	CtkWidget *button;

	button = ctk_button_new_with_mnemonic (button_text);
	ctk_button_set_image (CTK_BUTTON (button), ctk_image_new_from_icon_name (icon_name, CTK_ICON_SIZE_BUTTON));

	ctk_button_set_use_underline (CTK_BUTTON (button), TRUE);
	ctk_style_context_add_class (ctk_widget_get_style_context (button), "text-button");
	ctk_widget_set_can_default (button, TRUE);
	ctk_widget_show (button);
	ctk_dialog_add_action_widget (CTK_DIALOG (dialog), button, response_id);

	return button;
}

static CtkWidget *
panel_file_chooser_dialog_new_valist (const gchar          *title,
				      CtkWindow            *parent,
				      CtkFileChooserAction  action,
				      const gchar          *first_button_text,
				      va_list               varargs)
{
	CtkWidget *result;
	const char *button_text = first_button_text;
	gint response_id;

	result = g_object_new (CTK_TYPE_FILE_CHOOSER_DIALOG,
			       "title", title,
			       "action", action,
			       NULL);

	if (parent)
		ctk_window_set_transient_for (CTK_WINDOW (result), parent);

	while (button_text)
		{
			response_id = va_arg (varargs, gint);

			if (g_strcmp0 (button_text, "process-stop") == 0)
				panel_dialog_add_button (CTK_DIALOG (result), _("_Cancel"), button_text, response_id);
			else if (g_strcmp0 (button_text, "document-open") == 0)
				panel_dialog_add_button (CTK_DIALOG (result), _("_Open"), button_text, response_id);
			else if (g_strcmp0 (button_text, "ctk-ok") == 0)
				panel_dialog_add_button (CTK_DIALOG (result), _("_OK"), button_text, response_id);
			else
				ctk_dialog_add_button (CTK_DIALOG (result), button_text, response_id);

			button_text = va_arg (varargs, const gchar *);
		}

	return result;
}

CtkWidget *
panel_file_chooser_dialog_new (const gchar          *title,
			       CtkWindow            *parent,
			       CtkFileChooserAction  action,
			       const gchar          *first_button_text,
			       ...)
{
	CtkWidget *result;
	va_list varargs;

	va_start (varargs, first_button_text);
	result = panel_file_chooser_dialog_new_valist (title, parent, action,
						       first_button_text,
						       varargs);
	va_end (varargs);

	return result;
}


static void
ensure_icon_settings (void)
{
	if (icon_settings != NULL)
	return;

	icon_settings = g_settings_new ("org.cafe.interface");

	panel_cleanup_register (panel_cleanup_unref_and_nullify,
					&icon_settings);
}

CtkWidget *
panel_image_menu_item_new_from_icon (const gchar *icon_name,
				     const gchar *label_name)
{
	gchar *concat;
	CtkWidget *icon;
	CtkStyleContext *context;
	CtkWidget *box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	CtkWidget *icon_box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

	if (icon_name)
		icon = ctk_image_new_from_icon_name (icon_name, CTK_ICON_SIZE_MENU);
	else
		icon = ctk_image_new ();

	concat = g_strconcat (label_name, "     ", NULL);
	CtkWidget *label_menu = ctk_label_new_with_mnemonic (concat);
	CtkWidget *menuitem = ctk_menu_item_new ();

	context = ctk_widget_get_style_context (CTK_WIDGET(icon_box));
	ctk_style_context_add_class(context,"cafe-panel-menu-icon-box");

	ctk_container_add (CTK_CONTAINER (icon_box), icon);
	ctk_container_add (CTK_CONTAINER (box), icon_box);
	ctk_container_add (CTK_CONTAINER (box), label_menu);

	ctk_container_add (CTK_CONTAINER (menuitem), box);
	ctk_widget_show_all (menuitem);

	ensure_icon_settings();
	g_settings_bind (icon_settings, "menus-have-icons", icon, "visible",
                         G_SETTINGS_BIND_GET);

	g_free (concat);

	return menuitem;
}

CtkWidget *
panel_image_menu_item_new_from_gicon (GIcon       *gicon,
				      const gchar *label_name)
{
	gchar *concat;
	CtkWidget *icon;
	CtkStyleContext *context;
	CtkWidget *box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	CtkWidget *icon_box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

	if (gicon)
		icon = ctk_image_new_from_gicon (gicon, CTK_ICON_SIZE_MENU);
	else
		icon = ctk_image_new ();

	concat = g_strconcat (label_name, "     ", NULL);
	CtkWidget *label_menu = ctk_label_new_with_mnemonic (concat);
	CtkWidget *menuitem = ctk_menu_item_new ();

	context = ctk_widget_get_style_context (CTK_WIDGET(icon_box));
	ctk_style_context_add_class(context,"cafe-panel-menu-icon-box");

	ctk_container_add (CTK_CONTAINER (icon_box), icon);
	ctk_container_add (CTK_CONTAINER (box), icon_box);
	ctk_container_add (CTK_CONTAINER (box), label_menu);

	ctk_container_add (CTK_CONTAINER (menuitem), box);
	ctk_widget_show_all (menuitem);

	ensure_icon_settings();
	g_settings_bind (icon_settings, "menus-have-icons", icon, "visible",
                         G_SETTINGS_BIND_GET);

	g_free (concat);

	return menuitem;
}

CtkWidget *
panel_check_menu_item_new (CtkWidget *widget_check)
{
	CtkWidget *box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	CtkWidget *menuitem = ctk_menu_item_new ();
	CtkWidget *label_name = ctk_bin_get_child (CTK_BIN (widget_check));
	gchar *concat = g_strconcat (ctk_label_get_label (CTK_LABEL (label_name)), "     ", NULL);

	ctk_label_set_text_with_mnemonic (CTK_LABEL (label_name), concat);

	ctk_widget_set_margin_start (widget_check, 2);
	ctk_widget_set_margin_start (ctk_bin_get_child (CTK_BIN (widget_check)), 11);
	ctk_box_pack_start (CTK_BOX (box), widget_check, FALSE, FALSE, 5);

	ctk_container_add (CTK_CONTAINER (menuitem), box);
	ctk_widget_show_all (menuitem);

	ctk_label_set_mnemonic_widget (CTK_LABEL (label_name), menuitem);

	g_free (concat);

	return menuitem;
}
