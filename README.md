# PhantomCtrl

A homebrew Atmosphère **sysmodule** for the Nintendo Switch **Lite** that enables
"Handheld + 1 wireless controller" local multiplayer in games that otherwise
reject the built-in Handheld controls as a player slot.

It reads your live built-in (Handheld) input and re-broadcasts it through a
**virtual FullKey (Pro-Controller-style) npad** using the `hiddbg` API, so a
game sees a "real" wireless-style Player 1 plus your one physical wireless
controller as Player 2. A **Tesla/Ultrahand overlay** lets you toggle it and
see status without restarting anything.

> Personal-use tool for local multiplayer on hardware you own. Local only — no
> network code, no online interaction, no NAND or save writes.

---

## Status / how finished is this

- **Builds cleanly** against devkitA64 + current libnx (tested with gcc 16.1.0,
  libnx with the fw 13+ Hdls session API).
- **Virtual pad path is complete** and is the well-trodden part (same core
  mechanism as sys-con / InputRedirection).
- **Ultrahand overlay is complete** - provides toggle and live status display.
- Handheld suppression has been **removed** due to crashes on certain firmware
  versions (it used unstable hiddbg API). If a game needs suppression to work,
  it will require a future hid-mitm approach.
- Overlay ↔ sysmodule uses a small **config file channel**, not a custom IPC
  service (deliberate — a malformed file is harmless; a buggy IPC server can
  hang the system service manager).

---

## 1. Toolchain setup (from scratch)

You need **devkitPro** with the Switch (devkitA64 + libnx) packages.

On this machine devkitPro is already installed at `/opt/devkitpro`. If you ever
set up a fresh machine:

1. Install devkitPro pacman: https://devkitpro.org/wiki/Getting_Started
2. Install the Switch toolchain group:
   ```
   sudo dkp-pacman -S switch-dev
   ```
3. Make sure these are in your environment (add to your shell profile):
   ```
   export DEVKITPRO=/opt/devkitpro
   export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
   ```

Verify:
```
aarch64-none-elf-gcc --version   # should print devkitA64 ...
ls $DEVKITPRO/libnx/lib/libnx.a  # should exist
```

---

## 2. Build

From the project root (`/home/kaze/PhantomCtrl`):

```
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH

make dist
```

That produces a ready-to-copy `dist/` folder:

```
dist/
├── atmosphere/contents/0100000000504843/
│   ├── exefs.nsp          <- the sysmodule
│   ├── toolbox.json       <- lets ovl-sysmodules toggle it
│   └── flags/boot2.flag   <- auto-start at boot
├── switch/.overlays/
│   └── PhantomCtrl.ovl     <- the overlay
└── config/phantomctrl/
    └── config.ini          <- default settings
```

Individual pieces, if you want them:
- `make sysmodule` → `sysmodule/PhantomCtrl.nsp`
- `make overlay`   → `overlay/PhantomCtrl.ovl`

---

## 3. Install to the SD card

Power off the Switch, pull the SD card, and **merge `dist/` onto the root of the
SD card** (it mirrors the SD layout, so it drops files into the right places):

| From `dist/` | Goes to on SD card |
|---|---|
| `atmosphere/contents/0100000000504843/` | `/atmosphere/contents/0100000000504843/` |
| `switch/.overlays/PhantomCtrl.ovl` | `/switch/.overlays/PhantomCtrl.ovl` |
| `config/phantomctrl/config.ini` | `/config/phantomctrl/config.ini` |

Requirements on the console:
- **Atmosphère** CFW (this uses the modern `contents/` layout).
- **Tesla/Ultrahand overlay loader** installed (`nx-ovlloader` + Tesla Menu, or
  Ultrahand). If you don't have it: install the Ultrahand overlay package, which
  includes the loader and is the current maintained successor to Tesla.

Reinsert the card and boot. The sysmodule auto-starts via `boot2.flag`.

---

## 4. Using it

1. Open the overlay menu (Ultrahand default is **ZL + ZR + DPad-Down**).
2. Select **PhantomCtrl**. You'll see:
   - **PhantomCtrl** toggle — master on/off (virtual pad feed).
   - **Status**: virtual pad attached/detached, its ID, and wireless pad count.
3. With PhantomCtrl ON, start your 2-player game and pair your one wireless
   controller as usual (HOME → Controllers → Change Grip/Order).

---

## Testing plan

Test on firmware 16.0.0+:

1. **Base check.** Enable PhantomCtrl, start the game. Does the game now show a
   virtual Pro Controller as a player? Many games are happy with virtual-FullKey
   + your wireless pad and just ignore the extra Handheld. If 2-player works
   here, you're done.

2. **If the game still won't give you two slots**, the game is hard-binding raw
   Handheld to P1 and this will require a `hid-mitm` approach (future work).

---

## Installation notes

- **Ultrahand required:** For firmware 16.0.0+, install Ultrahand instead of
  Tesla Menu (see https://github.com/ppkantorski/Ultrahand-Overlay). The included
  `tesla.hpp` may cause error 2168-0002 on newer firmware.
- If you see error 2168-0002 when opening overlays, your Tesla/Menu setup is
  outdated — update to Ultrahand or a compatible Tesla version.

---

## Safety

- **Ban risk: effectively nil.** No network code, no online services touched.
  This is consistent with how sys-con / InputRedirection are treated.
- **Brick / save-corruption risk: none from this code.** It writes only
  `/config/phantomctrl/config.ini` on the SD card. No NAND, no saves.
- The virtual controller is attached once at startup and stays attached — toggling
  only controls whether input is forwarded. This avoids handle-leak crashes that
  occur when attach/detach cycles corrupt the HDLS work buffer.

---

## Layout

```
PhantomCtrl/
├── Makefile              top-level: build both + assemble dist/
├── sysmodule/
│   ├── Makefile
│   ├── config/PhantomCtrl.json   NPDM: perms (hid, hiddbg, sm, fs)
│   ├── toolbox.json
│   └── source/
│       ├── phantom.h     shared defs + runtime state struct
│       ├── main.c        init, poll loop, virtual pad feed
│       └── config.c      SD config-file read/write channel
├── overlay/
│   ├── Makefile
│   ├── lib/libultrahand/     symlink to Status-Monitor-Overlay libultrahand
│   └── source/
│       └── main.cpp       Ultrahand overlay UI
└── dist/                    ready-to-copy sd card layout
    └── switch/.overlays/    PhantomCtrl.ovl
    └── config/phantomctrl/  config.ini for sysmodule state
    └── config/ultrahand/  config.ini for Ultrahand settings
```

## Migration to Ultrahand

On firmware 16.0.0+, the Tesla overlay may crash with error 2168-0002.
To migrate to Ultrahand:

1. Download `sdout.zip` from https://github.com/ppkantorski/Ultrahand-Overlay/releases
2. Extract to SD card root (replaces Tesla Menu)
3. Place `PhantomCtrl.ovl` in `/switch/.overlays/`
4. Use Ultrahand hotkey (default: ZL + ZR + DDown) to open the menu

## Title ID

`0100000000504843` — the low bytes are ASCII `PHC`.
