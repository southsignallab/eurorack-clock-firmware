<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# Contributing to CLOCK

CLOCK is an embedded real-time project. Changes are welcome when they preserve the module's timing, safety, DIY maintainability, and documentation standards.

## Before changing code

Read:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)
- [`docs/TEST_COVERAGE.md`](docs/TEST_COVERAGE.md)
- [`docs/HIL_TEST_PLAN.md`](docs/HIL_TEST_PLAN.md) for hardware-facing changes

## Non-negotiable engineering rules

- Hardware/framework APIs stay behind HAL or the declarative pin map.
- UI code never generates musical timing.
- ISR/scheduler paths stay bounded, non-blocking, allocation-free, and free of display/logging/persistence work.
- User-visible static firmware strings belong in `src/ui_text.h`.
- Public C++ declarations require concise Doxygen API documentation.
- Keep source code reviewable by humans: use descriptive names, explicit state-machine dispatch, named units/sentinels, and comments that explain invariants or intent rather than narrating syntax.
- New behavior requires regression tests at the lowest meaningful level and integration coverage when subsystem boundaries are involved.
- Real electrical claims require HIL evidence; host mocks are not timing instruments.
- Do not weaken coverage, warning, sanitizer, architecture, persistence, or boot-safety gates to make a change pass.

## Local verification

```bash
python scripts/check_architecture.py
python scripts/check_documentation.py
python scripts/check_doxygen.py
python -m unittest discover -s scripts/tests -p 'test_*.py' -v
python scripts/check_static_analysis.py --clang-tidy <clang-tidy 23.1.4>  # Windows: add the two --extra-arg options below
pio test -e native
python scripts/run_host_tests.py --skip-sanitizers
python scripts/run_host_tests.py --sanitizers-only
cmake --preset simulator-headless
cmake --build --preset simulator-headless
ctest --preset simulator-headless
```

Target builds:

```bash
pio run -e blackpill_f401cc
pio run -e blackpill_f401cc_spi
```

### Formatting

C and C++ sources in `src/`, `lib/`, `sim/` and `vcv/` follow `.clang-format`, formatted with **clang-format 23.1.5**; other versions can format differently. `sim/tutorial/.clang-format` inherits the repository style and packs call arguments. Format a file in place with `clang-format -i <file>` and keep its LF line endings. Generated sources listed in `.clang-format-ignore` keep the exact generator output. A formatting-only change must leave the firmware image unchanged (next section).

### Static analysis

`.clang-tidy` is the project profile, with the reason for every enabled and every disabled check. `python scripts/check_static_analysis.py --clang-tidy <clang-tidy 23.1.4>` analyzes the host configuration of `src/` and `lib/` and fails on a finding that is not in `scripts/static_analysis_baseline.json`; findings are compared by file, check and message, not by line. After fixing findings, lower the baseline with `--write-baseline`. On Windows, pass `--extra-arg=--target=x86_64-w64-windows-gnu --extra-arg=--sysroot=C:/msys64/ucrt64`. CI runs the same check on Windows (job `static-analysis`) and fails on a new finding.

### Proving that a change leaves the firmware image unchanged

A change that only formats code or edits comments must leave `firmware.bin` byte-identical in all ten firmware environments (the five target builds and the five `release_*` flavors). Record the reference on the commit before the change, then compare the working tree:

```bash
python scripts/check_image_identity.py record --manifest build/image-identity/reference.json
python scripts/check_image_identity.py compare --manifest build/image-identity/reference.json
```

Each environment is built into its own directory under `build/image-identity/`. The script reports `equal` or `UNEQUAL` per environment and exits non-zero on any difference; for a difference it names the loaded ELF sections that changed. Debug information and the linker map are not compared because they depend on line numbers and the build directory. A refactoring without intended behavior change is proven by equal test results instead; any image difference it causes must be explained in its commit message.

## Code documentation

Public C++ declarations carry concise Doxygen comments. The rules:

- **What:** every public function, method, type, enumerator, constant and public struct field has a `@brief` (or a one-line `/** ... */` for fields and constants). Parameters and return values get `@param` and `@return` when their meaning is not obvious from the type and name. Relevant shared state and non-obvious data structures are documented as well.
- **Content:** explain why a declaration exists, ownership and side effects, units and ranges, and non-obvious safety behavior. Inline comments explain why, not what the next line does.
- **Units:** every time, rate and frequency parameter, field and return value states its unit, in the name (`…Ms`, `…Us`, `…Hz`, `…Q32` for Q32 fixed point) and in the comment when the name does not carry it.
- **Interrupt context:** a function that may run in an interrupt says so. APIs meant for interrupt context end in `FromIsr()`; their comment and the comment of every other function reachable from an interrupt entry point states the contract: bounded run time, non-blocking, no allocation, no output, no interrupt lock opened. Mark it with `@isr <contract>`; Doxygen collects every such function on the page *Interrupt-context functions*.
- **File headers:** `@file`, `@brief`, `@author`, `@copyright` and `@license`; the Doxyfile renders `@license` as a paragraph.
- **Check:** the tree has no Doxygen warning, and the Doxyfile sets `WARN_AS_ERROR = FAIL_ON_WARNINGS`, so `doxygen Doxyfile` (CI) and `python scripts/check_doxygen.py` (local chain; its baseline `scripts/doxygen_warning_baseline.json` is empty) fail on any new undocumented declaration or documentation error. CI uploads the generated HTML as the artifact `doxygen-api-<commit>`.

## Documentation changes

Update the documentation in the same change when behavior, wiring, settings, persistence schema, simulator controls, architecture, or build procedure changes. [`docs/DOCUMENTATION_MAP.md`](docs/DOCUMENTATION_MAP.md) names the authoritative document for each topic and lists every documentation file; a new Markdown file needs a row there. The pin and display-macro tables in `docs/CONFIGURATION.md` are generated from `src/pin_map.h` and `src/config.h`: run `python scripts/generate_reference_tables.py` after changing them. Mermaid diagrams belong in Markdown; reusable publication artwork belongs in `docs/manual-source/assets/` as plain SVG.

## Commit hygiene

Do not commit build directories, `.pio`, coverage output, simulator state, generated compile databases with local paths, or editor/user-specific state. The checked-in `.gitignore` is the minimum rule, not permission to commit other local artifacts.

## Generated Wiki

The GitHub Wiki is generated from canonical repository Markdown by `scripts/build_wiki.py` and published by `.github/workflows/wiki.yml` after pushes to the default branch. Do not maintain duplicate Wiki-only documentation. When changing documentation structure, update the generator page manifest and run `python scripts/build_wiki.py --output build/wiki-preview` locally. The Wiki manual page must always expose the version-frozen ODT manual for the current firmware version.
