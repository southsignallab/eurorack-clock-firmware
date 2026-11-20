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
python -m unittest discover -s scripts/tests -p 'test_*.py' -v
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

### Proving that a change leaves the firmware image unchanged

A change that only formats code or edits comments must leave `firmware.bin` byte-identical in all ten firmware environments (the five target builds and the five `release_*` flavors). Record the reference on the commit before the change, then compare the working tree:

```bash
python scripts/check_image_identity.py record --manifest build/image-identity/reference.json
python scripts/check_image_identity.py compare --manifest build/image-identity/reference.json
```

Each environment is built into its own directory under `build/image-identity/`. The script reports `equal` or `UNEQUAL` per environment and exits non-zero on any difference; for a difference it names the loaded ELF sections that changed. Debug information and the linker map are not compared because they depend on line numbers and the build directory. A refactoring without intended behavior change is proven by equal test results instead; any image difference it causes must be explained in its commit message.

## Documentation changes

Update the documentation in the same change when behavior, wiring, settings, persistence schema, simulator controls, architecture, or build procedure changes. [`docs/DOCUMENTATION_MAP.md`](docs/DOCUMENTATION_MAP.md) names the authoritative document for each topic and lists every documentation file; a new Markdown file needs a row there. The pin and display-macro tables in `docs/CONFIGURATION.md` are generated from `src/pin_map.h` and `src/config.h`: run `python scripts/generate_reference_tables.py` after changing them. Mermaid diagrams belong in Markdown; reusable publication artwork belongs in `docs/manual-source/assets/` as plain SVG.

## Commit hygiene

Do not commit build directories, `.pio`, coverage output, simulator state, generated compile databases with local paths, or editor/user-specific state. The checked-in `.gitignore` is the minimum rule, not permission to commit other local artifacts.

## Generated Wiki

The GitHub Wiki is generated from canonical repository Markdown by `scripts/build_wiki.py` and published by `.github/workflows/wiki.yml` after pushes to the default branch. Do not maintain duplicate Wiki-only documentation. When changing documentation structure, update the generator page manifest and run `python scripts/build_wiki.py --output build/wiki-preview` locally. The Wiki manual page must always expose the version-frozen ODT manual for the current firmware version.
