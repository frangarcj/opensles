TARGET_LIB = libOpenSLES.a
OBJS       = OpenSLESUT.o             \
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
CFLAGS  = -Wl,-q -Wall -O3 $(INCLUDES) -DUSE_OUTPUTMIXEXT -DUSE_SDL -DUSE_SNDFILE
CXXFLAGS  = $(CFLAGS)

ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(OBJS)
	$(AR) -rc $@ $^

clean:
	rm -rf $(TARGET_LIB) $(OBJS)

install: $(TARGET_LIB)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/
	cp $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib/
	@mkdir -p $(DESTDIR)$(PREFIX)/include/
	cp -R ../include $(DESTDIR)$(PREFIX)