ROOT := ../..
BUILD_DIR := $(ROOT)/build/host-tests
LIB_BUILD_DIR := $(BUILD_DIR)/libopensles
TEST_BUILD_DIR := $(BUILD_DIR)/tests
STATIC_LIB := $(BUILD_DIR)/libOpenSLES-host.a
SAMPLE_WAV := $(BUILD_DIR)/sample.wav
HOST_ADAPTER := $(TEST_BUILD_DIR)/host_adapter.o

PKG_CONFIG ?= pkg-config
CC ?= cc
CXX ?= c++
AR ?= ar
SOX ?= sox

INCLUDES := -I$(ROOT)/include -I$(ROOT)/libopensles -I$(ROOT)/tests/host/include
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl2)
SNDFILE_CFLAGS := $(shell $(PKG_CONFIG) --cflags sndfile)
SNDFILE_LIBS := $(shell $(PKG_CONFIG) --libs sndfile)

CFLAGS := -std=gnu17 -Wall -O0 -g $(INCLUDES) $(SDL_CFLAGS) $(SNDFILE_CFLAGS) \
	-DUSE_OUTPUTMIXEXT -DUSE_SDL -DUSE_SNDFILE -DHAVE_PTHREAD
CXXFLAGS := -std=gnu++17 -Wall -O0 -g $(INCLUDES) $(SDL_CFLAGS) $(SNDFILE_CFLAGS)
LDLIBS := $(SDL_LIBS) $(SNDFILE_LIBS) -lpthread -lm

LIB_SOURCES := \
	$(ROOT)/libopensles/OpenSLESUT.c \
	$(ROOT)/libopensles/MPH_to.c \
	$(ROOT)/libopensles/OpenSLES_IID.c \
	$(ROOT)/libopensles/classes.c \
	$(ROOT)/libopensles/devices.c \
	$(ROOT)/libopensles/trace.c \
	$(ROOT)/libopensles/locks.c \
	$(ROOT)/libopensles/sles.c \
	$(ROOT)/libopensles/sllog.c \
	$(ROOT)/libopensles/SndFile.c \
	$(ROOT)/libopensles/SDL.c \
	$(ROOT)/libopensles/IOutputMix.c \
	$(ROOT)/libopensles/IOutputMixExt.c \
	$(ROOT)/libopensles/sync.c \
	$(ROOT)/libopensles/IID_to_MPH.c \
	$(ROOT)/libopensles/ThreadPool.c \
	$(ROOT)/libopensles/C3DGroup.c \
	$(ROOT)/libopensles/CAudioPlayer.c \
	$(ROOT)/libopensles/CAudioRecorder.c \
	$(ROOT)/libopensles/CEngine.c \
	$(ROOT)/libopensles/COutputMix.c \
	$(ROOT)/libopensles/IBassBoost.c \
	$(ROOT)/libopensles/IBufferQueue.c \
	$(ROOT)/libopensles/IEffectSend.c \
	$(ROOT)/libopensles/IEngine.c \
	$(ROOT)/libopensles/IDynamicInterfaceManagement.c \
	$(ROOT)/libopensles/IEnvironmentalReverb.c \
	$(ROOT)/libopensles/IEqualizer.c \
	$(ROOT)/libopensles/IMuteSolo.c \
	$(ROOT)/libopensles/IObject.c \
	$(ROOT)/libopensles/IPlay.c \
	$(ROOT)/libopensles/IPlaybackRate.c \
	$(ROOT)/libopensles/IPrefetchStatus.c \
	$(ROOT)/libopensles/IPresetReverb.c \
	$(ROOT)/libopensles/IRecord.c \
	$(ROOT)/libopensles/ISeek.c \
	$(ROOT)/libopensles/IVirtualizer.c \
	$(ROOT)/libopensles/IVolume.c

LIB_OBJECTS := $(patsubst $(ROOT)/libopensles/%.c,$(LIB_BUILD_DIR)/%.o,$(LIB_SOURCES))

TEST_PROGRAMS := \
	$(TEST_BUILD_DIR)/slesTestLoopUri \
	$(TEST_BUILD_DIR)/slesTestPlayUri \
	$(TEST_BUILD_DIR)/slesTestSlowDownUri \
	$(TEST_BUILD_DIR)/slesTestGetPositionUri \
	$(TEST_BUILD_DIR)/slesTestPlayUri2 \
	$(TEST_BUILD_DIR)/slesTest_playStates \
	$(TEST_BUILD_DIR)/slesTestManyPlayers \
	$(TEST_BUILD_DIR)/slesTestManyPlayersChurn \
	$(TEST_BUILD_DIR)/slesTestUriMimeHost \
	$(TEST_BUILD_DIR)/slesTestSrcSinkHost \
	$(TEST_BUILD_DIR)/slesTestEngineHost \
	$(TEST_BUILD_DIR)/slesTestObjectHost \
	$(TEST_BUILD_DIR)/slesTestConfigBqHost

TEST_SUPPORT := $(TEST_BUILD_DIR)/test_support.o

.PHONY: all clean smoke run-loopuri run-playuri run-slowdown run-getposition run-playuri2 run-playstates run-manyplayers run-churn run-urimime-host run-srcsink-host run-engine-host run-object-host run-configbq-host

all: $(TEST_PROGRAMS)

$(BUILD_DIR) $(LIB_BUILD_DIR) $(TEST_BUILD_DIR):
	mkdir -p $@

$(STATIC_LIB): $(LIB_OBJECTS) | $(BUILD_DIR)
	$(AR) -rc $@ $^

$(LIB_BUILD_DIR)/%.o: $(ROOT)/libopensles/%.c | $(LIB_BUILD_DIR)
	$(CC) $(CFLAGS) $(if $(filter SDL.c,$(notdir $<)),-include SDL.h,) -c $< -o $@

$(TEST_BUILD_DIR)/test_support.o: $(ROOT)/tests/host/test_support.c | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(HOST_ADAPTER): $(ROOT)/tests/host/host_adapter.c | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/%: $(ROOT)/tests/mimeUri/%.cpp $(STATIC_LIB) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(TEST_BUILD_DIR)/slesTestUriMimeHost: $(ROOT)/tests/sandbox/slesTestUriMimeHost.c $(STATIC_LIB) $(HOST_ADAPTER) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $< $(HOST_ADAPTER) $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(TEST_BUILD_DIR)/slesTestSrcSinkHost: $(ROOT)/tests/sandbox/slesTestSrcSinkHost.c $(STATIC_LIB) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $< $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(TEST_BUILD_DIR)/slesTestEngineHost: $(ROOT)/tests/sandbox/slesTestEngineHost.c $(STATIC_LIB) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $< $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(TEST_BUILD_DIR)/slesTestObjectHost: $(ROOT)/tests/sandbox/slesTestObjectHost.c $(STATIC_LIB) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $< $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(TEST_BUILD_DIR)/slesTestConfigBqHost: $(ROOT)/tests/sandbox/slesTestConfigBqHost.c $(STATIC_LIB) $(TEST_SUPPORT) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $< $(TEST_SUPPORT) $(STATIC_LIB) $(LDLIBS) -o $@

$(SAMPLE_WAV): | $(BUILD_DIR)
	$(SOX) -n -r 44100 -b 16 -c 2 $@ synth 4 sine 440 sine 660 vol 0.3

smoke: run-playstates run-playuri2

run-loopuri: $(TEST_BUILD_DIR)/slesTestLoopUri $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestLoopUri $(abspath $(SAMPLE_WAV))

run-playuri: $(TEST_BUILD_DIR)/slesTestPlayUri $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestPlayUri $(abspath $(SAMPLE_WAV))

run-slowdown: $(TEST_BUILD_DIR)/slesTestSlowDownUri $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestSlowDownUri $(abspath $(SAMPLE_WAV))

run-getposition: $(TEST_BUILD_DIR)/slesTestGetPositionUri $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestGetPositionUri $(abspath $(SAMPLE_WAV))

run-playuri2: $(TEST_BUILD_DIR)/slesTestPlayUri2 $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestPlayUri2 $(abspath $(SAMPLE_WAV)) $(abspath $(SAMPLE_WAV))

run-playstates: $(TEST_BUILD_DIR)/slesTest_playStates $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTest_playStates $(abspath $(SAMPLE_WAV))

run-manyplayers: $(TEST_BUILD_DIR)/slesTestManyPlayers $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestManyPlayers $(abspath $(SAMPLE_WAV))

run-churn: $(TEST_BUILD_DIR)/slesTestManyPlayersChurn $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestManyPlayersChurn $(abspath $(SAMPLE_WAV)) 20 5000

run-urimime-host: $(TEST_BUILD_DIR)/slesTestUriMimeHost $(SAMPLE_WAV)
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestUriMimeHost $(abspath $(SAMPLE_WAV))

run-srcsink-host: $(TEST_BUILD_DIR)/slesTestSrcSinkHost
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestSrcSinkHost

run-engine-host: $(TEST_BUILD_DIR)/slesTestEngineHost
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestEngineHost

run-object-host: $(TEST_BUILD_DIR)/slesTestObjectHost
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestObjectHost

run-configbq-host: $(TEST_BUILD_DIR)/slesTestConfigBqHost
	SDL_AUDIODRIVER=dummy $(TEST_BUILD_DIR)/slesTestConfigBqHost

clean:
	rm -rf $(BUILD_DIR)