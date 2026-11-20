# VitaShelf Launcher

VitaShelf Launcher es una shell/launcher homebrew para PS Vita/PSTV: reúne biblioteca local, catálogo personal de RomM y tiendas de homebrew en una interfaz navegable con temas y vistas intercambiables.

> Estado: núcleo portable y frontend Vita2D implementados. La interfaz Blueglass incorpora Inter, logos de plataformas, fondos por consola y descargas en segundo plano. Compilación VPK con VitaSDK/Docker; las comprobaciones visuales y de rendimiento en hardware siguen siendo necesarias.

## Alcance

- **Fase 1 — Shell:** inicio, biblioteca local, favoritos, recientes, vistas `grid`/`list`/`carousel` y temas `.ini`.
- **Fase 2 — RomM:** proveedor basado en `GET /api/roms`, búsqueda, artwork remoto, carátulas/fondo y cola de descargas.
- **Biblioteca:** agrupada por consola (`nds`, `snes`, `psp`, …) con navegación por las cuatro flechas; al entrar a una consola se listan sólo sus ROMs con carátula, fondo y `summary`.
- **Tiendas:** sección reservada; por ahora muestra sólo un placeholder `Proximamente`.
- **Fase 3 — Homebrew:** catálogo estático compatible con NeoVitaDB/VitaHomebrewDB, búsqueda, detalle, descarga e instalación delegada de VPK.

La tienda oficial de PlayStation queda como acceso delegado a la aplicación oficial: VitaShelf Launcher no reproduce, raspa ni automatiza compras/autenticación de Sony.

## Árbol

```text
include/                 API portable del núcleo
src/core/                modelo, catálogo, temas, providers y transferencias
src/vita/                frontend Vita2D y presentación Blueglass
themes/default/          tema base editable
assets/blueglass/        logos, fondos, Inter, símbolos gráficos y atribución del tema activo
assets/aurora/           artwork de la presentación anterior
assets/legacy/           inventario original y provenance.tsv histórico
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

El VPK queda en `dist/vitashelflauncher.vpk`. Opciones: `--clean` borra el build anterior, `--build-type Debug` cambia el tipo de build y `--image <tag>` usa otra imagen de VitaSDK (`VITASDK_IMAGE` también sirve).

Con VitaSDK local, instalar las dependencias `libvita2d`, `curl`, `zlib`, `libpng` y `libjpeg`; luego:

```sh
cmake -S . -B build/vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build/vita
```

El VPK queda en `build/vita/vitashelflauncher.vpk`.

La interfaz usa **Inter** en pesos 400, 500, 600 y 700, en la pantalla nativa de 960×544.

## Interfaz Blueglass

Blueglass mantiene la estructura núcleo/proveedores/frontend y los controles existentes:
flechas para navegar, `X` para abrir/descargar/jugar, círculo para volver, `L/R` para
cambiar de sección, triángulo para cambiar de vista y cuadrado para favoritos. La pantalla
táctil frontal permite tocar las tarjetas, filas, dock, selector de vista y el área de volver:
el primer toque selecciona y el segundo confirma acciones potencialmente costosas (descargar,
jugar o instalar).
En Ajustes, triángulo conserva la acción de canjear y guardar el código de RomM.

- Biblioteca con carrusel horizontal de logos originales y fondo de la consola seleccionada.
- Carrusel de juegos con portada central, lista de cinco filas con detalles y mosaico de 5×2 carátulas.
- Navegación superior, fecha/hora/batería reales y barra inferior de botones gráficos con ayudas contextuales.
- 25 identidades de plataforma, 26 fondos y símbolos de interfaz disponibles sin conexión; fuentes y atribuciones incluidas.
- Tipografía compartida con tamaños de 14–30 px y truncado UTF-8; foco azul luminoso y superficies translúcidas.
- Ajustes: General, Interfaz, Descargas, Conexión (RomM), Almacenamiento y Acerca de. General e Interfaz muestran información y la configuración actual; RomM y Descargas conservan sus acciones.

Los temas conservan sus colores y su vista inicial. `ui_motion_ms=0` desactiva el
movimiento; `ui_wallpaper_alpha` controla la opacidad del fondo. `ui_radius`,
`card_width` y `card_height` se siguen leyendo por compatibilidad; Blueglass usa
geometría fija compartida entre render y controles. La tienda continúa mostrando
su pantalla «Próximamente».

Preparación de assets, actualización del proxy, pruebas y verificación visual:
[docs/blueglass-ui.md](docs/blueglass-ui.md).

## Configuración en Vita

```text
ux0:data/vitashelflauncher/config.ini
ux0:data/vitashelflauncher/themes/<tema>/theme.ini
ux0:data/vitashelflauncher/cache/
ux0:data/vitashelflauncher/cache/covers/
ux0:data/vitashelflauncher/downloads/<plataforma>/
```

Las carátulas se descargan desde RomM, se envían como bytes al servicio de imágenes y se activan automáticamente.
El servicio queda en el mismo host que RomM por defecto (`<romm_url>/image`) o podés establecer
`image_proxy_url` en `config.ini` si usás otro puerto o proxy inverso.
Antes de decodificarlas, VitaShelf Launcher acepta sólo PNG RGB/RGBA de 8 bits, sin interlace, de hasta
`512×512` píxeles y `512 KiB`; una imagen ausente o rechazada conserva la navegación sin artwork.
Si una instalación anterior tiene artwork problemático, borrá `ux0:data/vitashelflauncher/cache/covers/`.

En **Ajustes**, configurá la `URL` y presioná `X` en `Pair code` para introducir el código corto que genera **Pair Device** en RomM. `Triángulo` canjea el código una sola vez, guarda internamente el token recibido y sincroniza el catálogo. El código no se persiste ni se vuelve a mostrar; el token nunca se muestra en pantalla.

Después de una sincronización correcta, VitaShelf Launcher guarda una caché local del catálogo y la muestra al abrir la app, sin esperar a la red. En **Ajustes → RomM**, seleccioná **Sincronizar ahora** y presioná `X` cuando quieras actualizarla manualmente. **Sincronizar metadata** recorre las carátulas de RomM en segundo plano, las transforma con el servicio de imágenes y las guarda en la caché.

Usá la URL base de tu servidor (por ejemplo, `http://192.168.1.100:8080`) y creá en RomM un Client API Token de permisos mínimos de lectura/descarga antes de generar el pair code. Nunca compartas `config.ini`: contiene el token recibido.

## Descargas y lanzamiento

Cada ROM de RomM muestra un ícono de descarga (flecha) o un check blanco cuando ya está en `ux0:data/vitashelflauncher/downloads/<plataforma>/`. Presioná `X` sobre una ROM sin descargar para agregarla a la cola en segundo plano; podés seguir navegando mientras termina y luego `X` la abre con el emulador configurado.

En **Ajustes → Descargas** se ve cada elemento con su estado y porcentaje. Con `X` cancelás una descarga pendiente o activa; sobre una descarga fallida, cancelada o pausada, `X` la vuelve a encolar (la pausada reinicia desde 0% porque el `.part` se descarta). Con `Cuadrado` la quitás de la cola. Bloquear la Vita o salir de la app pausa la descarga activa; al volver, `X` la reanuda. La cola admite hasta 16 ROMs y procesa una por vez para no bloquear la interfaz.

El archivo se guarda con el título de la ROM en RomM formateado en minúsculas y sin espacios, más su id para evitar colisiones (por ejemplo, `metroid_45.zip`), conservando la extensión real que reporta RomM (`fs_name` o la URL de contenido): `sfc`, `nds`, `zip`, etc. La descarga usa `<archivo>.part` y renombra al final, así que un corte de red no deja una ROM marcada como lista. Las sincronizaciones, descargas e instalaciones se ejecutan en segundo plano; el estado se vuelve a comprobar al arrancar y después de sincronizar RomM.

Las ROMs que RomM marca con varios archivos (`has_multiple_files`, por ejemplo un disco PSX con `.cue` y `.bin`) no se descargan desde la UI: el endpoint devuelve un ZIP que el emulador no puede abrir directamente. Descargalas desde RomM, extraelas y agregá el `.cue`/`.m3u` como juego local.

El lanzamiento se configura por plataforma en `ux0:data/vitashelflauncher/config.ini`:

```text
launch_snes=RETROVITA:app0:/snes9x2005_libretro.self
launch_nds=DSVITA000:
launch_psp=VSCU00001:
```

El formato es `launch_<platform>=<titleid>[:<core>]`. El `titleid` es el de la aplicación (por ejemplo, `RETROVITA` para RetroArch); el `core` opcional es el archivo `.self` que RetroArch debe cargar. Si omitís el core, VitaShelf Launcher manda sólo la ROM, útil para emuladores como DSVita. Para RetroArch, instalá los cores que uses en `app0:`.

Perfiles por defecto (todos con `titleid=RETROVITA`): `snes`→`snes9x2005`, `nes`/`famicom`→`fceumm`, `gba`→`gpsp`, `gb`/`gbc`→`gambatte`, `genesis`/`mega-drive`/`megadrive`/`mastersystem`/`sms`/`gamegear`→`genesis_plus_gx`, `psx`/`playstation`→`pcsx_rearmed`, `arcade`/`neogeo`→`fbneo`, `pce`/`pc-engine`/`turbografx-16`→`mednafen_pce_fast`, `atari2600`→`stella2014`, `wonderswan`→`mednafen_wswan`. `nds` y `psp` no tienen core de RetroArch en Vita: configurá su `titleid` (`DSVITA000`, `VSCU00001`, …) o la ROM mostrará un aviso.
