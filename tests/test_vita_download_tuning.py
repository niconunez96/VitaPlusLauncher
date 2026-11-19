"""Source-level contract for Vita download transfer tuning."""
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
BACKEND = (ROOT / "src" / "vita" / "vita_backend.c").read_text()


def assert_source_regex(test, source, pattern):
    test.assertTrue(re.search(pattern, source) is not None, pattern)


def function_source(name):
    start = BACKEND.index(name)
    end = BACKEND.index("\n}\n", start) + 2
    return BACKEND[start:end]


class VitaDownloadTuningTests(unittest.TestCase):
    def test_rom_transfer_requests_a_large_receive_buffer(self):
        transfer = function_source("bool vp_vita_transfer_with_progress")
        assert_source_regex(
            self, BACKEND, r"#define\s+VP_TRANSFER_BUFFER_SIZE\s+\(512\s*\*\s*1024\)"
        )
        assert_source_regex(
            self, transfer, r"CURLOPT_BUFFERSIZE,\s*\(long\)\s*VP_TRANSFER_BUFFER_SIZE"
        )

    def test_transfer_raises_the_socket_receive_window(self):
        transfer = function_source("bool vp_vita_transfer_with_progress")
        socket_cb = function_source("static int transfer_socket_options")
        self.assertIn("#include <sys/socket.h>", BACKEND)
        self.assertIn("CURLOPT_SOCKOPTFUNCTION, transfer_socket_options", transfer)
        assert_source_regex(
            self, socket_cb, r"setsockopt\(\(int\)fd,\s*SOL_SOCKET,\s*SO_RCVBUF,"
        )
        self.assertIn("VP_TRANSFER_BUFFER_SIZE", socket_cb)
        self.assertIn("return CURL_SOCKOPT_OK;", socket_cb)

    def test_progress_callback_is_throttled_without_losing_cancellation(self):
        progress = function_source("static int transfer_progress")
        assert_source_regex(
            self, BACKEND, r"#define\s+VP_TRANSFER_PROGRESS_INTERVAL_US\s+\(100\s*\*\s*1000ULL\)"
        )
        self.assertIn("sceKernelGetSystemTimeWide()", progress)
        self.assertIn("!progress->reported", progress)
        self.assertIn("last_report_us", progress)
        assert_source_regex(self, progress, r"return\s+progress->callback\(")


if __name__ == "__main__":
    unittest.main()
