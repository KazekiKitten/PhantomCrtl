# PhantomCtrl top-level build.
# Builds the sysmodule (.nsp) and the Tesla overlay (.ovl), then assembles a
# ready-to-copy SD card layout under dist/.

PROGRAM_ID := 0100000000504843

.PHONY: all sysmodule overlay dist clean

all: sysmodule overlay

sysmodule:
	@$(MAKE) --no-print-directory -C sysmodule

overlay:
	@$(MAKE) --no-print-directory -C overlay

# Assemble the exact folder tree that goes on the SD card root.
dist: all
	@echo "Assembling dist/ ..."
	@rm -rf dist
	@mkdir -p dist/atmosphere/contents/$(PROGRAM_ID)/flags
	@mkdir -p dist/switch/.overlays
	@mkdir -p dist/config/phantomctrl
	@mkdir -p dist/config/ultrahand
	@cp sysmodule/PhantomCtrl.nsp dist/atmosphere/contents/$(PROGRAM_ID)/exefs.nsp
	@cp sysmodule/toolbox.json     dist/atmosphere/contents/$(PROGRAM_ID)/toolbox.json
	@touch dist/atmosphere/contents/$(PROGRAM_ID)/flags/boot2.flag
	@cp overlay/PhantomCtrl.ovl    dist/switch/.overlays/PhantomCtrl.ovl
	@mkdir -p dist/switch/.packages/UltrahandOverlay 2>/dev/null || true
	@printf 'enabled=1\nvirt_attached=false\nvirt_handle=0\nwireless_count=0\n' > dist/config/phantomctrl/config.ini
	@printf '[ultrahand]\nkey_combo=L+DDOWN+RS\nstartup_notification = false\n\n[PhantomCtrl]\nkey_combo=L+RS\n\n' > dist/config/ultrahand/overlays.ini
	@printf '[ultrahand]\nkey_combo=L+DDOWN+RS\nstartup_notification = false\n\n' > dist/config/ultrahand/config.ini
	@echo "dist/ ready. Copy its contents to the root of your SD card."
	@echo ""
	@echo "Note: Install ovlmenu.ovl from Ultrahand-Overlay release for menu support."

clean:
	@$(MAKE) --no-print-directory -C sysmodule clean
	@$(MAKE) --no-print-directory -C overlay clean
	@rm -rf dist