#include "catalog_cache.h"
#include <stddef.h>
#include <string.h>

bool vp_catalog_cache_read(FILE *file,unsigned int version,VpCatalog *catalog) {
    size_t i,count=0,legacy_size=offsetof(VpItem,background_url);
    if (!file || !catalog) return false;
    memset(catalog,0,sizeof(*catalog));
    if (version==2U) return fread(catalog,sizeof(*catalog),1,file)==1 && catalog->count<=VP_MAX_ITEMS;
    if (version!=1U) return false;
    /* background_url was appended to the original ABI; every old field and the
     * alignment of its prefix are unchanged on the device that wrote the file. */
    for (i=0;i<VP_MAX_ITEMS;i++)
        if (fread(&catalog->items[i],legacy_size,1,file)!=1) return false;
    if (fread(&count,sizeof(count),1,file)!=1 || count>VP_MAX_ITEMS) return false;
    catalog->count=count;
    return true;
}
