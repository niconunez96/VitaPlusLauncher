# VitaPlus

VitaPlus es una shell/launcher homebrew para PS Vita/PSTV: reúne biblioteca local, catálogo personal de RomM y tiendas de homebrew en una interfaz navegable con temas y vistas intercambiables.

> Estado: las fases 1–3 están implementadas y probadas en el núcleo portable; el frontend Vita2D muestra la shell inicial. Falta compilar y conectar los adaptadores Vita de HTTP/instalación en hardware porque VitaSDK no está instalado en este entorno.

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
src/vita/                frontend Vita2D inicial
themes/default/          tema base editable
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

## Configuración en Vita

```text
ux0:data/vitaplus/config.ini
ux0:data/vitaplus/themes/<tema>/theme.ini
ux0:data/vitaplus/cache/
ux0:data/vitaplus/cache/covers/
ux0:data/vitaplus/downloads/<plataforma>/
```

Las carátulas se obtienen directamente desde Libretro Thumbnails y se activan automáticamente.
Antes de decodificarlas, VitaPlus acepta sólo PNG RGB/RGBA de 8 bits, sin interlace, de hasta
`512×512` píxeles y `512 KiB`; una imagen ausente o rechazada conserva la navegación sin artwork.
Si una instalación anterior tiene artwork problemático, borrá `ux0:data/vitaplus/cache/covers/`.

En **Ajustes**, configurá la `URL` y presioná `X` en `Pair code` para introducir el código corto que genera **Pair Device** en RomM. `Triángulo` canjea el código una sola vez, guarda internamente el token recibido y sincroniza el catálogo. El código no se persiste ni se vuelve a mostrar; el token nunca se muestra en pantalla.

Después de una sincronización correcta, VitaPlus guarda una caché local del catálogo y la muestra al abrir la app, sin esperar a la red. En **Ajustes → RomM**, seleccioná **Sincronizar ahora** y presioná `X` cuando quieras actualizarla manualmente.

Usá la URL base de tu servidor (por ejemplo, `http://192.168.1.100:8080`) y creá en RomM un Client API Token de permisos mínimos de lectura/descarga antes de generar el pair code. Nunca compartas `config.ini`: contiene el token recibido.

## Descargas y lanzamiento

Cada ROM de RomM muestra un ícono de descarga (flecha) o un check blanco cuando ya está en `ux0:data/vitaplus/downloads/<plataforma>/`. Presioná `X` sobre una ROM sin descargar para bajarla en segundo plano; podés seguir navegando mientras termina y luego `X` la abre con el emulador configurado.

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
