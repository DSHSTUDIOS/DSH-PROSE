# 7.5" ePaper Frame Enclosure

A lightweight, 3D-printable slotted frame enclosure designed specifically for a 7.5" monochrome ePaper display and custom PCB stack. Built to bypass complex mesh structures with a simple, high-strength layered track layout.

![Project Banner](link-to-your-banner-art.png)

## Features

* **Custom-Fit Profile:** Engineered to house a 7.5" ePaper panel (170.2mm × 111.2mm × 1.2mm).
* **Slotted Track System:** Allows seamless stacking of the display panel and driver PCB.
* **Open Channel Access:** Keeps side ports and the 24-pin FPC ribbon cable accessible without pinching.
* **Low Filament & Quick Print:** Designed as a hollow perimeter frame for fast print times and minimal material usage.

## Hardware & Dimensions

* **Display:** Seeed Studio 7.5" Monochrome ePaper Display (800×480 resolution)
* **Frame Outer Dimensions:** [Insert your length] mm × [Insert your width] mm × [Insert your height] mm
* **Material:** Printable in PLA or PETG (0.2mm layer height recommended)

## Print Settings & Assembly

1. **Slicing:** Import `frame.stl` into your slicer of choice (PrusaSlicer, Bambu Studio, Cura).
2. **Infill:** 15–20% Gyroid or Grid infill.
3. **Supports:** None required if oriented flat on the print bed.
4. **Embedded Hardware Option:** Add a print pause (`M600`) at the top lip layer height to drop the screen into the frame slot mid-print for a permanently trapped fit.

## Repository Contents

* `/cad` - STL files and source 3D models
* `/src` - Display driver code and pinout configurations
* `/schematics` - PCB layout and hardware schematics
* `/assets` - Graphics and demo media

---

*Built for shipping.*
