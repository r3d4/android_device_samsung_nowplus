
CAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_PREBUILT_LIBS :=  \
    Neon/libyuv.a

include $(BUILD_MULTI_PREBUILT)


