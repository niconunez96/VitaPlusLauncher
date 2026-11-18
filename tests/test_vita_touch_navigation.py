"""Source-level contract for Vita front-touch navigation wiring."""
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "vita" / "main.c").read_text()
CMAKE = (ROOT / "CMakeLists.txt").read_text()


class VitaTouchNavigationTests(unittest.TestCase):
    def test_front_touch_is_started_and_read_from_the_vita_api(self):
        self.assertIn("#include <psp2/touch.h>", MAIN)
        self.assertIn("sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT", MAIN)
        self.assertIn("sceTouchPeek(SCE_TOUCH_PORT_FRONT", MAIN)
        self.assertIn("SceTouchData", MAIN)
        self.assertIn("SceTouch_stub", CMAKE)

    def test_touch_taps_share_the_existing_controller_actions(self):
        self.assertIn("static unsigned int touch_tap_pressed", MAIN)
        self.assertIn("vp_ui_touch_dock_screen", MAIN)
        self.assertIn("vp_ui_touch_tile_slot", MAIN)
        self.assertIn("vp_ui_touch_list_slot", MAIN)
        self.assertIn("SCE_CTRL_CROSS", MAIN)
        self.assertIn("SCE_CTRL_TRIANGLE", MAIN)

    def test_raw_touch_coordinates_are_normalized_to_aurora_viewport(self):
        self.assertIn("VP_TOUCH_WIDTH", MAIN)
        self.assertIn("VP_TOUCH_HEIGHT", MAIN)
        self.assertIn("* 960 / VP_TOUCH_WIDTH", MAIN)
        self.assertIn("* 544 / VP_TOUCH_HEIGHT", MAIN)


if __name__ == "__main__":
    unittest.main()
