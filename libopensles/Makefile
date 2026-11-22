TARGET_LIB = libOpenSLES.a
OBJS       = threads.o OpenSLESUT.o             \
        MPH_to.o                      \
        OpenSLES_IID.o                \
        classes.o                     \
        devices.o                     \
        trace.o                       \
        locks.o                       \
        sles.o                        \
        sllog.o                       \
        SndFile.o                     \
        Vita.o                         \
        IOutputMix.o                  \
        IOutputMixExt.o               \
        sync.o                        \
        IID_to_MPH.o                  \
        ThreadPool.o                  \
        C3DGroup.o                    \
        CAudioPlayer.o                \
        CAudioRecorder.o              \
        CEngine.o                     \
        COutputMix.o                  \
        IBassBoost.o                  \
        IBufferQueue.o                \
        IEffectSend.o                 \
        IEngine.o                     \
        IDynamicInterfaceManagement.o \
        IEnvironmentalReverb.o        \
        IEqualizer.o                  \
        IMuteSolo.o                   \
        IObject.o                     \
        IPlay.o                       \
        IPlaybackRate.o               \
        IPrefetchStatus.o             \
        IPresetReverb.o               \
        IRecord.o                     \
        ISeek.o                       \
        IVirtualizer.o                \
        IVolume.o
INCLUDES   = -I. -I../include

PREFIX  ?= ${VITASDK}/arm-vita-eabi
CC      = arm-vita-eabi-gcc
CXX      = arm-vita-eabi-g++
AR      = arm-vita-eabi-ar
CFLAGS  = -std=gnu17 -Wl,-q -Wall -O3 $(INCLUDES) -DUSE_OUTPUTMIXEXT -DUSE_SDL -DUSE_SNDFILE

ifeq ($(HAVE_PTHREAD),1)
CFLAGS += -DHAVE_PTHREAD
endif

ifeq ($(HAVE_SCE_THREADS),1)
ifeq ($(HAVE_PTHREAD),1)
$(error HAVE_SCE_THREADS and HAVE_PTHREAD are mutually exclusive)
endif
CFLAGS += -DHAVE_SCE_THREADS
endif

ifeq ($(SYBERIA),1)
CFLAGS += -DSYBERIA
endif

CXXFLAGS  = $(CFLAGS)

ASFLAGS = $(CFLAGS)

.PHONY: all clean install FORCE
all: $(TARGET_LIB)

.build-config: FORCE
	@printf '%s\n' '$(CC) $(CFLAGS) $(CPPFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp

$(OBJS): .build-config

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(OBJS)
	$(AR) -rc $@ $^

clean:
	rm -f $(TARGET_LIB) $(OBJS) $(OBJS:.o=.d) .build-config .build-config.tmp

install: $(TARGET_LIB)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/
	cp $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib/
	@mkdir -p $(DESTDIR)$(PREFIX)/include/
	cp -R ../include $(DESTDIR)$(PREFIX)
