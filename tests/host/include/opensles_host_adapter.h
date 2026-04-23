#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t host_now_ms(void);
void host_sleep_ms(uint32_t ms);
int host_path_to_file_uri(const char *path, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
