#ifndef OPENSLES_TEST_WATCHDOG_H
#define OPENSLES_TEST_WATCHDOG_H

#ifdef OPENSLES_TEST_VITA_NATIVE
void opensles_test_watchdog(unsigned seconds);
#else
#include <unistd.h>
static inline void opensles_test_watchdog(unsigned seconds)
{
    alarm(seconds);
}
#endif

#endif
