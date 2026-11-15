# DSH-Prose UI

This portable C++11 UI targets the Rev D portrait display at 480 × 800. It provides Boot, Library, Reader, Scribe, Settings, page templates, and page renaming screens. The boot splash and app header use the DSH logo traced for the case. The 1-bit logo bitmap is in `dsh_logo.hpp`.

## Firmware integration

- Supply the display callbacks for rectangles, lines, text, symbols and the note canvas. Map local display coordinates to the panel's portrait rotation. Text positions are baselines. For the logo symbol, read each row of the 100 × 100 MSB-first bitmap using its 13-byte stride.
- Supply the featured book and notebook labels, the active notebook title, paginated book content, page turning, ink callbacks, and filtered library entries through `Services`. In All, the first two items back the featured book and notebook covers. In each filtered list, the first item backs its cover. `openEntry` loads the selected document before the UI changes screens. Provide non-null text pointers for the duration of a draw. The UI draws up to 13 pre-wrapped lines per reader page. Filesystem browsing, pagination, SD storage and rasterizing strokes belong to the application adapter.
- Use the front switches on GPIO 1, GPIO 2 and GPIO 42 for Prev, Mode and Next. Send debounced press events with the hold state. Mode switches between reading and writing. Holding Mode returns to the library. In Scribe, Prev undoes and holding Prev opens the clear confirmation. Next saves.
- Send portrait touch coordinates as down (0), move (1) and up (2), and cancel (3). Pass the last known X/Y with up if the controller reports release without coordinates. Library scrolling and Reader swipes commit once on release. Taps select on release. The application adapter can apply palm filtering and arbitrate touch and Bluetooth pen input before dispatching. Provide the ink layer separately so display refreshes can batch updates.
- Call `saveResult` after a save completes. The application owns the canvas and should keep it if an SD write fails. Call `finishBoot()` when the startup timer expires.

This directory contains the UI and its logo asset. The Rev D display, touch, Bluetooth and SD drivers still need to be connected and tested on hardware.


## Appearance and reading fonts

Open Settings from the menu icon in the Library, Reader or Scribe header. The font setting changes Reader body text. Keep the UI labels in their existing fonts.

- Implement `Display::palette` so `true` drawing colors and lines are foreground and `false` colors are background. Invert those physical colors for dark mode, including the ink and logos, while retaining the original note data.
- Map the Reader font choices to serif, sans serif, monospace and OpenDyslexic faces. The OpenDyslexic Latin Regular source is in `fonts/` with its SIL OFL license, from Fontsource 5.3.0. Convert it to the device renderer's bitmap or glyph format as needed.
- The `page` callback receives the reading preferences. Wrap to the chosen font at `readingSize(preferences)` in the 410-pixel text column. Paginate at `lineStep(preferences)` from baseline 253 up to (excluding) 689. Preserve the reading position as a text offset when reflowing.
- Load presentation preferences at boot with `restorePreferences`, and persist later changes through `Services::savePreferences` (for example, in ESP32 NVS).

## Battery indicator

The header badge and Settings footer read the same cached percentage from `Services::batteryPercent`. Return `-1` when there is no valid measurement. Rev D routes VBAT to the power circuit and charger STAT to the indicator LED. Neither reaches the ESP32-S3, so the shipping Rev D adapter should report unavailable. A later board needs a battery sense circuit or a fuel-gauge IC to provide a real reading.


## Resume, notebooks, and book notes

- On boot, reopen the last document and load its durable position before calling `restoreSession`. Call `finishBoot` after the logo timer. The `screenChanged` hook lets the adapter remember Reader, Scribe, or Library. Settings, paper selection, and renaming are temporary views.
- `InkAction::Checkpoint` fires after a completed stroke, clear, or undo. The adapter should checkpoint the current document and page, and retain in-memory ink on a failed write. The dedicated Save button still requests a save explicitly. A hard power-off can lose a stroke that has not reached a checkpoint. The ESP32 SD adapter will need recoverable writes and a bench power-cycle test.
- The active notebook adapter owns separate named pages, their strokes and their paper templates. `NotebookAction` handles page creation, switching and template selection. `renamePage` receives a null-terminated name up to 24 ASCII characters.
- Use stable document IDs and source-text offsets for reading positions and bookmarks. `Page::paragraphAnchors` maps rendered lines back to paragraph start offsets. Supply -1 for separators. `Page::annotated` gives per-line note markers. Opening a paragraph loads that document's anchored ink in Scribe; an anchor of -1 closes it. Keep bookmarks and paragraph-note anchors independent of pagination.
