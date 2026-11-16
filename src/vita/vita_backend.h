#ifndef VITAPLUS_VITA_BACKEND_H
#define VITAPLUS_VITA_BACKEND_H

#include "vitaplus.h"

bool vp_vita_fetch(const char *url, const char *bearer_token,
                   char *response, size_t response_capacity, void *context);
bool vp_vita_transfer(const char *url, const char *bearer_token, const char *destination,
                      VpTransferAction action, void *context);

#endif
