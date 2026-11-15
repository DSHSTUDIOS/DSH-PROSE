# DSH-Prose · Rev D · 7.5-inch touch

A pocket e-paper reader and sketchpad. The carrier board has a 170.2 × 111.2 mm main body and extends up to 11.2 mm past the display at the three front-button positions. It is 1.0 mm thick, with two copper layers and four 2.2 mm mounting holes. The ESP32-S3-WROOM-1-N8R2 RF module mounts directly to the board and has 8 MB flash, 2 MB quad PSRAM, and an on-module antenna that overhangs the right edge. Keep the display backing, battery, fasteners and metal enclosure features clear of the antenna area. The front silkscreen reads “made by pratik dash” and “happy birthday dwig <3)”. [Espressif module pinout and layout guidance](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)

Rev D targets the **Good Display GDEY075T7-T01**, a 7.5-inch, 800 × 480 monochrome e-paper panel with bonded five-point capacitive touch. The UC8179 display uses four-wire SPI and the GT911 uses I²C. The panel body is **170.2 × 111.2 × 2.13 mm**. Its active area is **163.2 × 97.92 mm**. The assembly is sized around that panel, with the carrier mounted behind it and three button extensions below it. [Good Display product specification](https://www.good-display.com/product/483.html) · [Good Display outline and touch-flex drawing](https://www.laskakit.cz/user/related_files/gdey075t7-t01.pdf)

Good Display quotes approximately 0.3 s for partial refresh, 1.5 s for fast refresh, and 3 s for full refresh. Capture touch points continuously and draw accumulated strokes in partial updates. Use a finger or a capacitive stylus. GT911 touch provides position without pen pressure. One packed 1-bit framebuffer uses 48,000 bytes, so two use 96,000 bytes. [Good Display product specification](https://www.good-display.com/product/483.html)

A reseller lists panel and kit variants. Good Display’s storefront and LaskaKit showed the panel out of stock when checked. Confirm availability and whether the seller includes an adapter before ordering. [ESP32s listing](https://esp32s.com/product/7-5-inch-e-ink-screen-module-4-grayscale-800x480-e-paper-raspberry-pi-touch-display/) · [Good Display storefront](https://www.buy-lcd.com/products/gdey075t7-t01) · [LaskaKit listing](https://www.laskakit.cz/en/good-display-gdey075t7-t01-7-5--800x480-epaper-displej-grayscale-dotykovy/)

## KiCad files

The schematic and routed PCB use KiCad 9 format and were checked with KiCad 9.0.8. The packaged ERC and DRC reports show zero violations, zero disconnected PCB pads and zero PCB-to-schematic mismatches. J3 and the ESP32 pin assignments carry over from Rev A. R13/R14 are 4.7 kΩ for the touch harness. There are no per-item rule exclusions. Rev D preserves all 198 pin connections from Rev C. SW4, SW5 and SW6 are top-actuated XUNPU TS-1088-AR02016 switches on the screen-facing PCB side (B.Cu). BOOT and RESET use the edge-actuated Alps SKRTLAE010. Assembly requires placing SW4–SW6 on the second side.

Keep both library tables, the symbol library and the `DSH_Prose.pretty` directory beside the project. Keep the `3dmodels` directory beside the board. The PCB 3D view uses the supplied USB-C receptacle, MSK12C02 switch and TS3USB221A models, a dimensioned JST-PH housing model based on the JST drawing, and standard KiCad models for the other components. The USB-C footprint uses a VRML conversion of the supplied STL. The source STL is included, and a simplified STEP with matching envelope dimensions is available for mechanical exports. [JST PH series drawing](https://s3.amazonaws.com/snapeda/datasheet/B2B-PH-SM4-TB_JST.pdf)

The layout targets two-layer 1 oz FR-4, with mostly 0.20 mm signal traces and smaller neck-downs. The DRC minima are 0.10 mm trace width/spacing, 0.25 mm copper-to-edge clearance and 0.20 mm finished drill for the module's exposed-pad vias. Confirm the assembler's process for those vias and the USB-C anchor slots. USB is full-speed (12 Mbit/s); USB impedance and signal integrity still need validation on an assembled board with the selected stackup. [JLCPCB PCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities) · [Espressif USB layout guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html)

## Power and interfaces

```text
USB-C J1
  CC1, CC2 --> 5.1k each to GND
  VBUS -----> MCP73831 (U2) --> protected 1S LiPo (J2)
     |             |            |
     |             +--> LED     +--> P-MOSFET Q1 --> AP2112K --> +3V3
     |                                         SW1 controls       |
     +--> VBUS sense --> USB switch /OE        Q1 and charger    +-- U1 ESP32-S3
  D+/D- --> ESD --> USB switch --> 22R each --> GPIO20/19         +-- J3 touch EPD
                                                                  +-- J4 microSD

 U1 SPI2/FSPI ---- MOSI/SCK/CS + DC/RST/BUSY ----> J3 display controller
 U1 I2C ---------- SDA/SCL + INT/RESET ---------> J3 touch controller
 U1 SDMMC slot 1 - CLK/CMD/DAT0 ----------------> J4 microSD
 U1 GPIOs -------- Prev/Clear, Mode, Next/Save and card detect
```

- **RUN** connects the battery through Q1 to the regulator. USB data is available with a charged battery and a connected USB host. SW1 opens the charger PROG return in this position.
- **CHARGE** disconnects the battery from the regulator and grounds the 10 kΩ PROG return. The MCP73831 charges at about 100 mA nominal from USB. The LED indicates that the charger is actively charging. SW1 only switches the MOSFET gate and PROG return. Select a protected 4.20 V single-cell pack whose manufacturer permits at least 120 mA charge and 600 mA pulse discharge. Check the pack's JST-PH polarity against J2 pin 1 (positive). Follow the pack's temperature range. The board has no cell thermistor input. [Microchip MCP73831 datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP73831-Family-Data-Sheet-DS20001984H.pdf)
- The AP2112K is rated 600 mA. At that load, allow up to 400 mV dropout plus the MOSFET and battery voltage drop. Allow about 3.75 V on VBAT under load as a conservative starting point for full 3.3 V regulation at peaks. Expect reduced usable battery capacity at high loads. Measure rail sag and temperature with the selected module, SD card and battery. Start bring-up with Wi-Fi disabled and keep SD writes separate from display refresh until the measured load budget permits overlap. [Diodes Inc. AP2112 datasheet](https://www.diodes.com/datasheet/download/AP2112.pdf)
- The native USB path includes a VBUS-controlled USB switch. VBUS absent disables the switch. Its powered-off isolation limits current into the USB path when the battery rail is off. The mode switch intentionally provides neither USB-only operation nor charge-while-playing. To enter the ROM downloader, use RUN and USB, hold BOOT, tap RESET, then release BOOT. [TI TS3USB221A datasheet](https://www.ti.com/lit/ds/symlink/ts3usb221a.pdf)

## ESP32-S3 pin-to-pin netlist

Pad numbers below are the physical WROOM-1 module pads. KiCad's full board-level interchange netlist is in `verification/DSH-Prose.net.xml`.

| U1 pad | Module pin | Net | Other endpoint or use |
|---:|---|---|---|
| 1, 40, 41 | GND, GND, exposed pad | GND | Main ground and antenna reference |
| 2 | 3V3 | +3V3 | U3.5 and local 10 µF + 100 nF |
| 3 | EN | CHIP_EN | R6 10 kΩ to 3.3 V, C6 1 µF to GND, SW3 to GND |
| 4 | GPIO4 | SD_DAT0 | J4.7, R16 pull-up |
| 5 | GPIO5 | SD_CMD | J4.3, R15 pull-up |
| 6 | GPIO6 | SD_CLK | J4.5 |
| 7 | GPIO7 | SD_DETECT_N | J4.9 switch, R20 pull-up |
| 8 | GPIO15 | TOUCH_SDA | J3.9, R13 pull-up |
| 9 | GPIO16 | TOUCH_SCL | J3.10, R14 pull-up |
| 10 | GPIO17 | TOUCH_INT | J3.11 |
| 11 | GPIO18 | TOUCH_RST_N | J3.12 |
| 12 | GPIO8 | NC | Spare |
| 13 | GPIO19 | MCU_USB_DM | R10 → U4.2, U4.7 → J1 A7 and B7 |
| 14 | GPIO20 | MCU_USB_DP | R9 → U4.1, U4.8 → J1 A6 and B6 |
| 15 | GPIO3 | NC | Strapping pin left open |
| 16 | GPIO46 | NC | Strapping pin left open |
| 17 | GPIO9 | NC | Spare |
| 18 | GPIO10 | EPD_CS_N | J3.5, R11 pull-up |
| 19 | GPIO11 | EPD_MOSI | J3.3 |
| 20 | GPIO12 | EPD_SCK | J3.4 |
| 21 | GPIO13 | EPD_DC | J3.6 |
| 22 | GPIO14 | EPD_RST_N | J3.7, R12 pull-up |
| 23 | GPIO21 | EPD_BUSY | J3.8 |
| 24 | GPIO47 | NC | Spare |
| 25 | GPIO48 | NC | Spare |
| 26 | GPIO45 | NC | Strapping pin left open |
| 27 | GPIO0 | BOOT_N | R7 10 kΩ to 3.3 V, SW2 to GND |
| 28, 29, 30 | GPIO35, GPIO36, GPIO37 | NC | Reserved for module variant compatibility |
| 31–34 | GPIO38, GPIO39, GPIO40, GPIO41 | NC | Spare |
| 35 | GPIO42 | BTN_NEXT_N | SW6 to GND |
| 36, 37 | GPIO44, GPIO43 | NC | UART0 pins unused |
| 38 | GPIO2 | BTN_MODE_N | SW5 to GND |
| 39 | GPIO1 | BTN_PREV_N | SW4 to GND |

Set the three user buttons to active-low with internal pull-ups. Use SPI2/FSPI for the display and SDMMC slot 1 in 1-bit mode via the ESP32-S3 GPIO matrix. J4 DAT1/2/3 each have a 10 kΩ pull-up and are independent of the display bus. The card-detect switch closes to ground when the card is inserted. Use low-speed SD initialization and validate the operating frequency on the prototype. [Espressif hardware checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html) · [Espressif SD pull-up requirements](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/sd_pullup_requirements.html) · [Hirose card socket](https://www.hirose.com/en/product/p/CL0609-0031-0-00)

## Display stack and 12-pin harness

Use a **DESPI-C02** 3.3 V boost adapter for the 24-pin e-paper flex. Set its RESE switch to **0.47 Ω** for the UC8179. Use the **LaskaKit e-Paper Touch adapter** with its dedicated GDEY075T7-T01 socket so all six touch conductors are accessible, including INT and RESET. The touch-tail pin numbers below are from Good Display's engineering drawing. Confirm the contact side and continuity at the actual flex before applying power. Use JST GH GHR-12V-S at the carrier end. Run shared 3.3 V and ground to both branches. [DESPI-C02 manual](https://www.laskakit.cz/user/related_files/despi-c02_connector_board_for_e-paper_display_v1-1.pdf) · [Good Display RESE selection table](https://v4.cecdn.yun300.cn/100001_1909185148/Evaluation%20Kit%20for%20E-Paper%20Display%20DEArduino-L%28C02%29.pdf) · [JST GH drawing](https://www.jst-mfg.com/product/pdf/eng/eGH.pdf)

| Carrier J3 | Signal | DESPI-C02 P2 | GT911 touch flex |
|---:|---|---|---|
| 1 | 3.3 V | 1 | 2 VDD |
| 2 | GND | 2 | 1 GND |
| 3 | EPD MOSI | 3 SDI | — |
| 4 | EPD SCK | 4 SCLK | — |
| 5 | EPD CS | 5 CS | — |
| 6 | EPD D/C | 6 D/C | — |
| 7 | EPD RESET | 7 RES | — |
| 8 | EPD BUSY | 8 BUSY | — |
| 9 | Touch SDA | — | 5 SDA |
| 10 | Touch SCL | — | 6 SCL |
| 11 | Touch interrupt | — | 4 INT |
| 12 | Touch RESET | — | 3 RST |

The vendor's sales-page touch table and engineering drawing disagree. Use the engineering drawing's order, **GND, VDD, RST, INT, SDA, SCL**, when counting the actual touch flex contacts 1 to 6. FPC connector numbers can run in the opposite direction from the flex. With the LaskaKit Touch adapter's dedicated GDEY075T7-T01 socket, FPC socket contacts 1–6 correspond to flex contacts 6–1. Wire J3 supply and I²C to the adapter's labeled contacts, INT to JP1.5 and RESET to the RST side of C6 (the side connected to R3 and the flex socket). A four-wire Qwiic cable alone cannot carry INT and RESET. If using this adapter, open its I2C_PULLUP jumper so the carrier's pull-ups set the bus. [Manufacturer panel drawing](https://www.laskakit.cz/user/related_files/gdey075t7-t01.pdf) · [LaskaKit touch adapter schematic](https://raw.githubusercontent.com/LaskaKit/ESPink/main/HW/touch-adapter/E-paper-touch-adapter.pdf) · [DESPI-C02 schematic](https://www.laskakit.cz/user/related_files/despi-c02_sch_v1-0.pdf)

For firmware bring-up, start SPI at a low clock rate in mode 0 and wait while UC8179 BUSY is low. Start I²C at 100 kHz. Use GPIO18 for GT911 RESET and briefly drive GPIO17 as an output during address selection. Hold INT low across reset to select the 7-bit address 0x5D, complete the controller's reset timing, then make GPIO17 a floating input. Rotate touch coordinates together with the display, and check all four corners. [Good Display GDEY075T7 datasheet](https://p.globalsources.com/IMAGES/PDT/SPEC/751/K1200241751.pdf) · [Goodix GT911 programming guide](https://www.topwaydisplay.com/sites/default/files/2025-05/GT911.pdf)

## Bare-panel adapter reference

For an integrated display adapter, follow the GDEY075T7 reference circuit for its 24-pin e-paper tail and the GDEY075T7-T01 drawing for the separate 6-pin GT911 tail. Copy the documented GDR/RESE boost loop and its high-voltage bypass network. The custom adapter must follow the panel-specific capacitor and NC assignments, which differ from the generic DESPI-C02 connector labels. Keep the high-voltage rails local to the adapter. Tie BS1 low for four-wire SPI and keep the SPI and I²C harness branches short. Confirm both FPC sockets' mating contact orientation against the drawing. [Panel datasheet and reference circuit](https://p.globalsources.com/IMAGES/PDT/SPEC/751/K1200241751.pdf) · [Manufacturer outline](https://www.laskakit.cz/user/related_files/gdey075t7-t01.pdf)

## Enclosure dimensions

The portrait enclosure measures **120.4 × 202 × 18.8 mm** before adding the optional stylus clips. The front button row sits below the display and reads Prev/Clear, Mode, Next/Save from left to right. The left edge has a widened Run/Charge slider opening and tool-access holes for BOOT and RESET. USB-C and microSD are on the right edge. The blue back has the recessed logo and four corner screws. See [enclosure and assembly files](../cad/) for the curved back and side stylus clips.

Print one frame, one back and three copies of [`03-Front-Keycap.stl`](../cad/03-Front-Keycap.stl). Set the frame face down and drop the matching caps into their openings from inside. Their flanges keep them in the frame. Fit the screen into the front ledge with thin perimeter tape. Screw the PCB to the back's standoffs, connect the flexes and battery, and close the back with four M2.5 countersunk screws suited to the printed pilot holes. The back has internal stops that support the button extensions. Check cap movement, switch click, connector access, fastener length, battery and flex fit on a prototype. [Display outline](https://www.laskakit.cz/user/related_files/gdey075t7-t01.pdf)

The separate `3dmodels/GDEY075T7-T01-envelope.step` gives the nominal panel body and active area for a CAD mockup. Flex tails, stiffeners, adapter assemblies, cable bends, and the battery pack require the chosen hardware or the corresponding mechanical drawings.

## ERC in KiCad

1. Open `DSH-Prose.kicad_pro` and launch Schematic Editor. Keep `DSH_PROSE.kicad_sym` and `sym-lib-table` in the same folder.
2. Check each block's nets against this document, especially the two CC resistors and the polarity of GPIO19 D− / GPIO20 D+. Check D+ and D− end to end through R9/R10, U4 and both USB-C contact rows.
3. Verify U1 EN and GPIO0 each have a 10 kΩ pull-up, EN has the 1 µF capacitor, the buttons close to GND, and GPIO3/GPIO45/GPIO46 have no-connect flags.
4. Keep the power flags on external VBUS, GND and SYS_BAT. VBAT and 3.3 V already have a charger and regulator power-output pin on their respective nets. Leave explicit no-connects on unused module pins, the USB SBU pins, the unused USB switch channel and the regulator NC.
5. Select **Inspect → Electrical Rules Checker → Run ERC**. Inspect errors and warnings with the default rule checks. To verify the supplied project from the command line, run `kicad-cli sch erc --severity-all --exit-code-violations DSH-Prose.kicad_sch`. See `verification/ERC.rpt` for the recorded result.
6. If the schematic changes, use **Tools → Update PCB from Schematic**. Run PCB DRC with schematic parity enabled. Check the antenna keepout, the USB signal route, enclosure access to the controls and the display harness length before ordering.

## First article checks

Verify USB VBUS and ground continuity, then measure the battery and rail with the switch both ways. In CHARGE, check the charge current, LED and input isolation. In RUN, verify that the charge current stops. Scope 3.3 V during cold boot, EPD refresh and a sustained microSD write at high and low battery voltage. Test USB enumeration with both orientations of a known data cable. Check the SD detect switch, all five tactile buttons, EPD BUSY behavior and I²C touch coordinates before fixing the case dimensions. Confirm the switch detents agree with the RUN and CHARGE markings and that the selected USB-C plug and microSD card can be fully inserted through the case. Verify the antenna clearance with both the actual screen and the battery in place. The board has not been assembled or bench-tested.
