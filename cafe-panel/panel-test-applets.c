/*
 * panel-test-applets.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *    Stefano Karapetsas <stefano@karapetsas.com>
 *
 * Copyright 2002 Sun Microsystems, Inc.
 *           2012 Stefano Karapetsas
 */

#include <config.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <gio/gio.h>

#include <libpanel-util/panel-cleanup.h>
#include <libcafe-desktop/cafe-dconf.h>

#include <libcafe-panel-applet-private/panel-applet-container.h>
#include <libcafe-panel-applet-private/panel-applets-manager-dbus.h>

#include "panel-modules.h"

G_GNUC_UNUSED void on_execute_button_clicked (CtkButton *button, gpointer dummy);

static CtkWidget *win = NULL;
static CtkWidget *applet_combo = NULL;
static CtkWidget *prefs_path_entry = NULL;
static CtkWidget *orient_combo = NULL;
static CtkWidget *size_combo = NULL;

static char *cli_iid = NULL;
static char *cli_prefs_path = NULL;
static char *cli_size = NULL;
static char *cli_orient = NULL;

static const GOptionEntry options [] = {
	{ "iid", 0, 0, G_OPTION_ARG_STRING, &cli_iid, N_("Specify an applet IID to load"), NULL},
	{ "prefs-path", 0, 0, G_OPTION_ARG_STRING, &cli_prefs_path, N_("Specify a gsettings path in which the applet preferences should be stored"), NULL},
	{ "size", 0, 0, G_OPTION_ARG_STRING, &cli_size, N_("Specify the initial size of the applet (xx-small, medium, large etc.)"), NULL},
	{ "orient", 0, 0, G_OPTION_ARG_STRING, &cli_orient, N_("Specify the initial orientation of the applet (top, bottom, left or right)"), NULL},
	{ NULL}
};

enum {
	COLUMN_TEXT,
	COLUMN_ITEM,
	NUMBER_COLUMNS
};

typedef struct {
	const char *name;
	guint       value;
} ComboItem;

static ComboItem orient_items [] = {
	{ NC_("Orientation", "Top"),    PANEL_ORIENTATION_TOP    },
	{ NC_("Orientation", "Bottom"), PANEL_ORIENTATION_BOTTOM },
	{ NC_("Orientation", "Left"),   PANEL_ORIENTATION_LEFT   },
	{ NC_("Orientation", "Right"),  PANEL_ORIENTATION_RIGHT  }
};


static ComboItem size_items [] = {
	{ NC_("Size", "XX Small"), 12  },
	{ NC_("Size", "X Small"),  24  },
	{ NC_("Size", "Small"),    36  },
	{ NC_("Size", "Medium"),   48  },
	{ NC_("Size", "Large"),    64  },
	{ NC_("Size", "X Large"),  80  },
	{ NC_("Size", "XX Large"), 128 }
};

static guint
get_combo_value (CtkWidget *combo_box)
{
	CtkTreeIter  iter;
	CtkTreeModel *model;
	guint         value;

	if (!ctk_combo_box_get_active_iter (CTK_COMBO_BOX (combo_box), &iter))
		return 0;

	model = ctk_combo_box_get_model (CTK_COMBO_BOX (combo_box));
	ctk_tree_model_get (model, &iter, COLUMN_ITEM, &value, -1);

	return value;
}

static gchar *
get_combo_applet_id (CtkWidget *combo_box)
{
	CtkTreeIter  iter;
	CtkTreeModel *model;
	char         *value;

	if (!ctk_combo_box_get_active_iter (CTK_COMBO_BOX (combo_box), &iter))
		return NULL;

	model = ctk_combo_box_get_model (CTK_COMBO_BOX (combo_box));
	ctk_tree_model_get (model, &iter, COLUMN_ITEM, &value, -1);

	return value;
}

static void
applet_broken_cb (CtkWidget *container,
		  CtkWidget *window)
{
	ctk_widget_destroy (window);
}

static void
applet_activated_cb (GObject      *source_object,
		     GAsyncResult *res,
		     CtkWidget    *applet_window)
{
	GError *error = NULL;

	if (!cafe_panel_applet_container_add_finish (CAFE_PANEL_APPLET_CONTAINER (source_object),
						res, &error)) {
		CtkWidget *dialog;

		dialog = ctk_message_dialog_new (CTK_WINDOW (applet_window),
						 CTK_DIALOG_MODAL|
						 CTK_DIALOG_DESTROY_WITH_PARENT,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_CLOSE,
						 _("Failed to load applet %s"),
						 error->message); // FIXME
		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		return;
	}

	ctk_widget_show (applet_window);
}

static void
load_applet_into_window (const char *title,
			 const char *prefs_path,
			 guint       size,
			 guint       orientation)
{
	CtkWidget       *container;
	CtkWidget       *applet_window;
	GVariantBuilder  builder;

	container = cafe_panel_applet_container_new ();

	applet_window = ctk_window_new (CTK_WINDOW_TOPLEVEL);
	//FIXME: we could set the window icon with the applet icon
	ctk_window_set_title (CTK_WINDOW (applet_window), title);
	ctk_container_add (CTK_CONTAINER (applet_window), container);
	ctk_widget_show (container);

	g_signal_connect (container, "applet-broken",
			  G_CALLBACK (applet_broken_cb),
			  applet_window);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&builder, "{sv}",
			       "prefs-path", g_variant_new_string (prefs_path));
	g_variant_builder_add (&builder, "{sv}",
			       "size", g_variant_new_uint32 (size));
	g_variant_builder_add (&builder, "{sv}",
			       "orient", g_variant_new_uint32 (orientation));
	cafe_panel_applet_container_add (CAFE_PANEL_APPLET_CONTAINER (container),
				    ctk_widget_get_screen (applet_window),
				    title, NULL,
				    (GAsyncReadyCallback)applet_activated_cb,
				    applet_window,
				    g_variant_builder_end (&builder));
}

static void
load_applet_from_command_line (void)
{
	guint size = 24, orient = PANEL_ORIENTATION_TOP;
	gint i;

	g_assert (cli_iid != NULL);

	if (cli_size || cli_orient) {
		if (cli_size) {
			for (i = 0; i < G_N_ELEMENTS (size_items); i++) {
				if (strcmp (g_dpgettext2 (NULL, "Size", size_items[i].name), cli_size) == 0) {
					size = size_items[i].value;
					break;
				}
			}
		}

		if (cli_orient) {
			for (i = 0; i < G_N_ELEMENTS (orient_items); i++) {
				if (strcmp (g_dpgettext2 (NULL, "Orientation", orient_items[i].name), cli_orient) == 0) {
					orient = orient_items[i].value;
					break;
				}
			}
		}
	}

	g_print ("Loading %s\n", cli_iid);

	load_applet_into_window (cli_iid, cli_prefs_path, size, orient);
}

G_GNUC_UNUSED void
on_execute_button_clicked (CtkButton *button,
			   gpointer   dummy)
{
	char *title;

	title = get_combo_applet_id (applet_combo);

	load_applet_into_window (title,
				 ctk_entry_get_text (CTK_ENTRY (prefs_path_entry)),
				 get_combo_value (size_combo),
				 get_combo_value (orient_combo));
	g_free (title);
}

static void
setup_combo (CtkWidget  *combo_box,
	     ComboItem  *items,
	     const char *context,
	     int         nb_items)
{
	CtkListStore          *model;
	CtkTreeIter            iter;
	CtkCellRenderer       *renderer;
	int                    i;

	model = ctk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_INT);

	ctk_combo_box_set_model (CTK_COMBO_BOX (combo_box),
				 CTK_TREE_MODEL (model));


	for (i = 0; i < nb_items; i++) {
		ctk_list_store_append (model, &iter);
		ctk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_dpgettext2 (NULL, context, items [i].name),
				    COLUMN_ITEM, items [i].value,
				    -1);
	}

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (combo_box),
				    renderer, TRUE);
	ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (combo_box),
					renderer, "text", COLUMN_TEXT, NULL);

	ctk_combo_box_set_active (CTK_COMBO_BOX (combo_box), 0);
}

static void
setup_options (void)
{
	CafePanelAppletsManager *manager;
	GList               *applet_list, *l;
	int                  i;
	int                  j;
	char                *prefs_path = NULL;
	char                *unique_key = NULL;
	gboolean             unique_key_found = FALSE;
	gchar              **dconf_paths;
	CtkListStore        *model;
	CtkTreeIter          iter;
	CtkCellRenderer     *renderer;

	model = ctk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	ctk_combo_box_set_model (CTK_COMBO_BOX (applet_combo),
				 CTK_TREE_MODEL (model));

	manager = g_object_new (PANEL_TYPE_APPLETS_MANAGER_DBUS, NULL);
	applet_list = CAFE_PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applets (manager);
	for (l = applet_list, i = 1; l; l = g_list_next (l), i++) {
		CafePanelAppletInfo *info = (CafePanelAppletInfo *)l->data;

		ctk_list_store_append (model, &iter);
		ctk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_strdup (cafe_panel_applet_info_get_name (info)),
				    COLUMN_ITEM, g_strdup (cafe_panel_applet_info_get_iid (info)),
				    -1);
	}
	g_list_free (applet_list);
	g_object_unref (manager);

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (applet_combo),
				    renderer, TRUE);
	ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (applet_combo),
					renderer, "text", COLUMN_TEXT, NULL);

	ctk_combo_box_set_active (CTK_COMBO_BOX (applet_combo), 0);

	setup_combo (size_combo, size_items, "Size",
		     G_N_ELEMENTS (size_items));
	setup_combo (orient_combo, orient_items, "Orientation",
		     G_N_ELEMENTS (orient_items));

	for (i = 0; !unique_key_found; i++)
	{
		g_free (unique_key);
		unique_key = g_strdup_printf ("cafe-panel-test-applet-%d", i);
		unique_key_found = TRUE;
		dconf_paths = cafe_dconf_list_subdirs ("/tmp/", TRUE);
		for (j = 0; dconf_paths[j] != NULL; j++)
		{
			if (g_strcmp0(unique_key, dconf_paths[j]) == 0) {
				unique_key_found = FALSE;
				break;
			}
		}
		if (dconf_paths)
			g_strfreev (dconf_paths);
	}

	prefs_path = g_strdup_printf ("/tmp/%s/", unique_key);
	if (unique_key)
		g_free (unique_key);
	ctk_entry_set_text (CTK_ENTRY (prefs_path_entry), prefs_path);
	g_free (prefs_path);
}

int
main (int argc, char **argv)
{
	CtkBuilder *builder;
	char       *applets_dir;
	GError     *error;

	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	error = NULL;
	if (!ctk_init_with_args (&argc, &argv,
				 "", (GOptionEntry *) options, GETTEXT_PACKAGE,
				 &error)) {
		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		} else
			g_printerr ("Cannot initialize CTK+.\n");

		return 1;
	}

	panel_modules_ensure_loaded ();

	if (g_file_test ("../libcafe-panel-applet", G_FILE_TEST_IS_DIR)) {
		applets_dir = g_strdup_printf ("%s:../libcafe-panel-applet", CAFE_PANEL_APPLETS_DIR);
		g_setenv ("CAFE_PANEL_APPLETS_DIR", applets_dir, FALSE);
		g_free (applets_dir);
	}

	if (cli_iid) {
		load_applet_from_command_line ();
		ctk_main ();
		panel_cleanup_do ();

		return 0;
	}

	builder = ctk_builder_new ();
	ctk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	ctk_builder_add_from_resource (builder, "/org/cafe/panel/test/panel-test-applets.ui", NULL);

	ctk_builder_connect_signals (builder, NULL);

	win             = CTK_WIDGET (ctk_builder_get_object (builder,
							      "toplevel"));
	applet_combo    = CTK_WIDGET (ctk_builder_get_object (builder,
							      "applet-combo"));
	prefs_path_entry = CTK_WIDGET (ctk_builder_get_object (builder,
							      "prefs-path-entry"));
	orient_combo    = CTK_WIDGET (ctk_builder_get_object (builder,
							      "orient-combo"));
	size_combo      = CTK_WIDGET (ctk_builder_get_object (builder,
							      "size-combo"));
	g_object_unref (builder);

	setup_options ();

	ctk_widget_show (win);

	ctk_main ();

	panel_cleanup_do ();

	return 0;
}
