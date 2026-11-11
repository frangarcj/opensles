# OpenSL ES for PS Vita

This port offers selectable private threading backends and a host regression suite.

With a Vita toolchain on `PATH`, build the native SCE backend:

```sh
make -C libopensles HAVE_SCE_THREADS=1 HAVE_PTHREAD=0
```

The output is `libopensles/libOpenSLES.a`. The existing pthread backend remains available:

```sh
make -C libopensles HAVE_SCE_THREADS=0 HAVE_PTHREAD=1
```

The callback and lifetime fixes apply to both backends. The no-flag legacy configuration is retained, but it mixes a raw SCE playback thread with pthread synchronization and is not recommended for the reported Android-game audio problem.

See [SCE threading, integration, investigation and testing](doc/SCE_THREADS.md) for the backend contract, loader ABI boundaries, reproduced defects, validation commands and remaining hardware-verification requirements.
