"""Tests for CLOCK user-manual versioning and release-publication contracts.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
import sys
from pathlib import Path, PurePosixPath, PureWindowsPath

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"Unable to load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PREPARE = load("prepare_release_manual", ROOT / "scripts" / "prepare_release_manual.py")
CHECK = load("check_user_manual", ROOT / "scripts" / "check_user_manual.py")
BUILD = load("build_user_manual", ROOT / "scripts" / "build_user_manual.py")
REFRESH = load("refresh_manual_screenshots", ROOT / "scripts" / "refresh_manual_screenshots.py")
UPDATE_CONTENTS = load("update_manual_contents", ROOT / "scripts" / "update_manual_contents.py")


class ManualToolingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.version = PREPARE.current_version()
        self.working = ROOT / "docs" / "manual-source" / "clock-user-manual.odt"
        self.frozen = (
            ROOT / "docs" / "manual" / f"clock-user-manual.{self.version}.odt"
        )

    def test_working_manual_exists(self) -> None:
        self.assertTrue(self.working.is_file())

    def test_working_manual_uses_post_1_1_layout_contract(self) -> None:
        import zipfile

        with zipfile.ZipFile(self.working, "r") as archive:
            content = archive.read("content.xml").decode("utf-8")
            metadata = archive.read("meta.xml").decode("utf-8")
        version = PREPARE.source_version(metadata, content)
        CHECK.validate_odt(self.working, version)
        self.assertTrue((ROOT / "docs/manual-source/assets/updates-qr.png").is_file())

    def test_post_1_1_release_stamping_preserves_layout_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            version = "1.1.1"
            output = Path(directory) / f"clock-user-manual.{version}.odt"
            PREPARE.stamp_odt(
                self.working,
                output,
                version,
                allow_font_substitution=True,
            )
            CHECK.validate_odt(output, version)

    def test_existing_frozen_manuals_match_their_version_scope(self) -> None:
        archive = ROOT / "docs" / "manual"
        frozen_manuals = sorted(archive.glob("clock-user-manual.*.odt"))
        self.assertTrue(frozen_manuals)
        for manual in frozen_manuals:
            version = manual.name.removeprefix("clock-user-manual.").removesuffix(".odt")
            self.assertEqual(manual.name, f"clock-user-manual.{version}.odt")
            CHECK.validate_odt(manual, version)

    def test_manual_directory_contains_only_stable_release_manuals(self) -> None:
        archive = ROOT / "docs" / "manual"
        release_records = ROOT / "docs" / "releases"
        stable_versions = {
            path.name
            for path in release_records.iterdir()
            if path.is_dir()
            and PREPARE.is_stable_release(path.name)
            and (path / "RELEASE_SUMMARY.md").is_file()
        }
        expected_files = {f"clock-user-manual.{version}.odt" for version in stable_versions}
        actual_entries = {path.name for path in archive.iterdir()}
        self.assertEqual(actual_entries, expected_files)
        self.assertTrue(all(path.is_file() for path in archive.iterdir()))
        self.assertEqual({"1.0.0", "1.0.1", "1.1.0", "1.2.0"}, stable_versions)

    def test_prerelease_manual_defaults_outside_archive(self) -> None:
        version = "1.1.0-beta.1"
        self.assertFalse(PREPARE.is_stable_release(version))
        self.assertEqual(
            PREPARE.default_output(version),
            ROOT / "build" / "manual" / f"clock-user-manual.{version}.odt",
        )
        self.assertEqual(
            BUILD.default_source(version),
            ROOT / "build" / "manual" / f"clock-user-manual.{version}.odt",
        )

    def test_prepare_output_path_is_version_scoped(self) -> None:
        expected = (
            ROOT / "docs" / "manual" / f"clock-user-manual.{self.version}.odt"
        )
        self.assertEqual(PREPARE.default_output(self.version), expected)
        self.assertEqual(BUILD.default_source(self.version), expected)

    def test_release_stage_labels_follow_semver_prerelease_phase(self) -> None:
        self.assertEqual(PREPARE.release_stage_label("0.19.0-alpha.63"), "Prototype")
        self.assertEqual(PREPARE.release_stage_label("0.19.0-beta.1"), "Beta")
        self.assertEqual(PREPARE.release_stage_label("1.0.0-rc.2"), "Release candidate")
        self.assertEqual(PREPARE.release_stage_label("1.0.0"), "Release")

    def test_stamping_rewrites_manual_version_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            target_version = "0.19.0-alpha.99"
            output = Path(directory) / f"clock-user-manual.{target_version}.odt"
            PREPARE.stamp_odt(
                self.working,
                output,
                target_version,
                allow_font_substitution=True,
            )
            CHECK.validate_odt(output, target_version)

    def test_firmware_stamping_never_rewrites_license_versions(self) -> None:
        import zipfile

        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            stable = directory_path / "clock-user-manual.1.0.0.odt"
            next_release = directory_path / "clock-user-manual.1.1.0.odt"

            PREPARE.stamp_odt(
                self.working,
                stable,
                "1.0.0",
                allow_font_substitution=True,
            )
            PREPARE.stamp_odt(
                stable,
                next_release,
                "1.1.0",
                allow_font_substitution=True,
            )

            with zipfile.ZipFile(next_release, "r") as archive:
                content = archive.read("content.xml").decode("utf-8")
                metadata = archive.read("meta.xml").decode("utf-8")

            self.assertIn("CLOCK 1.1.0", content)
            self.assertIn("firmware 1.1.0", content.lower())
            self.assertIn("PolyForm Noncommercial License 1.0.0", content)
            self.assertNotIn("PolyForm Noncommercial License 1.1.0", content)
            self.assertIn("PolyForm Noncommercial License 1.0.0", metadata)
            self.assertNotIn("PolyForm Noncommercial License 1.1.0", metadata)
            CHECK.validate_odt(next_release, "1.1.0")

    def test_pdf_font_check_accepts_libreoffice_subset_without_light_suffix(self) -> None:
        fonts = (
            "name                                 type              encoding         emb sub uni object ID\n"
            "------------------------------------ ----------------- ---------------- --- --- --- ---------\n"
            "ABCDEE+Ubuntu                        TrueType          WinAnsi          yes yes yes     12  0\n"
        )
        CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_pdf_font_check_rejects_missing_ubuntu_family(self) -> None:
        fonts = (
            "name                                 type              encoding         emb sub uni object ID\n"
            "ABCDEE+NotoSans                      TrueType          WinAnsi          yes yes yes     12  0\n"
        )
        with self.assertRaisesRegex(RuntimeError, "Ubuntu-family"):
            CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_pdf_font_check_rejects_unembedded_ubuntu(self) -> None:
        fonts = (
            "name                                 type              encoding         emb sub uni object ID\n"
            "Ubuntu                               TrueType          WinAnsi           no  no yes     12  0\n"
        )
        with self.assertRaisesRegex(RuntimeError, "embedded/subset"):
            CHECK.validate_ubuntu_pdf_fonts(fonts)

    PDFFONTS_HEADER = (
        "name                                 type              encoding         emb sub uni object ID\n"
        "------------------------------------ ----------------- ---------------- --- --- --- ---------\n"
    )

    def test_pdf_font_check_rejects_an_embedded_substitute_font(self) -> None:
        # Shape of the Windows build output: the Ubuntu faces are present, but
        # LibreOffice fell back to Segoe UI for some glyphs.
        fonts = self.PDFFONTS_HEADER + (
            "BAAAAA+Ubuntu-Light                  TrueType          Identity-H       yes yes yes     10  0\n"
            "CAAAAA+Ubuntu-Bold                   TrueType          Identity-H       yes yes yes     15  0\n"
            "DAAAAA+UbuntuMono-Regular            Type 3            Custom           yes yes yes     20  0\n"
            "EAAAAA+SegoeUI                       TrueType          Identity-H       yes yes yes     25  0\n"
        )
        with self.assertRaisesRegex(RuntimeError, "SegoeUI"):
            CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_pdf_font_check_rejects_an_unnamed_font(self) -> None:
        fonts = self.PDFFONTS_HEADER + (
            "BAAAAA+Ubuntu-Regular                TrueType          Identity-H       yes yes yes     10  0\n"
            "[none]                               Type 3            Custom           yes no  no      30  0\n"
        )
        with self.assertRaisesRegex(RuntimeError, "none"):
            CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_pdf_font_check_rejects_an_unembedded_face_beside_embedded_ones(self) -> None:
        fonts = self.PDFFONTS_HEADER + (
            "BAAAAA+Ubuntu-Regular                TrueType          Identity-H       yes yes yes     10  0\n"
            "Ubuntu-Bold                          TrueType          WinAnsi          no  no  no      11  0\n"
        )
        with self.assertRaisesRegex(RuntimeError, "Ubuntu-Bold"):
            CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_pdf_font_check_accepts_only_ubuntu_family_faces(self) -> None:
        fonts = self.PDFFONTS_HEADER + (
            "AAAAAA+Ubuntu                        TrueType          Identity-H       yes yes yes      8  0\n"
            "BAAAAA+Ubuntu-Light                  TrueType          Identity-H       yes yes yes     10  0\n"
            "CAAAAA+Ubuntu-Bold                   TrueType          Identity-H       yes yes yes     15  0\n"
            "DAAAAA+UbuntuMono-Regular            Type 3            Custom           yes yes yes     20  0\n"
        )
        CHECK.validate_ubuntu_pdf_fonts(fonts)

    def test_release_workflow_publishes_odt_and_pdf(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")
        self.assertIn("scripts/generate_manual_screenshots.py", workflow)
        self.assertIn("scripts/prepare_release_manual.py", workflow)
        self.assertIn("scripts/refresh_manual_screenshots.py", workflow)
        self.assertIn("scripts/build_user_manual.py", workflow)
        self.assertIn("scripts/check_user_manual.py", workflow)
        self.assertIn('clock-user-manual.${VERSION}.odt', workflow)
        self.assertIn('clock-user-manual.${VERSION}.pdf', workflow)
        self.assertIn('if [[ "${VERSION}" == *-* ]]', workflow)
        self.assertIn('docs/manual-source/clock-user-manual.odt', workflow)
        self.assertIn('Missing frozen stable-release manual', workflow)
        self.assertIn("--require-ubuntu-fonts", workflow)
        self.assertLess(
            workflow.index("scripts/generate_manual_screenshots.py"),
            workflow.index("scripts/prepare_release_manual.py"),
        )
        self.assertLess(
            workflow.index("scripts/prepare_release_manual.py"),
            workflow.index("scripts/refresh_manual_screenshots.py"),
        )
        self.assertLess(
            workflow.index("scripts/refresh_manual_screenshots.py"),
            workflow.index("scripts/build_user_manual.py"),
        )

    def test_ci_and_release_install_manual_tooling_dependencies_before_tests(self) -> None:
        for workflow_name in ("ci.yml", "release.yml"):
            workflow = (ROOT / ".github" / "workflows" / workflow_name).read_text(encoding="utf-8")
            install = 'python -m pip install "platformio==6.1.19" pillow lxml'
            self.assertIn(install, workflow, workflow_name)
            self.assertIn("Run release-tooling tests", workflow, workflow_name)
            self.assertLess(
                workflow.index(install),
                workflow.index("Run release-tooling tests"),
                workflow_name,
            )

    def test_direct_manual_screenshot_contract_is_complete(self) -> None:
        targets = REFRESH.direct_frame_targets(
            self.working, ROOT / "docs" / "manual-source" / "assets"
        )
        self.assertEqual(len(targets), len(REFRESH.DIRECT_SCREENSHOTS) + len(REFRESH.DIRECT_ILLUSTRATIONS))
        self.assertIn("ManualImage15", targets)
        self.assertEqual(REFRESH.DIRECT_SCREENSHOTS["ManualImage15"], "settings-input-config-print.png")
        self.assertEqual(REFRESH.DIRECT_SCREENSHOTS["ManualImage23"], "settings-licenses-print.png")
        self.assertEqual(REFRESH.DIRECT_SCREENSHOTS["ManualImage24"], "settings-general-print.png")
        self.assertEqual(REFRESH.DIRECT_SCREENSHOTS["ManualImage25"], "settings-master-print.png")
        # The manual embeds only print variants (D-12 point 5).
        self.assertTrue(all(name.endswith("-print.png") for name in REFRESH.DIRECT_SCREENSHOTS.values()))
        self.assertEqual(REFRESH.DIRECT_ILLUSTRATIONS["ManualImage1"], "front-panel-anatomy-print.svg")
        self.assertEqual(REFRESH.DIRECT_ILLUSTRATIONS["ManualCoverPanel"], "front-panel-print.svg")

    def test_untypeset_manuals_use_the_pre_typeset_screen_variant_contract(self) -> None:
        # F-054: the list is the contract of the working manual before the
        # typeset cover, not that of a frozen release manual. Frozen 1.1.0 has
        # fewer frames; a released version is reproducible only from its tag.
        import re
        import zipfile

        frozen = ROOT / "docs" / "manual" / "clock-user-manual.1.1.0.odt"
        self.assertFalse(REFRESH.is_typeset_manual(frozen))
        self.assertTrue(REFRESH.is_typeset_manual(self.working))
        self.assertFalse(hasattr(REFRESH, "LEGACY_DIRECT_SCREENSHOTS"))
        self.assertEqual(REFRESH.frame_contract(frozen), REFRESH.PRE_TYPESET_SCREENSHOTS)
        self.assertTrue(all(not name.endswith("-print.png") for name in REFRESH.PRE_TYPESET_SCREENSHOTS.values()))
        with zipfile.ZipFile(frozen) as archive:
            frames = set(re.findall(r'draw:name="(ManualImage\d+)"', archive.read("content.xml").decode("utf-8")))
        self.assertEqual({"ManualImage24", "ManualImage25"}, set(REFRESH.PRE_TYPESET_SCREENSHOTS) - frames)

    def test_direct_screenshots_are_not_duplicated(self) -> None:
        # W-297: every direct screenshot shows a different state.
        assets = list(REFRESH.DIRECT_SCREENSHOTS.values())
        self.assertEqual(len(assets), len(set(assets)))

    def test_refresh_replaces_direct_frames_and_galleries(self) -> None:
        import zipfile
        from lxml import etree

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "manual.odt"
            direct_count, gallery_count = REFRESH.refresh(
                self.working, output, ROOT / "docs" / "manual-source" / "assets"
            )
            self.assertEqual(
                direct_count, len(REFRESH.DIRECT_SCREENSHOTS) + len(REFRESH.DIRECT_ILLUSTRATIONS)
            )
            self.assertEqual(gallery_count, 22)
            with zipfile.ZipFile(output, "r") as archive:
                root = etree.fromstring(archive.read("content.xml"))
                ns = REFRESH.NS
                frame = root.xpath(
                    '//draw:frame[@draw:name="ManualImage15"]/draw:image', namespaces=ns
                )[0]
                href = frame.get(f"{{{ns['xlink']}}}href")
                self.assertEqual(
                    archive.read(href),
                    (ROOT / "docs" / "manual-source" / "assets" / "settings-input-config-print.png").read_bytes(),
                )
                self.assertEqual(
                    archive.read("Pictures/gallery_manualgalleryscreensavers_01_01.png"),
                    (ROOT / "docs" / "manual-source" / "assets" / "screensaver-clock-print.png").read_bytes(),
                )
                licenses = root.xpath(
                    '//draw:frame[@draw:name="ManualImage23"]/draw:image', namespaces=ns
                )[0]
                licenses_href = licenses.get(f"{{{ns['xlink']}}}href")
                self.assertEqual(
                    archive.read(licenses_href),
                    (ROOT / "docs" / "manual-source" / "assets" / "settings-licenses-print.png").read_bytes(),
                )
                panel = root.xpath('//draw:frame[@draw:name="ManualImage1"]/draw:image', namespaces=ns)[0]
                self.assertEqual(
                    archive.read(panel.get(f"{{{ns['xlink']}}}href")),
                    (ROOT / "docs" / "manual-source" / "assets" / "front-panel-anatomy-print.svg").read_bytes(),
                )

    def test_contents_updater_sees_24_semantic_chapters(self) -> None:
        import zipfile
        from lxml import etree

        with zipfile.ZipFile(self.working, "r") as archive:
            root = etree.fromstring(archive.read("content.xml"))
        headings = UPDATE_CONTENTS.chapter_headings(root)
        self.assertEqual(24, len(headings))
        self.assertEqual("1 Start here", headings[0])
        self.assertEqual("24 Technical status and specifications", headings[-1])

    def test_manual_builder_refreshes_static_contents_after_first_pdf_export(self) -> None:
        source = (ROOT / "scripts" / "build_user_manual.py").read_text(encoding="utf-8")
        self.assertIn("update_manual_contents.update(odt_out, pdf_out)", source)
        self.assertIn("clock-lo-profile-toc-", source)

    def test_libreoffice_profile_url_is_a_file_url_for_windows_paths(self) -> None:
        cases = {
            r"C:\Users\clock\AppData\Local\Temp\clock-lo-profile-abc":
                "file:///C:/Users/clock/AppData/Local/Temp/clock-lo-profile-abc",
            r"C:\Users\Clock User\Temp\clock-lo-profile-abc":
                "file:///C:/Users/Clock%20User/Temp/clock-lo-profile-abc",
            r"\\server\share\clock-lo-profile-abc":
                "file://server/share/clock-lo-profile-abc",
        }
        for path, url in cases.items():
            with self.subTest(path=path):
                self.assertEqual(
                    f"-env:UserInstallation={url}",
                    BUILD.user_installation_argument(PureWindowsPath(path)),
                )

    def test_libreoffice_profile_url_is_a_file_url_for_posix_paths(self) -> None:
        cases = {
            "/tmp/clock-lo-profile-abc": "file:///tmp/clock-lo-profile-abc",
            "/tmp/clock lo/clock-lo-profile-abc": "file:///tmp/clock%20lo/clock-lo-profile-abc",
        }
        for path, url in cases.items():
            with self.subTest(path=path):
                self.assertEqual(
                    f"-env:UserInstallation={url}",
                    BUILD.user_installation_argument(PurePosixPath(path)),
                )

    def test_libreoffice_profile_url_rejects_relative_paths(self) -> None:
        with self.assertRaises(ValueError):
            BUILD.user_installation_argument(PurePosixPath("clock-lo-profile-abc"))

    def test_manual_builder_uses_the_profile_url_helper_for_every_export(self) -> None:
        source = (ROOT / "scripts" / "build_user_manual.py").read_text(encoding="utf-8")
        self.assertEqual(2, source.count("user_installation_argument(Path(profile).resolve())"))
        self.assertNotIn("UserInstallation=file://{", source)

    def test_manual_pdf_export_keeps_screenshot_pixels(self) -> None:
        # D-12 point 5: LibreOffice's default 300 dpi image reduction would
        # resample the nearest-neighbour screenshots; every export disables it.
        source = (ROOT / "scripts" / "build_user_manual.py").read_text(encoding="utf-8")
        self.assertIn('"ReduceImageResolution":{"type":"boolean","value":"false"}', BUILD.PDF_EXPORT_FILTER)
        self.assertTrue(BUILD.PDF_EXPORT_FILTER.startswith("pdf:writer_pdf_Export:"))
        self.assertEqual(2, source.count("PDF_EXPORT_FILTER,"))
        self.assertNotIn('"--convert-to",\n                "pdf",', source)

    def test_manual_screenshot_generator_recovers_from_generator_mismatch(self) -> None:
        generator = load(
            "generate_manual_screenshots_test",
            ROOT / "scripts" / "generate_manual_screenshots.py",
        )
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory) / "simulator-headless"
            build_dir.mkdir(parents=True)
            (build_dir / "CMakeCache.txt").write_text(
                "CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n",
                encoding="utf-8",
            )
            (build_dir / "stale-artifact").write_text("stale", encoding="utf-8")
            generator.ensure_compatible_build_tree(build_dir)
            self.assertFalse(build_dir.exists())

    def test_ci_and_release_use_headless_cmake_preset_consistently(self) -> None:
        for workflow_name in ("ci.yml", "release.yml"):
            workflow = (ROOT / ".github" / "workflows" / workflow_name).read_text(encoding="utf-8")
            self.assertIn("cmake --preset simulator-headless", workflow, workflow_name)
            self.assertIn("cmake --build --preset simulator-headless", workflow, workflow_name)
            self.assertIn("ctest --preset simulator-headless", workflow, workflow_name)
            self.assertNotIn("cmake -S . -B build/simulator-headless", workflow, workflow_name)

    def test_manual_publication_smoke_workflow_exists(self) -> None:
        workflow = ROOT / ".github" / "workflows" / "manual-publication.yml"
        self.assertTrue(workflow.is_file())
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("fonts-ubuntu", text)
        self.assertIn("libreoffice-writer", text)
        self.assertIn("clock-user-manual", text)
        self.assertIn("scripts/generate_manual_screenshots.py", text)
        self.assertIn("scripts/refresh_manual_screenshots.py", text)


QR = load("generate_manual_qr_codes", ROOT / "scripts" / "generate_manual_qr_codes.py")
SCREENSHOTS = load("generate_manual_screenshots_print", ROOT / "scripts" / "generate_manual_screenshots.py")
PANEL = load("generate_front_panel_illustration", ROOT / "scripts" / "generate_front_panel_illustration.py")
ASSETS = ROOT / "docs" / "manual-source" / "assets"


class ManualPrintLayoutTests(unittest.TestCase):
    """D-12 print contract of the maintained manual (B-04)."""

    def test_qr_encoder_reproduces_the_frozen_release_repository_code(self) -> None:
        import io
        import zipfile

        from PIL import Image

        frozen = ROOT / "docs" / "manual" / "clock-user-manual.1.1.0.odt"
        with zipfile.ZipFile(frozen) as archive, Image.open(
            io.BytesIO(archive.read("Pictures/repository-qr.png"))
        ) as image:
            matrix = QR.matrix_from_image(image)
        self.assertEqual(QR.encode(CHECK.LEGACY_REPOSITORY_QR_URL), matrix)
        self.assertNotEqual(QR.encode(QR.REPOSITORY_URL), matrix)

    def test_qr_assets_are_black_and_encode_the_d12_addresses(self) -> None:
        self.assertEqual(QR.KOFI_URL, "https://ko-fi.com/southsignallab")
        self.assertEqual(QR.UPDATES_URL, "https://github.com/southsignallab/eurorack-clock-firmware/releases")
        self.assertEqual(QR.REPOSITORY_URL, "https://github.com/southsignallab/eurorack-clock-firmware")
        self.assertEqual(QR.generate(ASSETS, check=True), [])
        CHECK.validate_qr_assets()

    def test_qr_check_rejects_a_coloured_or_wrong_code(self) -> None:
        from PIL import Image

        image = QR.render(QR.encode(QR.KOFI_URL))
        self.assertTrue(QR.encodes(image, QR.KOFI_URL))
        self.assertFalse(QR.encodes(image, "https://example.com/southsignallab"))
        with tempfile.TemporaryDirectory() as directory:
            QR.generate(Path(directory), check=False)
            self.assertEqual(QR.generate(Path(directory), check=True), [])
            # The former blue update code: same modules, coloured.
            coloured = Image.new("RGB", image.size, (255, 255, 255))
            coloured.paste((11, 79, 192), mask=image.convert("L").point(lambda value: 255 - value))
            coloured.save(Path(directory) / "kofi-support-qr.png")
            with self.assertRaises(SystemExit):
                QR.generate(Path(directory), check=True)

    def test_print_variant_inverts_and_scales_pixels_without_resampling(self) -> None:
        pixels = bytes(255 if (x + y) % 3 == 0 else 0 for y in range(64) for x in range(128))
        image = SCREENSHOTS.print_variant_image(128, 64, pixels)
        scale = SCREENSHOTS.PRINT_PIXEL_SCALE
        left = (image.width - 128 * scale) // 2
        top = (image.height - 64 * scale) // 2
        for y in range(0, 64, 7):
            for x in range(0, 128, 5):
                expected = 255 - pixels[y * 128 + x]
                for dy in (0, scale - 1):
                    for dx in (0, scale - 1):
                        self.assertEqual(image.getpixel((left + x * scale + dx, top + y * scale + dy)), expected)

    def test_print_variant_is_framed_by_the_display_cutout(self) -> None:
        width, height, radius = SCREENSHOTS.display_cutout()
        self.assertEqual((width, height), (25.0, 13.6))
        self.assertAlmostEqual(radius, 1.4999)
        image = SCREENSHOTS.print_variant_image(128, 64, bytes(128 * 64))
        px_per_mm = 128 * SCREENSHOTS.PRINT_PIXEL_SCALE / SCREENSHOTS.OLED_ACTIVE_WIDTH_MM
        self.assertEqual(image.size, (round(25.12 * px_per_mm), round(13.72 * px_per_mm)))
        middle_x, middle_y = image.width // 2, image.height // 2
        stroke_px = round(SCREENSHOTS.PRINT_FRAME_STROKE_MM * px_per_mm)
        self.assertLess(min(image.getpixel((middle_x, y)) for y in range(stroke_px + 1)), 128)  # top outline
        self.assertLess(min(image.getpixel((x, middle_y)) for x in range(stroke_px + 1)), 128)  # left outline
        self.assertEqual(image.getpixel((0, 0)), 255)  # rounded corner stays open
        self.assertEqual(image.getpixel((middle_x, middle_y)), 255)  # dark OLED pixel prints white

    def test_screenshot_catalog_lists_both_variants(self) -> None:
        rows = [line.split("\t") for line in (ASSETS / "manual-screenshots.tsv").read_text(encoding="utf-8").splitlines()]
        self.assertTrue(rows)
        for name, print_name, _description in rows:
            self.assertEqual(print_name, f"{name}-print")
            self.assertTrue((ASSETS / f"{name}.png").is_file(), name)
            self.assertTrue((ASSETS / f"{print_name}.png").is_file(), print_name)
        parsed = SCREENSHOTS.parse_manifest(ASSETS / "manual-screenshots.tsv")
        self.assertEqual(parsed[0], (rows[0][0], rows[0][2]))

    def test_panel_line_drawing_has_no_panel_fill(self) -> None:
        for callouts in (False, True):
            svg = PANEL.generate_print(PANEL.DEFAULT_LAYOUT, PANEL.DEFAULT_ARTWORK, callouts=callouts)
            self.assertIn('<path id="panel" fill="none" stroke="#000"', svg)
            self.assertIn('<g id="engraving" clip-path="url(#panel-clip)" fill="#000" stroke="#000"', svg)
            for colour in ("#141414", "#d9d9d9", PANEL.ACCENT):
                self.assertNotIn(colour, svg)
            self.assertEqual(svg.count("<text "), 9 if callouts else 0)
        self.assertEqual(
            PANEL.generate_print(PANEL.DEFAULT_LAYOUT, PANEL.DEFAULT_ARTWORK, callouts=True),
            (ASSETS / "front-panel-anatomy-print.svg").read_text(encoding="utf-8"),
        )

    def test_palette_check_rejects_colour_and_fills(self) -> None:
        clean = '<style:text-properties fo:color="#222222"/>'
        CHECK.validate_print_palette(clean, clean)
        for bad in (
            '<style:text-properties fo:color="#0b4fc0"/>',
            '<style:table-cell-properties fo:background-color="#fcebd7"/>',
            '<style:table-cell-properties fo:border-left="2.25pt solid #f07d1a"/>',
            '<style:graphic-properties draw:fill="solid" draw:fill-color="#000000"/>',
        ):
            with self.assertRaises(RuntimeError, msg=bad):
                CHECK.validate_print_palette(bad, clean)

    def test_every_figure_needs_a_figure_caption(self) -> None:
        from xml.etree import ElementTree as ET

        text = "urn:oasis:names:tc:opendocument:xmlns:text:1.0"
        good = ET.fromstring(
            f'<r xmlns:text="{text}"><text:p text:style-name="ManualScreenshotP"/>'
            '<text:p text:style-name="ManualCaptionP">Figure 3.1 — Boot screen.</text:p></r>'
        )
        CHECK.validate_figure_captions(good)
        for following in ('<text:p text:style-name="P16">Body</text:p>',
                          '<text:p text:style-name="ManualCaptionP">Boot screen.</text:p>', ""):
            bad = ET.fromstring(
                f'<r xmlns:text="{text}"><text:p text:style-name="ManualScreenshotP"/>{following}</r>'
            )
            with self.assertRaises(RuntimeError):
                CHECK.validate_figure_captions(bad)

    def test_typeset_cover_is_stamped_through_its_text_line(self) -> None:
        import zipfile

        working = ROOT / "docs" / "manual-source" / "clock-user-manual.odt"
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "clock-user-manual.1.2.0-beta.1.odt"
            PREPARE.stamp_odt(working, output, "1.2.0-beta.1", allow_font_substitution=False)
            with zipfile.ZipFile(output) as archive:
                content = archive.read("content.xml").decode("utf-8")
            self.assertIn("Firmware 1.2.0-beta.1 · Digital master clock", content)
            CHECK.validate_odt(output, "1.2.0-beta.1")

    def test_working_manual_uses_the_d12_support_address(self) -> None:
        import zipfile

        with zipfile.ZipFile(ROOT / "docs" / "manual-source" / "clock-user-manual.odt") as archive:
            content = archive.read("content.xml").decode("utf-8")
        self.assertEqual(CHECK.KOFI_SUPPORT_URL, "https://ko-fi.com/southsignallab")
        self.assertIn("https://ko-fi.com/southsignallab", content)
        self.assertNotIn("X8X21EOLQ9", content)
        self.assertNotIn("→", content)  # no glyph outside the Ubuntu family
        self.assertIn("The screens in this manual are shown dark on light for printing", content)

    def test_panel_chapter_introduction_names_the_inputs_as_engraved(self) -> None:
        import re
        import zipfile

        with zipfile.ZipFile(ROOT / "docs" / "manual-source" / "clock-user-manual.odt") as archive:
            content = archive.read("content.xml").decode("utf-8")
        start = content.find("Panel, power and boot</text:span></text:h>")
        self.assertGreater(start, 0)
        introduction = re.sub(r"<[^>]+>", "", content[start:content.find("<draw:frame", start)])
        self.assertIn("IN 1 and IN 2 (factory roles SYNC and RESET)", introduction)
        self.assertNotIn("RST", introduction)

    def test_storage_chapter_explains_save_error_and_update_firmware(self) -> None:
        # F-059 / W-318: the printed manual explains both banners like user guide chapter 17.
        import re
        import zipfile

        with zipfile.ZipFile(ROOT / "docs" / "manual-source" / "clock-user-manual.odt") as archive:
            content = archive.read("content.xml").decode("utf-8")
        start = content.find("Presets, autosave and templates</text:span></text:h>")
        end = content.find("Screensaver and display protection</text:span></text:h>", start)
        self.assertGreater(start, 0)
        self.assertGreater(end, start)
        chapter = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", content[start:end]))
        for expected in (
            "SAVE ERROR",
            "after 1, 2, 4, 8, 16 and 32 seconds",
            "until the next change or until the module is switched off and on",
            "for 3 seconds after every front-panel action",
            "UPDATE FIRMWARE",
            "Only FACTORY RESET (ERASE ALL DATA) lifts the lock",
        ):
            self.assertIn(expected, chapter)

    def working_manual_text(self) -> str:
        import re
        import zipfile

        with zipfile.ZipFile(ROOT / "docs" / "manual-source" / "clock-user-manual.odt") as archive:
            content = archive.read("content.xml").decode("utf-8")
        return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", content))

    def test_introduction_names_the_inputs_as_engraved(self) -> None:
        # F-057: chapter 1 uses the panel names with their factory roles.
        text = self.working_manual_text()
        self.assertNotIn("dedicated SYNC and RST inputs", text)
        self.assertIn("Hardware Rev 1 has two digital inputs, IN 1 and IN 2 (factory roles SYNC and RESET),", text)

    def test_specifications_name_the_inputs_and_mark_the_pins_as_diy_build(self) -> None:
        # F-057: chapter 24 keeps the pin facts, names the inputs as engraved and
        # marks the pins as those of the DIY build; no other variant is named.
        import re

        text = self.working_manual_text()
        match = re.search(r"Inputs (IN 1 .*?) Controls", text)
        self.assertIsNotNone(match)
        row = match.group(1)
        self.assertIn("IN 1 (factory role SYNC) on PA8", row)
        self.assertIn("IN 2 (factory role RESET) on PA9", row)
        self.assertIn("DIY", row)
        for absent in ("RST", "SMD", "PA0", "PA6"):
            self.assertNotIn(absent, row)
        self.assertNotIn("SYNC on PA8 and RST on PA9", text)

    def test_qr_codes_on_the_licenses_page_keep_space_to_the_text_above(self) -> None:
        # F-058: a proportional line height below 100 % lets an inline QR image
        # rise into the wrapped address above it; the paragraphs holding the
        # repository and Ko-fi codes need full line height and a top margin.
        import re
        import zipfile

        with zipfile.ZipFile(ROOT / "docs" / "manual-source" / "clock-user-manual.odt") as archive:
            content = archive.read("content.xml").decode("utf-8")
        for frame in ("RepositoryQR", "KofiSupportQR"):
            with self.subTest(frame=frame):
                position = content.index(f'draw:name="{frame}"')
                paragraph = content.rfind("<text:p ", 0, position)
                style = re.match(r'<text:p text:style-name="([^"]+)"', content[paragraph:]).group(1)
                definition = re.search(
                    rf'<style:style style:name="{re.escape(style)}" style:family="paragraph"[^>]*>(.*?)</style:style>',
                    content,
                    re.S,
                ).group(1)
                line_height = re.search(r'fo:line-height="(\d+)%"', definition)
                self.assertTrue(line_height is None or int(line_height.group(1)) >= 100, definition)
                margin_top = re.search(r'fo:margin-top="([\d.]+)in"', definition)
                self.assertIsNotNone(margin_top, definition)
                self.assertGreaterEqual(float(margin_top.group(1)), 0.1)


# Output of a documentation tool that is not representable in a legacy code
# page: U+25CF and U+2192 have no cp1252 or ASCII encoding, the path has a
# non-ASCII letter. Poppler and fontconfig write UTF-8 on every platform.
UTF8_TOOL_OUTPUT = "Title: Clock \u2192 manual \u25cf \u2022\nPages: 3\n/fonts/\u00fcbuntu/\u25cf.ttf\n"

# Runs one documentation-script call in a child interpreter whose locale
# encoding is not UTF-8 (Windows: the ANSI code page; POSIX: the C locale with
# coercion and UTF-8 mode disabled, i.e. ASCII). Every external command is
# replaced by one that prints ``output`` as UTF-8 bytes, so only the decoding
# in the code under test is exercised.
NON_UTF8_CHILD = r"""
import importlib, json, locale, subprocess, sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
# Hex keeps the arguments ASCII: a C-locale child would decode UTF-8 argv as surrogates.
output = bytes.fromhex(sys.argv[3])
real_run = subprocess.run
def fake_run(command, *args, **kwargs):
    writer = "import sys; sys.stdout.buffer.write(%r)" % output
    return real_run([sys.executable, "-c", writer], *args, **kwargs)
subprocess.run = fake_run
module = importlib.import_module(sys.argv[2])
result = eval(sys.argv[4], {"module": module, "Path": Path})
sys.stdout.buffer.write(json.dumps({
    "encoding": locale.getpreferredencoding(False),
    "result": result,
}).encode("ascii"))
"""


class ManualToolEncodingTests(unittest.TestCase):
    """F-053: documentation scripts decode tool output as UTF-8, not with the locale."""

    def run_under_non_utf8_locale(self, module: str, call: str, output: str = UTF8_TOOL_OUTPUT):
        import json
        import os
        import subprocess

        environment = {
            key: value for key, value in os.environ.items()
            if key not in {"PYTHONUTF8", "PYTHONIOENCODING", "LANG", "LANGUAGE"} and not key.startswith("LC_")
        }
        if os.name != "nt":
            environment.update({"LC_ALL": "C", "PYTHONCOERCECLOCALE": "0"})
        completed = subprocess.run(
            [sys.executable, "-X", "utf8=0", "-c", NON_UTF8_CHILD, str(SCRIPTS), module, output.encode("utf-8").hex(), call],
            capture_output=True,
            env=environment,
            cwd=str(ROOT),
        )
        self.assertEqual(
            0, completed.returncode,
            completed.stderr.decode("utf-8", errors="replace")[-2000:],
        )
        payload = json.loads(completed.stdout.decode("ascii"))
        import codecs
        self.assertNotEqual(
            "utf-8", codecs.lookup(payload["encoding"]).name,
            "the child interpreter must run with a non-UTF-8 locale encoding",
        )
        return payload["result"]

    def test_manual_check_reads_pdftotext_output_as_utf8(self) -> None:
        self.assertEqual(
            UTF8_TOOL_OUTPUT,
            self.run_under_non_utf8_locale("check_user_manual", 'module.run_text(["pdftotext", "m.pdf", "-"])'),
        )

    def test_font_environment_check_reads_fc_match_output_as_utf8(self) -> None:
        output = "Ubuntu Mono\nRegular Light Bold\n/fonts/\u00fcbuntu/\u25cf.ttf\n"
        self.assertIsNone(
            self.run_under_non_utf8_locale(
                "check_user_manual", "module.validate_ubuntu_font_environment()", output
            )
        )

    def test_contents_updater_reads_pdfinfo_and_pdftotext_output_as_utf8(self) -> None:
        self.assertEqual(
            3, self.run_under_non_utf8_locale("update_manual_contents", 'module.page_count(Path("m.pdf"))')
        )
        self.assertEqual(
            " ".join(UTF8_TOOL_OUTPUT.split()),
            self.run_under_non_utf8_locale("update_manual_contents", 'module.pdf_page_text(Path("m.pdf"), 1)'),
        )

    def test_release_stamping_reads_fc_match_output_as_utf8(self) -> None:
        output = "Ubuntu\n/fonts/\u00fcbuntu/\u25cf.ttf\n"
        self.assertEqual(
            "/fonts/\u00fcbuntu/\u25cf.ttf",
            self.run_under_non_utf8_locale(
                "prepare_release_manual",
                'module.resolve_font("Regular", allow_substitution=False)',
                output,
            ),
        )

    def test_tutorial_renderer_reads_captured_tool_output_as_utf8(self) -> None:
        self.assertEqual(
            UTF8_TOOL_OUTPUT,
            self.run_under_non_utf8_locale(
                "render_tutorials", 'module.run_checked(["ffprobe", "m.mp4"], capture=True).stdout'
            ),
        )


class ManualToolDiscoveryTests(unittest.TestCase):
    """F-051: the manual build finds LibreOffice on Windows and names missing Poppler tools."""

    def test_libreoffice_is_found_in_the_default_windows_install_location(self) -> None:
        from unittest import mock

        with tempfile.TemporaryDirectory() as directory:
            program = Path(directory) / "LibreOffice" / "program"
            program.mkdir(parents=True)
            soffice = program / "soffice.exe"
            soffice.write_bytes(b"")
            with mock.patch.object(BUILD.shutil, "which", return_value=None):
                self.assertEqual(
                    str(soffice),
                    BUILD.libreoffice_binary(windows=True, environ={"ProgramFiles": directory}),
                )

    def test_libreoffice_on_the_path_wins_over_the_default_install_location(self) -> None:
        from unittest import mock

        with mock.patch.object(BUILD.shutil, "which", side_effect=lambda name: f"/usr/bin/{name}"):
            self.assertEqual(
                "/usr/bin/libreoffice",
                BUILD.libreoffice_binary(windows=True, environ={"ProgramFiles": "C:\\Program Files"}),
            )

    def test_missing_libreoffice_names_the_searched_locations(self) -> None:
        from unittest import mock

        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(BUILD.shutil, "which", return_value=None):
                with self.assertRaises(RuntimeError) as raised:
                    BUILD.libreoffice_binary(windows=True, environ={"ProgramFiles": directory})
        message = str(raised.exception)
        self.assertIn("PATH", message)
        self.assertIn(str(Path(directory) / "LibreOffice" / "program" / "soffice.exe"), message)

    def test_libreoffice_default_location_is_only_searched_on_windows(self) -> None:
        from unittest import mock

        with tempfile.TemporaryDirectory() as directory:
            program = Path(directory) / "LibreOffice" / "program"
            program.mkdir(parents=True)
            (program / "soffice.exe").write_bytes(b"")
            with mock.patch.object(BUILD.shutil, "which", return_value=None):
                with self.assertRaises(RuntimeError):
                    BUILD.libreoffice_binary(windows=False, environ={"ProgramFiles": directory})

    def test_missing_poppler_tools_are_named_with_an_install_hint(self) -> None:
        from unittest import mock

        with mock.patch.object(CHECK.shutil, "which", side_effect=lambda name: None if name != "pdfinfo" else "/bin/pdfinfo"):
            with self.assertRaises(RuntimeError) as raised:
                CHECK.require_poppler_tools(("pdfinfo", "pdffonts", "pdftotext"))
        message = str(raised.exception)
        self.assertIn("pdffonts, pdftotext", message)
        self.assertNotIn("pdfinfo,", message)
        self.assertIn("poppler-utils", message)
        self.assertIn("mingw-w64-ucrt-x86_64-poppler", message)

    def test_pdf_check_names_missing_poppler_tools(self) -> None:
        from unittest import mock

        with mock.patch.object(CHECK.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, r"Poppler tools are missing: pdfinfo, pdftotext, pdffonts"):
                CHECK.validate_pdf(Path("clock-user-manual.1.1.0.pdf"), "1.1.0", require_ubuntu_fonts=True)

    def test_contents_updater_names_missing_poppler_tools(self) -> None:
        from unittest import mock

        working = ROOT / "docs" / "manual-source" / "clock-user-manual.odt"
        with mock.patch.object(CHECK.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, r"Poppler tools are missing: pdfinfo, pdftotext"):
                UPDATE_CONTENTS.update(working, Path("clock-user-manual.1.1.0.pdf"))

    def test_manual_build_names_missing_poppler_tools_before_the_pdf_export(self) -> None:
        import zipfile
        from unittest import mock

        working = ROOT / "docs" / "manual-source" / "clock-user-manual.odt"
        with zipfile.ZipFile(working, "r") as archive:
            version = PREPARE.source_version(
                archive.read("meta.xml").decode("utf-8"), archive.read("content.xml").decode("utf-8")
            )
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(CHECK.shutil, "which", return_value=None), \
                    mock.patch.object(BUILD, "libreoffice_binary", return_value="soffice"), \
                    mock.patch.object(BUILD.subprocess, "run") as export:
                with self.assertRaisesRegex(RuntimeError, r"Poppler tools are missing: pdfinfo, pdftotext"):
                    BUILD.build(working, Path(directory), version, require_ubuntu_fonts=False)
            export.assert_not_called()


class FirmwareUpdateQrTests(unittest.TestCase):
    """W-045: the on-device update QR code is the encoder output for the module address."""

    URL = "https://southsignallab.com/clock/firmware"
    SOURCE = ROOT / "src" / "ui" / "settings_renderer.cpp"

    def source(self) -> str:
        return self.SOURCE.read_text(encoding="utf-8")

    def constant(self, name: str) -> int:
        import re

        match = re.search(rf"constexpr\s+std::int16_t\s+{name}\s*=\s*(\d+);", self.source())
        self.assertIsNotNone(match, name)
        return int(match.group(1))

    def test_firmware_rows_match_the_encoder_output_for_the_module_address(self) -> None:
        import re

        source = self.source()
        self.assertIn(f"QR payload for exactly: {self.URL}\n", source)
        match = re.search(r"kUpdateQrRows\{\{(.*?)\}\};", source, re.S)
        self.assertIsNotNone(match)
        rows = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]+)UL", match.group(1))]
        matrix = QR.encode(self.URL)
        self.assertEqual(29, len(matrix))  # version 3
        expected = [sum(1 << column for column, dark in enumerate(row) if dark) for row in matrix]
        self.assertEqual(expected, rows)

    def test_update_qr_fills_the_white_field_with_a_three_pixel_quiet_zone(self) -> None:
        field_x = self.constant("kFieldX")
        field_size = self.constant("kFieldSize")
        scale = self.constant("kScale")
        modules = 29
        self.assertEqual(64, field_size)
        self.assertEqual(2, scale)
        self.assertEqual(58, modules * scale)
        self.assertEqual(field_x + 3, self.constant("kQrX"))
        self.assertEqual(3, self.constant("kQrY"))
        self.assertEqual(field_size, modules * scale + 2 * 3)


class SvgLineEndingTests(unittest.TestCase):
    """F-056: generated SVGs are compared byte for byte, so checkouts must keep them LF."""

    @unittest.skipUnless((ROOT / ".git").exists(), "needs a Git checkout")
    def test_every_tracked_svg_is_checked_out_with_lf(self) -> None:
        import subprocess

        listing = subprocess.run(
            ["git", "ls-files", "-z", "--", "*.svg"], cwd=ROOT, capture_output=True, check=True
        ).stdout.decode("utf-8")
        svgs = [name for name in listing.split("\0") if name]
        self.assertTrue(svgs)
        attributes = subprocess.run(
            ["git", "check-attr", "-z", "text", "eol", "--", *svgs], cwd=ROOT, capture_output=True, check=True
        ).stdout.decode("utf-8").split("\0")
        # -z output: path, attribute, value triples.
        values = {(attributes[i], attributes[i + 1]): attributes[i + 2] for i in range(0, len(attributes) - 2, 3)}
        offenders = [
            name for name in svgs
            if values.get((name, "text")) != "set" or values.get((name, "eol")) != "lf"
        ]
        self.assertEqual([], offenders)


if __name__ == "__main__":
    unittest.main()
