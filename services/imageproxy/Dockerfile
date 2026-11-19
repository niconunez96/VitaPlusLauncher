FROM golang:1.26-alpine AS build

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . ./
RUN CGO_ENABLED=0 go build -trimpath -ldflags='-s -w' -o /out/imageproxy .

FROM alpine:3.22

RUN addgroup -S imageproxy && adduser -S -G imageproxy imageproxy
COPY --from=build /out/imageproxy /usr/local/bin/imageproxy

USER imageproxy
EXPOSE 8090

ENTRYPOINT ["/usr/local/bin/imageproxy"]
