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
	@cp sysmodule/PhantomCtrl.nsp dist/atmosphere/contents/$(PROGRAM_ID)/exefs.nsp
	@cp sysmodule/toolbox.json     dist/atmosphere/contents/$(PROGRAM_ID)/toolbox.json
	@touch dist/atmosphere/contents/$(PROGRAM_ID)/flags/boot2.flag
	@cp overlay/PhantomCtrl.ovl    dist/switch/.overlays/PhantomCtrl.ovl
	@printf 'enabled=1\n' > dist/config/phantomctrl/config.ini
	@echo "dist/ ready. Copy its contents to the root of your SD card."

clean:
	@$(MAKE) --no-print-directory -C sysmodule clean
	@$(MAKE) --no-print-directory -C overlay clean
	@rm -rf dist
