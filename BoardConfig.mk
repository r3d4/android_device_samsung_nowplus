# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# This file sets variables that control the way modules are built
# thorughout the system. It should not be used to conditionally
# disable makefiles (the proper mechanism to control what gets
# included in a build is to use PRODUCT_PACKAGES in a product
# definition file).
#

# WARNING: This line must come *before* including the proprietary
# variant, so that it gets overwritten by the parent (which goes
# against the traditional rules of inheritance).


# The generic product target doesn't have any hardware-specific pieces.


TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_NO_RECOVERY      := true
TARGET_NO_RADIOIMAGE := true

TARGET_BOARD := GT-I8320
TARGET_BOOTLOADER_BOARD_NAME := nowplus
TARGET_USES_OLD_LIBSENSORS_HAL := true
TARGET_SENSORS_NO_OPEN_CHECK := true
TARGET_PROVIDES_INIT_RC := true

#cpu
TARGET_BOARD_PLATFORM := omap3
TARGET_CPU_ABI  := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_GLOBAL_CFLAGS += -mtune=cortex-a8
TARGET_GLOBAL_CPPFLAGS += -mtune=cortex-a8
ARCH_ARM_HAVE_TLS_REGISTER := true
TARGET_OMAP3 := true
COMMON_GLOBAL_CFLAGS += -DTARGET_OMAP3 -DOMAP_COMPAT

#sound
BOARD_USES_ALSA_AUDIO := true
BUILD_WITH_ALSA_UTILS := true
ALSA_DEFAULT_SAMPLE_RATE := 44100

#gps, atm only dummy
#BOARD_GPS_LIBRARIES := "libsecgps.so"
#BOARD_USES_GPSSHIM := true

# Wifi
USES_TI_WL1271 := true
BOARD_WPA_SUPPLICANT_DRIVER := CUSTOM
BOARD_WLAN_DEVICE           := wl1271
BOARD_SOFTAP_DEVICE 		:= wl1271
WPA_SUPPLICANT_VERSION      := VER_0_6_X
WIFI_DRIVER_MODULE_PATH     := "/system/etc/wifi/tiwlan_drv.ko"
WIFI_DRIVER_MODULE_NAME     := "tiwlan_drv"
#WIFI_DRIVER_FW_STA_PATH     := "/system/etc/wifi/fw_wlan1271.bin"
WIFI_FIRMWARE_LOADER        := "wlan_loader"
#PRODUCT_WIRELESS_TOOLS      := true

#AP_CONFIG_DRIVER_WILINK := true
#WIFI_DRIVER_FW_AP_PATH := "/system/etc/wifi/fw_tiwlan_ap.bin"

#egl
BOARD_USE_YUV422I_DEFAULT_COLORFORMAT := true
#BOARD_EGL_CFG := device/samsung/nowplus/egl.cfg
# Workaround for eglconfig error
BOARD_NO_RGBX_8888 := true
#TARGET_LIBAGL_USE_GRALLOC_COPYBITS := true
TARGET_ELECTRONBEAM_FRAMES := 10

# Bluetooth
BOARD_HAVE_BLUETOOTH := true

# no GPS atm
BOARD_HAVE_FAKE_GPS := true

# FM
#!!! atm breaks 'OMAP_ENHANCEMENT := true'
BOARD_HAVE_FM_RADIO := true
BOARD_FM_DEVICE := si4709
BOARD_GLOBAL_CFLAGS += -DHAVE_FM_RADIO


# Camera
#USE_CAMERA_STUB := true
BOARD_CAMERA_LIBRARIES := libcamera


#dsp
HARDWARE_OMX := true
#BUILD_WITHOUT_PV := false
#use hw ti omx audio codecs
BUILD_WITH_TI_AUDIO := 1
BUILD_PV_VIDEO_ENCODERS := 1
#FW3A := true
#ICAP := true
#IMAGE_PROCESSING_PIPELINE := true 
ifdef HARDWARE_OMX
OMX_JPEG := true
OMX_VENDOR := ti
OMX_VENDOR_INCLUDES := \
   hardware/ti/omx/system/src/openmax_il/omx_core/inc \
   hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc
OMX_VENDOR_WRAPPER := TI_OMX_Wrapper
BOARD_OPENCORE_LIBRARIES := libOMX_Core
BOARD_OPENCORE_FLAGS := -DHARDWARE_OMX=1
endif

WITH_JIT := true
ENABLE_JSC_JIT := true

#vold
# use pre-kernel.35 vold usb mounting
BOARD_USE_USB_MASS_STORAGE_SWITCH := true
BOARD_VOLD_EMMC_SHARES_DEV_MAJOR := true

#WITH_DEXPREOPT := true 