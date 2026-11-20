#ifndef VP_CATALOG_CACHE_H
#define VP_CATALOG_CACHE_H
#include "vitaplus.h"
#include <stdio.h>
/* Reads the payload after the existing magic/version/server header. The caller
 * publishes the result only after this function and fclose both succeed. */
bool vp_catalog_cache_read(FILE *file,unsigned int version,VpCatalog *catalog);
#endif
