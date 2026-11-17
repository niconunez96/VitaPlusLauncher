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
ux0:data/vitaplus/downloads/
```

En **Ajustes**, configurá la `URL` y presioná `X` en `Pair code` para introducir el código corto que genera **Pair Device** en RomM. `Triángulo` canjea el código una sola vez, guarda internamente el token recibido y sincroniza el catálogo. El código no se persiste ni se vuelve a mostrar; el token nunca se muestra en pantalla.

Usá la URL base de tu servidor (por ejemplo, `http://192.168.1.100:8080`) y creá en RomM un Client API Token de permisos mínimos de lectura/descarga antes de generar el pair code. Nunca compartas `config.ini`: contiene el token recibido.
