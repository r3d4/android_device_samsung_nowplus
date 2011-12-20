LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := libril-h1.c libril-h1_ipc.c
LOCAL_MODULE := libh1-ril
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES += libdl libutils libcutils
include $(BUILD_SHARED_LIBRARY)
