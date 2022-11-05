size = scalable

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
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

ctk_update_icon_cache = ctk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(ctk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated. After (un)install, run this:"; \
		echo "***   $(ctk_update_icon_cache)"; \
	fi

EXTRA_DIST =		\
	$(icons_DATA)

-include $(top_srcdir)/git.mk
