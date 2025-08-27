# 🍓 KEKOS C++ Edition - Raspberry Pi 4 Kernel

<div align="center">

![C++17](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![Architecture](https://img.shields.io/badge/arch-AArch64-orange.svg)
![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%204-ff69b4.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

</div>

> Ein Bare-Metal-Kernel für den **Raspberry Pi 4**, von Grund auf in modernem C++17 geschrieben. Dieses Projekt dient als Lernplattform für Low-Level-Systementwicklung auf der ARM-Architektur.

---

⚠️ Disclaimer
---
Dieses Projekt ist ein reines Lern- und Hobbyprojekt. Es dient ausschließlich dazu, die Grundlagen der Betriebssystementwicklung zu erforschen und zu verstehen. Es ist ausdrücklich nicht für den Einsatz in einer produktiven Umgebung gedacht. Die Verwendung erfolgt auf eigene Gefahr!
---

## 🚀 Features

* **🎯 Ziel-Architektur:** ARMv8-A (AArch64), speziell für den Raspberry Pi 4 (Cortex-A72).
* **💻 Sprache:** Modernes **C++17** in einer Freestanding-Umgebung (ohne Standardbibliothek, Exceptions oder RTTI).
* **👢 Boot-Prozess:** Erzeugt ein `kernel8.img`, das direkt vom Raspberry Pi 4 Bootloader geladen werden kann.
* **⌨️ Konsolen I/O:** Nutzt die **PL011 UART** (serielle Konsole) für die gesamte Textein- und -ausgabe.
* **⚡ Interrupts:** Implementiert ARM GICv2 (GIC-400) und ARMv8-A Exception Handling.
* **🕒 System-Timer:** Verwendet den ARM Generic Timer für periodische Interrupts.
* **🧠 Memory Management:**
    * Grundlegende MMU-Einrichtung mit Identity Mapping (2MB-Blöcke).
    * Eigener, simpler Bump-Allocator für `new` und `delete`.
* **📁 In-Memory Filesystem:** Ein einfaches, blockbasiertes Dateisystem im RAM.
* **🐚 Interaktive Shell:** Mit Befehlen wie `ls`, `cat`, `edit`, `echo`, `clear` und `help`.
* **📝 Text-Editor:** Ein einfacher Vollbild-Texteditor zum Bearbeiten von Dateien.

---

## 🏗️ Projektstruktur

```

.
├── 📂 arch/arm/         \# ARM-spezifischer Code (boot, core, peripherals)
├── 📂 include/          \# Globale Header (Kernel-Interfaces, C++ Support)
├── 📂 kernel/           \# Kern-Komponenten (main, console, fs, shell, editor)
├── 📂 lib/              \# Hilfsbibliotheken (kstd, printf)
├── 📂 toolchain/        \# Toolchain-Dateien (Linker-Script)
├── 📜 Makefile          \# Build-System
└── 📖 README.md         \# Diese Datei

````

---

## 🛠️ Voraussetzungen

* Eine **AArch64 Cross-Compiler Toolchain** (z.B. `aarch64-elf-gcc`).
* Das `make` Build-Tool.
* **QEMU** (`qemu-system-aarch64`) für die Emulation.
* Ein **Raspberry Pi 4** mit SD-Karte und einem **USB-zu-TTL Serial Adapter** für das Deployment.

---

## ⚙️ Bauen des Kernels

1.  **Repository klonen** (falls noch nicht geschehen).
2.  Ins Projektverzeichnis wechseln.
3.  **Kernel bauen:**
    ```bash
    make
    ```
    Dieser Befehl erstellt das `build/`-Verzeichnis mit dem Kernel-Image (`kernel8.img`), der ELF-Datei (`kernel8.elf`) und einer Disassembly-Liste.

4.  **Build-Dateien aufräumen:**
    ```bash
    make clean
    ```

---

## 🏃‍♂️ Ausführen mit QEMU

Der schnellste Weg, den Kernel zu testen, ist über QEMU.

1.  **Kernel bauen**, um `build/kernel8.img` zu erhalten.
2.  **QEMU starten:** Das Makefile bietet einen einfachen Befehl:
    ```bash
    make qemu
    ```
    Dadurch wird der Kernel in QEMU gestartet und die serielle Konsole mit deinem Terminal verbunden.

---

## 🐞 Debugging mit QEMU und GDB

Für eine detaillierte Fehlersuche kannst du QEMU mit einem GDB-Server starten.

1.  **Starte QEMU im Debug-Modus:**
    ```bash
    make qemu-debug
    ```
    Der Emulator wartet nun auf eine GDB-Verbindung auf Port `1234`.

2.  **Starte GDB** in einem zweiten Terminal und verbinde dich:
    ```gdb
    # Starte den AArch64 GDB
    aarch64-elf-gdb

    # In der GDB-Sitzung
    target remote localhost:1234
    symbol-file build/kernel8.elf

    # Setze Breakpoints und starte die Ausführung
    b kernel_main
    c
    ```

---

## 🍓 Deployment auf dem Raspberry Pi 4

1.  **SD-Karte vorbereiten:**
    * Stelle sicher, dass die aktuelle Raspberry Pi Bootloader-Firmware (`bootcode.bin`, `start4.elf` etc.) vorhanden ist.
    * Erstelle eine `config.txt` mit folgendem Inhalt:
        ```txt
        arm_64bit=1
        enable_uart=1
        kernel=kernel8.img
        ```

2.  **Kernel kopieren:** Kopiere die `build/kernel8.img` in das Hauptverzeichnis der SD-Karte.

3.  **Serielle Konsole verbinden:** Verbinde deinen USB-zu-TTL-Adapter mit den GPIO-Pins 14 (TX) und 15 (RX) sowie Ground. Öffne ein Terminalprogramm (z.B. `minicom`, `PuTTY`) mit den Einstellungen `115200 8N1`.

4.  **Pi starten:** Schließe den Strom an. Du solltest die Boot-Nachrichten deines Kernels in der seriellen Konsole sehen!

---

## 💡 Zukünftige Entwicklung

- [ ] Robusteres Speichermanagement (Buddy/Slab Allocator).
- [ ] User-Mode und Prozess-Management.
- [ ] Erweiterte Konsolen-Features (ANSI Escape Codes).
- [ ] Weitere Treiber (SD-Karte, USB, Netzwerk).
- [ ] Device Tree Blob (DTB) Parsing für flexible Hardware-Konfiguration.
- [ ] Besseres Editor-UI und mehr Features.

````
