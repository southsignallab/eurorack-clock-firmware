<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# South Signal Lab CLOCK Documentation Style

This file defines the repository documentation conventions. It exists to keep the User Guide, developer documentation, maintained typeset manual, code comments, and GitHub landing page consistent as the project grows.

## Language and tone

- Canonical repository prose is **US English**.
- Prefer direct technical language over marketing language.
- Explain the behavior a user or developer can rely on; separate implemented behavior from planned work.
- Introduce acronyms at first use unless they are universally obvious in context (`BPM`, `GPIO`, `ISR`).
- Use `One Clock`, `Divider Bank`, `Euclid`, and `Sequencer` as user-facing names; internal C++ identifiers may differ.

## GitHub Markdown

Use GitHub-native features when they improve comprehension:

- tables for reference data;
- fenced code blocks with language tags;
- `[!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]`, and `[!CAUTION]` admonitions;
- Mermaid for software architecture, sequence, state, and dependency diagrams;
- `<details>` only for genuinely optional long material;
- relative repository links so documentation works on branches and forks.

Do not hide required safety or qualification information inside collapsible blocks.

## Images

- Production OLED screenshots must come from the real framebuffer/renderer.
- Pixel UI screenshots are enlarged with nearest-neighbor scaling.
- Reusable explanatory diagrams are plain SVG under `docs/manual-source/assets/`.
- SVGs must not embed fonts, scripts, external resources, or raster data unless there is a documented reason.
- Conceptual front-panel diagrams must not be presented as manufacturing drawings.
- Every Markdown image needs meaningful alt text.

## Numbers and units

- Use a space between value and SI unit in prose: `50 µs`, `16 KiB`, `20 kHz`.
- Keep firmware/UI literals exact when quoting the display: `MIN BPM`, `ONE CLOCK`, `FRACTAL`.
- Use decimal commas only in translated/localized prose; canonical US-English repository text uses decimal points.
- State whether a limit is a factory default, user boundary, technical boundary, or current implementation limit.

## Code documentation

Each project-owned C/C++ file starts with Doxygen metadata:

```cpp
/**
 * @file example.h
 * @brief One-sentence purpose of this translation unit or interface.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
```

Public API declarations belong in headers and use concise Doxygen comments:

```cpp
/**
 * @brief Rebuilds one channel on the global musical reference grid.
 * @param channelIndex Zero-based physical output index.
 * @param includeCurrentBoundary Whether an event exactly at the current position is eligible.
 */
void scheduleChannel(std::size_t channelIndex, bool includeCurrentBoundary);
```

Document parameters, return semantics, units, ownership/lifetime, thread/ISR restrictions, and non-obvious side effects where they matter. Do not duplicate header API prose above the implementation definition; implementation comments should explain algorithms and invariants instead.

## Typeset manual layout

The maintained ODT under `docs/manual-source/` follows a publication-layout contract in addition to the Markdown content rules. The manual is set for home printing on 99 × 210 mm pages: black, greys (`#222`, `#555`, `#bbb`, `#e3e3e3`) and white only, no filled areas apart from QR codes and screenshots, and hierarchy from size, weight and white space rather than colour. The typeface is the Ubuntu family (Ubuntu, Ubuntu Light, Ubuntu Bold, Ubuntu Mono).

- the cover is typeset, not an image: overline `EURORACK · 3U · 10 HP`, `CLOCK`, `User Manual`, a rule, the stamp line `Firmware <version> · Digital master clock`, the front panel as a line drawing without callouts, and below a hairline the South Signal Lab logo with its name and the black firmware-update QR; author attribution belongs in the Colophon;
- `Contents` is a dedicated page immediately after the cover, uses the normal content-page frame, and keeps its table inside the same horizontal text bounds as the body;
- Contents page numbers are resolved from the actual exported PDF and written back to the ODT before the final publication export;
- chapter headings are left-aligned with a thin grey number, a bold black title and a 0.5 pt rule below, and retain semantic outline levels for PDF navigation;
- intermediate headings are left-aligned, black and bold, with no inherited list/outline indentation;
- manually numbered headings must not rely on LibreOffice's implicit outline list-tab positioning;
- direct screenshots and gallery screenshots use explicit, consistent top/bottom spacing styles; captions and neighboring body text must not collapse against the image edge;
- screenshots are the generator's print variant: dark on light, framed by the display cut-out of the front panel, at 2.2 : 1; one sentence at the first screenshot explains the inverted display;
- every figure carries a caption `Figure <chapter>.<n> — …` in 7 pt grey; the panel figure is the KiCad-derived line drawing with numbered callouts;
- ordinary data tables use a bold header with a 0.5 pt rule and 0.25 pt grey rules between rows, without fills or vertical rules; notes carry a bold keyword and a rule on the left, without a background; special callout/gallery/Contents tables are exempt;
- numbered lists show the number only, not a bullet as well;
- long read-only UI overflow is illustrated with a functional/legal value such as `CORE LIC`, not by making personal attribution visually dominant;
- the source-repository URL/QR receives its own page near the end of the manual;
- release stamping updates the firmware version in the cover's stamp line and in the Colophon; it does not touch the QR codes or the manual-license line;
- the page footer shows `CLOCK User Manual` and the page number in grey under a hairline.

The frozen files in `docs/manual/` are release-history artifacts. Layout improvements after a release apply to the maintained source and future manuals; published historical ODTs are not silently rewritten.

## Version-sensitive statements

The active alpha version may be stated in landing pages, test baselines, and release instructions. Historical changelog sections remain immutable except for factual corrections.

When a feature is not implemented, say so explicitly. Do not describe the intended External Sync capture path as shipping hardware until comparator/input-capture HIL tests exist.

## README footer

README-style documents end without a footer.
