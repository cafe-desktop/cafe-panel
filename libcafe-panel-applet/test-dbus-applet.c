#include <config.h>
#include <string.h>

#include "cafe-panel-applet.h"

static void
test_applet_on_do (CtkAction *action,
		   gpointer   user_data G_GNUC_UNUSED)
{
        g_message ("%s called\n", ctk_action_get_name (action));
}

static const CtkActionEntry test_applet_menu_actions[] = {
	{ "TestAppletDo1", NULL, "TestAppletDo1",
	  NULL, NULL,
	  G_CALLBACK (test_applet_on_do) },
	{ "TestAppletDo2", NULL, "TestAppletDo2",
	  NULL, NULL,
	  G_CALLBACK (test_applet_on_do) },
	{ "TestAppletDo3", NULL, "TestAppletDo3",
	  NULL, NULL,
	  G_CALLBACK (test_applet_on_do) }
};

static const char test_applet_menu_xml[] =
	"<menuitem name=\"Test Item 1\" action=\"TestAppletDo1\" />\n"
	"<menuitem name=\"Test Item 2\" action=\"TestAppletDo2\" />\n"
	"<menuitem name=\"Test Item 3\" action=\"TestAppletDo3\" />\n";

typedef struct _TestApplet      TestApplet;
typedef struct _TestAppletClass TestAppletClass;

struct _TestApplet {
	CafePanelApplet   base;
	CtkWidget    *label;
};

struct _TestAppletClass {
	CafePanelAppletClass   base_class;
};

static GType test_applet_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestApplet, test_applet, PANEL_TYPE_APPLET);

static void
test_applet_init (TestApplet *applet G_GNUC_UNUSED)
{
}

static void
test_applet_class_init (TestAppletClass *klass G_GNUC_UNUSED)
{
}

static void
test_applet_handle_orient_change (TestApplet           *applet,
				  CafePanelAppletOrient orient G_GNUC_UNUSED,
				  gpointer              dummy G_GNUC_UNUSED)
{
        gchar *text;

        text = g_strdup (ctk_label_get_text (CTK_LABEL (applet->label)));

        g_strreverse (text);

        ctk_label_set_text (CTK_LABEL (applet->label), text);

        g_free (text);
}

static void
test_applet_handle_size_change (TestApplet *applet,
				gint        size,
				gpointer    dummy G_GNUC_UNUSED)
{
	switch (size) {
	case 12:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"xx-small\">Hello</span>");
		break;
	case 24:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"x-small\">Hello</span>");
		break;
	case 36:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"small\">Hello</span>");
		break;
	case 48:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"medium\">Hello</span>");
		break;
	case 64:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"large\">Hello</span>");
		break;
	case 80:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"x-large\">Hello</span>");
		break;
	case 128:
		ctk_label_set_markup (
			CTK_LABEL (applet->label), "<span size=\"xx-large\">Hello</span>");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
test_applet_handle_background_change (TestApplet                   *applet,
				      CafePanelAppletBackgroundType type,
				      CdkColor                     *color,
				      cairo_pattern_t              *pattern,
				      gpointer                      dummy G_GNUC_UNUSED)
{
	CdkWindow *window = ctk_widget_get_window (applet->label);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		g_message ("Setting background to default");
		cdk_window_set_background_pattern (window, NULL);
		break;
	case PANEL_COLOR_BACKGROUND:
		g_message ("Setting background to #%2x%2x%2x",
			   color->red, color->green, color->blue);
		cdk_window_set_background_pattern (window, NULL);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		g_message ("Setting background to '%p'", pattern);
		cdk_window_set_background_pattern (window, pattern);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
test_applet_fill (TestApplet *applet)
{
	CtkActionGroup *action_group;

	applet->label = ctk_label_new (NULL);

	ctk_container_add (CTK_CONTAINER (applet), applet->label);

	ctk_widget_show_all (CTK_WIDGET (applet));

	test_applet_handle_size_change (applet,
					cafe_panel_applet_get_size (CAFE_PANEL_APPLET (applet)),
					NULL);
	test_applet_handle_orient_change (applet,
					  cafe_panel_applet_get_orient (CAFE_PANEL_APPLET (applet)),
					  NULL);

	action_group = ctk_action_group_new ("TestAppletActions");
	ctk_action_group_add_actions (action_group,
				      test_applet_menu_actions,
				      G_N_ELEMENTS (test_applet_menu_actions),
				      applet);

	cafe_panel_applet_setup_menu (CAFE_PANEL_APPLET (applet),
				 test_applet_menu_xml,
				 action_group);
	g_object_unref (action_group);

	ctk_widget_set_tooltip_text (CTK_WIDGET (applet), "Hello Tip");

	cafe_panel_applet_set_flags (CAFE_PANEL_APPLET (applet), CAFE_PANEL_APPLET_HAS_HANDLE);

	g_signal_connect (G_OBJECT (applet),
			  "change_orient",
			  G_CALLBACK (test_applet_handle_orient_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_size",
			  G_CALLBACK (test_applet_handle_size_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_background",
			  G_CALLBACK (test_applet_handle_background_change),
			  NULL);

	return TRUE;
}

static gboolean
test_applet_factory (TestApplet  *applet,
		     const gchar *iid,
		     gpointer     data G_GNUC_UNUSED)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "TestApplet"))
		retval = test_applet_fill (applet);

	return retval;
}


CAFE_PANEL_APPLET_OUT_PROCESS_FACTORY ("TestAppletFactory",
				  test_applet_get_type (),
				  "A Test Applet for the CAFE-3.0 Panel",
				  (CafePanelAppletFactoryCallback) test_applet_factory,
				  NULL)

