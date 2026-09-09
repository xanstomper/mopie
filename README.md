# Mopie

[![M5StickC Plus 1.1](https://img.shields.io/badge/hardware-M5StickC%2B%201.1-blue)](https://shop.m5stack.com/)
[![ESP32](https://img.shields.io/badge/SoC-ESP32--PICO-informational)](https://www.espressif.com/)
[![C++](https://img.shields.io/badge/language-C%2B%2B-00599C)](https://isocpp.org/)
[![PlatformIO](https://img.shields.io/badge/platformio-ESP32-orange)](https://platformio.org/)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](LICENSE)
[![built for red team](https://img.shields.io/badge/use-red--team-red)](#)

Security-research and red-team firmware for the **M5StickC Plus 1.1** (ESP32-PICO, 520 KB SRAM, no PSRAM). Mopie was forked from the [Bruce](https://github.com/BruceDevices/firmware) firmware and rebuilt into its own thing: a different UI, more offensive/defensive tooling, on-device AI, and no upstream branding.

The short version: if Bruce is the "default ESP32 hacker stick," Mopie is the same hardware cut toward a single board, redone from the menu up, with tools Bruce doesn't ship.

> Mopie is a fork/rewrite of Bruce. It is not affiliated with the Bruce project.

---

## Quick start

```bash
pio run --environment m5stack-cplus1_1                          # build
pio run --target upload --environment m5stack-cplus1_1          # flash (USB, /dev/ttyUSB0)
```

You need an M5StickC Plus 1.1 and PlatformIO. Nothing else.

---

## What actually changed vs. Bruce

Not a re-skin. The table below is the honest diff of what I did to the codebase, not a marketing pitch.

### UI and interaction
| | Bruce | Mopie |
|---|---|---|
| Menu | classic list menus, nested | **paged grid** — all apps visible as icons, arrows navigate, one tap opens |
| Icons | stock Bruce art | hand-drawn **vector glyphs** drawn with TFT primitives; no bitmap mush, scale clean |
| Look | dark, flat | **tactical chrome** — corner brackets, bracketed headers, control hint bar, maroon/dark |
| Themes | one dark theme | **11 themes**, switch live from a menu (red = default) |
| Boot | static splash | animated intro: radar sweep + hex reveal + a short boot sound |
| Screen estate | menugrams eat the 240×135 | compact header/label so more tools fit per page |

### What Bruce won't do for you (added in Mopie)
- **On-device AI agent.** Chat from the stick's screen *or* from any browser on the same WiFi at `http://<ip>/ai`. Store an OpenAI-compatible API key + endpoint on the device, pull the live model list, and the model can drive the radio through real commands (`wifi_scan`, `ble_scan`, `deauth`, `troll_beacon`, `led`, `beep`, `sysinfo`, `reboot`, uptime/battery). The web dashboard is thin-client on purpose because this chip only has 520 KB RAM.
- **Phone Hijack.** Pair the stick as a BLE keyboard and, once the phone enables it, fire HID payloads at it. Has Android and iOS shortcut sets — endless chaos, search storm, screen tweak, keystroke flood, iOS Spotlight / app-mash / ESC flood / a "mega" mode that chains them.
- **WiFi Troll Toolbox.** SSID blast, kick-everyone, deauth-and-mock, and MAC spoofing, presented as prank tools on top of the real deauther/sniffer/responder primitives.
- **iOS AirDrop spoof** (BLE spam), plus iOS-oriented HID modes.
- **DetectiveMopie**, a pwnagotchi-style recon display for handshake hunting.

### Same proven core, de-branded
The WiFi attack engine (deauth, beacon flood, evil portal, netcut, Responder, karma, sniffer, handshake capture, ARP scan, SSH/Telnet/SOCKS4), the BLE spam suite (Apple/Samsung/Google/Windows + FastPair), and TV-B-Gone are Bruce's mature primitives carried forward — renamed and cleaned of Bruce strings so nothing in the visible UI says "Bruce."

### Things I fixed along the way
- **TV-B-Gone** silently did nothing on this board because the default IR pin is the *visible* front LED (GPIO 9), which can't emit IR. It now detects that, tells you to wire an IR LED to G26/G25/Grove, and forces an IR-pin pick before transmitting.
- **Web dashboard reboot on small RAM** — the model fetch ran over TLS on the async web task and used to crash the stick. Moved it to a dedicated task with a heap gate + 60 KB response cap.
- **BLE keyboard invisible to iOS** — advertising now sets `GEN_DISC|BREDR_UNSUP` so iPhones list it under Other Devices.
- **Menu debounce** — the input task stall was 10 ms; dropped to 2 ms so multi-press navigation isn't laggy.

---

## Feature map

**WiFi** — deauth (all / per-channel / enhanced + auto), beacon flood, evil portal, netcut, Responder, karma, sniffer, handshake capture, channel analyzer, jam detector, ARP scan, WireGuard, SSH/Telnet/SOCKS4, hidden-AP reveal, client list, signal meter, Troll Toolbox, WiFi pass recovery.

**BLE** — full spam suite (Apple pairing/action/not-yours, Samsung, Google FastPair, Windows SwiftPair, random-all), iOS AirDrop spoof, Beacon Clone, RSSI scan, distance track, device finder, GATT dump, jammer, adv sniffer, UUID scan, SwiftPair spam, Phone Hijack (HID).

**IR** — TV-B-Gone (NA / EU / universal), IR jammer, raw capture & repeat, custom codes.

**AI** — on-device chat, `/ai` web dashboard, model detection, 10 radio/hardware commands.

**Misc tools** — port scanner, subnet sweep, packet flooder, base64 / hash / hex, GPIO + voltage monitor, entropy pool, net scanner, MAC spoofer, panic wipe, DetectiveMopie.

---

## Hardware notes (read these)

- **M5StickC Plus 1.1 only** in this branch — `m5stack-cplus1_1`. Bruce's other board configs aren't maintained here.
- It's a 520 KB / no-PSRAM chip. The web dashboard and BLE-heavy tools are intentionally heap-guarded; don't run the web dashboard, BLE spam, and a big TLS scan at once or you'll trip the watchdog.
- **IR** needs an external IR LED wired to an IR-capable GPIO (G26 / G25 / Grove). No on-board IR.
- **BLE HID (Phone Hijack)** needs the phone to pair *and* enable the keyboard — iOS and Android both require this. No firmware bypasses that consent.
- **iOS beacon popups** — Apple killed pairing/AirDrop spam in iOS 16/17+. Mopie emits valid, current Apple Continuity + AirDrop beacons, but modern iOS often won't show a banner. That's the OS, not the code.

---

## Honest scope limits

- This branch is maintained for the M5StickC Plus 1.1, not the whole Bruced board lineup.
- Radio behavior (deauth actually kicking a client, a beacon visible in a scanner app, a TV turning off) can only be verified against live hardware — static checks pass, but the radio is the radio.
- Offensive tooling here is for **authorized testing, learning, and research only**. Use it only on systems you own or have explicit permission to test.

---

## License

[AGPL-3.0](LICENSE) — same as the upstream Bruce project. If you modify and distribute, the source of your changes must be made available under the same terms.