#include "opensles_host_adapter.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

uint64_t host_now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t) tv.tv_sec * 1000ULL) + ((uint64_t) tv.tv_usec / 1000ULL);
}

void host_sleep_ms(uint32_t ms)
{
    usleep((useconds_t) ms * 1000U);
}

int host_path_to_file_uri(const char *path, char *out, size_t out_size)
{
    if (path == NULL || out == NULL || out_size == 0) {
        return -1;
    }

    // Host OpenSLES backends often expect a filesystem path even for URI locators.
    if (strncmp(path, "file://", 7) == 0) {
        const char *trimmed = path + 7;
        int n = snprintf(out, out_size, "%s", trimmed);
        return (n > 0 && (size_t) n < out_size) ? 0 : -1;
    }

    int n = snprintf(out, out_size, "%s", path);
    return (n > 0 && (size_t) n < out_size) ? 0 : -1;
}
