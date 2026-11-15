"""Contract tests for the experimental VCV Rack Trial-First port.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import json
import configparser
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class VcvManifestTests(unittest.TestCase):
    def test_manifest_uses_stable_unique_identity_and_rack2_major(self) -> None:
        manifest = json.loads((ROOT / "vcv/plugin.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["slug"], "SouthSignalLab-CLOCK")
        self.assertEqual(manifest["brand"], "South Signal Lab")
        self.assertEqual(manifest["name"], "South Signal Lab Clock")
        self.assertEqual(manifest["modules"][0]["slug"], "CLOCK")
        self.assertEqual(manifest["modules"][0]["name"], "South Signal Lab Clock")
        self.assertTrue(str(manifest["version"]).startswith("2."))
        self.assertIn("Clock generator", manifest["modules"][0]["tags"])
        self.assertEqual(
            manifest["license"],
            "https://polyformproject.org/licenses/noncommercial/1.0.0/",
        )

    def test_makefile_compiles_production_sources_in_cpp17_mode(self) -> None:
        makefile = (ROOT / "vcv/Makefile").read_text(encoding="utf-8")
        self.assertIn("../src", makefile)
        self.assertIn("../lib/clock_core/src", makefile)
        self.assertIn("CLOCK_HOST_TEST=1", makefile)
        self.assertIn("CLOCK_SIMULATOR=1", makefile)
        self.assertIn("EXTRA_CXXFLAGS += -std=c++17", makefile)
        self.assertIn("-fno-unsafe-math-optimizations", makefile)
        self.assertNotIn("ClockEngine.cpp", makefile)

    def test_plugin_shell_does_not_implement_musical_scheduler(self) -> None:
        shell = (ROOT / "vcv/src/CLOCK.cpp").read_text(encoding="utf-8")
        bridge = (ROOT / "vcv/clock_vcv_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("ClockVcvRuntime", shell)
        self.assertIn("runtime_.advanceMicroseconds(config::kSchedulerTickUs)", bridge)
        self.assertNotRegex(shell, r"bpm|euclid|grooveAmount|sequencerStep")

    def test_rack_input_edges_are_buffered_before_scheduler_quantization(self) -> None:
        header = (ROOT / "vcv/clock_vcv_runtime.h").read_text(encoding="utf-8")
        bridge = (ROOT / "vcv/clock_vcv_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("InputTransitionBridge", header)
        self.assertIn("kCapacity = 16U", header)
        self.assertIn("nextSchedulerLevel()", bridge)
        self.assertIn("syncPulseCountForTest", header)
        self.assertIn("resetPulseCountForTest", header)
        self.assertIn("openGeneralSettings", header)
        self.assertIn("generalSettingsOpenForTest", header)
        self.assertIn("runtime_.openGeneralSettings()", bridge)


    def test_panel_is_generated_from_native_simulator_geometry(self) -> None:
        subprocess.run(
            [sys.executable, str(ROOT / "scripts/generate_vcv_panel.py"), "--check"],
            cwd=ROOT,
            check=True,
        )
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(ROOT / "sim/panel_layout.ini", encoding="utf-8")
        generated = (ROOT / "vcv/generated_panel_layout.hpp").read_text(encoding="utf-8")
        shell = (ROOT / "vcv/src/CLOCK.cpp").read_text(encoding="utf-8")
        panel = (ROOT / "vcv/res/CLOCK.svg").read_text(encoding="utf-8")

        self.assertIn("kPanelWidthMm = 50.5F", generated)
        self.assertIn("kPanelHeightMm = 128.5F", generated)
        self.assertIn("kButtonActuatorDiameterMm = 9.0F", generated)
        self.assertIn("kLedDiameterMm = 3.0F", generated)
        self.assertIsNone(re.search(r"(?<![A-Za-z0-9_.])[-+]?\d+F\b", generated))
        self.assertIn("kOutputCenters", shell)
        self.assertIn("kLedCenters", shell)
        self.assertIn("MediumLight<RedLight>", shell)
        self.assertIn("ClockEncoderWidget", shell)
        self.assertIn("queuedEncoderDetents", shell)
        self.assertIn("kPixelsPerDetent = 5.0F", shell)
        self.assertIn("kDragThresholdPx = 2.0F", shell)
        self.assertIn("beginEncoderGesture", shell)
        self.assertIn("beginEncoderRotation", shell)
        self.assertIn("endEncoderGesture", shell)
        self.assertIn("EncoderGestureMode::Pending", shell)
        self.assertIn("EncoderGestureMode::Rotate", shell)
        self.assertIn("EncoderGestureMode::Press", shell)
        self.assertIn("kEncoderPushArmSeconds = 0.180", shell)
        self.assertNotIn("ClockEncoderPushButton", shell)
        self.assertNotIn("createParamCentered<ClockEncoderKnob>", shell)
        # Panel lettering is the KiCad engraving inside res/CLOCK.svg; no text overlay is drawn.
        self.assertNotIn("ClockPanelLabels", shell)
        self.assertNotIn("APP->window->uiFont->handle", shell)
        self.assertIn('configButton(PLAY_PARAM, "START / PAUSE")', shell)
        self.assertIn('configButton(TAP_PARAM, "TAP TEMPO / SHIFT")', shell)
        self.assertIn('configButton(STOP_PARAM, "STOP / BACK")', shell)
        self.assertIn('configInput(SYNC_INPUT, "IN 1")', shell)
        self.assertIn('configInput(RESET_INPUT, "IN 2")', shell)
        self.assertIn("setBrightnessSmooth", shell)
        self.assertIn("appendContextMenu", shell)
        self.assertIn('"General Settings..."', shell)
        self.assertIn("requestGeneralSettings", shell)
        self.assertIn("onHoverKey", shell)
        self.assertIn("GLFW_KEY_LEFT_SHIFT", shell)
        self.assertIn("GLFW_KEY_ENTER", shell)
        self.assertIn("GLFW_KEY_UP", shell)
        self.assertIn("keyboardLeftShiftHeld", shell)
        self.assertIn("keyboardRightShiftHeld", shell)
        self.assertIn("keyboardTapHeld", shell)
        # Key handling lives in the Rack-independent, host-tested keyboard_controls module.
        self.assertIn('#include "keyboard_controls.h"', shell)
        self.assertIn("applyKeyboardEvent", shell)
        self.assertIn("setKeyboardHeldControls", shell)
        self.assertNotIn("setKeyboardShiftHeld", shell)
        self.assertNotIn("activityHoldSeconds", shell)
        self.assertNotIn("0.075F", shell)
        self.assertIn("std::abs(event.mouseDelta.y)", shell)
        self.assertNotIn("event.mouseDelta.norm()", shell)
        self.assertIn("event.mouseDelta.y", shell)
        self.assertIn("event.scrollDelta.y", shell)
        self.assertNotIn("setEncoderPointerState", shell)
        self.assertNotIn("releaseEncoderPointer", shell)
        self.assertNotIn("requestEncoderPush", shell)
        self.assertNotIn("encoderPushSamplesRemaining", shell)
        self.assertNotIn("constexpr float kOutputX[2]", shell)
        self.assertNotIn("Vec(75.0F, 129.0F)", shell)
        artwork = (ROOT / "docs/assets/front-panel/clock-front-panel.svg").read_text(encoding="utf-8")
        self.assertIn("GENERATED from docs/assets/front-panel/clock-front-panel.svg and sim/panel_layout.ini", panel)
        self.assertIn("GENERATED by scripts/import_kicad_front_panel.py", artwork)
        self.assertIn('fill-rule="evenodd"', panel)
        self.assertNotIn("<text", panel)
        self.assertNotIn("clip-path", panel)
        artwork_engraving = re.search(r'<g id="engraving"[^>]*>\n(.*?)\n  </g>', artwork, re.S).group(1)
        self.assertIn(artwork_engraving, panel)
        self.assertGreaterEqual(panel.count("<path d=\""), 100)
        self.assertFalse((ROOT / "vcv/res/encoder-push-0.svg").exists())
        self.assertFalse((ROOT / "vcv/res/encoder-push-1.svg").exists())
        self.assertFalse((ROOT / "vcv/res/encoder.svg").exists())
        self.assertIn("4x2", (ROOT / "vcv/README.md").read_text(encoding="utf-8"))
        end_user = (ROOT / "README_VCV.md").read_text(encoding="utf-8")
        self.assertIn("General Settings...", end_user)
        self.assertIn("Shift", end_user)
        self.assertIn("docs/assets/vcv-rack-clock.png", end_user)
        self.assertTrue((ROOT / "docs/assets/vcv-rack-clock.png").exists())
        main_readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("README_VCV.md", main_readme)
        self.assertIn("docs/assets/vcv-rack-clock.png", main_readme)


    def test_native_simulator_uses_kicad_panel_artwork(self) -> None:
        renderer = (ROOT / "sim/panel_renderer.cpp").read_text(encoding="utf-8")
        layout = (ROOT / "sim/panel_layout.ini").read_text(encoding="utf-8")
        docs = (ROOT / "docs/SIMULATOR.md").read_text(encoding="utf-8")

        # The committed layout uses the KiCad-derived engraving as panel background; the built-in
        # debug labels stay available for custom layouts without artwork.
        self.assertIn("background_image = panel_background.png", layout)
        self.assertIn("draw_builtin_labels = false", layout)
        self.assertTrue((ROOT / "sim/panel_background.png").is_file())
        self.assertIn("# Front-panel source: Simple Frontpanel.kicad_pcb", layout)
        self.assertIn('"IN 1"', renderer)
        self.assertIn('"IN 2"', renderer)
        self.assertNotIn('"SYNC IN"', renderer)
        self.assertNotIn('"RST IN"', renderer)
        self.assertNotIn('"HI" : "LO"', renderer)
        self.assertIn("off_color = #111113", layout)
        self.assertIn("KiCad front-panel project", docs)

class VcvWorkflowTests(unittest.TestCase):
    def test_workflow_uses_node24_generation_actions_and_pinned_sdk(self) -> None:
        workflow = (ROOT / ".github/workflows/vcv.yml").read_text(encoding="utf-8")
        self.assertIn("actions/checkout@v7", workflow)
        self.assertIn("actions/upload-artifact@v7", workflow)
        self.assertIn("actions/setup-python@v7", workflow)
        self.assertIn("build-essential cmake curl binutils jq zstd unzip", workflow)
        self.assertIn("permissions:\n  contents: read", workflow)
        self.assertIn("RACK_SDK_VERSION: '2.6.6'", workflow)
        self.assertIn("RACK_SDK_PLATFORM: 'lin-x64'", workflow)
        self.assertIn("https://vcvrack.com/downloads/Rack-SDK-${RACK_SDK_VERSION}-${RACK_SDK_PLATFORM}.zip", workflow)
        self.assertIn("python scripts/generate_vcv_panel.py --check", workflow)
        self.assertIn("python -m unittest scripts.tests.test_vcv_tooling -v", workflow)
        self.assertIn("CMakePresets.json", workflow)
        self.assertIn("scripts/check_vcv_runtime_coverage.py", workflow)
        self.assertIn("make -C vcv", workflow)
        self.assertIn("tar --zstd -tf", workflow)
        self.assertIn("make -C vcv install", workflow)
        self.assertIn("plugins-lin-x64", workflow)
        self.assertIn("readelf -d vcv/plugin.so", workflow)
        self.assertIn("libRack.so", workflow)
        self.assertIn("Rack-SDK|include|dep", workflow)
        self.assertNotRegex(workflow, r"actions/(checkout|upload-artifact|cache)@v[1-4]\b")

    def test_local_developer_guide_covers_build_test_install_and_license_boundary(self) -> None:
        guide = (ROOT / "docs/VCV_DEVELOPMENT.md").read_text(encoding="utf-8")
        for required in (
            "Rack-SDK-2.6.6-win-x64.zip",
            "MSYS2 MinGW 64-bit",
            '$env:RACK_DIR = "C:\\SDK\\Rack-SDK"',
            'export RACK_DIR="/c/SDK/Rack-SDK"',
            "command -v make",
            "pacman -S --needed make",
            "Get-ChildItem C:\\SDK -Filter plugin.mk -Recurse",
            "Rack-SDK-2.6.6-mac-x64.zip",
            "Rack-SDK-2.6.6-mac-arm64.zip",
            "Rack-SDK-2.6.6-lin-x64.zip",
            "cmake --preset simulator-headless",
            "vcv_runtime_adapter_tests",
            "check_vcv_runtime_coverage.py",
            "make -C vcv dist",
            "make -C vcv install",
            "RACK_USER_DIR",
            "log.txt",
            "VCV Rack Non-Commercial Plugin License Exception",
            "General Settings...",
            "Shift+Enter",
            "Rack API use: confined to `vcv/`",
        ):
            self.assertIn(required, guide)

    def test_rack_sdk_is_not_vendored_and_api_boundary_is_documented(self) -> None:
        self.assertFalse((ROOT / "Rack-SDK").exists())
        self.assertFalse((ROOT / ".rack-sdk").exists())
        self.assertFalse(any(ROOT.glob("Rack-SDK-*")))
        gitignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
        for ignored in (
            "Rack-SDK/",
            "Rack-SDK-*/",
            "vcv/build/",
            "vcv/dep/",
            "vcv/dist/",
            "vcv/plugin.so",
            "vcv/plugin.dylib",
            "vcv/plugin.dll",
            "vcv/*.vcvplugin",
            "AppDataLocalRack2*/",
            "Rack2/",
        ):
            self.assertIn(ignored, gitignore)
        self.assertNotIn("vcv/res/encoder", gitignore)
        generator = (ROOT / "scripts/generate_vcv_panel.py").read_text(encoding="utf-8")
        self.assertIn("LEGACY_ENCODER_ASSETS", generator)
        self.assertIn("path.unlink()", generator)
        architecture = (ROOT / "scripts/check_architecture.py").read_text(encoding="utf-8")
        self.assertIn("check_vcv_api_boundary", architecture)
        self.assertIn("Rack API references are only permitted under vcv/", architecture)

    def test_licensing_docs_record_external_sdk_boundary(self) -> None:
        licensing = (ROOT / "docs/LICENSING.md").read_text(encoding="utf-8")
        dependencies = (ROOT / "docs/DEPENDENCIES.md").read_text(encoding="utf-8")
        notices = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
        self.assertIn("VCV Rack Non-Commercial Plugin License Exception", licensing)
        self.assertIn("Rack SDK 2.6.6", dependencies)
        self.assertIn("not vendored", notices)
        self.assertIn("not bundled", notices)

    def test_vcv_docs_disclose_single_instance_limit(self) -> None:
        readme = (ROOT / "vcv/README.md").read_text(encoding="utf-8")
        docs = (ROOT / "docs/VCV_RACK.md").read_text(encoding="utf-8")
        self.assertIn("one CLOCK instance", readme)
        self.assertIn("one active CLOCK instance", docs)
        self.assertIn("0/+5 V", readme)
        self.assertIn("Trial First", readme)


if __name__ == "__main__":
    unittest.main()
