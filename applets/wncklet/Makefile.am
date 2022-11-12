AM_CPPFLAGS =							\
	$(LIBCAFE_PANEL_APPLET_CFLAGS)				\
	$(VNCKLET_CFLAGS)					\
	-I$(top_builddir)					\
	-I$(top_builddir)/applets/vncklet			\
	-I$(top_srcdir)/libcafe-panel-applet				\
	-I$(top_builddir)/libcafe-panel-applet			\
	-DCAFELOCALEDIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

VNCKLET_SOURCES = \
	vncklet.c \
	vncklet.h \
	window-menu.c \
	window-menu.h \
	window-list.c \
	window-list.h \
	workspace-switcher.c \
	workspace-switcher.h \
	showdesktop.c \
	showdesktop.h \
	$(BUILT_SOURCES)

VNCKLET_LDADD =						\
	../../libcafe-panel-applet/libcafe-panel-applet-4.la	\
	$(VNCKLET_LIBS)					\
	$(LIBCAFE_PANEL_APPLET_LIBS)

if VNCKLET_INPROCESS
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(pkglibdir)/libvnck-applet.so

vnck_appletlibdir = $(pkglibdir)
vnck_appletlib_LTLIBRARIES = libvnck-applet.la
libvnck_applet_la_SOURCES = $(VNCKLET_SOURCES)
libvnck_applet_la_LIBADD = $(VNCKLET_LDADD)
libvnck_applet_la_LDFLAGS = -module -avoid-version
libvnck_applet_la_CFLAGS = $(AM_CFLAGS)
else
APPLET_IN_PROCESS = false
APPLET_LOCATION   = $(libexecdir)/vnck-applet

libexec_PROGRAMS = vnck-applet
vnck_applet_SOURCES = $(VNCKLET_SOURCES)
vnck_applet_LDADD = $(VNCKLET_LDADD)
vnck_applet_CFLAGS = $(AM_CFLAGS)
endif

appletdir       = $(datadir)/cafe-panel/applets
applet_in_files = org.cafe.panel.Vncklet.cafe-panel-applet.desktop.in
applet_DATA     = $(applet_in_files:.cafe-panel-applet.desktop.in=.cafe-panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@IN_PROCESS\@|$(APPLET_IN_PROCESS)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

$(applet_DATA): $(applet_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@

if !VNCKLET_INPROCESS
servicedir       = $(datadir)/dbus-1/services
service_in_files = org.cafe.panel.applet.VnckletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.cafe.panel.applet.VnckletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@
endif

ui_FILES = \
	showdesktop-menu.xml \
	window-list-menu.xml \
	window-list.ui \
	window-menu-menu.xml \
	workspace-switcher-menu.xml \
	workspace-switcher.ui

vncklet-resources.c: vncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/vncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name vncklet $<

vncklet-resources.h: vncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/vncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name vncklet $<

BUILT_SOURCES = 		\
	vncklet-resources.c	\
	vncklet-resources.h

if HAVE_WINDOW_PREVIEWS
vncklet_gschemas_in = \
	org.cafe.panel.applet.window-list.gschema.xml.in \
	org.cafe.panel.applet.window-list-previews.gschema.xml.in \
	org.cafe.panel.applet.workspace-switcher.gschema.xml.in
else
vncklet_gschemas_in = \
	org.cafe.panel.applet.window-list.gschema.xml.in \
	org.cafe.panel.applet.workspace-switcher.gschema.xml.in
endif
gsettings_SCHEMAS = $(vncklet_gschemas_in:.xml.in=.xml)

@GSETTINGS_RULES@

EXTRA_DIST = \
	org.cafe.panel.Vncklet.cafe-panel-applet.desktop.in.in \
	$(service_in_files) \
	$(vncklet_gschemas_in) \
	$(ui_FILES) \
	vncklet.gresource.xml

CLEANFILES = $(BUILT_SOURCES) $(applet_DATA) $(applet_in_files) $(service_DATA) $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
