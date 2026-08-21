# DSH-PROSE

An open-source, low-power desktop dashboard hardware system featuring a custom ESP32-S3 driver board and a 7.5" monochrome ePaper display, housed inside a 3D-printed slotted track frame enclosure.

![Project Banner](assets/banner.png)

## Overview

DSH-PROSE combines a custom PCB layout, a high-contrast 7.5" ePaper screen, and an optimized 3D-printable frame into a clean, low-profile desktop device. Built to operate efficiently with low power overhead, it displays real-time updates while keeping ports, controls, and ribbon cabling clean and strain-free.

---

## Hardware Specifications

### Mainboard & Display
* **Microcontroller:** ESP32-S3 (Wi-Fi / Bluetooth LE, onboard storage, GPIO layout)
* **Display Panel:** Seeed Studio 7.5" Monochrome ePaper Display
  * **Resolution:** 800 × 480 pixels
  * **Interface:** SPI via 24-Pin FPC connection
  * **Panel Dimensions:** 170.2mm × 111.2mm × 1.2mm
* **Power & Connectivity:** USB-C power/flashing, onboard power management, and break-out headers.

### Enclosure Design
* **Form Factor:** 3D-printed hollow perimeter frame with integrated stacking tracks.
* **Cable Routing:** Strain-relief notch for the 24-pin FPC ribbon cable.
* **Assembly Method:** Slide-in track layout (supports mid-print `M600` pause-and-embed manufacturing).

---

## Repository Structure

```text
├── assets/         # Project banners, photos, and UI diagrams
├── cad/            # STL files, Tinkercad exports, and 3D frame models
├── firmware/       # ESP32-S3 source code, display drivers, and Wi-Fi modules
└── hardware/       # KiCad schematics, PCB Gerber files, and BOM
