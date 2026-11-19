"""Check the explicit VPK artwork selection without requiring VitaSDK."""
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source_path(name):
    if name in ("wallpaper", "emblem"):
        return ROOT / "assets" / "aurora" / f"{name}.png"
    if name.startswith("platform_"):
        return ROOT / "assets" / "aurora" / "console_icons" / f"{name}.png"
    return ROOT / "assets" / "aurora" / "ui_icons" / f"{name}.png"


class ArtworkInventoryTests(unittest.TestCase):
    def test_aurora_artwork_is_grouped_and_registered(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        renderer = (ROOT / "src/vita/ui_renderer.c").read_text()
        match = re.search(r"set\(VP_AURORA_ASSETS\s+(.*?)\)", cmake, re.S)
        self.assertIsNotNone(match, "VP_AURORA_ASSETS")
        runtime = match.group(1).split()
        console_icons = {p.stem for p in (ROOT / "assets/aurora/console_icons").glob("platform_*.png")}
        ui_icons = {p.stem for p in (ROOT / "assets/aurora/ui_icons").glob("ui_*.png")}
        self.assertEqual(len(runtime), 42)
        self.assertEqual(len(set(runtime)), 42)
        self.assertEqual(console_icons, {n for n in runtime if n.startswith("platform_")})
        self.assertEqual(ui_icons, {n for n in runtime if n.startswith("ui_")})
        self.assertIn('"ui_romm","ui_settings","ui_download"', renderer)
        self.assertIn("assets/aurora/console_icons/${asset}.png", cmake)
        self.assertIn("assets/aurora/ui_icons/${asset}.png", cmake)
        self.assertRegex(
            cmake.replace("\n", " "),
            r'"\$\{CMAKE_CURRENT_SOURCE_DIR\}/\$\{asset_source\}"\s+"assets/aurora/\$\{asset\}\.png"',
        )
        for notice in ("RomM-AGPL-3.0.txt", "romm-isotipo.svg", "NOTICE.md"):
            self.assertTrue((ROOT / "assets/aurora/licenses" / notice).is_file(), notice)
            self.assertIn(f"assets/aurora/licenses/{notice}", cmake)
        for name in runtime:
            path = source_path(name)
            data = path.read_bytes()
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            width, height = struct.unpack(">II", data[16:24])
            size = 128 if name.startswith("platform_") or name == "emblem" else 64
            self.assertEqual((width, height), (480, 272) if name == "wallpaper" else (size, size), name)
            self.assertEqual(data[24], 8, name)
            self.assertEqual(data[28], 0, name)
            self.assertLessEqual(len(data), 512 * 1024, name)
            self.assertIn('{"' + name + '",', renderer)


if __name__ == "__main__":
    unittest.main()
