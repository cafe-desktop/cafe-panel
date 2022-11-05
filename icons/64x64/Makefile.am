size = 64x64

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
	cafe-panel-clock.png			\
	cafe-panel-drawer.png			\
	cafe-panel-fish.png			\
	cafe-panel-force-quit.png		\
	cafe-panel-launcher.png			\
	cafe-panel-notification-area.png	\
	cafe-panel.png				\
	cafe-panel-separator.png		\
	cafe-panel-window-list.png		\
	cafe-panel-window-menu.png		\
	cafe-panel-workspace-switcher.png

ctk_update_icon_cache = ctk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Ctk icon cache."; \
		$(ctk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated. After (un)install, run this:"; \
		echo "***   $(ctk_update_icon_cache)"; \
	fi

EXTRA_DIST =		\
	$(icons_DATA)

-include $(top_srcdir)/git.mk
