"""Source-level lifecycle guards; these do not emulate the Vita GPU or PNG decoder."""

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/vita/main.c").read_text()


def function_body(signature):
    start = SOURCE.index(signature)
    return SOURCE[start : SOURCE.index("\n}", start) + 2]


class VitaRenderLifecycleTests(unittest.TestCase):
    def test_frame_waits_before_preparation_and_pool_reuse(self):
        body = function_body("int main(void)")
        loop = body[body.index("for (;;) {") : body.index("\n    vp_ui_shutdown();")]
        calls = [
            "vita2d_wait_rendering_done();",
            "prepare_shell(&frame, font, &app, pressed);",
            "vita2d_start_drawing();",
            "vita2d_clear_screen();",
            "vp_ui_render(&frame);",
            "vita2d_end_drawing();",
            "vita2d_swap_buffers();",
        ]
        # Ignore the existing dialog-only swap: normal UI frames follow this order.
        normal_frame = loop[loop.index("vita2d_wait_rendering_done();") :]
        positions = [normal_frame.index(call) for call in calls]
        self.assertEqual(positions, sorted(positions))

    def test_texture_preparation_does_not_draw(self):
        body = function_body("static void prepare_shell(")
        self.assertIn("covers_step(app, cover_indexes, cover_count);", body)
        for call in ("vita2d_start_drawing(", "vita2d_clear_screen(", "vp_ui_render("):
            self.assertNotIn(call, body)

    def test_background_texture_is_resolved_after_slot_eviction(self):
        body = function_body("static void prepare_shell(")
        prepare = body.index("covers_step(app, cover_indexes, cover_count);")
        resolve = body.index("background_texture = cover_texture_for(")
        self.assertLess(prepare, resolve)
        self.assertIn("frame->background = background_texture;", body)

    def test_cover_diagnostics_bracket_decode_and_close_the_log(self):
        body = function_body("static vita2d_texture *load_texture_file(")
        start = body.index('record_cover_event("load-start", id, path);')
        decode = body.index("vita2d_load_PNG_file(path)")
        done = body.index('record_cover_event(texture != NULL ? "load-ok" : "load-failed"')
        self.assertLess(start, decode)
        self.assertLess(decode, done)
        for event in ("load-open-failed", "load-size-rejected", "load-header-rejected"):
            self.assertIn(event, body)
        log = function_body("static void record_cover_event(")
        self.assertIn("fclose(log);", log)
        self.assertIn("64 * 1024", log)
        self.assertNotIn("cover_url", log)


if __name__ == "__main__":
    unittest.main()
