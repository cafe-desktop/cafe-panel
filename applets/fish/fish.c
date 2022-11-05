/* fish.c:
 *
 * Copyright (C) 1998-2002 Free Software Foundation, Inc.
 * Copyright (C) 2002-2005 Vincent Untz
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
 *      George Lebl  <jirka@5z.com>
 *      Mark McLoughlin <mark@skynet.ie>
 *      Vincent Untz <vuntz@gnome.org>
 *      Stefano Karapetsas <stefano@karapetsas.com>
 */

#include <config.h>

#include <math.h>
#include <string.h>
#include <time.h>

#include <cairo.h>

#include <glib/gi18n.h>
#include <glib-object.h>
#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>
#include <gio/gio.h>

#include <cafe-panel-applet.h>
#include <cafe-panel-applet-gsettings.h>

#define FISH_APPLET(o) \
	(G_TYPE_CHECK_INSTANCE_CAST((o), fish_applet_get_type(), FishApplet))
#define FISH_IS_APPLET(o) \
	(G_TYPE_CHECK_INSTANCE_TYPE((o), FISH_TYPE_APPLET))

#define FISH_ICON "cafe-panel-fish"
#define FISH_RESOURCE_PATH "/org/cafe/panel/applet/fish/"

#define FISH_SCHEMA      "org.cafe.panel.applet.fish"
#define FISH_NAME_KEY    "name"
#define FISH_IMAGE_KEY   "image"
#define FISH_COMMAND_KEY "command"
#define FISH_FRAMES_KEY  "frames"
#define FISH_SPEED_KEY   "speed"
#define FISH_ROTATE_KEY  "rotate"

#define LOCKDOWN_SCHEMA                       "org.cafe.lockdown"
#define LOCKDOWN_DISABLE_COMMAND_LINE_KEY     "disable-command-line"

typedef struct {
	CafePanelApplet        applet;

	GSettings         *settings;
	GSettings         *lockdown_settings;

	char              *name;
	char              *image;
	char              *command;
	int                n_frames;
	gdouble            speed;
	gboolean           rotate;

	CafePanelAppletOrient  orientation;

	CtkWidget         *frame;
	CtkWidget         *drawing_area;
	CtkRequisition     requisition;
	CdkRectangle       prev_allocation;
	cairo_surface_t   *surface;
	gint               surface_width;
	gint               surface_height;

	guint              timeout;
	int                current_frame;
	gboolean           in_applet;

	CdkPixbuf         *pixbuf;

	CtkWidget         *preferences_dialog;
	CtkWidget         *name_entry;
	CtkWidget         *command_label;
	CtkWidget         *command_entry;
	CtkWidget         *preview_image;
	CtkWidget         *image_chooser;
	CtkWidget         *frames_spin;
	CtkWidget         *speed_spin;
	CtkWidget         *rotate_toggle;

	CtkWidget         *fortune_dialog;
	CtkWidget         *fortune_view;
	CtkWidget         *fortune_label;
	CtkWidget         *fortune_cmd_label;
	CtkTextBuffer	  *fortune_buffer;

	unsigned int       source_id;
	GIOChannel        *io_channel;

	gboolean           april_fools;
} FishApplet;

typedef struct {
	CafePanelAppletClass klass;
} FishAppletClass;


static gboolean load_fish_image          (FishApplet *fish);
static void     update_pixmap            (FishApplet *fish);
static void     something_fishy_going_on (FishApplet *fish, const char *message);
static void     display_fortune_dialog   (FishApplet *fish);
static void     set_tooltip              (FishApplet *fish);

static GType fish_applet_get_type (void);

static GObjectClass *parent_class;

static int fools_day        = 0;
static int fools_month      = 0;
static int fools_hour_start = 0;
static int fools_hour_end   = 0;

static char* get_image_path(FishApplet* fish)
{
	char *path;

	if (g_path_is_absolute (fish->image))
		path = g_strdup (fish->image);
	else
		path = g_strdup_printf ("%s/%s", FISH_ICONDIR, fish->image);

	return path;
}

static void show_help(FishApplet* fish, const char* link_id)
{
	GError *error = NULL;
	char   *uri;
#define FISH_HELP_DOC "cafe-fish"

	if (link_id)
		uri = g_strdup_printf ("help:%s/%s", FISH_HELP_DOC, link_id);
	else
		uri = g_strdup_printf ("help:%s", FISH_HELP_DOC);

	ctk_show_uri_on_window (NULL, uri,
			  ctk_get_current_event_time (), &error);
	g_free (uri);

	if (error &&
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_error_free (error);
	else if (error) {
		CtkWidget *dialog;
		char      *primary;

		primary = g_markup_printf_escaped (
				_("Could not display help document '%s'"),
				FISH_HELP_DOC);
		dialog = ctk_message_dialog_new (
				NULL,
				CTK_DIALOG_DESTROY_WITH_PARENT,
				CTK_MESSAGE_ERROR,
				CTK_BUTTONS_CLOSE,
				"%s", primary);

		ctk_message_dialog_format_secondary_text (
					CTK_MESSAGE_DIALOG (dialog),
					"%s", error->message);

		g_error_free (error);
		g_free (primary);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (ctk_widget_destroy),
				  NULL);

		ctk_window_set_icon_name (CTK_WINDOW (dialog), FISH_ICON);
		ctk_window_set_screen (CTK_WINDOW (dialog),
				       ctk_widget_get_screen (CTK_WIDGET (fish)));
		/* we have no parent window */
		ctk_window_set_skip_taskbar_hint (CTK_WINDOW (dialog), FALSE);
		ctk_window_set_title (CTK_WINDOW (dialog),
				      _("Error displaying help document"));

		ctk_widget_show (dialog);
	}
}

static void name_value_changed(CtkEntry* entry, FishApplet* fish)
{
	const char *text;

	text = ctk_entry_get_text (entry);

	if (!text || !text [0])
		return;

	g_settings_set_string (fish->settings, FISH_NAME_KEY, text);
}

static void image_value_changed(CtkFileChooser* chooser, FishApplet* fish)
{	char *path;
	char *image;
	char *path_gsettings;

	path = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (chooser));

	if (!path || !path[0]) {
		g_free (path);
		return;
	}

	path_gsettings = get_image_path (fish);
	if (!strcmp (path, path_gsettings)) {
		g_free (path);
		g_free (path_gsettings);
		return;
	}
	g_free (path_gsettings);

	if (!strncmp (path, FISH_ICONDIR, strlen (FISH_ICONDIR))) {
		image = path + strlen (FISH_ICONDIR);
		while (*image && *image == G_DIR_SEPARATOR)
			image++;
	} else
		image = path;

	g_settings_set_string (fish->settings, FISH_IMAGE_KEY, image);

	g_free (path);
}

static void command_value_changed(CtkEntry* entry, FishApplet *fish)
{
	const char *text;

	text = ctk_entry_get_text (entry);

	if (!text || !text [0]) {
		g_settings_set_string (fish->settings, FISH_COMMAND_KEY, "");
		return;
	}

	if (!strncmp (text, "ps ", 3)  ||
	    !strcmp  (text, "ps")      ||
	    !strncmp (text, "who ", 4) ||
	    !strcmp  (text, "who")     ||
	    !strcmp  (text, "uptime")  ||
	    !strncmp (text, "tail ", 5)) {
		static gboolean message_given = FALSE;
		char       *message;
		const char *warning_format =
				_("Warning:  The command "
				  "appears to be something actually useful.\n"
				   "Since this is a useless applet, you "
				   "may not want to do this.\n"
				   "We strongly advise you against "
				   "using %s for anything\n"
				   "which would make the applet "
				   "\"practical\" or useful.");

		if ( ! message_given) {
			message = g_strdup_printf (warning_format, fish->name);

			something_fishy_going_on (fish, message);

			g_free (message);

			message_given = TRUE;
		}
	}

	g_settings_set_string (fish->settings, FISH_COMMAND_KEY, text);
}

static void n_frames_value_changed(CtkSpinButton* button, FishApplet* fish)
{
        g_settings_set_int (
			fish->settings,
			FISH_FRAMES_KEY,
			ctk_spin_button_get_value_as_int (button));
}

static void speed_value_changed (CtkSpinButton* button, FishApplet* fish)
{
        g_settings_set_double (
			fish->settings,
			FISH_SPEED_KEY,
			ctk_spin_button_get_value (button));
}

static void rotate_value_changed(CtkToggleButton* toggle, FishApplet* fish)
{
		g_settings_set_boolean (
			fish->settings,
			FISH_ROTATE_KEY,
			ctk_toggle_button_get_active (toggle));
}

static gboolean delete_event(CtkWidget* widget, FishApplet* fish)
{
	ctk_widget_hide (widget);

	return TRUE;
}

static void handle_response(CtkWidget* widget, int id, FishApplet* fish)
{
	if (id == CTK_RESPONSE_HELP) {
		show_help (fish, "fish-settings");
		return;
	}

	ctk_widget_hide (fish->preferences_dialog);
}

static void setup_sensitivity(FishApplet* fish, CtkBuilder* builder, const char* wid, const char* label, const char* label_post, const char* key)
{
	CtkWidget *w;

	if (g_settings_is_writable (fish->settings, key)) {
		return;
	}

	w = CTK_WIDGET (ctk_builder_get_object (builder, wid));
	g_assert (w != NULL);
	ctk_widget_set_sensitive (w, FALSE);

	if (label != NULL) {
		w = CTK_WIDGET (ctk_builder_get_object (builder, label));
		g_assert (w != NULL);
		ctk_widget_set_sensitive (w, FALSE);
	}
	if (label_post != NULL) {
		w = CTK_WIDGET (ctk_builder_get_object (builder, label_post));
		g_assert (w != NULL);
		ctk_widget_set_sensitive (w, FALSE);
	}

}

static void chooser_preview_update(CtkFileChooser* file_chooser, gpointer data)
{
	CtkWidget *preview;
	char      *filename;
	CdkPixbuf *pixbuf;
	gboolean   have_preview;

	preview = CTK_WIDGET (data);
	filename = ctk_file_chooser_get_preview_filename (file_chooser);

	if (filename == NULL)
		return;

	pixbuf = cdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
	have_preview = (pixbuf != NULL);
	g_free (filename);

	ctk_image_set_from_pixbuf (CTK_IMAGE (preview), pixbuf);
	if (pixbuf)
		g_object_unref (pixbuf);

	ctk_file_chooser_set_preview_widget_active (file_chooser,
						    have_preview);
}

static void display_preferences_dialog(CtkAction* action, FishApplet* fish)
{
	CtkBuilder    *builder;
	CtkWidget     *button;
	CtkFileFilter *filter;
	CtkWidget     *chooser_preview;
	char          *path;

	if (fish->preferences_dialog) {
		ctk_window_set_screen (CTK_WINDOW (fish->preferences_dialog),
				       ctk_widget_get_screen (CTK_WIDGET (fish)));
		ctk_window_present (CTK_WINDOW (fish->preferences_dialog));
		return;
	}

	builder = ctk_builder_new ();
	ctk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	ctk_builder_add_from_resource (builder, FISH_RESOURCE_PATH "fish.ui", NULL);

	fish->preferences_dialog = CTK_WIDGET (ctk_builder_get_object (builder, "fish_preferences_dialog"));

	g_object_add_weak_pointer (G_OBJECT (fish->preferences_dialog),
				   (void**) &fish->preferences_dialog);

	ctk_window_set_icon_name (CTK_WINDOW (fish->preferences_dialog),
				  FISH_ICON);
	ctk_dialog_set_default_response (
		CTK_DIALOG (fish->preferences_dialog), CTK_RESPONSE_OK);

	fish->name_entry = CTK_WIDGET (ctk_builder_get_object (builder, "name_entry"));
	ctk_entry_set_text (CTK_ENTRY (fish->name_entry), fish->name);

	g_signal_connect (fish->name_entry, "changed",
			  G_CALLBACK (name_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "name_entry" /* wid */,
			   "name_label" /* label */,
			   NULL /* label_post */,
			   FISH_NAME_KEY /* key */);

	fish->preview_image = CTK_WIDGET (ctk_builder_get_object (builder, "preview_image"));
	if (fish->pixbuf)
		ctk_image_set_from_pixbuf (CTK_IMAGE (fish->preview_image),
					   fish->pixbuf);

	fish->image_chooser =  CTK_WIDGET (ctk_builder_get_object (builder, "image_chooser"));
	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, _("Images"));
	ctk_file_filter_add_pixbuf_formats (filter);
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (fish->image_chooser),
				     filter);
	ctk_file_chooser_set_filter (CTK_FILE_CHOOSER (fish->image_chooser),
				     filter);
	chooser_preview = ctk_image_new ();
	ctk_file_chooser_set_preview_widget (CTK_FILE_CHOOSER (fish->image_chooser),
					     chooser_preview);
	g_signal_connect (fish->image_chooser, "update-preview",
			  G_CALLBACK (chooser_preview_update), chooser_preview);
	path = get_image_path (fish);
	ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (fish->image_chooser),
				       path);
	g_free (path);

	g_signal_connect (fish->image_chooser, "selection-changed",
			  G_CALLBACK (image_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "image_chooser" /* wid */,
			   "image_label" /* label */,
			   NULL /* label_post */,
			   FISH_IMAGE_KEY /* key */);

	fish->command_label = CTK_WIDGET (ctk_builder_get_object (builder, "command_label"));
	fish->command_entry = CTK_WIDGET (ctk_builder_get_object (builder, "command_entry"));
	ctk_entry_set_text (CTK_ENTRY (fish->command_entry), fish->command);

	g_signal_connect (fish->command_entry, "changed",
			  G_CALLBACK (command_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "command_entry" /* wid */,
			   "command_label" /* label */,
			   NULL /* label_post */,
			   FISH_COMMAND_KEY /* key */);

	if (g_settings_get_boolean (fish->lockdown_settings, LOCKDOWN_DISABLE_COMMAND_LINE_KEY)) {
		ctk_widget_set_sensitive (fish->command_label, FALSE);
		ctk_widget_set_sensitive (fish->command_entry, FALSE);
	}

	fish->frames_spin = CTK_WIDGET (ctk_builder_get_object (builder, "frames_spin"));
	ctk_spin_button_set_value (CTK_SPIN_BUTTON (fish->frames_spin),
				   fish->n_frames);

	g_signal_connect (fish->frames_spin, "value_changed",
			  G_CALLBACK (n_frames_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "frames_spin" /* wid */,
			   "frames_label" /* label */,
			   "frames_post_label" /* label_post */,
			   FISH_FRAMES_KEY /* key */);

	fish->speed_spin = CTK_WIDGET (ctk_builder_get_object (builder, "speed_spin"));
	ctk_spin_button_set_value (CTK_SPIN_BUTTON (fish->speed_spin), fish->speed);

	g_signal_connect (fish->speed_spin, "value_changed",
			  G_CALLBACK (speed_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "speed_spin" /* wid */,
			   "speed_label" /* label */,
			   "speed_post_label" /* label_post */,
			   FISH_SPEED_KEY /* key */);

	fish->rotate_toggle = CTK_WIDGET (ctk_builder_get_object (builder, "rotate_toggle"));
	ctk_toggle_button_set_active (
		CTK_TOGGLE_BUTTON (fish->rotate_toggle), fish->rotate);

	g_signal_connect (fish->rotate_toggle, "toggled",
			  G_CALLBACK (rotate_value_changed), fish);

	setup_sensitivity (fish, builder,
			   "rotate_toggle" /* wid */,
			   NULL /* label */,
			   NULL /* label_post */,
			   FISH_ROTATE_KEY /* key */);

	g_signal_connect (fish->preferences_dialog, "delete_event",
			  G_CALLBACK (delete_event), fish);
	g_signal_connect (fish->preferences_dialog, "response",
			  G_CALLBACK (handle_response), fish);

	button = CTK_WIDGET (ctk_builder_get_object (builder, "done_button"));
        g_signal_connect_swapped (button, "clicked",
				  (GCallback) ctk_widget_hide,
				  fish->preferences_dialog);

	ctk_window_set_screen (CTK_WINDOW (fish->preferences_dialog),
			       ctk_widget_get_screen (CTK_WIDGET (fish)));
	ctk_window_set_resizable (CTK_WINDOW (fish->preferences_dialog), FALSE);
	ctk_window_present (CTK_WINDOW (fish->preferences_dialog));

	g_object_unref (builder);
}

static void display_help_dialog(CtkAction* action, FishApplet* fish)
{
	show_help(fish, NULL);
}

static void display_about_dialog(CtkAction* action, FishApplet* fish)
{
	const char* author_format = _("%s the Fish");
	const char* about_format = _("%s has no use what-so-ever. "
				     "It only takes up disk space and "
				     "compilation time, and if loaded it also "
				     "takes up precious panel space and "
				     "memory. Anybody found using it should be "
				     "promptly sent for a psychiatric "
				     "evaluation.");
	const char* documenters [] = {
		"Telsa Gwynne <hobbit@aloss.ukuu.org.uk>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
          	NULL
	};

	char* authors[3];
	char* descr;

	authors[0] = g_strdup_printf(author_format, fish->name);
	authors[1] = _("(with minor help from George)");
	authors[2] = NULL;

	descr = g_strdup_printf(about_format, fish->name);

	ctk_show_about_dialog(NULL,
		"program-name", _("Fish"),
		"title", _("About Fish"),
		"authors", authors,
		"comments", descr,
		"copyright", _("Copyright \xc2\xa9 1998-2002 Free Software Foundation, Inc.\n"
		               "Copyright \xc2\xa9 2002-2005 Vincent Untz\n"
		               "Copyright \xc2\xa9 2012-2020 CAFE developers"),
		"documenters", documenters,
		"logo-icon-name", FISH_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION, // "3.4.7.4ac19"
		"website", "http://cafe-desktop.org/",
		NULL);

	g_free(descr);
	g_free(authors[0]);
}

static void set_ally_name_desc(CtkWidget* widget, FishApplet* fish)
{
	const char *name_format = _("%s the Fish");
	const char *desc_format = _("%s the Fish, a contemporary oracle");
	AtkObject  *obj;
	char       *desc, *name;

	obj = ctk_widget_get_accessible (widget);
	/* Return immediately if GAIL is not loaded */
	if (!CTK_IS_ACCESSIBLE (obj))
		return;

	name = g_strdup_printf (name_format, fish->name);
	atk_object_set_name (obj, name);
	g_free (name);

	desc = g_strdup_printf (desc_format, fish->name);
	atk_object_set_description (obj, desc);
	g_free (desc);
}

static void something_fishy_going_on(FishApplet* fish, const char* message)
{
	CtkWidget *dialog;

	dialog = ctk_message_dialog_new (NULL,
					 CTK_DIALOG_DESTROY_WITH_PARENT,
					 CTK_MESSAGE_ERROR,
					 CTK_BUTTONS_CLOSE,
					 "%s", message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ctk_widget_destroy),
			  NULL);

	ctk_window_set_icon_name (CTK_WINDOW (dialog), FISH_ICON);
	ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);
	ctk_window_set_screen (CTK_WINDOW (dialog),
			       ctk_widget_get_screen (CTK_WIDGET (fish)));
	ctk_widget_show (dialog);
}

static gboolean locate_fortune_command (FishApplet* fish, int* argcp, char*** argvp)
{
	char *prog = NULL;

	if (fish->command
	    && g_shell_parse_argv (fish->command, argcp, argvp, NULL)) {
		prog = g_find_program_in_path ((*argvp)[0]);
		if (prog) {
			g_free (prog);
			return TRUE;
		}

		g_strfreev (*argvp);
	}

	prog = g_find_program_in_path ("fortune");
	if (prog) {
		g_free (prog);
		if (g_shell_parse_argv ("fortune", argcp, argvp, NULL))
			return FALSE;
	}

	if (g_file_test ("/usr/games/fortune", G_FILE_TEST_IS_EXECUTABLE)
	    && g_shell_parse_argv ("/usr/games/fortune", argcp, argvp, NULL))
		return FALSE;

	something_fishy_going_on (fish,
				  _("Unable to locate the command to execute"));
	*argvp = NULL;
	return FALSE;
}

#define FISH_RESPONSE_SPEAK 1
static inline void fish_close_channel(FishApplet* fish)
{
	if (fish->io_channel) {
		g_io_channel_shutdown (fish->io_channel, TRUE, NULL);
		g_io_channel_unref (fish->io_channel);
	}
	fish->io_channel = NULL;
}

static void handle_fortune_response(CtkWidget* widget, int id, FishApplet* fish)
{
	if (id == FISH_RESPONSE_SPEAK)
		display_fortune_dialog (fish);
	else {
		/* if there is still a pipe, close it: if we hide the widget,
		 * the * output can't be seen */
		if (fish->source_id)
			g_source_remove (fish->source_id);
		fish->source_id = 0;
		fish_close_channel (fish);
		ctk_widget_hide (fish->fortune_dialog);
	}
}

static void update_fortune_dialog(FishApplet* fish)
{
	char *label_text;
	char *text;

	if (!fish->fortune_dialog || !fish->name)
		return;

	/* xgettext:no-c-format */
	text = g_strdup_printf (_("%s the Fish"), fish->name);
	ctk_window_set_title (CTK_WINDOW (fish->fortune_dialog), text);
	g_free (text);

	/* xgettext:no-c-format */
	label_text = g_strdup_printf (_("%s the Fish Says:"), fish->name);

	text = g_strdup_printf ("<big><big>%s</big></big>", label_text);
	ctk_label_set_markup (CTK_LABEL (fish->fortune_label), text);
	g_free (text);

	g_free (label_text);

	set_ally_name_desc (fish->fortune_view, fish);
}

static void insert_fortune_text(FishApplet* fish, const char* text)
{
	CtkTextIter iter;

	ctk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &iter, -1);

	ctk_text_buffer_insert_with_tags_by_name (fish->fortune_buffer, &iter,
						  text, -1, "monospace_tag",
						  NULL);

	while (ctk_events_pending ())
	  ctk_main_iteration ();
}

static void clear_fortune_text(FishApplet* fish)
{
	CtkTextIter begin, end;

	ctk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &begin, 0);
	ctk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &end, -1);

	ctk_text_buffer_delete (fish->fortune_buffer, &begin, &end);
	ctk_text_buffer_remove_tag_by_name (fish->fortune_buffer,
					    "monospace_tag", &begin, &end);

	/* insert an empty line */
	insert_fortune_text (fish, "\n");
}

static gboolean fish_read_output(GIOChannel* source, GIOCondition condition, gpointer data)
{
	char        output[4096];
	char       *utf8_output;
	gsize       bytes_read;
	GError     *error = NULL;
	GIOStatus   status;
	FishApplet *fish;

	fish = (FishApplet *) data;

	if (!(condition & G_IO_IN)) {
		fish->source_id = 0;
		fish_close_channel (fish);
		return FALSE;
	}

	status = g_io_channel_read_chars (source, output, 4096, &bytes_read,
					  &error);

	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to read output from command\n\nDetails: %s"),
					   error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		fish->source_id = 0;
		fish_close_channel (fish);
		return FALSE;
	}

	if (status == G_IO_STATUS_AGAIN)
		return TRUE;

	if (bytes_read > 0) {
		/* The output is not guarantied to be in UTF-8 format, most
		 * likely it's just in ASCII-7 or in the user locale
		 */
		if (!g_utf8_validate (output, -1, NULL))
			utf8_output = g_locale_to_utf8 (output, bytes_read,
							NULL, NULL, NULL);
		else
			utf8_output = g_strndup (output, bytes_read);

		if (utf8_output)
			insert_fortune_text (fish, utf8_output);

		g_free (utf8_output);
	}

	if (status == G_IO_STATUS_EOF) {
		fish->source_id = 0;
		fish_close_channel (fish);
	}
	return (status != G_IO_STATUS_EOF);
}

/*
 * Set the DISPLAY variable, to be use by g_spawn_async.
 */
static void
set_environment (gpointer display)
{
	g_setenv ("DISPLAY", display, TRUE);
}

static CtkWidget*
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

static void display_fortune_dialog(FishApplet* fish)
{
	GError      *error = NULL;
	gboolean     user_command;
	int          output;
	const char  *charset;
	int          argc;
	char       **argv;
	CdkDisplay  *display;
	CdkScreen   *screen;
	char        *display_name;

	/* if there is still a pipe, close it */
	if (fish->source_id)
		g_source_remove (fish->source_id);
	fish->source_id = 0;
	fish_close_channel (fish);

	user_command = locate_fortune_command (fish, &argc, &argv);
	if (!argv)
		return;

	if (!fish->fortune_dialog) {
		CtkWidget *scrolled;
		CtkWidget *vbox;
		CdkMonitor *monitor;
		CdkRectangle monitor_geom;

		fish->fortune_dialog = ctk_dialog_new ();
		ctk_window_set_title (CTK_WINDOW (fish->fortune_dialog), "");

		ctk_dialog_add_button (CTK_DIALOG (fish->fortune_dialog),
				       _("_Speak again"),
				       FISH_RESPONSE_SPEAK);

		panel_dialog_add_button (CTK_DIALOG (fish->fortune_dialog),
					 _("_Close"), "window-close",
					 CTK_RESPONSE_CLOSE);

		ctk_window_set_icon_name (CTK_WINDOW (fish->fortune_dialog),
					  FISH_ICON);

		ctk_dialog_set_default_response (
			CTK_DIALOG (fish->fortune_dialog), CTK_RESPONSE_CLOSE);

		g_signal_connect (fish->fortune_dialog, "delete_event",
				  G_CALLBACK (delete_event), fish);
		g_signal_connect (fish->fortune_dialog, "response",
				  G_CALLBACK (handle_fortune_response), fish);

		monitor = cdk_display_get_monitor_at_window (ctk_widget_get_display (CTK_WIDGET (fish)),
							     ctk_widget_get_window (CTK_WIDGET (fish)));
		cdk_monitor_get_geometry(monitor, &monitor_geom);
		ctk_window_set_default_size (CTK_WINDOW (fish->fortune_dialog),
					     MIN (600, monitor_geom.width  * 0.9),
					     MIN (350, monitor_geom.height * 0.9));

		fish->fortune_view = ctk_text_view_new ();
		ctk_text_view_set_editable (CTK_TEXT_VIEW (fish->fortune_view), FALSE);
		ctk_text_view_set_cursor_visible (CTK_TEXT_VIEW (fish->fortune_view), FALSE);
		ctk_text_view_set_left_margin (CTK_TEXT_VIEW (fish->fortune_view), 10);
		ctk_text_view_set_right_margin (CTK_TEXT_VIEW (fish->fortune_view), 10);
		fish->fortune_buffer =
			ctk_text_view_get_buffer (CTK_TEXT_VIEW (fish->fortune_view));

		ctk_text_buffer_create_tag (CTK_TEXT_BUFFER (fish->fortune_buffer),
					    "monospace_tag", "family",
					    "Monospace", NULL);

		scrolled = ctk_scrolled_window_new (NULL, NULL);
		ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (scrolled),
						CTK_POLICY_AUTOMATIC,
						CTK_POLICY_AUTOMATIC);
		ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (scrolled),
						     CTK_SHADOW_IN);

		ctk_container_add (CTK_CONTAINER (scrolled), fish->fortune_view);

		fish->fortune_label = ctk_label_new ("");
		ctk_label_set_ellipsize (CTK_LABEL (fish->fortune_label),
					 PANGO_ELLIPSIZE_MIDDLE);
		fish->fortune_cmd_label = ctk_label_new ("");
		ctk_label_set_xalign (CTK_LABEL (fish->fortune_cmd_label), 0.0);
		ctk_label_set_yalign (CTK_LABEL (fish->fortune_cmd_label), 0.5);

		vbox = ctk_dialog_get_content_area (CTK_DIALOG (fish->fortune_dialog));
		ctk_box_pack_start (CTK_BOX (vbox),
				    fish->fortune_label,
				    FALSE, FALSE, 6);

		ctk_box_pack_start (CTK_BOX (vbox),
				    scrolled,
				    TRUE, TRUE, 6);

		ctk_box_pack_start (CTK_BOX (vbox),
				    fish->fortune_cmd_label,
				    FALSE, FALSE, 6);

		update_fortune_dialog (fish);

		/* We don't show_all for the dialog since fortune_cmd_label
		 * might need to be hidden
		 * The dialog will be shown with ctk_window_present later */
		ctk_widget_show (scrolled);
		ctk_widget_show (fish->fortune_view);
		ctk_widget_show (fish->fortune_label);
	}

	if (!user_command) {
		char *command;
		char * text;

		command = g_markup_printf_escaped ("<tt>%s</tt>", argv[0]);
		text = g_strdup_printf (_("The configured command is not "
					  "working and has been replaced by: "
					  "%s"), command);
		ctk_label_set_markup (CTK_LABEL (fish->fortune_cmd_label),
				      text);
		g_free (command);
		g_free (text);
		ctk_widget_show (fish->fortune_cmd_label);
	} else {
		ctk_widget_hide (fish->fortune_cmd_label);
	}

	clear_fortune_text (fish);

	screen = ctk_widget_get_screen (CTK_WIDGET (fish));
	display = cdk_screen_get_display (screen);
	display_name = g_strdup (cdk_display_get_name (display));
	g_spawn_async_with_pipes (NULL, /* working directory */
							  argv,
							  NULL, /* envp */
							  G_SPAWN_SEARCH_PATH|G_SPAWN_STDERR_TO_DEV_NULL,
							  set_environment,
							  &display_name,
							  NULL, /* child pid */
							  NULL, /* stdin */
							  &output,
							  NULL, /* stderr */
							  &error);
	g_free (display_name);

	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to execute '%s'\n\nDetails: %s"),
					   argv[0], error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		g_strfreev (argv);
		return;
	}

	fish->io_channel = g_io_channel_unix_new (output);
	/* set the correct encoding if the locale is not using UTF-8 */
	if (!g_get_charset (&charset))
		g_io_channel_set_encoding(fish->io_channel, charset, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to read from '%s'\n\nDetails: %s"),
					   argv[0], error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		g_strfreev (argv);
		return;
	}

	g_strfreev (argv);

	fish->source_id = g_io_add_watch (fish->io_channel,
					  G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
					  fish_read_output, fish);

	ctk_window_set_screen (CTK_WINDOW (fish->fortune_dialog),
			       ctk_widget_get_screen (CTK_WIDGET (fish)));
	ctk_window_present (CTK_WINDOW (fish->fortune_dialog));
}

static void name_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	char *value;

	value = g_settings_get_string (settings, key);

	if (!value [0] || (fish->name && !strcmp (fish->name, value)))
		return;

	if (fish->name)
		g_free (fish->name);
	fish->name = g_strdup (value);

	update_fortune_dialog (fish);
	set_tooltip (fish);
	set_ally_name_desc (CTK_WIDGET (fish), fish);

	if (fish->name_entry &&
	    strcmp (ctk_entry_get_text (CTK_ENTRY (fish->name_entry)), fish->name))
		ctk_entry_set_text (CTK_ENTRY (fish->name_entry), fish->name);

	if (value)
		g_free (value);
}

static void image_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	char *value;

	value = g_settings_get_string (settings, key);

	if (!value [0] || (fish->image && !strcmp (fish->image, value)))
		return;

	if (fish->image)
		g_free (fish->image);
	fish->image = g_strdup (value);

	load_fish_image (fish);
	update_pixmap (fish);

	if (fish->image_chooser) {
		char *path_gsettings;
		char *path_chooser;

		path_gsettings = get_image_path (fish);
		path_chooser = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (fish->image_chooser));
		if (strcmp (path_gsettings, path_chooser))
			ctk_file_chooser_set_filename (CTK_FILE_CHOOSER (fish->image_chooser),
						       path_gsettings);

		g_free (path_gsettings);
		g_free (path_chooser);
	}

	if (value)
		g_free (value);
}

static void command_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	char *value;

	value = g_settings_get_string (settings, key);

	if (fish->command && !strcmp (fish->command, value))
		return;

	if (fish->command)
		g_free (fish->command);
	fish->command = g_strdup (value);

	if (fish->command_entry &&
	    strcmp (ctk_entry_get_text (CTK_ENTRY (fish->command_entry)), fish->command))
		ctk_entry_set_text (CTK_ENTRY (fish->command_entry), fish->command);

	if (value)
		g_free (value);
}

static void n_frames_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	int value;

	value = g_settings_get_int (settings, key);

	if (fish->n_frames == value)
		return;

	fish->n_frames = value;

	if (fish->n_frames <= 0)
		fish->n_frames = 1;

	update_pixmap (fish);

	if (fish->frames_spin &&
	    ctk_spin_button_get_value_as_int (CTK_SPIN_BUTTON (fish->frames_spin)) != fish->n_frames)
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (fish->frames_spin), fish->n_frames);
}

static char* get_location(void)
{
	static char  location [256];
	char        *buffer;
	FILE        *zone;
	int          i, len, count;

	/* Old method : works for glibc < 2.2 */
	zone = fopen("/etc/timezone", "r");
	if (zone) {
		count = fscanf (zone, "%255s", location);
		fclose (zone);
		/* if we could read it, we return what we got */
		if (count == 1)
			return location;
	}

	/* New method : works for glibc 2.2 */
	/* FIXME: this is broken for many distros, see the clock code */
	buffer = g_file_read_link ("/etc/localtime", NULL);
	if (!buffer)
		return NULL;

	len = strlen (buffer);
	for (i = len, count = 0; (i > 0) && (count != 2); i--)
		if (buffer [i] == '/')
			count++;

	if (count != 2) {
		g_free (buffer);
		return NULL;
	}

	memcpy (location, &buffer [i + 2], len - i - 2);
	g_free (buffer);

	return location;
}

static void init_fools_day(void)
{
	const char *spanish_timezones [] = {
		"Europe/Madrid",
		"Africa/Ceuta",
		"Atlantic/Canary",
		"America/Mexico_City",
		"Mexico/BajaSur",
		"Mexico/BajaNorte",
		"Mexico/General",
		NULL
	};
	char *location;
	int  i;

	if (!(location = get_location ()))
		return;

	fools_day        = 1;  /* 1st */
	fools_month      = 3;  /* April */
	fools_hour_start = 0;  /* Midnight */
	fools_hour_end   = 12; /* Apparently jokes should stop at midday */

	for (i = 0; spanish_timezones [i]; i++)
		if (!g_ascii_strcasecmp (spanish_timezones [i], location)) {
			/* Hah!, We are in Spain or Mexico
			 * Spanish fool's day is 28th December
			 */
			fools_day = 28;
			fools_month = 11;
			return;
		}
}

static void check_april_fools(FishApplet* fish)
{
	struct tm *tm;
	time_t     now;

	time (&now);
	tm = localtime (&now);

	if (fish->april_fools &&
	    (tm->tm_mon  != fools_month ||
	     tm->tm_mday != fools_day   ||
	     tm->tm_hour >= fools_hour_end)) {
		fish->april_fools = FALSE;
		update_pixmap (fish);
	} else if (tm->tm_mon  == fools_month    &&
		 tm->tm_mday == fools_day        &&
		 tm->tm_hour >= fools_hour_start &&
		 tm->tm_hour <= fools_hour_end) {
		fish->april_fools = TRUE;
		update_pixmap (fish);
	}
}

static gboolean timeout_handler(gpointer data)
{
	FishApplet *fish = (FishApplet *) data;

	check_april_fools (fish);

	if (fish->april_fools)
		return TRUE;

	fish->current_frame++;
	if (fish->current_frame >= fish->n_frames)
		fish->current_frame = 0;

	ctk_widget_queue_draw (fish->drawing_area);

	return TRUE;
}

static void setup_timeout(FishApplet *fish)
{
	if (fish->timeout)
		g_source_remove (fish->timeout);

	fish->timeout = g_timeout_add (fish->speed * 1000,
				       timeout_handler,
				       fish);
}

static void speed_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	gdouble value;

	value = g_settings_get_double (settings, key);

	if (fish->speed == value)
		return;
	fish->speed = value;

	setup_timeout (fish);

	if (fish->speed_spin &&
	    ctk_spin_button_get_value (CTK_SPIN_BUTTON (fish->frames_spin)) != fish->speed)
		ctk_spin_button_set_value (CTK_SPIN_BUTTON (fish->speed_spin), fish->speed);
}

static void rotate_changed_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	gboolean value;

	value = g_settings_get_boolean (settings, key);

	if (fish->rotate == value)
		return;
	fish->rotate = value;

	if (fish->orientation == CAFE_PANEL_APPLET_ORIENT_LEFT ||
	    fish->orientation == CAFE_PANEL_APPLET_ORIENT_RIGHT)
		update_pixmap (fish);

	if (fish->rotate_toggle &&
	    ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (fish->rotate_toggle)) != fish->rotate)
		ctk_toggle_button_set_active (
			CTK_TOGGLE_BUTTON (fish->rotate_toggle), fish->rotate);
}

static void fish_disable_commande_line_notify(GSettings* settings, gchar* key, FishApplet* fish)
{
	gboolean locked_down;

	locked_down = !g_settings_get_boolean (settings, key);

	if (fish->command_label != NULL)
		ctk_widget_set_sensitive (fish->command_label, locked_down);
	if (fish->command_entry != NULL)
		ctk_widget_set_sensitive (fish->command_entry, locked_down);
}

static void setup_gsettings(FishApplet* fish)
{
	CafePanelApplet *applet = (CafePanelApplet *) fish;

	fish->settings = cafe_panel_applet_settings_new (applet, FISH_SCHEMA);
	fish->lockdown_settings = g_settings_new (LOCKDOWN_SCHEMA);

	g_signal_connect (fish->settings,
					  "changed::" FISH_NAME_KEY,
					  G_CALLBACK (name_changed_notify),
					  fish);
	g_signal_connect (fish->settings,
					  "changed::" FISH_IMAGE_KEY,
					  G_CALLBACK (image_changed_notify),
					  fish);
	g_signal_connect (fish->settings,
					  "changed::" FISH_COMMAND_KEY,
					  G_CALLBACK (command_changed_notify),
					  fish);
	g_signal_connect (fish->settings,
					  "changed::" FISH_FRAMES_KEY,
					  G_CALLBACK (n_frames_changed_notify),
					  fish);
	g_signal_connect (fish->settings,
					  "changed::" FISH_SPEED_KEY,
					  G_CALLBACK (speed_changed_notify),
					  fish);
	g_signal_connect (fish->settings,
					  "changed::" FISH_ROTATE_KEY,
					  G_CALLBACK (rotate_changed_notify),
					  fish);
	g_signal_connect (fish->lockdown_settings,
					  "changed::" LOCKDOWN_DISABLE_COMMAND_LINE_KEY,
					  G_CALLBACK (fish_disable_commande_line_notify),
					  fish);
}

static gboolean load_fish_image(FishApplet* fish)
{
	CdkPixbuf *pixbuf;
	GError    *error = NULL;
	char      *path = NULL;

	if (!fish->image)
		return FALSE;

	path = get_image_path (fish);

	pixbuf = cdk_pixbuf_new_from_file (path, &error);
	if (error) {
		g_warning ("Cannot load '%s': %s", path, error->message);
		g_error_free (error);
		g_free (path);
		return FALSE;
	}

	if (fish->pixbuf)
		g_object_unref (fish->pixbuf);
	fish->pixbuf = pixbuf;

	if (fish->preview_image)
		ctk_image_set_from_pixbuf (CTK_IMAGE (fish->preview_image),
					   fish->pixbuf);

	g_free (path);

	return TRUE;
}

static gboolean
update_pixmap_in_idle (gpointer data)
{
	update_pixmap (FISH_APPLET (data));
	return FALSE;
}

static void update_pixmap(FishApplet* fish)
{
	CtkWidget     *widget = fish->drawing_area;
	CtkRequisition prev_requisition;
	CtkAllocation  allocation;
	int            width  = -1;
	int            height = -1;
	int            pixbuf_width = -1;
	int            pixbuf_height = -1;
	gboolean       rotate = FALSE;
	cairo_t       *cr;
	cairo_matrix_t matrix;
	cairo_pattern_t *pattern;

	ctk_widget_get_allocation (widget, &allocation);

	if (!ctk_widget_get_realized (widget) ||
	    allocation.width <= 0 ||
	    allocation.height <= 0)
		return;

	if (!fish->pixbuf && !load_fish_image (fish))
		return;

	if (fish->rotate &&
	    (fish->orientation == CAFE_PANEL_APPLET_ORIENT_LEFT ||
	     fish->orientation == CAFE_PANEL_APPLET_ORIENT_RIGHT))
		rotate = TRUE;

	pixbuf_width  = cdk_pixbuf_get_width  (fish->pixbuf);
	pixbuf_height = cdk_pixbuf_get_height (fish->pixbuf);

	prev_requisition = fish->requisition;

	if (fish->orientation == CAFE_PANEL_APPLET_ORIENT_UP ||
	    fish->orientation == CAFE_PANEL_APPLET_ORIENT_DOWN) {
		height = allocation.height;
		width  = pixbuf_width * ((gdouble) height / pixbuf_height);
		fish->requisition.width = width / fish->n_frames;
		fish->requisition.height = height;
	} else {
		if (!rotate) {
			width = allocation.width * fish->n_frames;
			height = pixbuf_height * ((gdouble) width / pixbuf_width);
			fish->requisition.width = allocation.width;
			fish->requisition.height = height;
		} else {
			width = allocation.width;
			height = pixbuf_width * ((gdouble) width / pixbuf_height);
			fish->requisition.width = width;
			fish->requisition.height = height / fish->n_frames;
		}
	}

	if (prev_requisition.width  != fish->requisition.width ||
	    prev_requisition.height != fish->requisition.height) {
		ctk_widget_set_size_request (widget,
					     fish->requisition.width,
					     fish->requisition.height);
		}

	g_assert (width != -1 && height != -1);

	if (width == 0 || height == 0)
		return;

	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = cdk_window_create_similar_surface (ctk_widget_get_window (widget),
													   CAIRO_CONTENT_COLOR_ALPHA,
													   width, height);
	fish->surface_width = width;
	fish->surface_height = height;

	ctk_widget_queue_resize (widget);

	g_assert (pixbuf_width != -1 && pixbuf_height != -1);

	cr = cairo_create (fish->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cdk_cairo_set_source_pixbuf (cr, fish->pixbuf, 0, 0);
	pattern = cairo_get_source (cr);
	cairo_pattern_set_filter (pattern, CAIRO_FILTER_BEST);

	cairo_matrix_init_identity (&matrix);

	if (fish->april_fools) {
		cairo_matrix_translate (&matrix,
					pixbuf_width - 1, pixbuf_height - 1);
		cairo_matrix_rotate (&matrix, M_PI);
	}

	if (rotate) {
		if (fish->orientation == CAFE_PANEL_APPLET_ORIENT_RIGHT) {
			cairo_matrix_translate (&matrix, pixbuf_width - 1, 0);
			cairo_matrix_rotate (&matrix, M_PI * 0.5);
		} else {
			cairo_matrix_translate (&matrix, 0, pixbuf_height - 1);
			cairo_matrix_rotate (&matrix, M_PI * 1.5);
		}
		cairo_matrix_scale (&matrix,
				    (double) (pixbuf_height - 1) / width,
				    (double) (pixbuf_width - 1) / height);
	} else {
		cairo_matrix_scale (&matrix,
				    (double) (pixbuf_width - 1) / width,
				    (double) (pixbuf_height - 1) / height);
	}

	cairo_pattern_set_matrix (pattern, &matrix);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	if (fish->april_fools) {
		cairo_set_source_rgb (cr, 1, 0.5, 0);
		cairo_paint_with_alpha (cr, 0.25);
	}

	cairo_destroy (cr);
}

static gboolean fish_applet_draw(CtkWidget* widget, cairo_t *cr, FishApplet* fish)
{
	int width, height;
	int src_x, src_y;

	g_return_val_if_fail (fish->surface != NULL, FALSE);

	g_assert (fish->n_frames > 0);

	width = fish->surface_width;
	height = fish->surface_height;
	src_x = 0;
	src_y = 0;

	if (fish->rotate) {
		if (fish->orientation == CAFE_PANEL_APPLET_ORIENT_RIGHT)
			src_y += ((height * (fish->n_frames - 1 - fish->current_frame)) / fish->n_frames);
		else if (fish->orientation == CAFE_PANEL_APPLET_ORIENT_LEFT)
			src_y += ((height * fish->current_frame) / fish->n_frames);
		else
			src_x += ((width * fish->current_frame) / fish->n_frames);
	} else
		src_x += ((width * fish->current_frame) / fish->n_frames);

	cairo_save (cr);
	cairo_set_source_surface (cr, fish->surface, -src_x, -src_y);
	cairo_paint (cr);
	cairo_restore (cr);

        return FALSE;
}

static void fish_applet_size_allocate(CtkWidget* widget, CtkAllocation* allocation, FishApplet* fish)
{
	if (allocation->width  == fish->prev_allocation.width &&
	    allocation->height == fish->prev_allocation.height)
		return;

	fish->prev_allocation = *allocation;
	g_idle_add (update_pixmap_in_idle, fish);
}

static void fish_applet_realize(CtkWidget* widget, FishApplet* fish)
{
	if (!fish->surface)
		update_pixmap (fish);
}

static void fish_applet_unrealize(CtkWidget* widget, FishApplet* fish)
{
	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = NULL;
	fish->surface_width = 0;
	fish->surface_height = 0;
}

static void fish_applet_change_orient(CafePanelApplet* applet, CafePanelAppletOrient orientation)
{
	FishApplet *fish = (FishApplet *) applet;

	if (fish->orientation == orientation)
		return;

	fish->orientation = orientation;

	if (fish->surface)
		update_pixmap (fish);
}

static void change_water(FishApplet* fish)
{
	CtkWidget *dialog;

	dialog = ctk_message_dialog_new (
			NULL, 0, CTK_MESSAGE_INFO,
			CTK_BUTTONS_OK,
			_("The water needs changing"));
	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  _("Look at today's date!"));
	ctk_window_set_icon_name (CTK_WINDOW (dialog), FISH_ICON);
	ctk_window_set_screen (CTK_WINDOW (dialog),
			       ctk_widget_get_screen (CTK_WIDGET (fish)));

	ctk_widget_show_all (dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ctk_widget_destroy), NULL);
}

static gboolean handle_keypress(CtkWidget* widget, CdkEventKey* event, FishApplet* fish)
{
	switch (event->keyval) {
	case CDK_KEY_space:
	case CDK_KEY_KP_Space:
	case CDK_KEY_Return:
	case CDK_KEY_KP_Enter:
	case CDK_KEY_ISO_Enter:
	case CDK_KEY_3270_Enter:
		if (fish->april_fools) {
			change_water (fish);
			return TRUE;
		}

		display_fortune_dialog (fish);
		break;
	default:
		return FALSE;
		break;
	}

	return TRUE;
}

static gboolean fish_enter_notify(CtkWidget* widget, CdkEventCrossing* event)
{
  FishApplet *fish;
  CtkWidget  *event_widget;

  fish = FISH_APPLET (widget);
  event_widget = ctk_get_event_widget ((CdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != CDK_NOTIFY_INFERIOR))
      fish->in_applet = TRUE;

  return FALSE;
}

static gboolean fish_leave_notify(CtkWidget* widget, CdkEventCrossing* event)
{
  FishApplet *fish;
  CtkWidget  *event_widget;

  fish = FISH_APPLET (widget);
  event_widget = ctk_get_event_widget ((CdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != CDK_NOTIFY_INFERIOR))
      fish->in_applet = FALSE;

  return FALSE;
}

static gboolean handle_button_release(FishApplet* fish, CdkEventButton* event)
{
	if (!fish->in_applet || event->button != 1)
		return FALSE;

	if (fish->april_fools) {
		change_water (fish);
		return TRUE;
	}

	display_fortune_dialog (fish);

	return TRUE;
}

static void set_tooltip(FishApplet* fish)
{
	const char *desc_format = _("%s the Fish, the fortune teller");
	char       *desc;

	desc = g_markup_printf_escaped (desc_format, fish->name);
	ctk_widget_set_tooltip_markup (CTK_WIDGET (fish), desc);
	g_free (desc);
}

static void setup_fish_widget(FishApplet* fish)
{
	CtkWidget *widget = (CtkWidget *) fish;

	fish->frame = ctk_frame_new (NULL);
	ctk_frame_set_shadow_type (CTK_FRAME (fish->frame), CTK_SHADOW_IN);
	ctk_container_add (CTK_CONTAINER (widget), fish->frame);

	fish->drawing_area = ctk_drawing_area_new ();
	ctk_container_add (CTK_CONTAINER (fish->frame), fish->drawing_area);

	g_signal_connect (fish->drawing_area, "realize",
			  G_CALLBACK (fish_applet_realize), fish);
	g_signal_connect (fish->drawing_area, "unrealize",
			  G_CALLBACK (fish_applet_unrealize), fish);
	g_signal_connect (fish->drawing_area, "size-allocate",
			  G_CALLBACK (fish_applet_size_allocate), fish);
	g_signal_connect (fish->drawing_area, "draw",
			  G_CALLBACK (fish_applet_draw), fish);

	ctk_widget_add_events (widget, CDK_ENTER_NOTIFY_MASK |
				       CDK_LEAVE_NOTIFY_MASK |
				       CDK_BUTTON_RELEASE_MASK);

	g_signal_connect_swapped (widget, "enter_notify_event",
				  G_CALLBACK (fish_enter_notify), fish);
	g_signal_connect_swapped (widget, "leave_notify_event",
				  G_CALLBACK (fish_leave_notify), fish);
	g_signal_connect_swapped (widget, "button_release_event",
				  G_CALLBACK (handle_button_release), fish);

	ctk_widget_add_events (fish->drawing_area, CDK_BUTTON_RELEASE_MASK);
	g_signal_connect_swapped (fish->drawing_area, "button_release_event",
				  G_CALLBACK (handle_button_release), fish);

	load_fish_image (fish);

	update_pixmap (fish);

	setup_timeout (fish);

	set_tooltip (fish);
	set_ally_name_desc (CTK_WIDGET (fish), fish);

	g_signal_connect (fish, "key_press_event",
			  G_CALLBACK (handle_keypress), fish);

	ctk_widget_show_all (widget);
}

static const CtkActionEntry fish_menu_verbs[] = {
	{ "FishPreferences", "document-properties", N_("_Preferences"),
	  NULL, NULL,
	  G_CALLBACK (display_preferences_dialog) },
	{ "FishHelp", "help-browser", N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (display_help_dialog) },
	{ "FishAbout", "help-about", N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (display_about_dialog) }
};

static gboolean fish_applet_fill(FishApplet* fish)
{
	CafePanelApplet* applet = (CafePanelApplet*) fish;
	CtkActionGroup* action_group;

	fish->orientation = cafe_panel_applet_get_orient (applet);

	setup_gsettings (fish);

	fish->name = g_settings_get_string (fish->settings, FISH_NAME_KEY);

	if (!fish->name)
	{
		fish->name = g_strdup ("Wanda"); /* Fallback */
	}

	fish->image = g_settings_get_string (fish->settings, FISH_IMAGE_KEY);

	if (!fish->image)
		fish->image = g_strdup ("fishanim.png"); /* Fallback */

	fish->command = g_settings_get_string (fish->settings, FISH_COMMAND_KEY);

	fish->n_frames = g_settings_get_int (fish->settings, FISH_FRAMES_KEY);

	if (fish->n_frames <= 0)
		fish->n_frames = 1;

	fish->speed = g_settings_get_double (fish->settings, FISH_SPEED_KEY);

	fish->rotate = g_settings_get_boolean (fish->settings, FISH_ROTATE_KEY);

	action_group = ctk_action_group_new ("Fish Applet Actions");
	ctk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions (action_group,
				      fish_menu_verbs,
				      G_N_ELEMENTS (fish_menu_verbs),
				      fish);
	cafe_panel_applet_setup_menu_from_resource (applet,
	                                            FISH_RESOURCE_PATH "fish-menu.xml",
	                                            action_group);

	if (cafe_panel_applet_get_locked_down (applet)) {
		CtkAction *action;

		action = ctk_action_group_get_action (action_group, "FishPreferences");
		ctk_action_set_visible (action, FALSE);
	}
	g_object_unref (action_group);

	#ifndef FISH_INPROCESS
		ctk_window_set_default_icon_name(FISH_ICON);
	#endif

	setup_fish_widget(fish);

	return TRUE;
}

static gboolean fishy_factory(CafePanelApplet* applet, const char* iid, gpointer data)
{
	gboolean retval = FALSE;

	if (!strcmp(iid, "FishApplet"))
	{
		retval = fish_applet_fill(FISH_APPLET(applet));
	}

	return retval;
}

static void fish_applet_dispose (GObject *object)
{
	FishApplet* fish = (FishApplet*) object;

	if (fish->settings != NULL)
		g_signal_handlers_disconnect_by_data (fish->settings,
					  fish);

	if (fish->timeout)
	{
		g_source_remove (fish->timeout);
	}

	fish->timeout = 0;

	if (fish->settings)
		g_object_unref (fish->settings);
	fish->settings = NULL;

	if (fish->lockdown_settings)
		g_object_unref (fish->lockdown_settings);
	fish->lockdown_settings = NULL;

	if (fish->name)
		g_free (fish->name);
	fish->name = NULL;

	if (fish->image)
		g_free (fish->image);
	fish->image = NULL;

	if (fish->command)
		g_free (fish->command);
	fish->command = NULL;

	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = NULL;
	fish->surface_width = 0;
	fish->surface_height = 0;

	if (fish->pixbuf)
		g_object_unref (fish->pixbuf);
	fish->pixbuf = NULL;

	if (fish->preferences_dialog)
		ctk_widget_destroy (fish->preferences_dialog);
	fish->preferences_dialog = NULL;

	if (fish->fortune_dialog)
		ctk_widget_destroy (fish->fortune_dialog);
	fish->fortune_dialog = NULL;

	if (fish->source_id)
		g_source_remove (fish->source_id);
	fish->source_id = 0;

	fish_close_channel (fish);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void fish_applet_instance_init(FishApplet* fish, FishAppletClass* klass)
{
	fish->name     = NULL;
	fish->image    = NULL;
	fish->command  = NULL;
	fish->n_frames = 1;
	fish->speed    = 0.0;
	fish->rotate   = FALSE;

	fish->orientation = CAFE_PANEL_APPLET_ORIENT_UP;

	fish->frame         = NULL;
	fish->drawing_area  = NULL;
	fish->surface       = NULL;
	fish->timeout       = 0;
	fish->current_frame = 0;
	fish->in_applet     = FALSE;

	fish->requisition.width  = -1;
	fish->requisition.height = -1;

	fish->prev_allocation.x      = -1;
	fish->prev_allocation.y      = -1;
	fish->prev_allocation.width  = -1;
	fish->prev_allocation.height = -1;

	fish->pixbuf = NULL;

	fish->preferences_dialog = NULL;
	fish->name_entry         = NULL;
	fish->command_label      = NULL;
	fish->command_entry      = NULL;
	fish->preview_image      = NULL;
	fish->image_chooser      = NULL;
	fish->frames_spin        = NULL;
	fish->speed_spin         = NULL;
	fish->rotate_toggle      = NULL;

	fish->fortune_dialog = NULL;
	fish->fortune_view   = NULL;
	fish->fortune_label  = NULL;
	fish->fortune_cmd_label = NULL;
	fish->fortune_buffer = NULL;

	fish->source_id  = 0;
	fish->io_channel = NULL;

	fish->april_fools = FALSE;

	cafe_panel_applet_set_flags (CAFE_PANEL_APPLET (fish), CAFE_PANEL_APPLET_EXPAND_MINOR);

	cafe_panel_applet_set_background_widget(CAFE_PANEL_APPLET(fish), CTK_WIDGET(fish));
}

static void fish_applet_class_init(FishAppletClass* klass)
{
	CafePanelAppletClass* applet_class = (CafePanelAppletClass*) klass;
	GObjectClass *gobject_class        = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent(klass);

	applet_class->change_orient = fish_applet_change_orient;

	gobject_class->dispose = fish_applet_dispose;

	init_fools_day();
}

static GType fish_applet_get_type(void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info = {
			sizeof(CafePanelAppletClass),
			NULL, NULL,
			(GClassInitFunc) fish_applet_class_init,
			NULL, NULL,
			sizeof(FishApplet),
			0,
			(GInstanceInitFunc) fish_applet_instance_init,
			NULL
		};

		type = g_type_register_static(PANEL_TYPE_APPLET, "FishApplet", &info, 0);
	}

	return type;
}

#ifdef FISH_INPROCESS
	CAFE_PANEL_APPLET_IN_PROCESS_FACTORY("FishAppletFactory", fish_applet_get_type(), "That-stupid-fish", fishy_factory, NULL)
#else
	CAFE_PANEL_APPLET_OUT_PROCESS_FACTORY("FishAppletFactory", fish_applet_get_type(), "That-stupid-fish", fishy_factory, NULL)
#endif
