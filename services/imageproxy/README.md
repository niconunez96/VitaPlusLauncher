# imageproxy

Tiny HTTP service that receives image bytes, center-crops them, resizes them, and returns a PNG.

It has no database, job queue, cache, framework, or remote URL fetcher. A caller can place a normal HTTP cache in front of it if needed.

## Run

```sh
go run .
```

Optional `IMAGE_PROXY_ADDR` defaults to `:8090`.

`GET /health` returns `200 OK` with `ok` and is intended for container health checks.
`GET /` returns `200 OK` with `imageproxy is running`, which is useful for checking the
public Dokploy domain before configuring VitaPlus.

## Deploy with Dokploy

Create a **Docker Compose** application in Dokploy from this repository, set its compose
file to `services/imageproxy/compose.dokploy.yml`, and set its build context to
`services/imageproxy`. In Dokploy's environment variables, define:

```text
IMAGE_PROXY_DOMAIN=images.example.com
```

Point that hostname's DNS A/AAAA record at the Dokploy server, then deploy. The compose
file joins Dokploy's external `dokploy-network`, exposes no host port, enables TLS through
Traefik, and has no persistent volume because the service is stateless.

Set VitaPlus's `image_proxy_url` to the public endpoint, including its path:

```text
image_proxy_url=https://images.example.com/image
```

## Request

```text
POST /image?width=256&height=320
Content-Type: application/octet-stream

<source image bytes>
```

The response is `image/png`, exactly `width × height`, with a center crop that preserves the source aspect ratio. PNG, JPEG, and GIF input are supported through Go's standard library. Output dimensions are limited to `1024×1024`; input bodies to `8 MiB`; and source dimensions to 16 megapixels.

The caller downloads the source image and sends its raw bytes. This keeps the service independent from RomM authentication and prevents it from becoming an SSRF proxy.
