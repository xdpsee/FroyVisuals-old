
WARNING_FLAGS := -Wall -Wstrict-aliasing -Wcast-align -Waddress -Wmissing-braces -Wimplicit -Wunused -Wno-unused-variable
DEBUG_FLAGS := -g -ggdb3 -DHAVE_PROFILING=1
OPTIM_FLAGS := -O3 -ftree-vectorize -ffast-math -funsafe-math-optimizations -fsingle-precision-constant
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    OPTIM_FLAGS += -DHAVE_ARM -DHAVE_NEON=1 -mfloat-abi=softfp -mfpu=neon
endif
ifeq ($(TARGET_ARCH_ABI),armeabi)
    OPTIM_FLAGS += -DHAVE_ARM=1 -mfloat-abi=softfp
endif
ifeq ($(TARGET_ARCH_ABI),x86)
    DEBUG_FLAGS := -g
endif

include $(call all-subdir-makefiles)


