#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include "clock.h"
#include "clock-face.h"
#include "clock-location-tile.h"
#include "clock-location.h"
#include "clock-utils.h"
#include "clock-marshallers.h"
#include "set-timezone.h"

enum {
        TILE_PRESSED,
        NEED_CLOCK_FORMAT,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
        ClockLocation *location;

        struct tm last_refresh;
        long last_offset;

        ClockFaceSize size;

        CtkWidget *box;
        CtkWidget *clock_face;
        CtkWidget *city_label;
        CtkWidget *time_label;

        CtkWidget *current_button;
        CtkWidget *current_label;
        CtkWidget *current_marker;
        CtkWidget *current_spacer;
        CtkSizeGroup *current_group;
        CtkSizeGroup *button_group;

        CtkWidget *weather_icon;

        gulong location_weather_updated_id;
} ClockLocationTilePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocationTile, clock_location_tile, CTK_TYPE_BIN)

static void clock_location_tile_finalize (GObject *);

static void clock_location_tile_fill (ClockLocationTile *this);
static void update_weather_icon (ClockLocation *loc, WeatherInfo *info, gpointer data);
static gboolean weather_tooltip (CtkWidget *widget,
                                 gint x, gint y,
                                 gboolean    keyboard_mode,
                                 CtkTooltip *tooltip,
                                 gpointer    data);

ClockLocationTile *
clock_location_tile_new (ClockLocation *loc,
                         ClockFaceSize size)
{
        ClockLocationTile *this;
        ClockLocationTilePrivate *priv;

        this = g_object_new (CLOCK_LOCATION_TILE_TYPE, NULL);

        priv = clock_location_tile_get_instance_private (this);

        priv->location = g_object_ref (loc);
        priv->size = size;

        clock_location_tile_fill (this);

        update_weather_icon (loc, clock_location_get_weather_info (loc), this);
        ctk_widget_set_has_tooltip (priv->weather_icon, TRUE);

        g_signal_connect (priv->weather_icon, "query-tooltip",
                          G_CALLBACK (weather_tooltip), this);
        priv->location_weather_updated_id = g_signal_connect (G_OBJECT (loc), "weather-updated",
                                                              G_CALLBACK (update_weather_icon), this);

        return this;
}

static void
clock_location_tile_class_init (ClockLocationTileClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->finalize = clock_location_tile_finalize;

        signals[TILE_PRESSED] = g_signal_new ("tile-pressed",
                                              G_TYPE_FROM_CLASS (g_obj_class),
                                              G_SIGNAL_RUN_FIRST,
                                              G_STRUCT_OFFSET (ClockLocationTileClass, tile_pressed),
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);
        signals[NEED_CLOCK_FORMAT] = g_signal_new ("need-clock-format",
                                                   G_TYPE_FROM_CLASS (g_obj_class),
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET (ClockLocationTileClass, need_clock_format),
                                                   NULL,
                                                   NULL,
                                                   _clock_marshal_INT__VOID,
                                                   G_TYPE_INT, 0);
}

static void
clock_location_tile_init (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (this);

        priv->location = NULL;

        memset (&(priv->last_refresh), 0, sizeof (struct tm));
        priv->last_offset = 0;

        priv->size = CLOCK_FACE_SMALL;

        priv->clock_face = NULL;
        priv->city_label = NULL;
        priv->time_label = NULL;
}

static void
clock_location_tile_finalize (GObject *g_obj)
{
        ClockLocationTile *this;
        ClockLocationTilePrivate *priv;

        this = CLOCK_LOCATION_TILE (g_obj);
        priv = clock_location_tile_get_instance_private (this);

        if (priv->location) {
                g_signal_handler_disconnect (priv->location, priv->location_weather_updated_id);
                priv->location_weather_updated_id = 0;

                g_object_unref (priv->location);
                priv->location = NULL;
        }

        if (priv->button_group) {
                g_object_unref (priv->button_group);
                priv->button_group = NULL;
        }

        if (priv->current_group) {
                g_object_unref (priv->current_group);
                priv->current_group = NULL;
        }

        G_OBJECT_CLASS (clock_location_tile_parent_class)->finalize (g_obj);
}

static gboolean
press_on_tile      (CtkWidget         *widget G_GNUC_UNUSED,
		    CdkEventButton    *event G_GNUC_UNUSED,
		    ClockLocationTile *tile)
{
        g_signal_emit (tile, signals[TILE_PRESSED], 0);

        return TRUE;
}

static void
make_current_cb (gpointer data G_GNUC_UNUSED,
		 GError  *error)
{
        CtkWidget *dialog;

        if (error) {
                dialog = ctk_message_dialog_new (NULL,
                                                 0,
                                                 CTK_MESSAGE_ERROR,
                                                 CTK_BUTTONS_CLOSE,
                                                 _("Failed to set the system timezone"));
                ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), "%s", error->message);
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (ctk_widget_destroy), NULL);
                ctk_window_present (CTK_WINDOW (dialog));
        }
}

static void
make_current (CtkWidget         *widget G_GNUC_UNUSED,
	      ClockLocationTile *tile)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (tile);

        clock_location_make_current (priv->location,
                                     (GFunc)make_current_cb, tile, NULL);
}

static gboolean
enter_or_leave_tile (CtkWidget         *widget G_GNUC_UNUSED,
		     CdkEventCrossing  *event,
		     ClockLocationTile *tile)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (tile);

        if (event->mode != CDK_CROSSING_NORMAL) {
                return TRUE;
        }

        if (clock_location_is_current (priv->location)) {
                ctk_widget_hide (priv->current_button);
                ctk_widget_hide (priv->current_spacer);
                ctk_widget_show (priv->current_marker);

                return TRUE;
        }

        if (event->type == CDK_ENTER_NOTIFY) {
                gint can_set;

                if (clock_location_is_current_timezone (priv->location))
                        can_set = 2;
                else
                        can_set = can_set_system_timezone ();
                if (can_set != 0) {
                        ctk_label_set_markup (CTK_LABEL (priv->current_label),
                                                can_set == 1 ?
                                                        _("<small>Set...</small>") :
                                                        _("<small>Set</small>"));
                        ctk_widget_hide (priv->current_spacer);
                        ctk_widget_hide (priv->current_marker);
                        ctk_widget_show (priv->current_button);
                }
                else {
                        ctk_widget_hide (priv->current_marker);
                        ctk_widget_hide (priv->current_button);
                        ctk_widget_show (priv->current_spacer);
                }
        }
        else {
                if (event->detail != CDK_NOTIFY_INFERIOR) {
                        ctk_widget_hide (priv->current_button);
                        ctk_widget_hide (priv->current_marker);
                        ctk_widget_show (priv->current_spacer);
                }
        }

        return TRUE;
}

static void
clock_location_tile_fill (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (this);
        CtkWidget *strut;
        CtkWidget *box;
        CtkWidget *tile;
        CtkWidget *head_section;

        priv->box = ctk_event_box_new ();

        ctk_widget_add_events (priv->box, CDK_BUTTON_PRESS_MASK | CDK_ENTER_NOTIFY_MASK | CDK_LEAVE_NOTIFY_MASK);
        g_signal_connect (priv->box, "button-press-event",
                          G_CALLBACK (press_on_tile), this);
        g_signal_connect (priv->box, "enter-notify-event",
                          G_CALLBACK (enter_or_leave_tile), this);
        g_signal_connect (priv->box, "leave-notify-event",
                          G_CALLBACK (enter_or_leave_tile), this);

        tile = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
        ctk_widget_set_margin_top (tile, 3);
        ctk_widget_set_margin_bottom (tile, 3);
        ctk_widget_set_margin_start (tile, 3);

        priv->city_label = ctk_label_new (NULL);
        ctk_widget_set_margin_end (priv->city_label, 3);
        ctk_label_set_xalign (CTK_LABEL (priv->city_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (priv->city_label), 0.0);

        head_section = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
        ctk_box_pack_start (CTK_BOX (head_section), priv->city_label, FALSE, FALSE, 0);

        priv->time_label = ctk_label_new (NULL);
        ctk_label_set_width_chars (CTK_LABEL (priv->time_label), 20);
        ctk_widget_set_margin_end (priv->time_label, 3);
        ctk_label_set_xalign (CTK_LABEL (priv->time_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (priv->time_label), 0.0);

        priv->weather_icon = ctk_image_new ();
        ctk_widget_set_valign (priv->weather_icon, CTK_ALIGN_START);

        box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
        ctk_box_pack_start (CTK_BOX (head_section), box, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (box), priv->weather_icon, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (box), priv->time_label, FALSE, FALSE, 0);

        priv->current_button = ctk_button_new ();
        /* The correct label is set on EnterNotify events */
        priv->current_label = ctk_label_new ("");
        ctk_widget_show (priv->current_label);
        ctk_widget_set_no_show_all (priv->current_button, TRUE);
        ctk_widget_set_valign (priv->current_button, CTK_ALIGN_CENTER);
        ctk_container_add (CTK_CONTAINER (priv->current_button), priv->current_label);
        ctk_widget_set_tooltip_text (priv->current_button,
                                     _("Set location as current location and use its timezone for this computer"));

        priv->current_marker = ctk_image_new_from_icon_name ("go-home", CTK_ICON_SIZE_BUTTON);
        ctk_widget_set_halign (priv->current_marker, CTK_ALIGN_END);
        ctk_widget_set_valign (priv->current_marker, CTK_ALIGN_CENTER);
        ctk_widget_set_margin_start (priv->current_marker, 75);
        ctk_widget_set_no_show_all (priv->current_marker, TRUE);

        priv->current_spacer = ctk_event_box_new ();
        ctk_widget_set_no_show_all (priv->current_spacer, TRUE);

        strut = ctk_event_box_new ();
        ctk_box_pack_start (CTK_BOX (box), strut, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (box), priv->current_marker, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (box), priv->current_spacer, FALSE, FALSE, 0);
        priv->button_group = ctk_size_group_new (CTK_SIZE_GROUP_VERTICAL);
        ctk_size_group_add_widget (priv->button_group, strut);

        /*
         * Avoid resizing the popup as the tiles display the current marker,
         * set button or nothing. For that purpose, replace 'nothing' with
         * an event box, and force the button, marker and spacer to have the
         * same size via a size group. The visibility of the three is managed
         * manually to ensure that only one of them is shown at any time.
         * (The all have to be shown initially to get the sizes worked out,
         * but they are never visible together).
         */
        priv->current_group = ctk_size_group_new (CTK_SIZE_GROUP_BOTH);
        ctk_size_group_add_widget (priv->current_group, priv->current_marker);
        ctk_size_group_add_widget (priv->current_group, priv->current_spacer);

        ctk_widget_show (priv->current_button);
        ctk_widget_show (priv->current_marker);
        ctk_widget_show (priv->current_spacer);

        g_signal_connect (priv->current_button, "clicked",
                          G_CALLBACK (make_current), this);

        priv->clock_face = clock_face_new_with_location (
                priv->size, priv->location, head_section);

        ctk_box_pack_start (CTK_BOX (tile), priv->clock_face, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (tile), head_section, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (tile), priv->current_button, FALSE, FALSE, 0);

        ctk_container_add (CTK_CONTAINER (priv->box), tile);
        ctk_container_add (CTK_CONTAINER (this), priv->box);
}

static gboolean
clock_needs_face_refresh (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (this);
        struct tm now;

        clock_location_localtime (priv->location, &now);

        if (now.tm_year > priv->last_refresh.tm_year
            || now.tm_mon > priv->last_refresh.tm_mon
            || now.tm_mday > priv->last_refresh.tm_mday
            || now.tm_hour > priv->last_refresh.tm_hour
            || now.tm_min > priv->last_refresh.tm_min) {
                return TRUE;
        }

        if ((priv->size == CLOCK_FACE_LARGE)
            && now.tm_sec > priv->last_refresh.tm_sec) {
                return TRUE;
        }

        return FALSE;
}

static gboolean
clock_needs_label_refresh (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (this);
        struct tm now;
        long offset;

        clock_location_localtime (priv->location, &now);
        offset = clock_location_get_offset (priv->location);

        if (now.tm_year > priv->last_refresh.tm_year
            || now.tm_mon > priv->last_refresh.tm_mon
            || now.tm_mday > priv->last_refresh.tm_mday
            || now.tm_hour > priv->last_refresh.tm_hour
            || now.tm_min > priv->last_refresh.tm_min
            || offset != priv->last_offset) {
                return TRUE;
        }

        return FALSE;
}

static void
copy_tm (struct tm *from, struct tm *to)
{
        to->tm_sec = from->tm_sec;
        to->tm_min = from->tm_min;
        to->tm_hour = from->tm_hour;
        to->tm_mday = from->tm_mday;
        to->tm_mon = from->tm_mon;
        to->tm_year = from->tm_year;
        to->tm_wday = from->tm_wday;
        to->tm_yday = from->tm_yday;
}

static char *
format_time (struct tm   *now,
             char        *tzname,
             ClockFormat  clock_format,
             long         offset)
{
        char buf[256];
        char *format;
        time_t local_t;
        struct tm local_now;
        char *utf8;
        char *tmp;
        long hours, minutes;

        time (&local_t);
        localtime_r (&local_t, &local_now);

        if (local_now.tm_wday != now->tm_wday) {
                if (clock_format == CLOCK_FORMAT_12) {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 12-hours format
                         * (eg, like in the US: 8:10 am), when the local
                         * weekday differs from the weekday at the location
                         * (the %A expands to the weekday). The %p expands to
                         * am/pm. */
                        format = _("%l:%M <small>%p (%A)</small>");
                }
                else {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 24-hours format
                         * (eg, like in France: 20:10), when the local
                         * weekday differs from the weekday at the location
                         * (the %A expands to the weekday). */
                        format = _("%H:%M <small>(%A)</small>");
                }
        }
        else {
                if (clock_format == CLOCK_FORMAT_12) {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 12-hours format
                         * (eg, like in the US: 8:10 am). The %p expands to
                         * am/pm. */
                        format = _("%l:%M <small>%p</small>");
                }
                else {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 24-hours format
                         * (eg, like in France: 20:10). */
                        format = _("%H:%M");
                }
        }

        if (strftime (buf, sizeof (buf), format, now) == 0) {
                strcpy (buf, "???");
        }

        hours = offset / 3600;
        minutes = labs (offset % 3600) / 60;

        if (minutes != 0) {
                tmp = g_strdup_printf ("%s <small>%s %+ld:%ld</small>", buf, tzname, hours, minutes);
        }
        else if (hours != 0) {
                tmp = g_strdup_printf ("%s <small>%s %+ld</small>", buf, tzname, hours);
        }
        else {
                tmp = g_strdup_printf ("%s <small>%s</small>", buf, tzname);
        }

        utf8 = g_locale_to_utf8 (tmp, -1, NULL, NULL, NULL);

        g_free (tmp);

        return utf8;
}

static char *
convert_time_to_str (time_t now, ClockFormat clock_format)
{
        const gchar *format;
        struct tm *tm;
        gchar buf[128];

        if (clock_format == CLOCK_FORMAT_12) {
                /* Translators: This is a strftime format string.
                 * It is used to display the time in 12-hours format (eg, like
                 * in the US: 8:10 am). The %p expands to am/pm.
                 */
                format = _("%l:%M %p");
        }
        else {
                /* Translators: This is a strftime format string.
                 * It is used to display the time in 24-hours format (eg, like
                 * in France: 20:10).
                 */
                format = _("%H:%M");
        }

        tm = localtime (&now);
        strftime (buf, sizeof (buf) - 1, format, tm);

        return g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
}

void
clock_location_tile_refresh (ClockLocationTile *this, gboolean force_refresh)
{
        ClockLocationTilePrivate *priv = clock_location_tile_get_instance_private (this);
        gchar *tmp, *tzname;
        struct tm now;
        long offset;
        int format;

        g_return_if_fail (IS_CLOCK_LOCATION_TILE (this));

        if (clock_location_is_current (priv->location)) {
                ctk_widget_hide (priv->current_spacer);
                ctk_widget_hide (priv->current_button);
                ctk_widget_show (priv->current_marker);
        }
        else {
                if (ctk_widget_get_visible (priv->current_marker)) {
                        ctk_widget_hide (priv->current_marker);
                        ctk_widget_hide (priv->current_button);
                        ctk_widget_show (priv->current_spacer);
                }
        }

        if (clock_needs_face_refresh (this)) {
                clock_face_refresh (CLOCK_FACE (priv->clock_face));
        }

        if (!force_refresh && !clock_needs_label_refresh (this)) {
                return;
        }

        clock_location_localtime (priv->location, &now);
        tzname = clock_location_get_tzname (priv->location);

        copy_tm (&now, &(priv->last_refresh));
        priv->last_offset = clock_location_get_offset (priv->location);

        tmp = g_strdup_printf ("<big><b>%s</b></big>",
                               clock_location_get_display_name (priv->location));
        ctk_label_set_markup (CTK_LABEL (priv->city_label), tmp);
        g_free (tmp);

        g_signal_emit (this, signals[NEED_CLOCK_FORMAT], 0, &format);

        offset = - priv->last_offset;

        tmp = format_time (&now, tzname, format, offset);

        ctk_label_set_markup (CTK_LABEL (priv->time_label), tmp);

        g_free (tmp);
}

void
weather_info_setup_tooltip (WeatherInfo *info, ClockLocation *location, CtkTooltip *tooltip,
                            ClockFormat clock_format)
{
        GdkPixbuf *pixbuf = NULL;
        CtkIconTheme *theme = NULL;
        const gchar *conditions, *wind;
        gchar *temp, *apparent;
        gchar *line1, *line2, *line3, *line4, *tip;
        const gchar *icon_name;
        const gchar *sys_timezone;
        time_t sunrise_time, sunset_time;
        gchar *sunrise_str, *sunset_str;
        gint icon_scale;

        theme = ctk_icon_theme_get_default ();
        icon_name = weather_info_get_icon_name (info);
        icon_scale = cdk_window_get_scale_factor (cdk_get_default_root_window ());

        pixbuf = ctk_icon_theme_load_icon_for_scale (theme, icon_name, 48, icon_scale,
                                                     CTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
        if (pixbuf)
                ctk_tooltip_set_icon (tooltip, pixbuf);

        conditions = weather_info_get_conditions (info);
        if (strcmp (conditions, "-") != 0)
                line1 = g_strdup_printf (_("%s, %s"),
                                         conditions,
                                         weather_info_get_sky (info));
        else
                line1 = g_strdup (weather_info_get_sky (info));

        /* we need to g_strdup() since both functions return the same address
         * of a static buffer */
        temp = g_strdup (weather_info_get_temp (info));
        apparent = g_strdup (weather_info_get_apparent (info));
        if (strcmp (apparent, temp) != 0 &&
            /* FMQ: it's broken to read from another module's translations; add some API to libcafeweather. */
            strcmp (apparent, dgettext ("cafe-applets-2.0", "Unknown")) != 0)
                /* Translators: The two strings are temperatures. */
                line2 = g_strdup_printf (_("%s, feels like %s"), temp, apparent);
        else
                line2 = g_strdup (temp);
        g_free (temp);
        g_free (apparent);

        wind = weather_info_get_wind (info);
        if (strcmp (wind, dgettext ("cafe-applets-2.0", "Unknown")) != 0)
                line3 = g_strdup_printf ("%s\n", wind);
        else
                line3 = g_strdup ("");

        sys_timezone = getenv ("TZ");
        setenv ("TZ", clock_location_get_timezone (location), 1);
        tzset ();
        if (weather_info_get_value_sunrise (info, &sunrise_time))
                sunrise_str = convert_time_to_str (sunrise_time, clock_format);
        else
                sunrise_str = g_strdup ("???");
        if (weather_info_get_value_sunset (info, &sunset_time))
                sunset_str = convert_time_to_str (sunset_time, clock_format);
        else
                sunset_str = g_strdup ("???");
        line4 = g_strdup_printf (_("Sunrise: %s / Sunset: %s"),
                                 sunrise_str, sunset_str);
        g_free (sunrise_str);
        g_free (sunset_str);

        if (sys_timezone)
                setenv ("TZ", sys_timezone, 1);
        else
                unsetenv ("TZ");
        tzset ();

        tip = g_strdup_printf ("<b>%s</b>\n%s\n%s%s", line1, line2, line3, line4);
        ctk_tooltip_set_markup (tooltip, tip);
        g_free (line1);
        g_free (line2);
        g_free (line3);
        g_free (line4);
        g_free (tip);
}

static gboolean
weather_tooltip (CtkWidget  *widget G_GNUC_UNUSED,
		 gint        x G_GNUC_UNUSED,
		 gint        y G_GNUC_UNUSED,
		 gboolean    keyboard_mode G_GNUC_UNUSED,
		 CtkTooltip *tooltip,
		 gpointer    data)
{
        ClockLocationTile *tile;
        ClockLocationTilePrivate *priv;
        WeatherInfo *info;
        int clock_format;

        tile = CLOCK_LOCATION_TILE (data);
        priv = clock_location_tile_get_instance_private (tile);
        info = clock_location_get_weather_info (priv->location);

        if (!info || !weather_info_is_valid (info))
                return FALSE;

        g_signal_emit (tile, signals[NEED_CLOCK_FORMAT], 0, &clock_format);

        weather_info_setup_tooltip (info, priv->location, tooltip, clock_format);

        return TRUE;
}

static void
update_weather_icon (ClockLocation *loc G_GNUC_UNUSED,
		     WeatherInfo   *info,
		     gpointer       data)
{
        ClockLocationTile *tile;
        ClockLocationTilePrivate *priv;
        cairo_surface_t *surface = NULL;
        CtkIconTheme *theme = NULL;
        const gchar *icon_name;
        gint icon_scale;

        if (!info || !weather_info_is_valid (info))
                return;

        tile = CLOCK_LOCATION_TILE (data);
        priv = clock_location_tile_get_instance_private (tile);
        theme = ctk_icon_theme_get_for_screen (ctk_widget_get_screen (CTK_WIDGET (priv->weather_icon)));
        icon_name = weather_info_get_icon_name (info);
        icon_scale = ctk_widget_get_scale_factor (CTK_WIDGET (priv->weather_icon));

        surface = ctk_icon_theme_load_surface (theme, icon_name, 16, icon_scale,
                                               NULL, CTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);

        if (surface) {
                ctk_image_set_from_surface (CTK_IMAGE (priv->weather_icon), surface);
                ctk_widget_set_margin_end (priv->weather_icon, 6);
                cairo_surface_destroy (surface);
        }
}

ClockLocation *
clock_location_tile_get_location (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv;

        g_return_val_if_fail (IS_CLOCK_LOCATION_TILE (this), NULL);

        priv = clock_location_tile_get_instance_private (this);

        return g_object_ref (priv->location);
}
