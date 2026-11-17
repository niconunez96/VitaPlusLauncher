package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestPrepare(t *testing.T) {
	for _, key := range []bool{true, false} {
		t.Run(map[bool]string{true: "keyed icon", false: "opaque wallpaper"}[key], func(t *testing.T) {
			dir := t.TempDir()
			src, dst := filepath.Join(dir, "source.png"), filepath.Join(dir, "out.png")
			im := image.NewNRGBA(image.Rect(0, 0, 32, 32))
			for y := 0; y < 32; y++ {
				for x := 0; x < 32; x++ {
					c := color.NRGBA{230, 70, 160, 255}
					if x >= 8 && x < 24 && y >= 8 && y < 24 {
						c = color.NRGBA{20, 180, 140, 255}
					}
					im.SetNRGBA(x, y, c)
				}
			}
			f, err := os.Create(src)
			if err != nil {
				t.Fatal(err)
			}
			if err := png.Encode(f, im); err != nil {
				t.Fatal(err)
			}
			f.Close()
			args := []string{"-in", src, "-out", dst, "-width", "16", "-height", "16"}
			if key {
				args = append(args, "-key")
			}
			if err := run(args); err != nil {
				t.Fatal(err)
			}
			f, err = os.Open(dst)
			if err != nil {
				t.Fatal(err)
			}
			defer f.Close()
			out, err := png.Decode(f)
			if err != nil {
				t.Fatal(err)
			}
			if out.Bounds().Dx() != 16 || out.Bounds().Dy() != 16 {
				t.Fatal(out.Bounds())
			}
			_, _, _, a := out.At(0, 0).RGBA()
			if key && a != 0 {
				t.Fatal("keyed background remains opaque")
			}
			if !key && a != 65535 {
				t.Fatal("wallpaper changed alpha")
			}
			r, g, b, a := out.At(8, 8).RGBA()
			if g <= r || g <= b || a != 65535 {
				t.Fatal("foreground color/alpha lost")
			}
		})
	}
}

func TestBundledAssets(t *testing.T) {
	dir := filepath.Join("..", "..", "assets", "aurora")
	manifest, err := os.ReadFile(filepath.Join(dir, "provenance.tsv"))
	if err != nil {
		t.Fatal(err)
	}
	seen := map[string]bool{}
	for _, line := range strings.Split(string(manifest), "\n") {
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		fields := strings.Split(line, "\t")
		if len(fields) != 3 || seen[fields[0]] {
			t.Fatalf("invalid provenance: %s", line)
		}
		seen[fields[0]] = true
		data, err := os.ReadFile(filepath.Join(dir, fields[0]+".png"))
		if err != nil {
			t.Fatal(err)
		}
		if len(data) < 29 || len(data) > 512*1024 || data[24] != 8 || (data[25] != 2 && data[25] != 6) || data[28] != 0 {
			t.Fatalf("unsafe PNG %s", fields[0])
		}
		im, err := png.Decode(bytes.NewReader(data))
		if err != nil {
			t.Fatal(err)
		}
		w, h := 128, 128
		if strings.HasPrefix(fields[0], "ui_") {
			w, h = 64, 64
		}
		if fields[0] == "wallpaper" {
			w, h = 480, 272
		}
		if im.Bounds().Dx() != w || im.Bounds().Dy() != h {
			t.Fatalf("wrong size: %s", fields[0])
		}
		if fields[0] != "wallpaper" {
			_, _, _, a := im.At(0, 0).RGBA()
			if a != 0 {
				t.Fatalf("background not keyed: %s", fields[0])
			}
		}
	}
	if len(seen) != 36 {
		t.Fatalf("expected 36 assets, got %d", len(seen))
	}
	files, err := filepath.Glob(filepath.Join(dir, "*.png"))
	if err != nil || len(files) != 36 {
		t.Fatal("unexpected asset inventory")
	}
}

func TestRejectDoesNotOverwrite(t *testing.T) {
	dir := t.TempDir()
	dst := filepath.Join(dir, "keep.png")
	before := []byte("existing asset")
	if err := os.WriteFile(dst, before, 0600); err != nil {
		t.Fatal(err)
	}
	for _, args := range [][]string{
		{"-in", "missing", "-out", dst},
		{"-in", "missing", "-out", dst, "-width", "0"},
	} {
		if run(args) == nil {
			t.Fatal("invalid command succeeded")
		}
		after, err := os.ReadFile(dst)
		if err != nil || !bytes.Equal(before, after) {
			t.Fatal("rejected input overwrote output")
		}
	}
}
