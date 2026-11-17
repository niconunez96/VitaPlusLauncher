// assetprep converts generated artwork into bounded, non-interlaced Vita PNGs.
package main

import (
	"errors"
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/draw"
	_ "image/jpeg"
	"image/png"
	"io"
	"os"
	"path/filepath"
)

func run(args []string) error {
	fs := flag.NewFlagSet("assetprep", flag.ContinueOnError)
	in := fs.String("in", "", "source PNG or JPEG")
	out := fs.String("out", "", "destination PNG")
	w := fs.Int("width", 128, "output width (1..512)")
	h := fs.Int("height", 128, "output height (1..512)")
	key := fs.Bool("key", false, "remove border-connected magenta and fit the icon")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *in == "" || *out == "" || *w < 1 || *h < 1 || *w > 512 || *h > 512 || fs.NArg() != 0 {
		return errors.New("provide -in, -out and dimensions from 1 to 512")
	}
	f, err := os.Open(*in)
	if err != nil {
		return err
	}
	defer f.Close()
	cfg, _, err := image.DecodeConfig(io.LimitReader(f, 32<<20))
	if err != nil {
		return err
	}
	if cfg.Width < 1 || cfg.Height < 1 || cfg.Width > 8192 || cfg.Height > 8192 || cfg.Width*cfg.Height > 16<<20 {
		return errors.New("source exceeds 16 megapixels")
	}
	if _, err = f.Seek(0, 0); err != nil {
		return err
	}
	source, _, err := image.Decode(io.LimitReader(f, 32<<20))
	if err != nil {
		return err
	}
	b := source.Bounds()
	im := image.NewNRGBA(image.Rect(0, 0, b.Dx(), b.Dy()))
	draw.Draw(im, im.Bounds(), source, b.Min, draw.Src)
	b = im.Bounds()
	if *key {
		// Flood from the border: magenta face buttons inside a controller survive.
		seen := make([]bool, b.Dx()*b.Dy())
		queue := make([]image.Point, 0, len(seen))
		push := func(x, y int) {
			if !image.Pt(x, y).In(b) {
				return
			}
			i := y*b.Dx() + x
			if seen[i] {
				return
			}
			seen[i] = true
			c := im.NRGBAAt(x, y)
			if int(c.R)-int(c.G) > 30 && int(c.B)-int(c.G) > 20 && c.R > 90 && c.B > 80 {
				queue = append(queue, image.Pt(x, y))
				im.SetNRGBA(x, y, color.NRGBA{})
			}
		}
		for x := 0; x < b.Dx(); x++ {
			push(x, 0)
			push(x, b.Dy()-1)
		}
		for y := 0; y < b.Dy(); y++ {
			push(0, y)
			push(b.Dx()-1, y)
		}
		for i := 0; i < len(queue); i++ {
			p := queue[i]
			push(p.X-1, p.Y)
			push(p.X+1, p.Y)
			push(p.X, p.Y-1)
			push(p.X, p.Y+1)
		}
		crop := image.Rectangle{}
		for y := 0; y < b.Dy(); y++ {
			for x := 0; x < b.Dx(); x++ {
				if im.NRGBAAt(x, y).A != 0 {
					crop = crop.Union(image.Rect(x, y, x+1, y+1))
				}
			}
		}
		if crop.Empty() {
			return errors.New("key removed entire image")
		}
		b = crop
	}
	result := image.NewNRGBA(image.Rect(0, 0, *w, *h))
	rw, rh := *w, *h
	if *key {
		rw, rh = *w*88/100, *h*88/100
		if rw < 1 {
			rw = 1
		}
		if rh < 1 {
			rh = 1
		}
		if b.Dx()*rh > b.Dy()*rw {
			rh = max(1, b.Dy()*rw/b.Dx())
		} else {
			rw = max(1, b.Dx()*rh/b.Dy())
		}
	}
	ox, oy := (*w-rw)/2, (*h-rh)/2
	// Area averaging premultiplied channels prevents dark fringes at alpha edges.
	for y := 0; y < rh; y++ {
		for x := 0; x < rw; x++ {
			x0, x1 := b.Min.X+x*b.Dx()/rw, b.Min.X+(x+1)*b.Dx()/rw
			y0, y1 := b.Min.Y+y*b.Dy()/rh, b.Min.Y+(y+1)*b.Dy()/rh
			x1 = max(x0+1, x1)
			y1 = max(y0+1, y1)
			var r, g, bl, a, n uint64
			for sy := y0; sy < y1; sy++ {
				for sx := x0; sx < x1; sx++ {
					c := im.NRGBAAt(sx, sy)
					alpha := uint64(c.A)
					r += uint64(c.R) * alpha
					g += uint64(c.G) * alpha
					bl += uint64(c.B) * alpha
					a += alpha
					n++
				}
			}
			if a > 0 {
				result.SetNRGBA(x+ox, y+oy, color.NRGBA{uint8(r / a), uint8(g / a), uint8(bl / a), uint8(a / n)})
			}
		}
	}
	// Complete conversion before replacing anything, including an existing output.
	tmp, err := os.CreateTemp(filepath.Dir(*out), ".asset-*.png")
	if err != nil {
		return err
	}
	name := tmp.Name()
	defer os.Remove(name)
	err = png.Encode(tmp, result)
	closeErr := tmp.Close()
	if err != nil {
		return err
	}
	if closeErr != nil {
		return closeErr
	}
	if err = os.Chmod(name, 0644); err != nil {
		return err
	}
	return os.Rename(name, *out)
}

func main() {
	if err := run(os.Args[1:]); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
