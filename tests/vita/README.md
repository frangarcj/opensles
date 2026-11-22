# Native Vita regressions

These executables run the OpenSLES threading and playback regressions on a real Vita. They do not use the host SCE or audio mocks. Each test is a separate process with its own engine state and watchdog.

## Build

Use one matching VitaSDK compiler, headers, codec libraries and tools. From the repository root:

```sh
make -C tests/vita -j4 BACKEND=sce all
make -C tests/vita -j4 BACKEND=pthread all
```

The outputs are `build/vita-tests-sce/` and `build/vita-tests-pthread/`. Each contains the unstripped `<test>.elf`, converted `<test>.velf`, and `<test>/eboot.bin`. Keep the exact ELF with the SELF used for each run. Build the two backends sequentially: they share the production library build directory.

The SCE build selects `HAVE_SCE_THREADS=1 HAVE_PTHREAD=0`. The pthread build selects the reverse and links the Vita pthread library with `--whole-archive`, as required by that dependency. The production library is built with `USE_DEBUG` so ownership assertions are active. Test assertions are explicitly enabled.

The codec link list matches the tested VitaSDK libsndfile build. Different codec configurations may need corresponding changes to `LDLIBS`; this is independent of the threading backend.

The linker option `--defsym=__sce_headroom=0x10000` reserves space for SCE metadata with the tested SDK's `armvita.x` linker script. It avoids `vita-elf-create` failing when the executable segment nearly reaches the following data segment. A toolchain whose linker script does not implement that symbol needs its own supported metadata-padding solution. Do not assume that a successful ELF link alone produced a runnable SELF.

## Tests

| Executable | Assertions exercised |
| --- | --- |
| `threads` | Nonrecursive mutex contention, bound-condition validation, timeout returns with mutex ownership restored, signals/broadcasts, self-join rejection, 128 worker creations and protected increments. |
| `thread-pool` | Predicate-false wakeups, shutdown while a handler is in flight, pending-work cleanup, repeated pool construction/destruction. |
| `callback-clear` | 64 iterations of clearing and refilling a queue from its own completion callback. |
| `callback-stop` | Stop from the mixer callback without self-deadlock; playback position resets correctly. |
| `sync-destroy` | A deterministic linker-wrapped transport update remains in flight while another thread destroys the player; destruction must wait for the internal reference. |
| `queue-churn` | Queue capacities 1, 2, 8 and 255; 384 players across four engine cycles; play, stop, clear, queue-state checks and dependency-ordered destruction. |

The shared host regressions omit mock-only fault injection in native builds. Actual allocation exhaustion, device removal and kernel error injection are not simulated on the device. Those error paths are separately covered by host mocks.

## Execution and evidence

On a development kit, serve a task-owned directory as `host0:` and launch each `eboot.bin` using the available device-control tooling. A layout such as `host0:/sce/threads/eboot.bin` and `host0:/pthread/threads/eboot.bin` keeps the backends distinct. No game assets or persistent application installation are required.

Before taking control, verify that no other session owns the device. Let boot services and homebrew hooks finish before launching; the recorded session needed an additional startup settle period after the USB link appeared. Keep an independent host-side deadline and power-off cleanup even though each executable has an in-process SCE watchdog. Do not leave the display powered on while inspecting logs.

The runner prints:

```text
OPENSL_TEST_BEGIN name=threads backend=sce reported_fw=... version=... query=...
OPENSL_TEST_PASS name=threads backend=sce result=0
```

Require exactly one matching BEGIN/PASS pair for every test/backend combination. `OPENSL_TEST_FAIL`, `OPENSL_TEST_TIMEOUT`, assertion failures, missing completion markers and launch refusals are not passes. A process-exit notification alone does not prove the assertions ran. On the tested devkit, application output appeared on the CP console's `TTY0` channel, not the separate DECI `tty.log`.

Record the compiler and SDK component revisions, the source revision, hashes of the ELF/SELF files, launch results, complete console output and final device power state. Compare the application's reported firmware with boot/system evidence: firmware spoofing can make the API report a different version from the running system.

See [the recorded validation and coverage limits](../../doc/TEST_COVERAGE.md). Passing these tests does not establish audible quality, arbitrary sample-rate conversion, real-time latency, suspend/resume behavior, all firmware variants, or compatibility with the complete game.
