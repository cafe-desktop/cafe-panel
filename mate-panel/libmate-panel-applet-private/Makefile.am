noinst_LTLIBRARIES =			\
	libcafe-panel-applet-private.la	\
	libcafe-panel-applet-private-mini.la

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	-I.							\
	-I$(srcdir)						\
	-I$(top_srcdir)/cafe-panel				\
	-I$(top_builddir)/cafe-panel				\
	-I$(top_builddir)/cafe-panel/libcafe-panel-applets-private	\
	-I$(top_builddir)/cafe-panel/libpanel-util		\
	-DDATADIR=\""$(datadir)"\"				\
	-DMATE_PANEL_APPLETS_DIR=\"$(appletsdir)\"			\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libcafe_panel_applet_private_la_SOURCES =	\
	panel-applets-manager-dbus.c	\
	panel-applets-manager-dbus.h	\
	panel-applet-container.c	\
	panel-applet-container.h	\
	panel-applet-frame-dbus.c	\
	panel-applet-frame-dbus.h

libcafe_panel_applet_private_mini_la_SOURCES =\
	panel-applet-mini.c		\
	panel-applets-manager-dbus.c	\
	panel-applets-manager-dbus.h	\
	panel-applet-container.c	\
	panel-applet-container.h

-include $(top_srcdir)/git.mk
