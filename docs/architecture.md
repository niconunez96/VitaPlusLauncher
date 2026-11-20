# Arquitectura de VitaShelf Launcher

## Principio

VitaShelf Launcher es una shell con un catálogo unificado. Una fuente no dibuja pantallas: aporta `VpItem` al catálogo y declara acciones disponibles.

```text
Vita2D UI
  -> VpApp (navegación, búsqueda, preferencias)
    -> VpCatalog (ítems normalizados)
      -> providers: local / RomM / homebrew
    -> VpTransferBackend (descarga e instalación)
```

## Providers

- **Local:** ítems instalados o ROMs ya presentes. El escáner de filesystem y el launcher nativo se conectarán en el adaptador Vita.
- **RomM:** usa token Bearer. El adaptador consulta plataformas/ROMs, normaliza resultados y entrega URLs de contenido/artwork.
- **Homebrew:** consume catálogos estáticos JSON. Instalar requiere confirmación explícita y un backend Vita que descargue el VPK y llame al instalador.
- **Official-store shortcut:** solo abre la aplicación oficial; no usa APIs privadas ni recoge credenciales.

## Seguridad

- Los providers nunca escriben paths arbitrarios: el backend resuelve el destino bajo `ux0:data/vitashelflauncher/` o una ruta de plataforma permitida.
- El token RomM no se imprime ni se incluye en logs.
- La instalación de VPK siempre es una acción explícita del usuario en UI.
- HTTPS debe verificar certificados; para LAN se recomienda HTTP explícito en una red de confianza antes que desactivar TLS.

## Temas

Los temas son datos y assets. No ejecutan scripts. `theme.ini` define colores y vista
inicial; la presentación Aurora lee además `ui_motion_ms`, `ui_radius` y
`ui_wallpaper_alpha`. La geometría fija de las grillas mantiene las columnas que
usa la navegación. Los campos históricos de tamaño de cards se conservan por compatibilidad.

`src/vita/main.c` conserva entrada, trabajos y caché de carátulas. Construye un
`VpUiFrame` de solo lectura; `ui_renderer.c` dibuja las pantallas sin modificar el
catálogo. `ui_presentation.c` contiene alias visuales, geometría y opciones de tema
comprobables en host. No se cambió el formato persistido ni los contratos de proveedores.

Los iconos originales se incluyen en `app0:assets/aurora/`. El renderer decodifica
como máximo un nuevo asset por frame, valida tamaño/formato y no reintenta archivos
ausentes. Los 36 PNG suman menos de 2 MiB de píxeles RGBA; la asignación real de GPU
puede ser mayor por alineación. La caché de carátulas sigue separada y limitada a 8.
