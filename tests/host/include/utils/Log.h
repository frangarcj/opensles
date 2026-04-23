#pragma once

#include <stdio.h>

#define LOGV(...) fprintf(stdout, __VA_ARGS__)
#define LOGD(...) fprintf(stdout, __VA_ARGS__)
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGW(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)