package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestImageReturnsCenterCroppedPNG(t *testing.T) {
	t.Parallel()

	handler := NewHandler(Config{})
	request := httptest.NewRequest(http.MethodPost, "/image?width=100&height=100", sourcePNG(t, 400, 200))
	response := httptest.NewRecorder()

	handler.ServeHTTP(response, request)

	if response.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d; body = %s", response.Code, http.StatusOK, response.Body.String())
	}
	if contentType := response.Header().Get("Content-Type"); contentType != "image/png" {
		t.Fatalf("Content-Type = %q, want image/png", contentType)
	}
	decoded, err := png.Decode(response.Body)
	if err != nil {
		t.Fatalf("decode response: %v", err)
	}
	if bounds := decoded.Bounds(); bounds.Dx() != 100 || bounds.Dy() != 100 {
		t.Fatalf("dimensions = %dx%d, want 100x100", bounds.Dx(), bounds.Dy())
	}
}

func TestImageRejectsNonPOSTRequests(t *testing.T) {
	t.Parallel()

	handler := NewHandler(Config{})
	request := httptest.NewRequest(http.MethodGet, "/image?width=100&height=100", nil)
	response := httptest.NewRecorder()

	handler.ServeHTTP(response, request)

	if response.Code != http.StatusMethodNotAllowed {
		t.Fatalf("status = %d, want %d", response.Code, http.StatusMethodNotAllowed)
	}
}

func sourcePNG(t *testing.T, width, height int) *bytes.Reader {
	t.Helper()
	source := image.NewRGBA(image.Rect(0, 0, width, height))
	for x := 0; x < width; x++ {
		for y := 0; y < height; y++ {
			source.Set(x, y, color.RGBA{R: uint8(x), G: uint8(y), B: 100, A: 255})
		}
	}
	var contents bytes.Buffer
	err := png.Encode(&contents, source)
	if err != nil {
		t.Fatalf("encode source PNG: %v", err)
	}
	return bytes.NewReader(contents.Bytes())
}
