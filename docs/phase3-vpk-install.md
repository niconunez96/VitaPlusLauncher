# Fase 3: instalación de VPK en Vita

El adaptador Vita descarga el VPK al destino elegido, lo extrae bajo `ux0:data/vitaplus/staging/latest/` y usa `scePromoterUtilityPromotePkg()` para promocionar el directorio extraído a LiveArea.

- La API de VitaSDK promociona **un directorio extraído**, no el archivo `.vpk` directamente.
- Se rechazan rutas absolutas, `..` y backslashes dentro del ZIP antes de extraer.
- La verificación de certificados TLS está habilitada; para un provider de LAN sin una CA compatible se debe configurar HTTP de forma explícita en una red confiable, no desactivar TLS globalmente.
- La promoción se vincula débilmente mediante `ScePromoterUtil_stub_weak`; si el módulo no está disponible, la instalación falla sin impedir que VitaPlus arranque.

Pendiente para endurecimiento en hardware: directorio de staging único por operación, validación SHA-256 desde el catálogo y limpieza al terminar.
