# VitaPlus

VitaPlus es una shell/launcher homebrew para PS Vita/PSTV: reúne biblioteca local, catálogo personal de RomM y tiendas de homebrew en una interfaz navegable con temas y vistas intercambiables.

> Estado: núcleo portable y frontend Vita2D implementados. La interfaz Aurora incluye assets originales, navegación por consola y descargas en segundo plano. Compilación VPK con VitaSDK/Docker; las comprobaciones visuales en hardware siguen siendo necesarias.

## Alcance

- **Fase 1 — Shell:** inicio, biblioteca local, favoritos, recientes, vistas `grid`/`list`/`carousel` y temas `.ini`.
- **Fase 2 — RomM:** proveedor basado en `GET /api/roms`, búsqueda, artwork remoto, carátulas/fondo y cola de descargas.
- **Biblioteca:** agrupada por consola (`nds`, `snes`, `psp`, …) con navegación por las cuatro flechas; al entrar a una consola se listan sólo sus ROMs con carátula, fondo y `summary`.
- **Tiendas:** sección reservada; por ahora muestra sólo un placeholder `Proximamente`.
- **Fase 3 — Homebrew:** catálogo estático compatible con NeoVitaDB/VitaHomebrewDB, búsqueda, detalle, descarga e instalación delegada de VPK.

La tienda oficial de PlayStation queda como acceso delegado a la aplicación oficial: VitaPlus no reproduce, raspa ni automatiza compras/autenticación de Sony.

## Árbol

```text
include/                 API portable del núcleo
src/core/                modelo, catálogo, temas, providers y transferencias
src/vita/                frontend Vita2D y presentación Aurora
themes/default/          tema base editable
assets/aurora/           iconos y fondo originales generados con FLUX
tests/                   pruebas host con gcc
docs/                    arquitectura y contrato de providers
```

## Probar el núcleo en host

```sh
make test
```

No necesita VitaSDK ni red real: las pruebas usan un backend de transferencia falso.

## Compilar para Vita

Con Docker (recomendado, no hace falta instalar VitaSDK en el host):

```sh
./scripts/build-vpk.sh
```

El VPK queda en `dist/vitaplus.vpk`. Opciones: `--clean` borra el build anterior, `--build-type Debug` cambia el tipo de build y `--image <tag>` usa otra imagen de VitaSDK (`VITASDK_IMAGE` también sirve).

Con VitaSDK local, instalar las dependencias `libvita2d`, `curl`, `zlib`, `libpng` y `libjpeg`; luego:

```sh
cmake -S . -B build/vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build/vita
```

El VPK queda en `build/vita/vitaplus.vpk`.

La interfaz usa la fuente **PGF del sistema**, en la pantalla nativa de 960×544.
Los archivos Noto Sans conservados en `assets/fonts/` no se cargan ni se incluyen en el VPK.

## Interfaz Aurora

Aurora mantiene la estructura núcleo/proveedores/frontend y los controles existentes:
flechas para navegar, `X` para abrir/descargar/jugar, círculo para volver, `L/R` para
cambiar de sección, triángulo para cambiar de vista y cuadrado para favoritos.
En Ajustes, triángulo conserva la acción de canjear y guardar el código de RomM.

- Biblioteca con iconos de controles por consola, colores propios y foco animado.
- Vistas de juegos `grid`, `list` y `carousel`, fondo con artwork y carátulas de RomM.
- Dock de secciones, ajustes visuales, progreso de descargas y avisos temporales.
- 36 PNG originales de FLUX incluidos; funcionan sin conexión y tienen fallback si falta un archivo.

Los temas anteriores siguen usando sus cinco colores y su vista inicial. Las opciones
adicionales son `ui_motion_ms` (0–600, 0 desactiva el movimiento), `ui_radius` (6–20)
y `ui_wallpaper_alpha` (0–255). Los valores inválidos se ignoran. `card_width` y
`card_height` se conservan por compatibilidad; la grilla usa geometría fija de 4×2
para mantener la navegación. No se agregaron funciones de búsqueda ni una tienda funcional.

Preparación de assets, pruebas y verificación visual: [docs/aurora-ui.md](docs/aurora-ui.md).

## Configuración en Vita

```text
ux0:data/vitaplus/config.ini
ux0:data/vitaplus/themes/<tema>/theme.ini
ux0:data/vitaplus/cache/
ux0:data/vitaplus/cache/covers/
ux0:data/vitaplus/downloads/<plataforma>/
```

Las carátulas se descargan desde RomM, se envían como bytes al servicio de imágenes y se activan automáticamente.
El servicio queda en el mismo host que RomM por defecto (`<romm_url>/image`) o podés establecer
`image_proxy_url` en `config.ini` si usás otro puerto o proxy inverso.
Antes de decodificarlas, VitaPlus acepta sólo PNG RGB/RGBA de 8 bits, sin interlace, de hasta
`512×512` píxeles y `512 KiB`; una imagen ausente o rechazada conserva la navegación sin artwork.
Si una instalación anterior tiene artwork problemático, borrá `ux0:data/vitaplus/cache/covers/`.

En **Ajustes**, configurá la `URL` y presioná `X` en `Pair code` para introducir el código corto que genera **Pair Device** en RomM. `Triángulo` canjea el código una sola vez, guarda internamente el token recibido y sincroniza el catálogo. El código no se persiste ni se vuelve a mostrar; el token nunca se muestra en pantalla.

Después de una sincronización correcta, VitaPlus guarda una caché local del catálogo y la muestra al abrir la app, sin esperar a la red. En **Ajustes → RomM**, seleccioná **Sincronizar ahora** y presioná `X` cuando quieras actualizarla manualmente. **Sincronizar metadata** recorre las carátulas de RomM en segundo plano, las transforma con el servicio de imágenes y las guarda en la caché.

Usá la URL base de tu servidor (por ejemplo, `http://192.168.1.100:8080`) y creá en RomM un Client API Token de permisos mínimos de lectura/descarga antes de generar el pair code. Nunca compartas `config.ini`: contiene el token recibido.

## Descargas y lanzamiento

Cada ROM de RomM muestra un ícono de descarga (flecha) o un check blanco cuando ya está en `ux0:data/vitaplus/downloads/<plataforma>/`. Presioná `X` sobre una ROM sin descargar para agregarla a la cola en segundo plano; podés seguir navegando mientras termina y luego `X` la abre con el emulador configurado.

En **Ajustes → Descargas** se ve cada elemento con su estado y porcentaje. Con `X` cancelás una descarga pendiente o activa; sobre una descarga fallida o cancelada, `X` la vuelve a encolar. La cola admite hasta 16 ROMs y procesa una por vez para no bloquear la interfaz.

El nombre del archivo conserva la extensión real que reporta RomM (`fs_name` o la URL de contenido): `sfc`, `nds`, `zip`, etc. La descarga usa `<archivo>.part` y renombra al final, así que un corte de red no deja una ROM marcada como lista. Las sincronizaciones, descargas e instalaciones se ejecutan en segundo plano; el estado se vuelve a comprobar al arrancar y después de sincronizar RomM.

Las ROMs que RomM marca con varios archivos (`has_multiple_files`, por ejemplo un disco PSX con `.cue` y `.bin`) no se descargan desde la UI: el endpoint devuelve un ZIP que el emulador no puede abrir directamente. Descargalas desde RomM, extraelas y agregá el `.cue`/`.m3u` como juego local.

El lanzamiento se configura por plataforma en `ux0:data/vitaplus/config.ini`:

```text
launch_snes=RETROVITA:app0:/snes9x2005_libretro.self
launch_nds=DSVITA000:
launch_psp=VSCU00001:
```

El formato es `launch_<platform>=<titleid>[:<core>]`. El `titleid` es el de la aplicación (por ejemplo, `RETROVITA` para RetroArch); el `core` opcional es el archivo `.self` que RetroArch debe cargar. Si omitís el core, VitaPlus manda sólo la ROM, útil para emuladores como DSVita. Para RetroArch, instalá los cores que uses en `app0:`.

Perfiles por defecto (todos con `titleid=RETROVITA`): `snes`→`snes9x2005`, `nes`/`famicom`→`fceumm`, `gba`→`gpsp`, `gb`/`gbc`→`gambatte`, `genesis`/`mega-drive`/`megadrive`/`mastersystem`/`sms`/`gamegear`→`genesis_plus_gx`, `psx`/`playstation`→`pcsx_rearmed`, `arcade`/`neogeo`→`fbneo`, `pce`/`pc-engine`/`turbografx-16`→`mednafen_pce_fast`, `atari2600`→`stella2014`, `wonderswan`→`mednafen_wswan`. `nds` y `psp` no tienen core de RetroArch en Vita: configurá su `titleid` (`DSVITA000`, `VSCU00001`, …) o la ROM mostrará un aviso.
