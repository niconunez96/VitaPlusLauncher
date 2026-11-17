#ifndef VP_TEST_IO_H
#define VP_TEST_IO_H
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
typedef int SceUID;
typedef off_t SceOff;
#define SCE_O_RDONLY O_RDONLY
#define SCE_SEEK_END SEEK_END
SceUID sceIoOpen(const char *,int,int);
#define sceIoRead read
#define sceIoClose close
#define sceIoLseek lseek
#endif
