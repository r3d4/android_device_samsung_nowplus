################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    CameraHal.cpp \
    CameraHal_Utils.cpp \
    MessageQueue.cpp \
    ExifCreator.cpp\
    
LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libcamera_client \
    libsurfaceflinger_client

LOCAL_C_INCLUDES += \
    frameworks/base/include/camera \
    frameworks/base/include/binder \
    frameworks/base/include/ui \
    hardware/ti/omap3/liboverlay \
    external/skia/include/core

LOCAL_CFLAGS += -fno-short-enums 

ifdef HARDWARE_OMX

LOCAL_SRC_FILES += \
    JpegEncoder.cpp \
    JpegDecoder.cpp
     #   scale.c 

LOCAL_C_INCLUDES += \
    hardware/ti/omap3-compat/dspbridge/api/inc \
    hardware/ti/omap3-compat/omx/system/src/openmax_il/lcml/inc \
    hardware/ti/omap3-compat/omx/system/src/openmax_il/omx_core/inc \
    hardware/ti/omap3-compat/omx/system/src/openmax_il/common/inc \
    hardware/ti/omap3-compat/omx/video/src/openmax_il/prepost_processor/inc \
    hardware/ti/omap3-compat/omx/image/src/openmax_il/jpeg_enc/inc/
#   hardware/ti/omx/system/src/openmax_il/resource_manager_proxy/inc \
#    hardware/ti/omx/system/src/openmax_il/resource_manager/resource_activity_monitor/inc

LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX -DOMAP_ENHANCEMENT

LOCAL_SHARED_LIBRARIES += \
    libbridge \
    libLCML \
    libOMX_Core \
    libskia
#    libOMX_ResourceManagerProxy

endif

ifdef OMAP_ENHANCEMENT	
ifdef FW3A

LOCAL_C_INCLUDES += \
    hardware/ti/omap3/mm_isp/ipp/inc \
    hardware/ti/omap3/mm_isp/capl/inc \
    hardware/ti/omap3/fw3A/include \
    hardware/ti/omap3/arcsoft \
    hardware/ti/omap3/arcsoft/include \
    hardware/ti/omap3/arcsoft/arc_redeye/include \
    hardware/ti/omap3/arcsoft/arc_facetracking/include \
    hardware/ti/omap3/arcsoft/arc_antishaking/include \
    hardware/ti/omap3/mms_ipp_new/ARM11/inc

LOCAL_SHARED_LIBRARIES += \
    libdl \
    libcapl \
    libImagePipeline

LOCAL_CFLAGS += -O0 -g3 -DIPP_LINUX -D___ANDROID___ -DFW3A -DICAP -DIMAGE_PROCESSING_PIPELINE #_MMS -DCAMERA_ALGO

endif
endif

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcamera

LOCAL_MODULE_TAGS := eng

LOCAL_WHOLE_STATIC_LIBRARIES:= \
                                    libyuv

include $(BUILD_SHARED_LIBRARY)
include $(LOCAL_PATH)/Neon/android.mk
################################################

#ifdef HARDWARE_OMX

#include $(CLEAR_VARS)

#LOCAL_SRC_FILES := JpegEncoderTest.cpp

#LOCAL_C_INCLUDES := hardware/ti/omx/system/src/openmax_il/omx_core/inc

#LOCAL_SHARED_LIBRARIES := libcamera

#LOCAL_MODULE := JpegEncoderTest

#include $(BUILD_EXECUTABLE)

#endif

################################################

#include $(CLEAR_VARS)

#LOCAL_SRC_FILES := CameraHalTest.cpp

#LOCAL_C_INCLUDES := hardware/ti/omap3/liboverlay

#LOCAL_SHARED_LIBRARIES := libcamera

#LOCAL_MODULE := CameraHalTest

#include $(BUILD_EXECUTABLE)

################################################
