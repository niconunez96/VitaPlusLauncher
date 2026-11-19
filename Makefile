CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
CORE = src/core/vitaplus.c
TEST = build/test_core

.PHONY: test test-ui test-vita-lifecycle test-assets test-renderer clean

test: $(TEST) test-ui test-vita-lifecycle
	$(TEST)

test-ui: build/test_presentation
	build/test_presentation
	python3 tests/test_ui_assets.py

test-vita-lifecycle:
	python3 tests/test_vita_render_lifecycle.py
	python3 tests/test_vita_audio.py
	python3 tests/test_vita_download_tuning.py
	python3 tests/test_vita_thread_priority.py
	python3 tests/test_romm_worker.py

build/test_presentation: src/vita/ui_presentation.c src/vita/ui_presentation.h tests/test_presentation.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc/vita src/vita/ui_presentation.c tests/test_presentation.c -o $@

test-assets:
	GO111MODULE=off go test ./scripts/assetprep

# Optional host layout smoke test; requires Cairo and pkg-config, not VitaSDK.
test-renderer: build/test_renderer
	build/test_renderer

build/test_renderer: src/vita/ui_renderer.c src/vita/ui_renderer.h src/vita/ui_presentation.c src/vita/ui_presentation.h src/core/vitaplus.c tests/preview_renderer.c tests/ui_stubs/vita2d.h tests/ui_stubs/psp2/io/fcntl.h tests/ui_stubs/psp2/kernel/processmgr.h include/vitaplus.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DVP_UI_TESTING -Isrc/vita -Itests/ui_stubs $$(pkg-config --cflags cairo) src/core/vitaplus.c src/vita/ui_presentation.c src/vita/ui_renderer.c tests/preview_renderer.c $$(pkg-config --libs cairo) -lm -o $@

$(TEST): $(CORE) tests/test_core.c include/vitaplus.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(CORE) tests/test_core.c -o $(TEST)

clean:
	rm -rf build
