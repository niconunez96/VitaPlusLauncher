# Providers de VitaPlus

## Contrato común

Un provider obtiene datos remotos y los normaliza a `VpItem`. El núcleo nunca ejecuta HTTP por sí mismo: recibe un `VpFetchBackend`, por lo que Vita2D/`libcurl`, tests y futuras alternativas comparten el mismo contrato.

## RomM

`vp_romm_sync()` solicita:

```text
GET {romm_url}/api/roms?limit=1&offset=<n>
Authorization: Bearer <token>
```

La respuesta debe incluir un `id` y `name`/`title`. Si están presentes, VitaPlus conserva `platform_slug`, `url_cover` y `download_url`; URLs relativas se convierten a absolutas contra `romm_url`.

La UI debe pedir confirmación antes de descargar y seleccionar una ruta de destino configurada por plataforma. El token debe tener sólo permisos de lectura/descarga requeridos por el cliente. VitaPlus lo envía como `Authorization: Bearer` para páginas de catálogo y recursos RomM del mismo origen; nunca lo adjunta a URLs externas de artwork o descargas. El catálogo se pagina de a un ítem para respetar el buffer fijo del core y se detiene al llegar a `total` o `VP_MAX_ITEMS`. Respuestas HTTP fuera de `2xx` hacen fallar el backend.

## Homebrew

`vp_homebrew_sync()` descarga un catálogo JSON estático. El formato mínimo por entrada es:

```json
{
  "id": "vitaftp",
  "name": "Vita FTP",
  "description": "FTP server",
  "url": "https://host.example/VitaFTP.vpk"
}
```

Es compatible con una capa adaptadora para catálogos como NeoVitaDB o VitaHomebrewDB. La UI debe mostrar el origen, versión/hash cuando exista y un aviso de que el software de terceros no fue auditado por VitaPlus.

## Instalación

El core diferencia `DOWNLOAD` de `INSTALL_VPK`. El backend Vita final deberá:

1. Descargar el VPK bajo `ux0:data/vitaplus/downloads/`.
2. Verificar tamaño/hash si el catálogo lo provee.
3. Solicitar confirmación.
4. Invocar el instalador Vita con privilegios homebrew.
5. Refrescar biblioteca local tras éxito.

No se instalará en segundo plano ni se sobrescribirá una app existente sin confirmación.
