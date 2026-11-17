# imageproxy

Tiny HTTP service that receives image bytes, center-crops them, resizes them, and returns a PNG.

It has no database, job queue, cache, framework, or remote URL fetcher. A caller can place a normal HTTP cache in front of it if needed.

## Run

```sh
go run .
```

Optional `IMAGE_PROXY_ADDR` defaults to `:8090`.

## Request

```text
POST /image?width=256&height=320
Content-Type: application/octet-stream

<source image bytes>
```

The response is `image/png`, exactly `width × height`, with a center crop that preserves the source aspect ratio. PNG, JPEG, and GIF input are supported through Go's standard library. Output dimensions are limited to `1024×1024`; input bodies to `8 MiB`; and source dimensions to 16 megapixels.

The caller downloads the source image and sends its raw bytes. This keeps the service independent from RomM authentication and prevents it from becoming an SSRF proxy.
