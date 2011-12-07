
  LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_SRC_FILES := \
    libgps.c

  LOCAL_MODULE_TAGS := optional
	
  LOCAL_MODULE := libgps

  LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdl \
    libc

  include $(BUILD_SHARED_LIBRARY)

ifeq (0,1)

    LOCAL_PATH:= $(call my-dir)

    include $(CLEAR_VARS)
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE := libsecgps.so
    LOCAL_MODULE_CLASS := SHARED_LIBRARIES
    LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
    LOCAL_SHARED_LIBRARIES := libsecril-client
    LOCAL_SRC_FILES := $(LOCAL_MODULE)
    OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
    include $(BUILD_PREBUILT)

    include $(CLEAR_VARS)
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE := libsecril-client.so
    LOCAL_MODULE_CLASS := SHARED_LIBRARIES
    LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
    LOCAL_SRC_FILES := $(LOCAL_MODULE)
    OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
    include $(BUILD_PREBUILT)

endif