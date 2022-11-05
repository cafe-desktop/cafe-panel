lib_LTLIBRARIES = libcafe-panel-applet-4.la
noinst_PROGRAMS = test-dbus-applet

AM_CPPFLAGS =							\
	$(LIBCAFE_PANEL_APPLET_CFLAGS)				\
	-I$(top_builddir)/libcafe-panel-applet			\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libcafe_panel_appletincludedir = $(includedir)/cafe-panel-4.0/libcafe-panel-applet
libcafe_panel_appletinclude_HEADERS =	\
	cafe-panel-applet.h			\
	cafe-panel-applet-gsettings.h		\
	cafe-panel-applet-enums.h

libcafe_panel_applet_4_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	cafe-panel-applet.h			\
	cafe-panel-applet.c			\
	cafe-panel-applet-factory.h		\
	cafe-panel-applet-factory.c		\
	cafe-panel-applet-gsettings.c		\
	cafe-panel-applet-gsettings.h       \
	panel-applet-private.h

if ENABLE_X11
libcafe_panel_applet_4_la_SOURCES +=	\
	panel-plug.c			\
	panel-plug-private.h
endif

libcafe_panel_applet_4_la_LIBADD  = \
	$(LIBCAFE_PANEL_APPLET_LIBS) \
	$(X_LIBS)

libcafe_panel_applet_4_la_LDFLAGS = \
	-version-info $(LIB_CAFE_PANEL_APPLET_LT_VERSION)	\
	-no-undefined

test_dbus_applet_LDADD =	\
	$(LIBCAFE_PANEL_APPLET_LIBS)	\
	libcafe-panel-applet-4.la

$(libcafe_panel_applet_4_la_OBJECTS) $(test_dbus_applet_OBJECTS): $(BUILT_SOURCES)

cafe-panel-applet-marshal.h: cafe-panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=cafe_panel_applet_marshal > $@

cafe-panel-applet-marshal.c: cafe-panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"cafe-panel-applet-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=cafe_panel_applet_marshal >> $@

cafe_panel_applet_enum_headers = $(top_srcdir)/libcafe-panel-applet/cafe-panel-applet.h

cafe-panel-applet-enums.c: @REBUILD@ $(cafe_panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"cafe-panel-applet-enums.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--fprod "\n#include \"@filename@\"\n" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(cafe_panel_applet_enum_headers) > $@

cafe-panel-applet-enums.h: @REBUILD@ $(cafe_panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __CAFE_PANEL_APPLET_ENUMS_H__\n" \
			--fhead "#define __CAFE_PANEL_APPLET_ENUMS_H__\n\n" \
			--fhead "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n" \
			--ftail "#ifdef __cplusplus\n}\n#endif\n\n" \
			--ftail "#endif /* __CAFE_PANEL_APPLET_ENUMS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMNAME@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(cafe_panel_applet_enum_headers) > $@

BUILT_SOURCES = \
	cafe-panel-applet-enums.c			\
	cafe-panel-applet-enums.h			\
	cafe-panel-applet-marshal.c			\
	cafe-panel-applet-marshal.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcafepanelapplet-4.0.pc

appletdir       = $(datadir)/cafe-panel/applets
applet_in_files = org.cafe.panel.TestApplet.cafe-panel-applet.desktop.in
noinst_DATA     = $(applet_in_files:.cafe-panel-applet.desktop.in=.cafe-panel-applet)
$(noinst_DATA): $(applet_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword Name --keyword Description --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =						\
	org.cafe.panel.TestApplet.cafe-panel-applet.desktop.in	\
	cafe-panel-applet-marshal.list			\
	libcafepanelapplet-4.0.pc.in			\
	libcafepanelapplet-4.0-uninstalled.pc.in

CLEANFILES = $(BUILT_SOURCES) $(noinst_DATA)

###############################
# Introspection generation

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir) --add-include-path=${prefix}/share/gir-1.0
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = 	\
	cafe-panel-applet.c		\
	cafe-panel-applet.h		\
	cafe-panel-applet-gsettings.c	\
	cafe-panel-applet-gsettings.h
built_introspection_sources =	\
	cafe-panel-applet-enums.c	\
	cafe-panel-applet-enums.h

CafePanelApplet-4.0.gir: libcafe-panel-applet-4.la
CafePanelApplet_4_0_gir_INCLUDES = GObject-2.0 Ctk-3.0 Gio-2.0
CafePanelApplet_4_0_gir_CFLAGS = $(AM_CPPFLAGS) -I$(srcdir)
CafePanelApplet_4_0_gir_LIBS = libcafe-panel-applet-4.la
CafePanelApplet_4_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources)) $(addprefix $(builddir)/,$(built_introspection_sources))
# When requiring gobject-introspection 0.9.5, use this:
#CafePanelApplet_4_0_gir_SCANNERFLAGS = --identifier-prefix=Panel --symbol-prefix=panel --pkg-export=libcafepanelapplet-4.0
CafePanelApplet_4_0_gir_SCANNERFLAGS = --strip-prefix=CafePanel --pkg-export=libcafepanelapplet-4.0
INTROSPECTION_GIRS += CafePanelApplet-4.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

################################

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
