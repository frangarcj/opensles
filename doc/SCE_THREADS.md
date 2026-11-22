# Native SCE threading and Android-game audio integration

## Build and integration

Run from this repository with the matching Vita compiler, headers and libraries available:

```sh
make -C libopensles HAVE_SCE_THREADS=1 HAVE_PTHREAD=0
```

The resulting archive is `libopensles/libOpenSLES.a`. Rebuild and relink the port against that archive; changing a loader-side macro does not change an already compiled OpenSLES archive. The build tracks compiler flags and header dependencies, so switching threading configurations rebuilds affected objects without requiring a manual clean.

| Selection | OpenSLES synchronization and workers | Vita playback thread |
| --- | --- | --- |
| `HAVE_SCE_THREADS=1 HAVE_PTHREAD=0` | Native SCE lightweight mutexes, conditions and threads | Native SCE |
| `HAVE_SCE_THREADS=0 HAVE_PTHREAD=1` | pthread | pthread |
| Neither option enabled | pthread | Raw SCE, legacy mixed configuration |

Enabling both options is an error. The legacy configuration remains buildable for compatibility, not as a recommended solution to this crash. The callback fixes are independent of backend selection; the patched pthread configuration is also a useful integration alternative when application callbacks still rely on pthread-managed thread state.

Native mode does not require pthread for OpenSLES itself. Keep the game's Bionic-to-Vita pthread wrappers and any pthread link dependency needed by the game, FalsoNDK or other libraries. Do not pass Android `pthread_mutex_t` or `pthread_cond_t` storage to the new private `sles_*` API. Public OpenSL ES interface definitions are unchanged.

The archive uses the existing audio and file libraries plus `SceLibKernel` threading imports. Codec dependencies depend on the selected libsndfile build. A smoke link with the tested SDK images used sndfile, Vorbis, Ogg, FLAC, mpg123, LAME and Opus libraries; those are not new threading requirements. Use one compatible toolchain and dependency set for the final application.

The default native worker stack is 512 KiB and the priority is `0x10000100`. These may be overridden with `OPENSLES_THREAD_STACK_SIZE` and `OPENSLES_THREAD_PRIORITY` through `CPPFLAGS`. The stack is per thread: four pool workers, a synchronization worker and playback consume several MiB in total. Reducing it requires stack-usage testing with the actual callbacks and codecs.

## Private synchronization contract

`libopensles/threads.h` defines a private abstraction, not a replacement implementation of the POSIX threads standard.

* Mutexes are nonrecursive. Try-lock reports contention as `EBUSY`; ownership errors are not treated as successful operations.
* Each condition is permanently associated with its mutex at initialization. Waiting with a different mutex returns `EINVAL`. The library uses predicate loops rather than assuming a signal is remembered or a wakeup proves the predicate.
* Timed waits accept relative microseconds. The native path passes that interval to SCE directly; it does not call `clock_gettime`. The POSIX path converts it to an absolute realtime deadline and checks clock/overflow failures. These are intentionally not identical clock domains under wall-clock adjustment.
* Normal condition wakes and timeout returns retain the associated mutex. Object debug-owner bookkeeping is cleared while a wait releases the mutex and restored after reacquisition.
* Threads are joinable, reject self-join, and are deleted only after termination. Failed starts delete the unstarted thread. The start argument is copied into the new thread's argument block rather than retaining a pointer to a creator's stack variable.

The native adapter does not implement cancellation, detached threads, arbitrary mutex attributes, process-shared synchronization, TLS or pthread return-value transport. These features are not required by the private callers. SCE errors are translated to positive errno-style results; unexpected errors are not interpreted as a successful wait or lock. This is not a claim of complete POSIX or OpenSL ES conformance certification.

The relative-wait and thread-start contracts were checked against local SDK declarations and the debug 4.00 firmware. In the analyzed ThreadMgr image, `_sceKernelWaitLwCond` leads from `0x8102b13c` through `0x8102de28` to the wait core at `0x81023f60`; both normal and timeout paths perform mutex reacquisition. These addresses identify that particular loaded image, not universal firmware offsets. No firmware-offset dependency was added to the implementation.

## Investigation: why the reported freeze is not just a mutex choice

The supplied `liborigingame.so` has SHA-256:

```text
f3c0f544605d834800d6ea94f7000ca7a0c996b91489ccf141f3cd85ffba022f
```

IDA analysis identified this path, using the input ELF's addresses rather than a runtime load address:

```text
bufferqueuePlayerCallback                 0x1ad182
  -> SoundOpenSL::reachedEndOfBuffer       0x1ad130
       -> buffer queue Clear()
       -> buffer queue Enqueue()
```

The nonstreamed looping path clears and refills the queue from its completion callback. `SoundOpenSL::createPlayer` at `0x1acfb8` confirms the Android simple-buffer-queue interface and callback registration. The original OpenSLES `Clear()` waits for the mixer to acknowledge a clear request, while the mixer is executing that callback. It therefore waits for itself. A public-API regression reproduces the hang on the unmodified library and passes with the fix.

The fix identifies the active mixer thread per engine. A clear from that thread, or a player without a mixer track, is completed synchronously. Other callers retain the mixer acknowledgement path. Re-entrant Stop is handled without waiting for the same mixer; already-consumed frame accounting occurs before invoking the callback so a callback's Stop can reset position to zero correctly.

Simply replacing pthread condition variables with SCE conditions would preserve this self-deadlock. Backend selection and callback correctness are separate changes.

### Why the no-pthread crash can appear beside a time call

Previously `HAVE_PTHREAD` changed only the playback thread, not OpenSLES locks, conditions, the synchronization worker or the worker pool. The no-flag build therefore entered pthread synchronization from a raw SCE thread.

In the inspected pthread-embedded Vita implementation, the cancellable semaphore-wait path obtains pthread per-thread data, reads process time, and then dereferences that thread data. Missing pthread-managed state on a raw SCE thread can consequently fault immediately after the time read. This fits the reported symptom, but it is a hypothesis for the original device crash, not a confirmed fault address or backtrace. Native mode removes that dependency from OpenSLES-private synchronization; it does not automatically make arbitrary game pthread/TLS use safe in a native callback.

### Loader and FalsoNDK boundaries

The inspected soloader maps Android pthread imports through `source/reimpl/pthr.c` and time imports through its system wrappers. Its static-initializer emulation checks the object registry separately from allocation, initialization and publication. That leaves an independent concurrent-first-use race; this OpenSLES change does not claim to fix it.

FalsoNDK's `ALooper` and sensor implementations use SCE synchronization and thread IDs to implement Android-facing platform APIs. FalsoNDK is the Vita compatibility layer here; the actual Android NDK does not use SCE primitives. The reviewed FalsoNDK tree does not supply the OpenSLES audio implementation. Neither loader nor FalsoNDK sources were modified in this change.

## Related lifecycle corrections

The synchronization worker now acquires an internal lifetime reference while an object is still published under the engine mutex. Destruction unpublishes it and waits for outstanding worker references before freeing it. A deterministic regression pauses an in-flight transport update: the original library destroys the player prematurely, while the patched library waits until the update is released.

Worker-pool shutdown now checks its predicate before waiting, including workers that start after a shutdown broadcast. Waiter counts are updated by the returning waiter, including predicate-false wakeups. Pending closures are drained with an advancing queue cursor, and partial initialization is safely unwound. Fault injection exercises failures followed by a successful realization retry.

Vita playback state is owned by its engine rather than shared global state. Port, volume and playback-thread startup failures return to realization; shutdown uses atomic state, joins the worker, and then releases the port and buffers. If audio output fails after startup, the worker continues mixer acknowledgements with a delay so cleanup requests do not hang. This does not reopen the failed device or restore audible output automatically.

## Validation and reproducible tests

Host prerequisites are a C/C++ compiler, make, pkg-config, SDL2, libsndfile, GoogleTest and sox. On Debian/Ubuntu, GoogleTest is provided by `libgtest-dev`. The synchronization/destruction gate uses the GNU linker `--wrap` facility and is included in the Linux test build.

```sh
make -C tests/host -j4 all
make -C tests/host run-bufferqueue-original run-sawtooth-original run-threads run-thread-pool run-callback-clear run-sync-destroy

make -C tests/host -j4 THREAD_BACKEND=sce-mock all
make -C tests/host THREAD_BACKEND=sce-mock \
    run-bufferqueue-original run-sawtooth-original run-threads run-thread-pool run-callback-clear run-engine-failures run-sync-destroy
```

The native mock tests compile the real SCE adapter against host implementations of the SCE API. To exercise the actual `Vita.c` playback implementation, including startup and output failures:

```sh
make -C tests/host -j4 THREAD_BACKEND=sce-mock AUDIO_BACKEND=vita-mock \
    BUILD_DIR=../../build/vita-mock EXTRA_CFLAGS=-DUSE_DEBUG all
make -C tests/host THREAD_BACKEND=sce-mock AUDIO_BACKEND=vita-mock \
    BUILD_DIR=../../build/vita-mock EXTRA_CFLAGS=-DUSE_DEBUG \
    run-bufferqueue-original run-sawtooth-original run-threads run-thread-pool run-callback-clear run-engine-failures run-sync-destroy
```

The `run-bufferqueue-original` target builds the original `tests/automated/BufferQueue_test.cpp` without changing its assertions and writes a GoogleTest XML report next to the test executable. This is the repository's AOSP-derived unit test, not the Khronos conformance test suite.

Add `-fsanitize=address,undefined -fno-omit-frame-pointer` to `EXTRA_CFLAGS` and `-fsanitize=address,undefined` to `EXTRA_LDFLAGS` for the sanitizer configurations. Use a distinct `BUILD_DIR` for parallel configurations.

Coverage includes mutex ownership after timeout, bound-condition misuse, contention and broadcasts, 128 worker creations, 64 callback loop iterations, Stop position reset, pending-work shutdown, object lifetime, failed creation/start cleanup, and repeated engine realization. The SDL/SCE-mock engine test exercises 15 injected failures; the Vita-audio mock exercises 19. The existing playback, URI, object-limit and churn tests remain available and are included in CI. Allow at least 300 seconds for the default `run-churn`: its intentional settle and prefetch delays already exceed a 120-second deadline.

The investigation also built and whole-archive-linked the native, pthread and legacy configurations with GCC 10 and GCC 15, checked backend switching, and rejected conflicting flags. Native archives had no unresolved `pthread_*` or `clock_gettime` symbols; their smoke executables linked without pthread. A subsequent GCC 15 native harness ran all six tests successfully with each backend on a real 3.65-based devkit. Its application API reported 3.74 while boot SDK/shell evidence identified 3.650; this is not a native 3.74 firmware test. The Android/AOSP makefile was updated to include the adapter, but that separate build was not executed.

The unchanged original ten-case buffer-queue unit suite exposed a pre-existing invalid free: unused heap-allocated queue slots contained uninitialized owned-buffer pointers. Zero-initializing the slot array fixes that failure. See [the detailed coverage record](TEST_COVERAGE.md) and [native harness instructions](../tests/vita/README.md) for evidence, original-sample failures, firmware provenance and the distinction from official conformance testing.

## Remaining integration limits

This work did not run the supplied game on hardware, test suspend/resume or audio routing changes, measure real-time latency, or certify all firmware/hardware variants. The native regressions exercised real SCE primitives/audio separately from the host mocks. A crash trace from the actual game is still required to confirm the time-adjacent crash hypothesis. The original `configbq.c` also exposes a separate, unfixed conversion defect with 48 kHz input into 44.1 kHz output; passing the threading regressions does not imply arbitrary sample-rate correctness.

Callbacks must remain bounded and must not create or destroy objects on the mixer thread. Destroy dependent players before the output mix and engine, and do not access interfaces after destruction. The internal worker lifetime reference does not make arbitrary concurrent application destruction/API use valid. The [Android OpenSL ES programming notes](https://developer.android.com/ndk/guides/audio/opensl/opensl-prog-notes#callbacks-and-threads) describe callback workload and thread-identity/TLS restrictions; the same document describes dependency-ordered destruction.

For the original game, validate entering gameplay with SFX enabled, repeated loops, stop/clear/restart, scene transitions and orderly shutdown. Compare the patched native and pthread builds. If a crash remains, capture the faulting PC, LR, thread identity and stack, including the exact pthread-embedded and loader revisions, rather than assuming another nearby time call is causal.

## Source revisions reviewed

* [OpenSLES baseline](https://github.com/frangarcj/opensles/tree/e35b0630ac4c9091db63374d5c111d719887dde9).
* [soloader pthread adaptation](https://github.com/memory-hunter/soloader-boilerplate/blob/f3bd913398bf82d9ac5b76a62445d1753a6282e5/source/reimpl/pthr.c).
* [FalsoNDK ALooper](https://github.com/elliencode/FalsoNDK/blob/6bf476b7d6f03abcbb8122f0b9646c48b426cb18/android/ALooper.cpp).
* [pthread-embedded Vita OS abstraction](https://github.com/vitasdk/pthread-embedded/blob/11d2e5722d98c86f33c908fc47b2cf6e55205db5/platform/vita/vita_osal.c).

The investigation workspace retains binary-analysis results, baseline failures, compiler/symbol logs and host-test results in the sibling `analysis/` directory. Those files are supporting evidence, not runtime dependencies or content to ship with the game.
