<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# CLOCK Manual Workspace

This directory contains the maintained, publication-layout CLOCK end-user manual together with its reusable diagrams and deterministic UI screenshots. The chapter-oriented repository User Guide under `docs/user-guide/` is the canonical GitHub/Wiki-readable operating structure; the ODT is the editable source for the typeset publication manual. `docs/USER_GUIDE.md` remains the consolidated reference view.


## Maintained user manual source

The editable end-user manual is maintained as [`clock-user-manual.odt`](clock-user-manual.odt). The browser-readable operating guide is split under [`../user-guide/`](../user-guide/README.md) along the same 24-chapter structure as the publication manual. [`../USER_GUIDE.md`](../USER_GUIDE.md) remains the consolidated compatibility/reference view; user-visible facts must remain synchronized across the chapter set, consolidated guide, and ODT. Its manual-specific license is documented in [`LICENSE.md`](LICENSE.md). The ODT represents the maintained working source. Final-release preparation freezes a version-matched copy as `../manual/clock-user-manual.<firmware-version>.odt`. The sibling `docs/manual/` directory is reserved exclusively for manuals of final stable releases (`X.Y.Z`); Alpha, Beta and RC manuals are publication artifacts and are not retained in that archive.

The release artifact naming contract follows the Quantizer/Drift convention:

```text
clock-user-manual.<version>.odt
clock-user-manual.<version>.pdf
```

Before creating a release tag, run:

```bash
python scripts/prepare_release_manual.py
```

This stamps the current `CLOCK_FIRMWARE_VERSION` into firmware-bearing ODT body text (including the cover's stamp line `Firmware <version> · Digital master clock`) and firmware metadata, then writes the frozen source to `docs/manual/clock-user-manual.<version>.odt`. License names and license versions are independent content and are never rewritten by firmware-version stamping. The release workflow refuses to publish when that exact frozen source is missing or version-mismatched.

The release workflow converts the frozen ODT with LibreOffice and publishes **both ODT and PDF** as GitHub Release assets. The release PDF is checked for a non-zero page count, the expected firmware version, and Ubuntu-family font embedding. A separate `Manual publication smoke test` workflow exercises the same publication path for documentation changes.

The maintained source also carries an explicit layout contract. `Contents` is a dedicated page directly behind the cover, uses the normal framed content-page master, and keeps one chapter/page pair per row inside the publication text frame. The page values are synchronized from the **actual exported PDF** by `scripts/update_manual_contents.py`, and `build_user_manual.py` performs that synchronize-and-rebuild pass automatically for post-1.1 publication manuals. Chapter headings remain left-aligned while retaining semantic outline levels; intermediate headings use one consistent black/bold style without outline indentation. Direct screenshots and gallery thumbnails use explicit top/bottom spacing styles rather than inheriting zero-margin body/image paragraphs, every figure has a `Figure n.m` caption, and ordinary data tables use one header/body typography contract. `Source repository` begins on its own page.

The manual is set for home printing (see [`DOCUMENTATION_STYLE.md`](../DOCUMENTATION_STYLE.md)): no filled areas apart from QR codes and screenshots, black and grey only. The cover is typeset: the front panel as a line drawing ([`assets/front-panel-print.svg`](assets/front-panel-print.svg)), the South Signal Lab logo ([`assets/south-signal-lab-logo.svg`](assets/south-signal-lab-logo.svg), the website geometry in black) and the firmware-update QR (`assets/updates-qr.png`) pointing to the GitHub Releases page. Release stamping rewrites the cover's text line `Firmware <version> · Digital master clock`; manuals up to 1.1.0 keep their full-page cover images and are stamped in the image as before. The back page is the Colophon. Within the device documentation, the `LICENSES` / `CORE LIC` overflow is the canonical long-read-only-value example; the author name is not used as a visually dominant instructional example.

Before the Colophon, the release manual contains a dedicated **Licenses and source** section with the PolyForm firmware license notice, third-party runtime notices, and the canonical repository QR code from [`assets/repository-qr.png`](assets/repository-qr.png). The maintained working manual additionally carries the current **Support CLOCK** block with the canonical Ko-fi QR asset [`assets/kofi-support-qr.png`](assets/kofi-support-qr.png); frozen historical release manuals are not rewritten retroactively.

All three QR codes are black on white without a logo and are written by `scripts/generate_manual_qr_codes.py` (`--check` compares): updates `https://github.com/southsignallab/eurorack-clock-firmware/releases`, repository `https://github.com/southsignallab/eurorack-clock-firmware`, Ko-fi `https://ko-fi.com/southsignallab`. `scripts/check_user_manual.py` verifies that each asset encodes its address, that the embedded payloads exactly match the assets, that the frozen manuals' repository QR still encodes the repository address of their release, and that the embedded panel drawings match the generated files.

The generated GitHub Wiki also exposes the matching version-frozen ODT directly from its Home and **Manual & Downloads** pages. `scripts/build_wiki.py` refuses to generate a Wiki for a firmware version whose frozen ODT is missing.

Local helpers:

```bash
python scripts/prepare_release_manual.py
python scripts/build_user_manual.py
python scripts/update_manual_contents.py <odt> <rendered-pdf>  # normally invoked by build_user_manual.py
```

`prepare_release_manual.py` requires the Ubuntu font family when it stamps the full-page cover images of manuals up to 1.1.0. `--allow-font-substitution` exists only for local non-release previews.

The helpers need LibreOffice, the Poppler tools `pdfinfo`, `pdftotext` and `pdffonts`, and fontconfig's `fc-match`; a missing Poppler tool is named before the PDF export. On Windows `build_user_manual.py` also finds LibreOffice in its default install folder; see [`DEVELOPER_README.md`](../DEVELOPER_README.md#15b-windows-prerequisites-for-the-full-local-check-suite-and-the-manual-build) for the Windows setup.

`build_user_manual.py` exports the PDF without LibreOffice's image-resolution reduction, so the screenshots keep their nearest-neighbour pixels. With `--require-ubuntu-fonts` the PDF check fails if any embedded font is outside the Ubuntu family, for example a substitute that LibreOffice embedded for a glyph Ubuntu does not have.

## Asset policy

[`assets/`](assets/) contains both explanatory SVG diagrams and deterministic simulator screenshots.

The SVG files use explicit `viewBox` dimensions, no embedded fonts, no external links, and no manufacturing tolerances unless a drawing explicitly says otherwise. `front-panel-anatomy.svg` (user guide and wiki), `front-panel-anatomy-print.svg` (manual chapter 2) and `front-panel-print.svg` (manual cover) are generated by `scripts/generate_front_panel_illustration.py`: it places the KiCad-derived panel artwork `docs/assets/front-panel/clock-front-panel.svg` on the page and adds callouts at the coordinates in `sim/panel_layout.ini`. Both inputs come from `scripts/import_kicad_front_panel.py`, so manual, simulator and VCV panel share the KiCad front panel as their only source. OLED screenshots are generated from the production firmware renderer and real 128×64 framebuffer, then enlarged with integer-only nearest-neighbour scaling. They are not hand-drawn UI mockups. Every screenshot has two variants, both listed in `manual-screenshots.tsv` (`name`, `print name`, description): `<name>.png` keeps the module's light-on-dark display for the user guide, wiki and README; `<name>-print.png` is the manual's print variant, inverted (lit pixels black), enlarged 8× nearest-neighbour and framed by the display cut-out outline (size and corner radius from `sim/panel_layout.ini` and the KiCad-derived panel). The size and position of the active OLED area inside the cut-out are an assumption (21.8 × 10.9 mm, centred) until the display module's drawing gives them.

The boot logo is no longer reconstructed from Bézier/line primitives at runtime. `clock-logo.svg` is the canonical artwork; it is rasterized once to the checked-in 1-bit `clock-logo-1bit.png`, and the firmware uses the corresponding packed bitmap data in `src/ui/clock_logo_bitmap.h`. This guarantees that the OLED logo is the exact approved raster rather than a second interpretation of the vector geometry.

| Asset | Intended manual use |
| --- | --- |
| [`front-panel-anatomy.svg`](assets/front-panel-anatomy.svg) | numbered front-panel key over the KiCad-derived panel artwork; identify display, DATA/ENTER encoder, START/PAUSE, TAP TEMPO/SHIFT, STOP/BACK, IN 1/IN 2, outputs, and LEDs |
| [`front-panel-anatomy-print.svg`](assets/front-panel-anatomy-print.svg) | black-on-white line drawing of the panel with numbered callouts; chapter 2 of the printed manual |
| [`front-panel-print.svg`](assets/front-panel-print.svg) | the same line drawing without callouts; manual cover |
| [`south-signal-lab-logo.svg`](assets/south-signal-lab-logo.svg) | South Signal Lab logo, website geometry in black; manual cover and Colophon |
| [`updates-qr.png`](assets/updates-qr.png) | black QR code for the firmware releases page, printed on the cover |
| [`kofi-support-qr.png`](assets/kofi-support-qr.png) | black QR code for `ko-fi.com/southsignallab`, printed in Support CLOCK |
| [`operating-modes.svg`](assets/operating-modes.svg) | explain Independent, One Clock, and Divider Bank |
| [`timing-swing-phase.svg`](assets/timing-swing-phase.svg) | explain ideal grid, Swing, phase, and One Clock Humanize |
| [`external-sync-flow.svg`](assets/external-sync-flow.svg) | separate the implemented GPIO EXTI capture path from pending analog input HIL |
| [`persistence-ab.svg`](assets/persistence-ab.svg) | explain the two-slot internal-Flash commit strategy |
| [`simulator-scope.svg`](assets/simulator-scope.svg) | explain the developer oscilloscope and musical ruler |
| [`clock-logo.svg`](assets/clock-logo.svg) | canonical CLOCK vector wordmark used as the logo source |
| [`clock-logo-1bit.png`](assets/clock-logo-1bit.png) | exact 128×41 monochrome raster used to derive the firmware boot-logo bitmap |
| [`clock-logo.png`](assets/clock-logo.png) | transparent raster export of the CLOCK logo for documents that need PNG assets |
| [`manual-screenshots.tsv`](assets/manual-screenshots.tsv) | generated screenshot catalog: screen variant, print variant and human-readable state description |
| [`boot-1000-1x.png`](assets/boot-1000-1x.png) | exact 1:1 pixel export of the fully rendered OLED boot screen |
| [`repository-qr.png`](assets/repository-qr.png) | canonical QR code for the firmware source repository, embedded in the Licenses and source section |

> [!NOTE]
> These are explanatory illustrations. `front-panel-anatomy.svg` is **not** a drill template, PCB drawing, or dimensional manufacturing source. Final mechanical drawings belong with the hardware CAD/KiCad deliverables.


## Authoring pipeline direction

The publication manual should not become a hand-maintained duplicate of the Markdown documentation. The intended long-term workflow is a **hybrid Markdown -> controlled ODT pipeline**:

1. user-facing prose and structured facts live in Markdown;
2. deterministic simulator screenshots and SVG diagrams remain source-controlled assets;
3. a fixed ODT reference/template owns page styles, typography, cover/back cover, callouts, figure widths, galleries, and page-break rules;
4. a generator/postprocessor maps the Markdown structure into those named ODT styles rather than accepting Pandoc defaults;
5. CI builds the PDF and performs the existing structural/font checks plus rendered-page regression checks for layout-sensitive pages.

Plain `pandoc Markdown -> ODT` is **not** considered sufficient for release publication because it cannot by itself guarantee the current cover, fixed figure sizing, gallery layout, page-break behavior, and typography. The existing ODT therefore remains the release source until a generated candidate reproduces the approved layout closely enough to pass visual regression.

This direction is meant to reduce duplicate editing without sacrificing the publication design. It also keeps the Markdown User Guide useful on GitHub even when LibreOffice is unavailable.

## Regenerating all OLED screenshots

Run the VS Code task **`CLOCK: Generate manual screenshots`**, or execute:

```bash
python scripts/generate_manual_screenshots.py
python scripts/refresh_manual_screenshots.py
```

The first command configures the SDL-free simulator preset, builds `clock-manual-screenshot-generator`, renders the complete curated firmware-state catalog, converts every native 128×64 framebuffer to a 4× grayscale PNG with nearest-neighbour scaling, removes only stale files recorded by the previous generated manifest, and writes the result directly beside the SVG assets in `docs/manual-source/assets/`.

The catalog currently covers:

- boot at 0/25/50/75/100 percent and power-off;
- PLAY, PAUSE, STOP, Off, Clock, Euclid, Sequencer, One Clock, and Divider Bank;
- Internal, External and Auto source states including locked/unlocked external timing;
- channel/global overviews, all six current positions of the horizontal mode carousel, mode confirmation, and Sequencer editor;
- every Settings page plus an active edit state and the full-value read-only Info popover;
- templates, preset load/save/name/overwrite flows;
- Custom Groove editor and live TAP-record views, including the engine-phase playhead;
- Clock, Plug, Heartbeat, Acid, Spectrum, Field, Blox, Matrix, Cube Cover, Fractal, Orbit, Make Music, Labyrinth, Starfield, and Fireworks screensavers;
- individual retro intros for Pixel Raid, Formula 1, Breakout, Egg Journey, and BEATKNECHT; shared initials-entry/Top-100 arcade screens; Pixel Raid, Formula 1 (including curve/crash states), Breakout (including modifier state), Egg Journey (action, broken-shell, and flattened-egg states), and BEATKNECHT gameplay.

The catalog is intentionally finite and representative. It covers every distinct user-visible operating/UI state rather than attempting the meaningless Cartesian product of every parameter value.

The shipped/default boot Easter egg is **BEATKNECHT** (`CLOCK_EASTER_EGG=5`). START starts it, START/PAUSE controls transport, STOP resets to step 1 and disables the output stage, TAP TEMPO changes style, and a long encoder push opens a guarded `EXIT GAME? / NO / YES` confirmation. Egg Journey is named consistently in both user-facing material and its `egg_journey_*` firmware sources.

The ODT publication manual contains generated **two-across** mini-galleries for Screensavers and Easter eggs, with one short caption below each individual screenshot. All OLED frames in the manual are refreshed by `scripts/refresh_manual_screenshots.py`; the tool replaces the fixed direct screenshot frames and rebuilds both galleries while preserving the approved page geometry. The release workflow always regenerates the framebuffer catalog from the tagged firmware source and refreshes the frozen release ODT before PDF conversion, so a release can never silently ship stale UI screenshots.

## Manual structure

The maintained manual follows this user-oriented publication structure:

1. Start here
2. Panel, power and boot
3. Two-minute quick start
4. The control language
5. Read the Performance screen
6. Select channels and change functions
7. Transport and tempo
8. Independent channels
9. Clock mode
10. Euclid mode
11. Sequencer mode
12. One Clock
13. Grooves and Custom Groove Record
14. Divider Bank
15. Settings map
16. Configurable external inputs
17. Presets, autosave and templates
18. Screensaver and display protection
19. Gate outputs and LEDs
20. Hidden boot features
21. Practical recipes
22. Troubleshooting
23. Firmware installation and updates
24. Technical status and specifications


The editable manual follows development continuously. A release-frozen manual must not be treated as final hardware documentation until External Sync comparator/Input-Capture HIL, SPI timing HIL, and the final scheduler implementation are validated.



## Release-version contract

The editable `clock-user-manual.odt` may follow ongoing development without a frozen copy for every intermediate source revision. For Alpha, Beta and RC publication the release workflow creates the versioned ODT transiently from the maintained source. A final stable release requires the committed `docs/manual/clock-user-manual.<version>.odt`, validates that ODT, converts it to PDF, validates the PDF, and publishes both artifacts. `docs/manual/` contains no working copy, artwork, README, prerelease snapshots, or transient files: only stable `X.Y.Z` ODT files with matching curated release records under `docs/releases/<version>/`.
