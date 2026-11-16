# Arquitectura de VitaPlus

## Principio

VitaPlus es una shell con un catálogo unificado. Una fuente no dibuja pantallas: aporta `VpItem` al catálogo y declara acciones disponibles.

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

- Los providers nunca escriben paths arbitrarios: el backend resuelve el destino bajo `ux0:data/vitaplus/` o una ruta de plataforma permitida.
- El token RomM no se imprime ni se incluye en logs.
- La instalación de VPK siempre es una acción explícita del usuario en UI.
- HTTPS debe verificar certificados; para LAN se recomienda HTTP explícito en una red de confianza antes que desactivar TLS.

## Temas

Los temas son datos y assets. No ejecutan scripts. `theme.ini` define colores, layout y dimensiones de cards; un tema posterior podrá incluir iconos, fondos y audio.
