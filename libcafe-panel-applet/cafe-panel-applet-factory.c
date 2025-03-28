/*
 * cafe-panel-applet-factory.c: panel applet writing API.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "cafe-panel-applet.h"
#include "panel-applet-private.h"

#include "cafe-panel-applet-factory.h"

#ifdef HAVE_X11
#include <cdk/cdkx.h>
#endif

struct _CafePanelAppletFactory {
	GObject    base;

	gchar     *factory_id;
	guint      n_applets;
	gboolean   out_of_process;
	GType      applet_type;
	GClosure  *closure;

	GDBusConnection *connection;
	gint             owner_id;
	gint             registration_id;

	GHashTable      *applets;
	guint            next_uid;
};

struct _CafePanelAppletFactoryClass {
	GObjectClass base_class;
};

#define CAFE_PANEL_APPLET_FACTORY_OBJECT_PATH  "/org/cafe/panel/applet/%s"
#define CAFE_PANEL_APPLET_FACTORY_SERVICE_NAME "org.cafe.panel.applet.%s"

G_DEFINE_TYPE (CafePanelAppletFactory, cafe_panel_applet_factory, G_TYPE_OBJECT)

static GHashTable *factories = NULL;

static void
cafe_panel_applet_factory_finalize (GObject *object)
{
	CafePanelAppletFactory *factory = CAFE_PANEL_APPLET_FACTORY (object);

	if (factory->registration_id) {
		g_dbus_connection_unregister_object (factory->connection, factory->registration_id);
		factory->registration_id = 0;
	}

	if (factory->owner_id) {
		g_bus_unown_name (factory->owner_id);
		factory->owner_id = 0;
	}

	g_hash_table_remove (factories, factory->factory_id);

	if (g_hash_table_size (factories) == 0) {
		g_hash_table_unref (factories);
		factories = NULL;
	}

	if (factory->factory_id) {
		g_free (factory->factory_id);
		factory->factory_id = NULL;
	}

	if (factory->applets) {
		g_hash_table_unref (factory->applets);
		factory->applets = NULL;
	}

	if (factory->closure) {
		g_closure_unref (factory->closure);
		factory->closure = NULL;
	}

	G_OBJECT_CLASS (cafe_panel_applet_factory_parent_class)->finalize (object);
}

static void
cafe_panel_applet_factory_init (CafePanelAppletFactory *factory)
{
	factory->applets = g_hash_table_new (NULL, NULL);
	factory->next_uid = 1;
}

static void
cafe_panel_applet_factory_class_init (CafePanelAppletFactoryClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = cafe_panel_applet_factory_finalize;
}

static void
cafe_panel_applet_factory_applet_removed (CafePanelAppletFactory *factory,
				     GObject            *applet)
{
	guint uid;

	uid = GPOINTER_TO_UINT (g_object_get_data (applet, "uid"));

	g_hash_table_remove (factory->applets, GUINT_TO_POINTER (uid));

	factory->n_applets--;
	if (factory->n_applets == 0)
		g_object_unref (factory);
}

CafePanelAppletFactory *
cafe_panel_applet_factory_new (const gchar *factory_id,
				gboolean     out_of_process,
			  GType        applet_type,
			  GClosure    *closure)
{
	CafePanelAppletFactory *factory;

	factory = CAFE_PANEL_APPLET_FACTORY (g_object_new (PANEL_TYPE_APPLET_FACTORY, NULL));
	factory->factory_id = g_strdup (factory_id);
	factory->out_of_process = out_of_process;
	factory->applet_type = applet_type;
	factory->closure = g_closure_ref (closure);
	if (factories == NULL) {
		factories = g_hash_table_new (g_str_hash, g_str_equal);
	}

	g_hash_table_insert (factories, factory->factory_id, factory);

	return factory;
}

static void
set_applet_constructor_properties (GObject  *applet,
				   GVariant *props)
{
	GVariantIter iter;
	GVariant    *value;
	gchar       *key;

	g_variant_iter_init (&iter, props);
	while (g_variant_iter_loop (&iter, "{sv}", &key, &value)) {
		switch (g_variant_classify (value)) {
		case G_VARIANT_CLASS_UINT32: {
			guint32 v = g_variant_get_uint32 (value);

			g_object_set (applet, key, v, NULL);
		}
			break;
		case G_VARIANT_CLASS_STRING: {
			const gchar *v = g_variant_get_string (value, NULL);

			g_object_set (applet, key, v, NULL);
		}
			break;
		case G_VARIANT_CLASS_BOOLEAN: {
			gboolean v = g_variant_get_boolean (value);

			g_object_set (applet, key, v, NULL);
		}
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}


static void
cafe_panel_applet_factory_get_applet (CafePanelAppletFactory    *factory,
				 GDBusConnection       *connection,
				 GVariant              *parameters,
				 GDBusMethodInvocation *invocation)
{
	GObject     *applet;
	const gchar *applet_id;
	gint         screen_num;
	GVariant    *props;
	GVariant    *return_value;
	guint32      xid;
	guint32      uid;
	const gchar *object_path;

	g_variant_get (parameters, "(&si@a{sv})", &applet_id, &screen_num, &props);

	applet = g_object_new (factory->applet_type,
                   "out-of-process", factory->out_of_process,
			       "id", applet_id,
			       "connection", connection,
			       "closure", factory->closure,
			       NULL);
	factory->n_applets++;
	g_object_weak_ref (applet, (GWeakNotify) cafe_panel_applet_factory_applet_removed, factory);

	set_applet_constructor_properties (applet, props);
	g_variant_unref (props);

#ifdef HAVE_X11
	if (CDK_IS_X11_DISPLAY (cdk_display_get_default ())) {
		CdkScreen   *screen;

		screen = screen_num != -1 ?
			cdk_display_get_default_screen (cdk_display_get_default ()) :
			cdk_screen_get_default ();
		xid = cafe_panel_applet_get_xid (CAFE_PANEL_APPLET (applet), screen);
	} else
#endif
	{ // Not using X11
		xid = 0;
	}

	uid = factory->next_uid++;
	object_path = cafe_panel_applet_get_object_path (CAFE_PANEL_APPLET (applet));
	g_hash_table_insert (factory->applets, GUINT_TO_POINTER (uid), applet);
	g_object_set_data (applet, "uid", GUINT_TO_POINTER (uid));

	return_value = g_variant_new ("(obuu)",
	                              object_path,
	                              factory->out_of_process,
	                              xid,
	                              uid);

	g_dbus_method_invocation_return_value (invocation, return_value);
}

static void
method_call_cb (GDBusConnection       *connection,
		const gchar           *sender G_GNUC_UNUSED,
		const gchar           *object_path G_GNUC_UNUSED,
		const gchar           *interface_name G_GNUC_UNUSED,
		const gchar           *method_name,
		GVariant              *parameters,
		GDBusMethodInvocation *invocation,
		gpointer               user_data)
{
	CafePanelAppletFactory *factory = CAFE_PANEL_APPLET_FACTORY (user_data);

	if (g_strcmp0 (method_name, "GetApplet") == 0) {
		cafe_panel_applet_factory_get_applet (factory, connection, parameters, invocation);
	}
}

static const gchar introspection_xml[] =
	"<node>"
	    "<interface name='org.cafe.panel.applet.AppletFactory'>"
	      "<method name='GetApplet'>"
	        "<arg name='applet_id' type='s' direction='in'/>"
	        "<arg name='screen' type='i' direction='in'/>"
	        "<arg name='props' type='a{sv}' direction='in'/>"
	        "<arg name='applet' type='o' direction='out'/>"
	        "<arg name='out-of-process' type='b' direction='out'/>"
	        "<arg name='xid' type='u' direction='out'/>"
	        "<arg name='uid' type='u' direction='out'/>"
	      "</method>"
	    "</interface>"
	  "</node>";

static const GDBusInterfaceVTable interface_vtable = {
	method_call_cb,
	NULL,
	NULL
};

static GDBusNodeInfo *introspection_data = NULL;

static void
on_bus_acquired (GDBusConnection        *connection,
		 const gchar            *name G_GNUC_UNUSED,
		 CafePanelAppletFactory *factory)
{
	gchar  *object_path;
	GError *error = NULL;

	if (!introspection_data)
		introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	object_path = g_strdup_printf (CAFE_PANEL_APPLET_FACTORY_OBJECT_PATH, factory->factory_id);
	factory->connection = connection;
	factory->registration_id = g_dbus_connection_register_object (connection,
					   object_path,
					   introspection_data->interfaces[0],
					   &interface_vtable,
					   factory, NULL,
					   &error);
	if (error) {
		g_printerr ("Failed to register object %s: %s\n", object_path, error->message);
		g_error_free (error);
	}

	g_free (object_path);
}

static void
on_name_lost (GDBusConnection        *connection G_GNUC_UNUSED,
	      const gchar            *name G_GNUC_UNUSED,
	      CafePanelAppletFactory *factory)
{
	g_object_unref (factory);
}

gboolean
cafe_panel_applet_factory_register_service (CafePanelAppletFactory *factory)
{
	gchar *service_name;

	if (!factory)
		return FALSE;

	service_name = g_strdup_printf (CAFE_PANEL_APPLET_FACTORY_SERVICE_NAME, factory->factory_id);
	factory->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
			service_name,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			(GBusAcquiredCallback) on_bus_acquired,
			NULL,
			(GBusNameLostCallback) on_name_lost,
			factory, NULL);
	g_free (service_name);

	return TRUE;
}

CtkWidget *
cafe_panel_applet_factory_get_applet_widget (const gchar *id,
                                        guint        uid)
{
	CafePanelAppletFactory *factory;
	GObject            *object;

	if (!factories)
		return NULL;

	factory = g_hash_table_lookup (factories, id);
	if (!factory)
		return NULL;

	object = g_hash_table_lookup (factory->applets, GUINT_TO_POINTER (uid));
	if (!object || !CTK_IS_WIDGET (object))
		return NULL;

	return CTK_WIDGET (object);
}
