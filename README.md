# :robot: Mopie

**Mopie** is a security research and red-team firmware for the **M5StickC Plus 1.1** (ESP32), built from a Bruce firmware base and diverged into its own identity. It keeps the offensive-security mission but reworks the entire experience — a clean tactical grid UI, more tools, on-device AI, and zero upstream branding.

> Mopie is a **fork/rewrite of Bruce** — it is not affiliated with the Bruce project. It targets the **M5StickC Plus 1.1** build specifically.

---

## :zap: What is Mopie?

A portable Swiss-army gadget for:
- **Red-team / offensive security** — WiFi, BLE, IR/RF probes and attacks from a device you can hold in your hand
- **Trolling / pranks** — paired-device HID chaos, beacon flooding, TV-B-Gone
- **Defensive recon** — network scanning, handshake capture, deauth detection, channel/jam monitors
- **AI** — an on-device LLM chatbot with real hardware control over the radio

It's built to be the **Ultimate RedTeam gadget**: everything lives on the stick, no laptop required.

---

## :construction_men_walking: Mopie vs Bruce — what changed

| Area | Bruce | Mopie |
|---|---|---|
| **Identity / branding** | Shark logo, "Bruce" everywhere | Renamed, de-branded, no visible Bruce traces |
| **UI engine** | Classic menus | Custom **tactical grid UI** — apps on a paged grid, vector icons, corner-bracket chrome |
| **Themes** | Fixed dark | **11 instant themes** (red default, blue, matrix, cyan, violet, more), switch live |
| **Boot anim** | Static splash | Crimson radar-sweep + hex reveal + boot chime |
| **Main nav** | Deep menu nesting | Paged grid, single-tap select, fewer clicks to tools |
| **AI** | — (not present) | **Mopie Agent**: on-device chatbot + `/ai` web dashboard, 12 device-control commands |
| **BLE HID** | BadUSB scripts | **Phone Hijack** — pair-gated chaos with Android + iOS shortcut payloads |
| **iOS surface** | Apple spam legacy | Current Apple Continuity + **AirDrop spoof**, iOS HID chaos modes |
| **WiFi trolling** | Standard attacks | **Troll Toolbox** (SSID blast, kick-all, deauth+mock, spoof MAC) on top of the full attack suite |
| **IR** | TV-B-Gone (silent default) | TV-B-Gone **fixed to require a real IR LED pin** with on-screen wiring guide |
| **Target board** | Many boards | Focused + tuned for **M5StickC Plus 1.1** (520KB RAM, no PSRAM) |

Mopie adds tools and quality-of-life that Bruce doesn't ship, while carrying forward Bruce's proven WiFi / BLE attack primitives (deauther, sniffer, netcut, evil portal, Responder, karma, BLE spam suite).

---

## :toolbox: Tool map

- **WiFi**: deauth (all/channel/enhanced), beacon flood, evil portal, netcut, Responder, karma, sniffer, handshake capture, channel analyzer, jam detector, ARP scan, WireGuard, SSH/Telnet/SOCKS, Troll Toolbox, DetectiveMopie recon
- **BLE**: full spam suite (Apple/Samsung/Google/Windows + FastPair), **iOS AirDrop spoof**, Beacon Clone, RSSI scan, device finder, GATT dump, jammer, "Phone Hijack" HID chaos (Android + iOS)
- **IR**: TV-B-Gone (NA/EU/universal), IR jammer, capture/repeat
- **AI**: on-device chat, `/ai` web dashboard, model detection, device control (`wifi_scan`, `ble_scan`, `deauth`, `troll_beacon`, `led`, `beep`, `reboot`, telemetry…)
- **Loss/Other**: port scanner, subnet sweep, packet flooder, base64/hash/hex tools, GPIO/voltage monitors, panic wipe

---

## :floppy_disk: Build & flash

```bash
cd bruce
pio run --environment m5stack-cplus1_1            # build
pio run --target upload --environment m5stack-cplus1_1   # flash to /dev/ttyUSB0
```

If disk space is full, `rm -rf .pio/buildcache` regenerates on the next build.

---

## :warning: Honest limitations (read me)

- **iOS BLE popups**: Apple's iOS 16/17+ suppresses pairing/AirDrop spam. Mopie emits valid, current Apple Continuity + AirDrop beacons, but iOS may not show banner popups — that's the OS, not the firmware.
- **BLE HID (Phone Hijack)** requires pairing + enabling the keyboard on the target (both Android and iOS enforce this). No firmware bypasses that consent.
- **IR (TV-B-Gone)** needs an **external IR LED** wired to G26/G25/Grove. The M5StickC Plus has no on-board IR emitter; Mopie now prompts you to pick a real IR pin before transmitting.
- **M5StickC Plus 1.1** is a small-RAM chip (520KB, no PSRAM). The web dashboard runs light and heap-gated by design.

---

## :books: Legal

Mopie is for **authorized security testing, learning, and research only**. You are responsible for using it lawfully and only on systems you own or have explicit permission to test.