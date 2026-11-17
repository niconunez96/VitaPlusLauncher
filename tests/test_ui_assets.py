"""Check the explicit VPK artwork selection without requiring VitaSDK."""
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ArtworkInventoryTests(unittest.TestCase):
    def test_controller_artwork_is_selected_and_registered(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        renderer = (ROOT / "src/vita/ui_renderer.c").read_text()
        def inventory(name):
            match = re.search(r"set\(" + name + r"\s+(.*?)\)", cmake, re.S)
            self.assertIsNotNone(match, name)
            return match[1].split()
        runtime = inventory("VP_AURORA_ASSETS")
        selected = inventory("VP_CONTROLLER_ASSETS")
        candidates = {p.stem for p in (ROOT / "assets/controller-tiles").glob("*.png")
                      if p.stem.startswith(("platform_", "ui_"))}
        self.assertEqual(len(runtime), 42)
        self.assertEqual(len(set(runtime)), 42)
        self.assertEqual(set(selected), candidates)
        self.assertEqual(len(selected), 29)
        self.assertTrue(candidates <= set(runtime))
        self.assertIn('"ui_romm","ui_settings","ui_download"', renderer)
        self.assertIn("assets/controller-tiles/${asset}.png", cmake)
        for notice in ("RomM-AGPL-3.0.txt", "romm-isotipo.svg"):
            self.assertIn(notice, cmake)
        for name in runtime:
            path = ROOT / "assets" / ("controller-tiles" if name in selected else "aurora") / (name + ".png")
            data = path.read_bytes()
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            width, height = struct.unpack(">II", data[16:24])
            size = 128 if name.startswith("platform_") or name == "emblem" else 64
            self.assertEqual((width, height), (480, 272) if name == "wallpaper" else (size, size))
            self.assertEqual(data[24], 8)
            self.assertEqual(data[28], 0)
            self.assertLessEqual(len(data), 512 * 1024)
            self.assertIn('{"' + name + '",', renderer)


if __name__ == "__main__":
    unittest.main()
