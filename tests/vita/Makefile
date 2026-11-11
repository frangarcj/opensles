ROOT := ../..
BACKEND ?= sce
BUILD_DIR ?= $(ROOT)/build/vita-tests-$(BACKEND)
CC := arm-vita-eabi-gcc
comma := ,
CFLAGS := -std=gnu17 -O1 -g -Wall -Wextra -UNDEBUG -DOPENSLES_TEST_VITA_NATIVE \
	-I$(ROOT)/include -I$(ROOT)/libopensles
LDFLAGS := -Wl,-q -Wl,--defsym=__sce_headroom=0x10000
LIBRARY := $(BUILD_DIR)/libOpenSLES.a

ifeq ($(BACKEND),sce)
BACKEND_FLAGS := HAVE_SCE_THREADS=1 HAVE_PTHREAD=0
CFLAGS += -DHAVE_SCE_THREADS
PTHREAD_LIBS :=
else ifeq ($(BACKEND),pthread)
BACKEND_FLAGS := HAVE_SCE_THREADS=0 HAVE_PTHREAD=1
CFLAGS += -DHAVE_PTHREAD
PTHREAD_LIBS := -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
else
$(error BACKEND must be sce or pthread)
endif

LDLIBS := -lsndfile -lvorbisenc -lvorbisfile -lvorbis -logg -lFLAC \
	-lmpg123 -lmp3lame -lopus $(PTHREAD_LIBS) -lSceAudio_stub \
	-lSceIofilemgr_stub -lSceKernelModulemgr_stub -lSceLibKernel_stub -lm
TESTS := threads thread-pool callback-clear callback-stop sync-destroy queue-churn
SELFS := $(addprefix $(BUILD_DIR)/,$(addsuffix /eboot.bin,$(TESTS)))

.PHONY: all library FORCE
all: $(SELFS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/.build-config: FORCE | $(BUILD_DIR)
	@printf '%s\n' '$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp

library: | $(BUILD_DIR)
	$(MAKE) -C $(ROOT)/libopensles $(BACKEND_FLAGS) CPPFLAGS='-g -DUSE_DEBUG'
	cmp -s $(ROOT)/libopensles/libOpenSLES.a $(LIBRARY) || cp $(ROOT)/libopensles/libOpenSLES.a $(LIBRARY)

$(LIBRARY): library

# Separate processes preserve each host regression's original static-state assumptions.
define TEST_template
$(BUILD_DIR)/$(1)-test.o: $(2) ../test_watchdog.h $(BUILD_DIR)/.build-config
	$(CC) $(CFLAGS) -Dmain=opensles_test_main -MMD -MP -c $$< -o $$@

$(BUILD_DIR)/$(1)-runner.o: runner.c ../test_watchdog.h $(BUILD_DIR)/.build-config
	$(CC) $(CFLAGS) -DOPENSLES_TEST_NAME='"$(1)"' $(3) -MMD -MP -c $$< -o $$@

$(BUILD_DIR)/$(1).elf: $(BUILD_DIR)/$(1)-test.o $(BUILD_DIR)/$(1)-runner.o $(LIBRARY)
	$(CC) $(CFLAGS) $(LDFLAGS) $(4) $$^ $(LDLIBS) -o $$@

$(BUILD_DIR)/$(1).velf: $(BUILD_DIR)/$(1).elf
	vita-elf-create $$< $$@

$(BUILD_DIR)/$(1)/eboot.bin: $(BUILD_DIR)/$(1).velf
	mkdir -p $$(@D)
	vita-make-fself -na $$< $$@
endef

$(eval $(call TEST_template,threads,../host/test_threads.c,,))
$(eval $(call TEST_template,thread-pool,../host/test_thread_pool.c,,))
$(eval $(call TEST_template,callback-clear,../host/test_callback_clear.c,-DOPENSLES_TEST_HAS_ARGS,))
$(eval $(call TEST_template,callback-stop,../host/test_callback_clear.c,-DOPENSLES_TEST_HAS_ARGS -DOPENSLES_TEST_STOP_CALLBACK,))
$(eval $(call TEST_template,sync-destroy,../host/test_sync_destroy.c,,-Wl$(comma)--wrap=audioPlayerTransportUpdate))
$(eval $(call TEST_template,queue-churn,test_queue_churn.c,,))

-include $(wildcard $(BUILD_DIR)/*.d)
.SECONDARY:
