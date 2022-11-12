AC_INIT([cafe-panel], [1.24.1], [https://github.com/cafe-desktop/cafe-panel/issues],
    [cafe-panel], [http://www.cafe-desktop.org])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar -Wno-portability check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])
CAFE_COMMON_INIT

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LIB_CAFE_PANEL_APPLET_LT_VERSION=1:1:0
AC_SUBST(LIB_CAFE_PANEL_APPLET_LT_VERSION)

AM_MAINTAINER_MODE
CAFE_MAINTAINER_MODE_DEFINES
CAFE_DEBUG_CHECK([no])

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL
#AC_PROG_SED available in 2.59b
AM_PROG_CC_C_O

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
AC_PATH_PROG([GDBUS_CODEGEN], [gdbus-codegen])

CAFE_COMPILE_WARNINGS

# Check for introspection
GOBJECT_INTROSPECTION_CHECK([0.6.7])

m4_pattern_allow([AM_V_GEN])dnl Make autoconf not complain about the rule below
PANEL_INTLTOOL_CAFE_PANEL_APPLET_RULE='%.cafe-panel-applet: %.cafe-panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([PANEL_INTLTOOL_CAFE_PANEL_APPLET_RULE])

AC_ARG_ENABLE(deprecation_flags,
              [AC_HELP_STRING([--enable-deprecation-flags],
                              [use *_DISABLE_DEPRECATED flags @<:@default=no@:>@])],,
              [enable_deprecation_flags=no])

if test "x$enable_deprecation_flags" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

LT_LIB_M

LIBCAFE_DESKTOP_REQUIRED=1.17.0
GDK_PIXBUF_REQUIRED=2.25.2
PANGO_REQUIRED=1.15.4
GLIB_REQUIRED=2.50.0
LIBCAFE_MENU_REQUIRED=1.21.0
CAIRO_REQUIRED=1.0.0
DCONF_REQUIRED=0.13.4
LIBRSVG_REQUIRED=2.36.2
CTK_REQUIRED=3.22.0
LIBVNCK_REQUIRED=3.4.6
LIBVNCK_PREVIEWS_OPTIONAL=3.32.0
WEATHER_REQUIRED=1.17.0

dnl pkg-config dependency checks

PKG_CHECK_MODULES(EGG_SMCLIENT, ice sm ctk+-3.0)

PKG_CHECK_MODULES(GMODULE, gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
PKG_CHECK_MODULES(PANEL, $GMODULE_ADD gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED pango >= $PANGO_REQUIRED ctk+-3.0 >= $CTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gio-unix-2.0 >= $GLIB_REQUIRED cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED gio-2.0 >= $GLIB_REQUIRED libcafe-menu >= $LIBCAFE_MENU_REQUIRED)
AC_SUBST(PANEL_CFLAGS)
AC_SUBST(PANEL_LIBS)

PKG_CHECK_MODULES(DCONF, dconf >= $DCONF_REQUIRED)

PKG_CHECK_MODULES(LIBCAFE_PANEL_APPLET, ctk+-3.0 >= $CTK_REQUIRED gio-2.0 >= $GLIB_REQUIRED gio-unix-2.0 >= $GLIB_REQUIRED)
AC_SUBST(LIBCAFE_PANEL_APPLET_CFLAGS)
AC_SUBST(LIBCAFE_PANEL_APPLET_LIBS)

PKG_CHECK_MODULES(FISH, ctk+-3.0 >= $CTK_REQUIRED cairo >= $CAIRO_REQUIRED cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED)
AC_SUBST(FISH_CFLAGS)
AC_SUBST(FISH_LIBS)

PKG_CHECK_MODULES(NOTIFICATION_AREA, ctk+-3.0 >= $CTK_REQUIRED cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED)
AC_SUBST(NOTIFICATION_AREA_CFLAGS)
AC_SUBST(NOTIFICATION_AREA_LIBS)

# Check if we have a version of libvnck that allows for window previews
PKG_CHECK_MODULES(VNCKLET, ctk+-3.0 >= $CTK_REQUIRED libvnck-3.0 >= $LIBVNCK_PREVIEWS_OPTIONAL cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED, have_window_previews=yes, [
   PKG_CHECK_MODULES(VNCKLET, ctk+-3.0 >= $CTK_REQUIRED libvnck-3.0 >= $LIBVNCK_REQUIRED cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED, have_window_previews=no)
])
AC_SUBST(VNCKLET_CFLAGS)
AC_SUBST(VNCKLET_LIBS)
AM_CONDITIONAL(HAVE_WINDOW_PREVIEWS, [test "x$have_window_previews" = "xyes"])
if test "x$have_window_previews" = "xyes"; then
   AC_DEFINE([HAVE_WINDOW_PREVIEWS], 1, [Defined when using a version of libvnck that provides window-list previews])
fi

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_FUNCS(nl_langinfo)

PKG_CHECK_MODULES(TZ, gio-2.0 >= $GLIB_REQUIRED)
AC_SUBST(TZ_CFLAGS)
AC_SUBST(TZ_LIBS)

PKG_CHECK_MODULES(CLOCK, pango >= $PANGO_REQUIRED ctk+-3.0 >= $CTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gio-2.0 >= $GLIB_REQUIRED librsvg-2.0 >= $LIBRSVG_REQUIRED cafeweather >= $WEATHER_REQUIRED cafe-desktop-2.0 >= $LIBCAFE_DESKTOP_REQUIRED)
AC_SUBST(CLOCK_CFLAGS)
AC_SUBST(CLOCK_LIBS)

# Make it possible to compile the applets in-process
PANEL_INPROCESS_NONE=
PANEL_INPROCESS_ALL=
PANEL_INPROCESS_APPLETS=
AC_ARG_WITH(in-process-applets,
	AC_HELP_STRING([--with-in-process-applets=APPLETS],
		[comma-separated list of applets to compile in-process (possible values: none, clock, fish, notification-area, vncklet, all) @<:@default=none@:>@]),
	[for i in `echo $withval | tr , ' '`; do
		if test $i = "none"; then
			PANEL_INPROCESS_NONE=1
		else if test $i = "all"; then
			PANEL_INPROCESS_ALL=1
		else if test $i = "clock"; then
			PANEL_INPROCESS_APPLETS="$PANEL_INPROCESS_APPLETS $i"
		else if test $i = "fish"; then
			PANEL_INPROCESS_APPLETS="$PANEL_INPROCESS_APPLETS $i"
		else if test $i = "notification-area"; then
			PANEL_INPROCESS_APPLETS="$PANEL_INPROCESS_APPLETS $i"
		else if test $i = "vncklet"; then
			PANEL_INPROCESS_APPLETS="$PANEL_INPROCESS_APPLETS $i"
		else
			echo "applet $i not recognized, ignoring..."
		fi; fi; fi; fi; fi; fi
	done],
	[])

if test -n "$PANEL_INPROCESS_ALL"; then
	PANEL_INPROCESS_APPLETS="clock fish notification-area vncklet"
fi
if test -n "$PANEL_INPROCESS_NONE"; then
	PANEL_INPROCESS_APPLETS=""
fi
for i in $PANEL_INPROCESS_APPLETS; do
	if test $i = "clock"; then
		CLOCK_COMPILE_INPROCESS=1
		AC_DEFINE([CLOCK_INPROCESS], 1,
		[Defined when compiling the clock applet in-process])
	else if test $i = "fish"; then
		FISH_COMPILE_INPROCESS=1
		AC_DEFINE([FISH_INPROCESS], 1,
		[Defined when compiling the fish applet in-process])
	else if test $i = "notification-area"; then
		NOTIFICATION_AREA_COMPILE_INPROCESS=1
		AC_DEFINE([NOTIFICATION_AREA_INPROCESS], 1,
		[Defined when compiling the notification-area applet in-process])
	else if test $i = "vncklet"; then
		VNCKLET_COMPILE_INPROCESS=1
		AC_DEFINE([VNCKLET_INPROCESS], 1,
		[Defined when compiling the vncklet applet in-process])
	fi; fi; fi; fi
done
if test "x$PANEL_INPROCESS_APPLETS" = "x"; then
	PANEL_INPROCESS_APPLETS="(none)"
fi

AM_CONDITIONAL(CLOCK_INPROCESS, test -n "$CLOCK_COMPILE_INPROCESS")
AM_CONDITIONAL(FISH_INPROCESS, test -n "$FISH_COMPILE_INPROCESS")
AM_CONDITIONAL(NOTIFICATION_AREA_INPROCESS, test -n "$NOTIFICATION_AREA_COMPILE_INPROCESS")
AM_CONDITIONAL(VNCKLET_INPROCESS, test -n "$VNCKLET_COMPILE_INPROCESS")

dnl **************************************************************************
dnl Check if struct dirent contains a d_type member, for run dialog
dnl **************************************************************************

AC_STRUCT_DIRENT_D_TYPE

# $enable_x11 and $enable_wayland will be set to "yes", "no" or "auto"
AC_ARG_ENABLE(x11,
              [AC_HELP_STRING([--enable-x11],
                              [Explicitly enable or disable X11 support
                              (default is to enable only if X development libraries are detected)])],
              [enable_x11=$enableval],
              [enable_x11=auto])

AC_ARG_ENABLE(wayland,
              [AC_HELP_STRING([--enable-wayland],
                              [Explicitly enable or disable Wayland support
                              (default is to enable only if Wayland client development library is detected)])],
              [enable_wayland=$enableval],
              [enable_wayland=auto])

# Check if we have ctk-layer-shell installed, and thus should build with Wayland support
have_wayland=no
if test "x$enable_wayland" != "xno"; then
  PKG_CHECK_MODULES(CTK_LAYER_SHELL, ctk-layer-shell-0, have_wayland=yes, [
        if test "x$enable_wayland" = "xyes"; then
          AC_MSG_ERROR([Wayland enabled but CTK Layer Shell library not found])
        fi
    ])
fi

AM_CONDITIONAL(ENABLE_WAYLAND, [test "x$have_wayland" = "xyes"])

if test "x$have_wayland" = "xyes"; then
  AC_DEFINE(HAVE_WAYLAND, 1, [Have the Wayland development library])
fi

AC_SUBST(CTK_LAYER_SHELL_CFLAGS)
AC_SUBST(CTK_LAYER_SHELL_LIBS)

# Check if we have the X development libraries
have_x11=no
if test "x$enable_x11" != "xno"; then
  PKG_CHECK_MODULES(X, x11 xau, have_x11=yes, [
      if test "x$enable_x11" = "xyes"; then
        AC_MSG_ERROR([X development libraries not found])
      fi
    ])
fi

AM_CONDITIONAL(ENABLE_X11, [test "x$have_x11" = "xyes"])

if test "x$have_x11" = "xyes"; then
  AC_DEFINE(HAVE_X11, 1, [Have the X11 development library])
fi

if test "x$have_x11" != "xyes" -a "x$have_wayland" != "xyes"; then
  if test "x$enable_wayland" = "xno" -a "x$enable_x11" = "xno"; then
    AC_MSG_ERROR([At least one backend must be enabled])
  else
    AC_MSG_ERROR([No usable backend found, install X11 or Wayland development libraries])
  fi
fi

AC_SUBST(X_LIBS)

dnl X RANDR extension

PKG_CHECK_MODULES(XRANDR, xrandr >= 1.3.0, have_randr=yes, have_randr=no)
if test "x$have_randr" = "xyes"; then
  AC_DEFINE(HAVE_RANDR, 1, [Have the Xrandr extension library])
fi

dnl Modules dir
AC_SUBST([modulesdir],"\$(libdir)/cafe-panel/modules")

dnl Applets dir
AC_SUBST([appletsdir],"\$(datadir)/cafe-panel/applets")


dnl  Language Support
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=cafe-panel
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
AC_SUBST(GETTEXT_PACKAGE)

dnl gtk-doc stuff

GTK_DOC_CHECK([1.0])

dnl yelp-tools stuff

YELP_HELP_INIT

GLIB_GSETTINGS

dnl platform test
case $host in
*-*-solaris*)
	ostype=solaris
	AC_DEFINE(HAVE_SOLARIS, 1, [Define to 1])
	;;
esac

dnl Don't use AC_PROG_AWK since we need the full pathname.
AC_PATH_PROGS(AWK, mawk gawk nawk awk, )
AC_PATH_PROGS(PERL, perl5 perl)

dnl define a MAINT-like variable REBUILD which is set if Perl
dnl and awk are found, so autogenerated sources can be rebuilt
AC_ARG_ENABLE(rebuilds, [  --disable-rebuilds      disable all source autogeneration rules],,enable_rebuilds=yes)
REBUILD=\#
if test "x$enable_rebuilds" = "xyes" && \
     test -n "$PERL" && \
     $PERL -e 'exit !($] >= 5.002)' > /dev/null 2>&1 && \
     test -n "$AWK" ; then
  REBUILD=
fi
AC_SUBST(REBUILD)

AC_OUTPUT([
Makefile
data/Makefile
data/theme/Makefile
data/org.cafe.panel.gschema.xml
data/org.cafe.panel.menubar.gschema.xml
data/org.cafe.panel.object.gschema.xml
data/org.cafe.panel.toplevel.gschema.xml
icons/Makefile
icons/16x16/Makefile
icons/22x22/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
icons/64x64/Makefile
icons/96x96/Makefile
icons/256x256/Makefile
icons/scalable/Makefile
cafe-panel/Makefile
cafe-panel/libegg/Makefile
cafe-panel/libcafe-panel-applet-private/Makefile
cafe-panel/libpanel-util/Makefile
libcafe-panel-applet/libcafepanelapplet-4.0.pc
libcafe-panel-applet/libcafepanelapplet-4.0-uninstalled.pc
libcafe-panel-applet/Makefile
po/Makefile.in
applets/Makefile
applets/clock/Makefile
applets/clock/org.cafe.panel.applet.clock.gschema.xml
applets/clock/pixmaps/Makefile
applets/fish/Makefile
applets/fish/org.cafe.panel.applet.fish.gschema.xml
applets/notification_area/Makefile
applets/notification_area/org.cafe.panel.applet.notification-area.gschema.xml
applets/notification_area/libstatus-notifier-watcher/Makefile
applets/notification_area/status-notifier/Makefile
applets/notification_area/system-tray/Makefile
applets/vncklet/Makefile
applets/vncklet/org.cafe.panel.applet.window-list.gschema.xml
applets/vncklet/org.cafe.panel.applet.window-list-previews.gschema.xml
applets/vncklet/org.cafe.panel.applet.workspace-switcher.gschema.xml
doc/Makefile
doc/reference/Makefile
doc/reference/cafe-panel-applet/Makefile
help/Makefile
help/cafe-clock/Makefile
help/cafe-fish/Makefile
man/Makefile
])

if echo foo | xgettext --from-code=UTF-8 -LC -o - - 2>/dev/null ; then
:;
else
echo "Warning: Your version of gettext does not support --from-code."
echo "         This will cause translation of some strings in non US-English"
echo "         locales to fail. For full support please upgrade to GNU"
echo "         gettext 0.12 or later."
echo
fi

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              cafe-panel $VERSION
              `echo cafe-panel $VERSION | sed "s/./=/g"`

        prefix:                        ${prefix}
        exec_prefix:                   ${exec_prefix}
        libdir:                        ${libdir}
        bindir:                        ${bindir}
        sbindir:                       ${sbindir}
        sysconfdir:                    ${sysconfdir}
        localstatedir:                 ${localstatedir}
        datadir:                       ${datadir}
        source code location:          ${srcdir}
        compiler:                      ${CC}
        cflags:                        ${CFLAGS}
        warning flags:                 ${WARN_CFLAGS}
        Maintainer mode:               ${USE_MAINTAINER_MODE}
        Use *_DISABLE_DEPRECATED:      ${enable_deprecation_flags}
        Applets to build in-process:   ${PANEL_INPROCESS_APPLETS}
        Wayland support:               ${have_wayland}
        X11 support:                   ${have_x11}
        XRandr support:                ${have_randr}
        Build introspection support:   ${found_introspection}
        Build gtk-doc documentation:   ${enable_gtk_doc}

"
