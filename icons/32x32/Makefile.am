size = 32x32

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
	cafe-panel-clock.png			\
	cafe-panel-drawer.png			\
	cafe-panel-fish.png			\
	cafe-panel-force-quit.png		\
	cafe-panel-launcher.png		\
	cafe-panel-notification-area.png	\
	cafe-panel-separator.png		\
	cafe-panel.png				\
	cafe-panel-window-list.png		\
	cafe-panel-window-menu.png		\
	cafe-panel-workspace-switcher.png

icons_SOURCE =					\
	cafe-panel-clock.svg			\
	cafe-panel-drawer.svg			\
	cafe-panel-fish.svg			\
	cafe-panel-force-quit.svg		\
	cafe-panel-launcher.svg		\
	cafe-panel-notification-area.svg	\
	cafe-panel-separator.svg		\
	cafe-panel.svg				\
	cafe-panel-window-list.svg		\
	cafe-panel-window-menu.svg		\
	cafe-panel-workspace-switcher.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated. After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST =		\
	$(icons_DATA)	\
	$(icons_SOURCE)

-include $(top_srcdir)/git.mk
