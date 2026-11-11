# Validation record and coverage boundaries

Recorded on 2026-11-11. This is an implementation regression report, not Khronos conformance certification.

## Revisions and scope

The comparison baseline is `frangarcj/opensles` at `e35b0630ac4c9091db63374d5c111d719887dde9`. The initial threading/lifecycle changes were published as `398ca9c53e2a785d2804fe38592b02ca3ca361f8`. This follow-up adds original-test integration, the native test harness, CI dispatch and a zero-initialization fix found by those original tests.

`tests/automated/BufferQueue_test.cpp` and `tests/examples/slesTestSawtoothBufferQueue.cpp` retain their original source and assertions. The former contains ten GoogleTest cases. It is an AOSP-derived repository test, not the Khronos Conformance Test Suite (CTS).

### Defect exposed by the original unit tests

The untouched baseline and initial published change both aborted in `testValidBuffer`. AddressSanitizer traced the failure to `BufferHeader_release`: dynamically allocated queue slots had uninitialized owned-buffer pointers, while cleanup visited unused slots too. The follow-up changes the array allocation in `IEngine_AllocateBufferQueueArray` from `malloc` to `calloc`.

All ten original cases passed after that change with both host threading backends and under AddressSanitizer/UndefinedBehaviorSanitizer. No assertion was removed, skipped or changed to obtain that result.

## Host results

| Configuration | Executed coverage | Result |
| --- | --- | --- |
| pthread, SDL dummy audio | Existing 13 URI/state/object/pressure/churn targets, original ten-case unit suite, threading/pool/callback/lifetime regressions | Pass |
| SCE adapter with host SCE mocks, SDL dummy audio | Same coverage plus engine failure-injection checks | Pass |
| pthread with `USE_DEBUG` | Original unit suite and synchronization regressions with ownership checks | Pass |
| SCE mock with `USE_DEBUG` | Same, plus engine failure injection | Pass |
| pthread with `USE_DEBUG` and ASan/UBSan | Original unit suite and synchronization regressions | Pass |
| Actual `Vita.c` with SCE/audio mocks, `USE_DEBUG` and ASan/UBSan | Original unit suite, synchronization regressions, audio startup/output and engine failure handling | Pass |

ASan was configured with leak detection and halt-on-error; UBSan halted on errors. Host mocks execute the production adapter and, in the last configuration, the production Vita playback source. They do not prove SCE kernel or hardware behavior.

Reproduction commands, with GoogleTest, SDL2, libsndfile and sox installed:

```sh
make -C tests/host -j4 THREAD_BACKEND=pthread all
make -C tests/host THREAD_BACKEND=pthread \
    run-bufferqueue-original run-sawtooth-original \
    run-threads run-thread-pool run-callback-clear run-sync-destroy

make -C tests/host -j4 THREAD_BACKEND=sce-mock all
make -C tests/host THREAD_BACKEND=sce-mock \
    run-bufferqueue-original run-sawtooth-original \
    run-threads run-thread-pool run-callback-clear run-sync-destroy run-engine-failures
```

For the complete existing host suite, also run:

```sh
make -C tests/host THREAD_BACKEND=pthread \
    run-playstates run-playuri2 run-getposition run-loopuri run-playuri \
    run-slowdown run-urimime-host run-srcsink-host run-engine-host \
    run-object-host run-configbq-host run-manyplayers run-churn
```

Repeat with `THREAD_BACKEND=sce-mock`. Use separate build directories for parallel configurations. The churn target intentionally includes long settle/prefetch delays; a 120-second deadline is insufficient. When imposing a shell timeout on SDL programs, use a kill grace period, for example `timeout -k 5s 360 ...`, rather than assuming SIGTERM will always terminate a stalled process.

`run-bufferqueue-original` writes GoogleTest XML beside the executable. The workflow `.github/workflows/host-opensles-tests.yml` runs these original tests, the existing host suite, debug regressions and Vita-audio mock sanitizers. It supports `workflow_dispatch`; GitHub run results are separate evidence from local execution and are associated with the run's exact commit SHA.

## Actual devkit execution

Six executables were run for each backend, twelve successful processes in total, using real SCE synchronization and Vita audio APIs. Every test emitted its BEGIN marker and matching `OPENSL_TEST_PASS ... result=0`; none emitted a test failure, watchdog timeout or assertion failure. The host session also returned zero and confirmed the devkit was powered off afterward.

| Native executable | SCE backend | pthread backend |
| --- | --- | --- |
| `threads` | Pass | Pass |
| `thread-pool` | Pass | Pass |
| `callback-clear` | Pass | Pass |
| `callback-stop` | Pass | Pass |
| `sync-destroy` | Pass | Pass |
| `queue-churn` | Pass: 384 players, four engine cycles | Pass: 384 players, four engine cycles |

See [tests/vita/README.md](../tests/vita/README.md) for build commands, assertions, watchdogs and evidence requirements. The native library was built with debug ownership checks enabled. The SCE build did not link pthread for OpenSLES-private synchronization.

### Firmware provenance

The captured boot console reports:

```text
[SDK]: 3.650
[Shell]: vsh/release_branches/release_03.650 r142704 (build#757167)
```

The application's `sceKernelGetSystemSwVersion` query instead reports `3.74`, numeric `0x03740000`, with success status zero. The differing application-reported version is consistent with version spoofing, but the spoof mechanism was not independently verified. This run is recorded as a **3.65-based devkit environment**, not evidence of execution on native 3.74 firmware or every firmware from 3.60 onward.

An initial attempt launched too early after USB enumeration and received LOADP `0x30083` / `0x80010058` before the test process started. It was not counted as a test execution. With the same binary hashes and an additional boot-services settle period, all twelve launches and test bodies succeeded. Complete CP console logs were retained; application output was on `TTY0`, not the separate DECI TTY log.

### Native build provenance

The harness used the VitaSDK `softfp` GCC 15.2.0 toolchain built on 2026-08-28. Recorded component revisions:

| Component | Revision |
| --- | --- |
| newlib | `892f530fa7d696dbca74abfb8fccfec7d21d269d` |
| pthread-embedded | `11d2e5722d98c86f33c908fc47b2cf6e55205db5` |
| vita-headers | `5e1e7d38d766e4c1634a77f6e5249caab8c8f9cb` |
| vita-toolchain | `eacff34d18e9872a78c0e520e1997ef71900ebb4` |

SHA-256 manifests of the twelve exact ELF/SELF pairs, build logs, device-control tool hashes, launch receipts and power-state checks were retained in the investigation evidence. Firmware, SDK files, private game binaries and full private device logs are not distributed with this repository. Earlier GCC 10/GCC 15 cross-build and whole-archive link checks remain build evidence, distinct from this GCC 15 hardware run.

## Remaining original tests and known failures

These outcomes are deliberately not merged into an all-tests-pass claim.

| Original source or group | Execution/classification |
| --- | --- |
| `tests/sandbox/engine.c`, `object.c` | Unchanged sources passed against the baseline, patched pthread and patched SCE mock. These old programs are not leak-cleanliness tests; the newer host wrappers also cover orderly cleanup. |
| `tests/examples/slesTestSawtoothBufferQueue.cpp` | Unchanged source passed against all three variants. Integrated as `run-sawtooth-original`. Dummy audio verifies API execution, not listening quality. |
| `tests/sandbox/configbq.c` | Timed out against all three variants. Line-buffered diagnosis reaches 48 kHz mono/8-bit input with 44.1 kHz output. The existing integer-repeat conversion in `IBufferQueue_Enqueue` computes a zero ratio and queues a zero-byte converted buffer; the queue does not drain. This pre-existing sample-rate conversion defect is not fixed by the threading change. The separate `run-configbq-host` target passing does not negate it. |
| `tests/listening/seekTorture.c` | Aborted against all three variants, using a generated permuted WAV/map fixture. It supplies `pFormat = NULL` for a URI source, which this library rejects with `URI data sources require SL_DATAFORMAT_MIME`. This is an existing sample/API contract mismatch, not a threading regression; the original source was not modified to hide it. |
| `tests/mimeUri_test.cpp` | Not run. This separate fixture hardcodes a 75-second `/sdcard/media_api/music/MP3_256kbps_2ch.mp3` asset that was not provided. The portable `tests/mimeUri/` targets above are different tests. |
| `slesTestPlayStreamType`, Android effect-capabilities and Android FD examples | Not claimed as covered by the native threading harness. They require their particular Android extension interfaces, services or media/FD fixtures; these need capability-specific adaptation rather than substituting a successful unrelated playback test. |
| Recorder/feedback examples | Not run as capture tests. They need an input implementation, input device and capture fixtures. Playback mocks and output-only native tests do not supply these. |
| Equalizer, bass boost, virtualizer, preset/environmental reverb and mute/solo listening cases | Not validated as audible DSP behavior. Some non-Android interface methods store settings without implementing the Android effects processing path. API return codes are not an acoustic verification. |
| Interactive `intbufq`, `reverb`, and indefinite `multiplay` | Not counted as completed automated suites. They require scripted interactive scenarios or explicit stop criteria, and some depend on unvalidated effects. |
| Android/AOSP `Android.mk` build | Adapter source inclusion was updated in the initial change, but a complete Android platform build was not executed. |

Arbitrary PCM conversion, particularly 48 kHz input into 44.1 kHz output, must not be inferred from the passing 44.1 kHz native regression. A separate resampling/format-correctness effort is needed for that known defect.

## CTS access and standards claims

The [public Khronos registry](https://registry.khronos.org/OpenSL-ES/) provides specifications, headers and related material, but the inspected page did not provide a downloadable official CTS package. Khronos lists OpenSL ES among inactive standards. Its [general adopter process](https://www.khronos.org/conformance/adopters/) describes agreements, fees, access to tests, result submission and review. No authorized OpenSL ES CTS package or applicable access entitlement was available in this investigation. No agreement, payment, enrollment or conformance submission was performed. Current access and acceptance of a submission for the required OpenSL ES version would need confirmation with Khronos rather than inference from the generic program page.

The [Android implementation documentation](https://developer.android.com/ndk/guides/audio/opensl/opensl-for-android) itself distinguishes Android's API subset from conformance to a complete OpenSL ES 1.0.1 game, music or phone profile. The appropriate framing here is tested compatibility of implemented features and the private synchronization contract, not a blanket full-profile claim. None of the local, mock, CI or device tests substitutes for an applicable official CTS and review process.

## Unverified integration scenarios

The runnable game port and its assets were not present in the investigated workspace. The supplied Android shared library was analyzed, but the complete game was not run in this expanded validation. The callback regressions exercise the identified Clear/refill pattern without claiming that every gameplay path now works.

Suspend/resume, route changes, device loss recovery, real-time latency, audible glitches, long-duration gameplay and all hardware/firmware variants remain unverified. Hardware tests check real API execution and synchronization outcomes; they are not a captured-waveform comparison or proof of absence of all races. The private native adapter also does not make arbitrary application pthread/TLS use safe from a raw SCE callback thread.
