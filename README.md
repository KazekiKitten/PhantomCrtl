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
- **Handheld suppression is EXPERIMENTAL and unverified on hardware.** This is
  the one genuinely uncertain feature — see [Testing plan](#testing-plan). It
  is **off by default** so the base behavior is safe to try first.
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

1. Open the overlay menu (default Tesla combo is **L + DPad-Down + R3**; Ultrahand
   may differ).
2. Select **PhantomCtrl**. You'll see:
   - **PhantomCtrl** toggle — master on/off (virtual pad feed).
   - **Suppress Handheld** toggle — the experimental lever (leave OFF first).
   - **Status**: virtual pad attached/detached, its ID, and wireless pad count.
3. With PhantomCtrl ON, start your 2-player game and pair your one wireless
   controller as usual (HOME → Controllers → Change Grip/Order).

---

## Testing plan

Do this in order. Stop at the first step that gives you working 2-player.

1. **Base check (suppression OFF).** Enable PhantomCtrl, start the game. Does the
   game now show a virtual Pro Controller as a player? Many games are happy with
   virtual-FullKey + your wireless pad and just ignore the extra Handheld. If
   2-player works here, you're done — leave suppression off.

2. **If the game still won't give you two slots**, turn **Suppress Handheld** ON
   and retest. This asks the system to re-evaluate npad slot ownership so the raw
   Handheld stops claiming a slot.

3. **If it still fails**, the game is hard-binding raw Handheld to P1 and a
   virtual-device approach can't fully hide it. That would require a `hid-mitm`
   approach (intercepting HID system-wide) — a larger, riskier change. Tell me
   which game and we'll decide whether it's worth it.

Please report which of the three you land on — that tells us whether suppression
actually works on real hardware, which is the main open question.

---

## Safety

- **Ban risk: effectively nil.** No network code, no online services touched.
  This is consistent with how sys-con / InputRedirection are treated.
- **Brick / save-corruption risk: none from this code.** It writes only
  `/config/phantomctrl/config.ini` on the SD card. No NAND, no saves.
- **Realistic worst case:** a bug in the input path hangs a game or drops a
  controller. Mitigations: it runs as a *contained* sysmodule (not a system-wide
  mitm), and the overlay toggle kills the virtual pad instantly. If something
  feels off, toggle PhantomCtrl OFF, or disable the module in ovl-sysmodules
  (no reboot needed — `requires_reboot` is false).

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
│       ├── suppress.c    experimental Handheld suppression
│       └── config.c      SD config-file read/write channel
└── overlay/
    ├── Makefile
    ├── include/          tesla.hpp + stb_truetype.h (fetched)
    └── source/main.cpp   Tesla overlay UI
```

## Title ID

`0100000000504843` — the low bytes are ASCII `PHC`.
