"""Source-level contract for Vita download pause, resume and removal."""
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = (ROOT / "include" / "vitaplus.h").read_text()
CORE = (ROOT / "src" / "core" / "vitaplus.c").read_text()
MAIN = (ROOT / "src" / "vita" / "main.c").read_text()
RENDERER = (ROOT / "src" / "vita" / "ui_renderer.c").read_text()
README = (ROOT / "README.md").read_text()


def function_source(source, name):
    start = source.rindex(name)
    end = source.index("\n}\n", start) + 2
    return source[start:end]


class VitaDownloadLifecycleTests(unittest.TestCase):
    def test_core_offers_pause_retry_and_remove(self):
        self.assertIn("VP_DOWNLOAD_PAUSED", HEADER)
        self.assertRegex(HEADER, r"bool\s+vp_download_queue_pause\s*\(")
        self.assertRegex(HEADER, r"bool\s+vp_download_queue_remove\s*\(")

        pause = function_source(CORE, "bool vp_download_queue_pause")
        self.assertIn("VP_DOWNLOAD_PAUSED", pause)

        retry = function_source(CORE, "bool vp_download_queue_retry")
        self.assertIn("VP_DOWNLOAD_PAUSED", retry)

        remove = function_source(CORE, "bool vp_download_queue_remove")
        self.assertRegex(remove, r"queue->count--")

    def test_main_polls_app_events_and_pauses_active_download(self):
        self.assertIn("#include <psp2/appmgr.h>", MAIN)
        self.assertIn("sceAppMgrReceiveSystemEvent", MAIN)
        self.assertIn("SCE_APPMGR_SYSTEMEVENT_ON_DEACTIVATE", MAIN)
        self.assertIn("SCE_APPMGR_SYSTEMEVENT_REQUEST_QUIT", MAIN)
        self.assertIn("pause_requested", MAIN)

        progress = function_source(MAIN, "static bool download_progress")
        self.assertRegex(progress, r"canceled\s*=.*cancel_requested\s*\|\|\s*[^\n]*pause_requested")

        pause_active = function_source(MAIN, "static bool pause_active_download")
        self.assertIn("vp_download_queue_pause", pause_active)

        poll = function_source(MAIN, "static void background_job_poll")
        self.assertIn("paused", poll)
        self.assertIn("return;", poll)

    def test_ui_exposes_resume_and_delete(self):
        self.assertIn("SCE_CTRL_SQUARE", MAIN)
        self.assertIn("vp_download_queue_remove", MAIN)
        self.assertIn("Pausada", RENDERER)
        self.assertRegex(RENDERER, r"Cuadrado\s+borra")
        self.assertIn("Cuadrado", README)


if __name__ == "__main__":
    unittest.main()
