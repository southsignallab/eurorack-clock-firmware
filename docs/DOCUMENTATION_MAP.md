<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# Documentation map

Each topic has one authoritative document. Other documents link to it instead of repeating its facts; where a fact comes from code, the code is the source and the document follows it. The inventory below gives every documentation file exactly one purpose. `scripts/check_documentation.py` fails when a Markdown file is missing from the inventory.

## Authoritative document per topic

| Topic | Authoritative document | Source in code or data |
| --- | --- | --- |
| Operating behavior | the chapter files in [`user-guide/`](user-guide/README.md) | — |
| Menus and settings | [`user-guide/15-settings-map.md`](user-guide/15-settings-map.md) | `src/ui_text.h`, `src/ui/menu_model.cpp` |
| Display variants and build flags | [`CONFIGURATION.md`](CONFIGURATION.md) | `platformio.ini`, `src/config.h` |
| Pin assignment | [`CONFIGURATION.md`](CONFIGURATION.md) | `src/pin_map.h` |
| Factory defaults | [`CONFIGURATION.md`](CONFIGURATION.md) | `src/defaults.h` |
| Persistence and Flash layout | [`ARCHITECTURE.md`](ARCHITECTURE.md) | `src/hal/persistent_layout.h` |
| Timing (scheduler, priorities, display transport timing) | [`TIMING.md`](TIMING.md) | `src/config.h`, `src/hal/` |
| Firmware install, update and recovery; power safety | [`FIRMWARE_UPDATE.md`](FIRMWARE_UPDATE.md) | — |
| Workstation setup and build | [`DEVELOPER_README.md`](DEVELOPER_README.md) | — |
| Contribution rules and the local check chain | [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | — |
| Memory budget | [`DEVELOPER_README.md`](DEVELOPER_README.md) | `platformio.ini`, `scripts/platformio_memory_gate.py` |
| Native test suites and case counts | [`../test/README.md`](../test/README.md) | `scripts/check_test_inventory.py` |
| Coverage figures | [`TEST_COVERAGE.md`](TEST_COVERAGE.md) | `scripts/run_host_tests.py` |
| HIL procedure and status | [`HIL_TEST_PLAN.md`](HIL_TEST_PLAN.md) | `qualification/v1_qualification.json` |
| Release procedure and firmware flavors | [`RELEASE_PROCESS.md`](RELEASE_PROCESS.md) | `platformio.ini` |
| Version and change history | [`../CHANGELOG.md`](../CHANGELOG.md) | `src/version.h` |
| Licensing policy | [`LICENSING.md`](LICENSING.md) | — |
| Third-party components | [`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) | `scripts/check_embedded_dependencies.py` |
| Citation and project identity | [`PROJECT_IDENTITY.md`](PROJECT_IDENTITY.md) | `CITATION.cff`, `codemeta.json` |
| Native simulator | [`SIMULATOR.md`](SIMULATOR.md) | — |
| VCV Rack edition: status / use | [`VCV_RACK.md`](VCV_RACK.md) / [`../README_VCV.md`](../README_VCV.md) | — |
| Generated Wiki | [`WIKI.md`](WIKI.md) | `scripts/build_wiki.py` |
| Typeset manual | [`manual-source/README.md`](manual-source/README.md) | `manual-source/clock-user-manual.odt` |

## Inventory

Paths are relative to the repository root. A row with a wildcard covers a group of files with the same purpose.

| Path | Purpose | Audience |
| --- | --- | --- |
| `.github/FUNDING.yml` | GitHub sponsor button config (Ko-fi) | CI/GitHub |
| `.github/SECURITY.md` | Security reporting policy | developer/contributor |
| `.github/agents/audit-triage.agent.md` | Instructions for the repository audit-triage agent: split audit issues into sub-issues | CI/GitHub (agent) |
| `.github/workflows/ci.yml` | CI workflow: checks, native tests, coverage, target builds | CI |
| `.github/workflows/manual-publication.yml` | Manual publication smoke test (ODT/PDF build) | CI |
| `.github/workflows/release.yml` | Release workflow: build, package, manual, release notes | CI + release |
| `.github/workflows/vcv.yml` | Experimental VCV Rack plugin build/test | CI |
| `.github/workflows/wiki.yml` | Publishes generated GitHub Wiki (scripts/build_wiki.py) | CI |
| `CHANGELOG.md` | Keep-a-Changelog history from 1.0.0 on, with [Unreleased] | release/legal + developer |
| `CITATION.cff` | Citation metadata (version, date, authors) | release/legal |
| `CONTRIBUTING.md` | Contribution rules, engineering rules, local verification commands | developer/contributor |
| `LICENSE.md` | PolyForm Noncommercial 1.0.0 license text (firmware) | release/legal |
| `NOTICE.txt` | Required Notice line for PolyForm; copied into release packages by package_release.py | release/legal |
| `README.md` | Project landing page: feature overview, release status, build/test quick start, documentation index | end user + developer |
| `README_VCV.md` | End-user guide for the VCV Rack edition | end user (VCV) |
| `THIRD_PARTY_NOTICES.md` | Third-party components and licenses (HAL, CMSIS, fonts, SDL3, toolchain, Rack SDK) | release/legal |
| `codemeta.json` | CodeMeta 3.0 software metadata | release/legal |
| `docs/ARCHITECTURE.md` | Firmware architecture: layers, scheduler, HAL, display, persistence, inputs | developer |
| `docs/CONFIGURATION.md` | Compile-time config (config.h, defaults.h, pin_map.h); points to ARCHITECTURE.md for persistence | developer + builder/DIY |
| `docs/DEPENDENCIES.md` | Dependency analysis (framework, HAL, display, toolchain) | developer + release/legal |
| `docs/DEVELOPER_README.md` | Workstation setup per OS, build/upload/simulator/VCV, coverage and memory policy | developer |
| `docs/DEVELOPMENT.md` | Day-to-day development: coding standard, tests, CI; links to the coverage and release documents | developer |
| `docs/DOCUMENTATION_MAP.md` | This file: authoritative document per topic and documentation inventory | all |
| `docs/DOCUMENTATION_STYLE.md` | Documentation style rules (units, admonitions, version statements) | developer/contributor |
| `docs/FIRMWARE_UPDATE.md` | Install/update/recovery via ST-LINK and DFU, variants, persistence safety | end user + builder/DIY |
| `docs/HIL_TEST_PLAN.md` | Hardware-in-the-loop test plan HIL-* (boot, gate, timing, groove, display) | developer + builder (bench) |
| `docs/LICENSING.md` | Licensing and distribution constraints | release/legal |
| `docs/PROJECT_IDENTITY.md` | Canonical repo identity, citation, DOI policy | release/legal |
| `docs/README.md` | Documentation entry point: task/audience index, doc rules, asset list | all |
| `docs/RELEASE_PROCESS.md` | Release procedure: preparation, tags, tag-build steps, manual publication, release notes | release |
| `docs/ROADMAP.md` | Version roadmap 1.0 to 1.5+, HIL policy, VCV track | end user + developer |
| `docs/SEQUENCER_2.md` | Sequencer 2.0 contract: scope released in 1.2.0 and parked follow-up work | developer |
| `docs/SIMULATOR.md` | Native SDL3 simulator: build, CLI, layout schema, scope, boundaries | developer + end user (trial) |
| `docs/SUPPORT.md` | Ko-fi support page | end user |
| `docs/TEST_COVERAGE.md` | Coverage gates, the release coverage baseline and its history, areas covered by host tests | developer |
| `docs/TIMING.md` | Timing contract: scheduler, gate writes, EXTI, display transports | developer |
| `docs/USER_GUIDE.md` | Index of the 24 User Guide chapters; repeats no operating facts | end user |
| `docs/V1_FORWARD_COMPATIBILITY.md` | Persistence/V1 forward-compat audit: byte budgets and constraints for later features; map and schemas in ARCHITECTURE.md | developer |
| `docs/VCV_DEVELOPMENT.md` | VCV plugin developer guide: SDK, build, smoke tests | developer (VCV) |
| `docs/VCV_RACK.md` | VCV Rack port status and verified boundary | developer + end user (VCV) |
| `docs/WIKI.md` | How the GitHub Wiki is generated and published | developer/maintainer |
| `docs/manual-source/LICENSE.md` | License of the user manual: CC BY-NC 4.0 (pointer, not full text) | release/legal |
| `docs/manual-source/README.md` | Manual workspace: ODT source, asset policy, publication workflow | maintainer (manual) |
| `docs/manual-source/clock-user-manual.odt` | Maintained (running) end-user manual source, ODT | end user |
| `docs/qualification/HIL_CAPTURE_FORMAT.md` | Normalized edge-capture format for analyze_hil_capture.py | bench/developer |
| `docs/qualification/HIL_EVIDENCE_TEMPLATE.md` | Template for HIL evidence records | bench/developer |
| `docs/qualification/README.md` | Index of V1 qualification evidence | developer + bench |
| `docs/qualification/V1_ACCEPTANCE.md` | Objective V1 physical timing acceptance contract | developer + bench |
| `docs/qualification/V1_BENCH_MATRIX.md` | Repeatable bench configurations QP-01 ff. | bench/builder |
| `docs/qualification/v1_qualification.json` | Machine-readable HIL ledger (PENDING/BLOCKED/PASS/FAIL/NOT_APPLICABLE) | developer + release |
| `docs/releases/CHANGELOG-0.x.md` | Archived change history of the 0.x prototype and beta releases | developer + release |
| `docs/tutorials/PHASE1_ACCEPTANCE.md` | Storybook phase-1 acceptance evidence | tutorial production |
| `docs/tutorials/README.md` | Storybook tutorial system overview | tutorial production |
| `docs/tutorials/STORYBOOK_ARCHITECTURE.md` | Storybook architecture, schema 1 / phase 1 | tutorial production |
| `docs/tutorials/STORY_SCHEMA_1.md` | Story YAML schema 1 reference | tutorial production |
| `docs/tutorials/VIDEO_GENERATION.md` | How to render narrated tutorial videos | tutorial production |
| `docs/tutorials/assets/video/README.md` | Pre-produced intro/outro clip description | tutorial production |
| `docs/tutorials/examples/README.md` | Index of example stories | tutorial production |
| `docs/tutorials/interaction_profiles.yaml` | Storybook interaction profiles (input to render_tutorials.py) | tutorial production |
| `docs/tutorials/stories/README.md` | Index of reference stories | tutorial production |
| `docs/tutorials/themes/README.md` | Storybook themes | tutorial production |
| `docs/tutorials/voiceover/README.md` | Voice-over script conventions | tutorial production |
| `docs/tutorials/voiceover/TIMING.md` | Voice-over/picture timing rules | tutorial production |
| `docs/user-guide/README.md` | Chapter index of the 24-chapter user guide | end user |
| `test/README.md` | Native test suites, counts per suite, host matrix, HIL boundary | developer |
| `third_party/LICENSE-Apache-2.0.txt` | Apache-2.0 license text (CMSIS, Roboto derivative) | release/legal |
| `third_party/LICENSE-BSD-3-Clause.txt` | BSD-3-Clause license text (STM32Cube HAL) | release/legal |
| `third_party/LICENSE-SDL-zlib.txt` | zlib license text (SDL3, simulator only) | release/legal |
| `third_party/fonts/ubuntu-sans/LICENCE.txt` | Ubuntu Font Licence 1.0 (README hero fonts) | release/legal |
| `vcv/LICENSE.md` | PolyForm Noncommercial 1.0.0 license text for VCV plugin | release/legal |
| `vcv/README.md` | VCV plugin source README: status, adapter tests, SDK build | developer (VCV) |
| `docs/manual/clock-user-manual.*.odt` | Frozen user manual of one stable release (written by `scripts/prepare_release_manual.py`) | end user + release |
| `docs/releases/*/RELEASE_SUMMARY.md` | User-facing summary of one release; input to `scripts/release_notes.py` | end user + release |
| `docs/tutorials/examples/*.yaml` | Storybook example story source (input to `scripts/render_tutorials.py`) | tutorial production |
| `docs/tutorials/stories/*.yaml` | Storybook reference story source (input to `scripts/render_tutorials.py`) | tutorial production |
| `docs/tutorials/stories/examples/*.yaml` | Storybook schema example (valid and invalid cases for the validator) | tutorial production |
| `docs/tutorials/themes/*.yaml` | Storybook theme (input to `scripts/render_tutorials.py`) | tutorial production |
| `docs/tutorials/voiceover/[0-9][0-9]-*.txt` | Narration script of one tutorial (input to `scripts/render_tutorials.py`) | tutorial production |
| `docs/user-guide/[0-9][0-9]-*.md` | One chapter of the User Guide, aligned with the manual chapter of the same number; authoritative for operating behavior | end user |
