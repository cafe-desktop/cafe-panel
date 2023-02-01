/*
 * calendar-window.c: toplevel window containing a calendar and
 * tasks/appointments
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
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
 *      Vincent Untz <vuntz@gnome.org>
 *
 * Most of the original code comes from clock.c
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <ctk/ctk.h>

#include "calendar-window.h"

#include "clock.h"
#include "clock-utils.h"
#include "clock-typebuiltins.h"

#define KEY_LOCATIONS_EXPANDED      "expand-locations"

enum {
	EDIT_LOCATIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _CalendarWindowPrivate {
	CtkWidget  *calendar;

	char       *prefs_path;

	gboolean     invert_order;
	gboolean     show_weeks;
	time_t      *current_time;

	CtkWidget *locations_list;

	GSettings  *settings;
};

G_DEFINE_TYPE_WITH_PRIVATE (CalendarWindow, calendar_window, CTK_TYPE_WINDOW)

enum {
	PROP_0,
	PROP_INVERTORDER,
	PROP_SHOWWEEKS,
	PROP_CURRENTTIMEP,
	PROP_PREFSPATH
};

static time_t *calendar_window_get_current_time_p (CalendarWindow *calwin);
static void    calendar_window_set_current_time_p (CalendarWindow *calwin,
						   time_t         *current_time);
static const char *calendar_window_get_prefs_path (CalendarWindow *calwin);
static void    calendar_window_set_prefs_path     (CalendarWindow *calwin,
						   const char           *prefs_path);
static CtkWidget * create_hig_frame 		  (CalendarWindow *calwin,
		  				   const char *title,
                  				   const char *button_label,
		  				   const char *key,
                  				   GCallback   callback);

static void calendar_mark_today(CtkCalendar *calendar)
{
	time_t now;
	struct tm tm1;
	guint year, month, day;

	ctk_calendar_get_date(calendar, &year, &month, &day);
	time(&now);
	localtime_r (&now, &tm1);
	if ((tm1.tm_mon == month) && (tm1.tm_year + 1900 == year)) {
		ctk_calendar_mark_day (CTK_CALENDAR (calendar), tm1.tm_mday);
	} else {
		ctk_calendar_unmark_day (CTK_CALENDAR (calendar), tm1.tm_mday);
	}
}

static gboolean calendar_update(gpointer user_data)
{
	CtkCalendar *calendar = user_data;
	calendar_mark_today(calendar);
	return G_SOURCE_REMOVE;
}

static void calendar_month_changed_cb(CtkCalendar *calendar, gpointer user_data)
{
	ctk_calendar_clear_marks(calendar);
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, calendar_update, user_data, NULL);
}

static CtkWidget *
calendar_window_create_calendar (CalendarWindow *calwin)
{
	CtkWidget                 *calendar;
	CtkCalendarDisplayOptions  options;
	struct tm                  tm1;

	calendar = ctk_calendar_new ();
	ctk_widget_set_size_request(CTK_WIDGET(calendar), 330, 100);
	options = ctk_calendar_get_display_options (CTK_CALENDAR (calendar));
	if (calwin->priv->show_weeks)
		options |= CTK_CALENDAR_SHOW_WEEK_NUMBERS;
	else
		options &= ~(CTK_CALENDAR_SHOW_WEEK_NUMBERS);
	ctk_calendar_set_display_options (CTK_CALENDAR (calendar), options);

	localtime_r (calwin->priv->current_time, &tm1);
	ctk_calendar_select_month (CTK_CALENDAR (calendar),
				   tm1.tm_mon, tm1.tm_year + 1900);
	ctk_calendar_select_day (CTK_CALENDAR (calendar), tm1.tm_mday);
	calendar_mark_today (CTK_CALENDAR(calendar));

	g_signal_connect(calendar, "month-changed",
			 G_CALLBACK(calendar_month_changed_cb), calendar);

	return calendar;
}

static void
expand_collapse_child (CtkWidget *child,
		       gpointer   data)
{
	gboolean expanded;

	if (data == child || ctk_widget_is_ancestor (data, child))
		return;

	expanded = ctk_expander_get_expanded (CTK_EXPANDER (data));
	g_object_set (child, "visible", expanded, NULL);
}

static void
expand_collapse (CtkWidget  *expander,
		 GParamSpec *pspec,
                 gpointer    data)
{
	CtkWidget *box = data;

	ctk_container_foreach (CTK_CONTAINER (box),
			       (CtkCallback)expand_collapse_child,
			       expander);
}

static void add_child (CtkContainer *container,
                       CtkWidget    *child,
                       CtkExpander  *expander)
{
	expand_collapse_child (child, expander);
}

static CtkWidget *
create_hig_frame (CalendarWindow *calwin,
		  const char *title,
                  const char *button_label,
		  const char *key,
                  GCallback   callback)
{
        CtkWidget *vbox;
        CtkWidget *hbox;
        char      *bold_title;
        CtkWidget *expander;

        vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);

        bold_title = g_strdup_printf ("<b>%s</b>", title);
	expander = ctk_expander_new (bold_title);
        g_free (bold_title);
	ctk_expander_set_use_markup (CTK_EXPANDER (expander), TRUE);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

	ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (hbox), expander, FALSE, FALSE, 0);
	ctk_widget_show_all (vbox);

	g_signal_connect (expander, "notify::expanded",
			  G_CALLBACK (expand_collapse), hbox);
	g_signal_connect (expander, "notify::expanded",
			  G_CALLBACK (expand_collapse), vbox);

	/* FIXME: this doesn't really work, since "add" does not
	 * get emitted for e.g. ctk_box_pack_start
	 */
	g_signal_connect (vbox, "add", G_CALLBACK (add_child), expander);
	g_signal_connect (hbox, "add", G_CALLBACK (add_child), expander);

        if (button_label) {
                CtkWidget *label;
                CtkWidget *button_box;
                CtkWidget *button;
                gchar *text;

                button_box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
                ctk_widget_show (button_box);

                button = ctk_button_new ();
                ctk_container_add (CTK_CONTAINER (button_box), button);

                text = g_markup_printf_escaped ("<small>%s</small>", button_label);
                label = ctk_label_new (text);
                g_free (text);
                ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
                ctk_container_add (CTK_CONTAINER (button), label);

                ctk_widget_show_all (button);

                ctk_box_pack_end (CTK_BOX (hbox), button_box, FALSE, FALSE, 0);

                g_signal_connect_swapped (button, "clicked", callback, calwin);

                g_object_bind_property (expander, "expanded",
                                        button_box, "visible",
                                        G_BINDING_DEFAULT|G_BINDING_SYNC_CREATE);
        }

	g_settings_bind (calwin->priv->settings, key, expander, "expanded",
			 G_SETTINGS_BIND_DEFAULT);

        return vbox;
}

static void
edit_locations (CalendarWindow *calwin)
{
	g_signal_emit (calwin, signals[EDIT_LOCATIONS], 0);
}

static void
calendar_window_pack_locations (CalendarWindow *calwin, CtkWidget *vbox)
{
	calwin->priv->locations_list = create_hig_frame (calwin,
							 _("Locations"), _("Edit"),
							 KEY_LOCATIONS_EXPANDED,
							 G_CALLBACK (edit_locations));

	/* we show the widget before adding to the container, since adding to
	 * the container changes the visibility depending on the state of the
	 * expander */
	ctk_widget_show (calwin->priv->locations_list);
	ctk_container_add (CTK_CONTAINER (vbox), calwin->priv->locations_list);

	//ctk_box_pack_start (CTK_BOX (vbox), calwin->priv->locations_list, TRUE, FALSE, 0);
}

static void
calendar_window_fill (CalendarWindow *calwin)
{
        CtkWidget *frame;
        CtkWidget *vbox;

        frame = ctk_frame_new (NULL);
        ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_OUT);
        ctk_container_add (CTK_CONTAINER (calwin), frame);
        ctk_widget_show (frame);

        vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);

        ctk_container_set_border_width (CTK_CONTAINER (vbox), 6);
        ctk_container_add (CTK_CONTAINER (frame), vbox);
        ctk_widget_show (vbox);

	calwin->priv->calendar = calendar_window_create_calendar (calwin);
        ctk_widget_show (calwin->priv->calendar);

	if (!calwin->priv->invert_order) {
                ctk_box_pack_start (CTK_BOX (vbox),
				    calwin->priv->calendar, TRUE, FALSE, 0);
		calendar_window_pack_locations (calwin, vbox);
	} else {
		calendar_window_pack_locations (calwin, vbox);
                ctk_box_pack_start (CTK_BOX (vbox),
				    calwin->priv->calendar, TRUE, FALSE, 0);
	}
}

CtkWidget *
calendar_window_get_locations_box (CalendarWindow *calwin)
{
	return calwin->priv->locations_list;
}

static GObject *
calendar_window_constructor (GType                  type,
			     guint                  n_construct_properties,
			     GObjectConstructParam *construct_properties)
{
	GObject        *obj;
	CalendarWindow *calwin;

	obj = G_OBJECT_CLASS (calendar_window_parent_class)->constructor (type,
									  n_construct_properties,
									  construct_properties);

	calwin = CALENDAR_WINDOW (obj);

	g_assert (calwin->priv->current_time != NULL);
	g_assert (calwin->priv->prefs_path != NULL);

	calendar_window_fill (calwin);

	return obj;
}

static void
calendar_window_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	CalendarWindow *calwin;

	g_return_if_fail (CALENDAR_IS_WINDOW (object));

	calwin = CALENDAR_WINDOW (object);

	switch (prop_id) {
	case PROP_INVERTORDER:
		g_value_set_boolean (value,
				     calendar_window_get_invert_order (calwin));
		break;
	case PROP_SHOWWEEKS:
		g_value_set_boolean (value,
				     calendar_window_get_show_weeks (calwin));
		break;
	case PROP_CURRENTTIMEP:
		g_value_set_pointer (value,
				     calendar_window_get_current_time_p (calwin));
		break;
	case PROP_PREFSPATH:
		g_value_set_string (value,
				    calendar_window_get_prefs_path (calwin));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
calendar_window_set_property (GObject       *object,
			      guint          prop_id,
			      const GValue  *value,
			      GParamSpec    *pspec)
{
	CalendarWindow *calwin;

	g_return_if_fail (CALENDAR_IS_WINDOW (object));

	calwin = CALENDAR_WINDOW (object);

	switch (prop_id) {
	case PROP_INVERTORDER:
		calendar_window_set_invert_order (calwin,
						  g_value_get_boolean (value));
		break;
	case PROP_SHOWWEEKS:
		calendar_window_set_show_weeks (calwin,
						g_value_get_boolean (value));
		break;
	case PROP_CURRENTTIMEP:
		calendar_window_set_current_time_p (calwin,
						    g_value_get_pointer (value));
		break;
	case PROP_PREFSPATH:
		calendar_window_set_prefs_path (calwin,
					        g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
calendar_window_dispose (GObject *object)
{
	CalendarWindow *calwin;

	calwin = CALENDAR_WINDOW (object);

	if (calwin->priv->settings)
		g_object_unref (calwin->priv->settings);
	calwin->priv->settings = NULL;

	G_OBJECT_CLASS (calendar_window_parent_class)->dispose (object);
}

static void
calendar_window_class_init (CalendarWindowClass *klass)
{
	GObjectClass   *gobject_class   = G_OBJECT_CLASS (klass);

	gobject_class->constructor = calendar_window_constructor;
	gobject_class->get_property = calendar_window_get_property;
	gobject_class->set_property = calendar_window_set_property;

	gobject_class->dispose = calendar_window_dispose;

	signals[EDIT_LOCATIONS] = g_signal_new ("edit-locations",
						G_TYPE_FROM_CLASS (gobject_class),
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET (CalendarWindowClass, edit_locations),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE, 0);

	g_object_class_install_property (
		gobject_class,
		PROP_INVERTORDER,
		g_param_spec_boolean ("invert-order",
				      "Invert Order",
				      "Invert order of the calendar and tree views",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_SHOWWEEKS,
		g_param_spec_boolean ("show-weeks",
				      "Show Weeks",
				      "Show weeks in the calendar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_CURRENTTIMEP,
		g_param_spec_pointer ("current-time",
				      "Current Time",
				      "Pointer to a variable containing the current time",
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		gobject_class,
		PROP_PREFSPATH,
		g_param_spec_string ("prefs-path",
				     "Preferences Path",
				     "Preferences path in GSettings",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
calendar_window_init (CalendarWindow *calwin)
{
	CtkWindow *window;

	calwin->priv = calendar_window_get_instance_private (calwin);

	window = CTK_WINDOW (calwin);
	ctk_window_set_type_hint (window, CDK_WINDOW_TYPE_HINT_DOCK);
	ctk_window_set_decorated (window, FALSE);
	ctk_window_set_resizable (window, FALSE);
	ctk_window_set_default_size (window, 337, -1);
	ctk_window_stick (window);
	ctk_window_set_title (window, _("Calendar"));
	ctk_window_set_icon_name (window, CLOCK_ICON);
}

CtkWidget *
calendar_window_new (time_t     *static_current_time,
		     const char *prefs_path,
		     gboolean    invert_order)
{
	CalendarWindow *calwin;

	calwin = g_object_new (CALENDAR_TYPE_WINDOW,
			       "type", CTK_WINDOW_TOPLEVEL,
			       "current-time", static_current_time,
			       "invert-order", invert_order,
			       "prefs-path", prefs_path,
			       NULL);

	return CTK_WIDGET (calwin);
}

void
calendar_window_refresh (CalendarWindow *calwin)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));
}

gboolean
calendar_window_get_invert_order (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), FALSE);

	return calwin->priv->invert_order;
}

void
calendar_window_set_invert_order (CalendarWindow *calwin,
				  gboolean        invert_order)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (invert_order == calwin->priv->invert_order)
		return;

	calwin->priv->invert_order = invert_order;
	//FIXME: update the order of the content of the window

	g_object_notify (G_OBJECT (calwin), "invert-order");
}

gboolean
calendar_window_get_show_weeks (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), FALSE);

	return calwin->priv->show_weeks;
}

void
calendar_window_set_show_weeks (CalendarWindow *calwin,
				gboolean        show_weeks)
{
	CtkCalendarDisplayOptions options;

	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (show_weeks == calwin->priv->show_weeks)
		return;

	calwin->priv->show_weeks = show_weeks;

	if (calwin->priv->calendar) {
		options = ctk_calendar_get_display_options (CTK_CALENDAR (calwin->priv->calendar));

		if (show_weeks)
			options |= CTK_CALENDAR_SHOW_WEEK_NUMBERS;
		else
			options &= ~(CTK_CALENDAR_SHOW_WEEK_NUMBERS);

		ctk_calendar_set_display_options (CTK_CALENDAR (calwin->priv->calendar),
						  options);
	}

	g_object_notify (G_OBJECT (calwin), "show-weeks");
}

ClockFormat
calendar_window_get_time_format (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin),
			      CLOCK_FORMAT_INVALID);

	return CLOCK_FORMAT_INVALID;
}

static time_t *
calendar_window_get_current_time_p (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), NULL);

	return calwin->priv->current_time;
}

static void
calendar_window_set_current_time_p (CalendarWindow *calwin,
				    time_t         *current_time)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (current_time == calwin->priv->current_time)
		return;

	calwin->priv->current_time = current_time;

	g_object_notify (G_OBJECT (calwin), "current-time");
}

static const char *
calendar_window_get_prefs_path (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), NULL);

	return calwin->priv->prefs_path;
}

static void
calendar_window_set_prefs_path (CalendarWindow *calwin,
				const char     *prefs_path)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (!calwin->priv->prefs_path && (!prefs_path || !prefs_path [0]))
		return;

	if (calwin->priv->prefs_path && prefs_path && prefs_path [0] &&
	    !strcmp (calwin->priv->prefs_path, prefs_path))
		return;

	if (calwin->priv->prefs_path)
		g_free (calwin->priv->prefs_path);
	calwin->priv->prefs_path = NULL;

	if (prefs_path && prefs_path [0])
		calwin->priv->prefs_path = g_strdup (prefs_path);

	g_object_notify (G_OBJECT (calwin), "prefs-path");

	if (calwin->priv->settings)
		g_object_unref (calwin->priv->settings);

	calwin->priv->settings = g_settings_new_with_path (CLOCK_SCHEMA, calwin->priv->prefs_path);
}
