# Providers de VitaShelf Launcher

## Contrato común

Un provider obtiene datos remotos y los normaliza a `VpItem`. El núcleo nunca ejecuta HTTP por sí mismo: recibe un `VpFetchBackend`, por lo que Vita2D/`libcurl`, tests y futuras alternativas comparten el mismo contrato.

## RomM

### Estado de conexión en Ajustes

Al iniciar y al abrir los ajustes de RomM, VitaShelf Launcher comprueba el acceso con una petición autenticada en segundo plano. Tener un token en `config.ini` no basta para mostrar la conexión como válida. También se comprueba antes de sincronizar y después de canjear un código.

| Estado | Mensaje y pair code |
| --- | --- |
| Conectado | Check de confirmación y pair code deshabilitado; una biblioteca vacía también cuenta como conexión válida. |
| Comprobando | Pair code temporalmente deshabilitado mientras se verifica el acceso. |
| Sin configurar | Advertencia para configurar URL y pair code. |
| Token rechazado (`401`) | Error indicando que el token ya no funciona y pair code habilitado. |
| Sin permisos (`403`) | Error de permisos de lectura y pair code habilitado. |
| Red, servidor o respuesta inesperada | Advertencia de que no se pudo verificar, sin borrar el token ni afirmar que es inválido; pair code habilitado. |

Para reintentar, vuelve a entrar en los ajustes de RomM o sincroniza con `X` en **Sincronizar ahora** o con `Triángulo`. Cambiar la URL invalida la comprobación anterior: los resultados pendientes del servidor anterior no se aplican a los ajustes actuales. El canje exitoso guarda el nuevo token aunque falle la sincronización posterior.

### Catálogo

`vp_romm_sync()` solicita:

```text
GET {romm_url}/api/roms?limit=1&offset=<n>
Authorization: Bearer <token>
```

La respuesta debe incluir un `id` y `name`/`title`. Si están presentes, VitaShelf Launcher conserva `platform_slug`, `platform_display_name`, `summary` (con `description` como respaldo), la carátula, `fs_name` y `download_url`; URLs relativas se convierten a absolutas contra `romm_url`. Para la carátula se prefiere `path_cover_small`, luego `path_cover_large`, luego `cover_url` y por último `url_cover`; RomM sirve esas rutas como PNG.

RomM no siempre expone `download_url` en el objeto de la ROM. Cuando falta, VitaShelf Launcher usa `fs_name` y construye `GET /api/roms/{id}/content/{fs_name}`. Ese camino también define la extensión real del archivo descargado (`sfc`, `nds`, `zip`, …). El archivo se guarda en `ux0:data/vitashelflauncher/downloads/<plataforma>/<nombre>_<id>.<ext>`, donde `<nombre>` se toma del título de la ROM en RomM formateado en minúsculas y sin espacios (con fallback al nombre de archivo), y `<id>` mantiene la descarga única. Si tampoco hay extensión disponible, se usa `.rom`. La descarga escribe primero en `<ruta>.part` y renombra al terminar, para que un corte de red no marque la ROM como descargada.

La UI descarga con `X` y vuelve a comprobar el archivo al arrancar y después de cada sincronización; así el estado `Descargado` sobrevive a un catálogo nuevo. El token debe tener sólo permisos de lectura/descarga requeridos por el cliente. VitaShelf Launcher lo envía como `Authorization: Bearer` para páginas de catálogo y recursos RomM del mismo origen; nunca lo adjunta a URLs externas de artwork o descargas. El catálogo se pagina de a un ítem para respetar el buffer fijo del core (`VP_FETCH_RESPONSE_SIZE`, 256 KiB) y se detiene al llegar a `total` o `VP_MAX_ITEMS`. Un objeto RomM individual ronda los 17-30 KiB con metadatos, por lo que el buffer debe conservar holgura; si una respuesta lo desborda, el backend reporta `Respuesta demasiado grande` (y `truncated=1` en `network.log`) en lugar del confuso error de escritura de libcurl. Respuestas HTTP fuera de `2xx` hacen fallar el backend.

## Lanzamiento de ROMs

Una ROM descargada se abre con el emulador configurado para su plataforma. La UI construye una URI `psgm:play` y la entrega a `sceAppMgrLaunchAppByUri`:

```text
psgm:play?titleid=RETROVITA&param=app0:/snes9x2005_libretro.self&param2=ux0:data/vitashelflauncher/downloads/snes/supermetroid_42.sfc
psgm:play?titleid=DSVITA000&param=ux0:data/vitashelflauncher/downloads/nds/metroid_7.nds
```

El destino se define en `config.ini` con `launch_<platform>=<titleid>[:<core>]`. Si se omite el core, la URI manda sólo `param`; es el formato que usan emuladores que abren la ROM sin seleccionar un core de RetroArch. Las plataformas sin perfil muestran un aviso y no intentan lanzar nada.

El importador marca `multi_file` cuando RomM reporta `has_multiple_files: true`. Esas ROMs quedan como remotas y no se descargan desde la UI porque el endpoint entrega un ZIP multiarchivo; se resuelven como juego local con el `.cue`/`.m3u` correspondiente.

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

Es compatible con una capa adaptadora para catálogos como NeoVitaDB o VitaHomebrewDB. La UI debe mostrar el origen, versión/hash cuando exista y un aviso de que el software de terceros no fue auditado por VitaShelf Launcher.

## Instalación

El core diferencia `DOWNLOAD` de `INSTALL_VPK`. El backend Vita final deberá:

1. Descargar el VPK bajo `ux0:data/vitashelflauncher/downloads/`.
2. Verificar tamaño/hash si el catálogo lo provee.
3. Solicitar confirmación.
4. Invocar el instalador Vita con privilegios homebrew.
5. Refrescar biblioteca local tras éxito.

No se instalará en segundo plano ni se sobrescribirá una app existente sin confirmación.
