#pragma once
#include <stdint.h>
int port_external_prepare(void);
void port_external_shutdown(void);
int port_external_open(const char *path,uint64_t *handle,uint32_t *size,uint64_t *features);
int port_external_open_status(void);
int port_external_stat(const char *path,uint8_t *kind,uint64_t *size);
