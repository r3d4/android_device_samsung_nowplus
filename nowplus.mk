#
# Copyright (C) 2009 Texas Instruments
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

DEVICE_PACKAGE_OVERLAYS := device/samsung/nowplus/overlay


# we have enough storage space to hold precise GC data
PRODUCT_TAGS += dalvik.gc.type-precise


PRODUCT_PACKAGES += \
    screenshot \
    CMScreenshot \
	FM \
	hciconfig \
	hcitool \
    AndroidTerm \
    FileManager \
    CMParts \
    CMStats \
    CMUpdateNotify \
    DSPManager \
    libcyanogen-dsp \
    Pacman \
    CMWallpapers \
    acoustics.default \
    libtiOsLib



#add external packages
#PRODUCT_PACKAGES += \
#	Openmanager
	
# OMX components
# Addition of LOCAL_MODULE_TAGS requires us to specify
# libraries needed for a particular device
PRODUCT_PACKAGES += \
    dspexec \
    libbridge \
	01_Vendor_ti_omx.cfg \
	libVendor_ti_omx \
	libVendor_ti_omx_config_parser \
	libOMX_Core \
	libLCML \
	libOMX.TI.Video.Decoder \
	libOMX.TI.Video.encoder \
	libOMX.TI.WBAMR.decode \
	libOMX.TI.WBAMR.encode \
	libOMX.TI.AAC.encode \
	libOMX.TI.AAC.decode \
#	libOMX.TI.MP3.decode \
	libOMX.TI.WMA.decode \
	libOMX.TI.VPP \
	libOMX.TI.JPEG.encoder \
	libOMX.TI.JPEG.decoder \
	libOMX.TI.AMR.encode \
	libOMX.TI.AMR.decode \
    libomap_mm_library_jni \
    tiomxplayer     

# SkiaHW
PRODUCT_PACKAGES += \
        libskiahw
		
PRODUCT_PROPERTY_OVERRIDES += \
    ro.com.android.dateformat=MM-dd-yyyy \
    ro.com.android.dataroaming=true \
    ro.url.legal=http://www.google.com/intl/%s/mobile/android/basic/phone-legal.html \
    ro.url.legal.android_privacy=http://www.google.com/intl/%s/mobile/android/basic/privacy.html
   
PRODUCT_COPY_FILES +=  \
    vendor/cyanogen/prebuilt/hdpi/media/bootanimation.zip:system/media/bootanimation.zip
   
# root/
PRODUCT_COPY_FILES += \
    device/samsung/nowplus/init.rc:root/init.rc \
    device/samsung/nowplus/init.samsung.rc:root/init.samsung.rc \
    device/samsung/nowplus/ueventd.samsung.rc:root/ueventd.samsung.rc


# system/etc/
PRODUCT_COPY_FILES += \
    device/samsung/nowplus/prebuilt/etc/asound.conf:system/etc/asound.conf \
    device/samsung/nowplus/prebuilt/etc/vold.fstab:system/etc/vold.fstab \
    device/samsung/nowplus/prebuilt/etc/media_profiles.xml:system/etc/media_profiles.xml \
    device/samsung/nowplus/prebuilt/etc/apns.xml:system/etc/apns-conf.xml \
    device/samsung/nowplus/prebuilt/etc/gps.conf:system/etc/gps.conf \
    device/samsung/nowplus/prebuilt/etc/init.samsung.sh:system/etc/init.samsung.sh

# system/etc/wifi
PRODUCT_COPY_FILES += \
    device/samsung/nowplus/prebuilt/etc/wifi/firmware.bin:system/etc/wifi/firmware.bin \
    device/samsung/nowplus/prebuilt/etc/wifi/nvs_map.bin:system/etc/wifi/nvs_map.bin \
    device/samsung/nowplus/prebuilt/etc/wifi/tiwlan_drv.ko:system/etc/wifi/tiwlan_drv.ko \
    device/samsung/nowplus/prebuilt/etc/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    device/samsung/nowplus/prebuilt/etc/wifi/tiwlan.ini:system/etc/wifi/tiwlan.ini


# kernel modules
PRODUCT_COPY_FILES += \
    device/samsung/nowplus/prebuilt/modules/rfs.ko:system/lib/modules/rfs.ko \
    device/samsung/nowplus/prebuilt/modules/camera.ko:system/lib/modules/camera.ko \
    device/samsung/nowplus/prebuilt/modules/camera_pmic.ko:system/lib/modules/camera_pmic.ko \
    device/samsung/nowplus/prebuilt/modules/dpram.ko:system/lib/modules/dpram.ko \
    device/samsung/nowplus/prebuilt/modules/kxsd9.ko:system/lib/modules/kxsd9.ko \
    device/samsung/nowplus/prebuilt/modules/main_sensor.ko:system/lib/modules/main_sensor.ko \
    device/samsung/nowplus/prebuilt/modules/multipdp.ko:system/lib/modules/multipdp.ko \
    device/samsung/nowplus/prebuilt/modules/param.ko:system/lib/modules/param.ko \
    device/samsung/nowplus/prebuilt/modules/pl_sensor.ko:system/lib/modules/pl_sensor.ko \
    device/samsung/nowplus/prebuilt/modules/rfs.ko:system/lib/modules/rfs.ko \
    device/samsung/nowplus/prebuilt/modules/Si4709_driver.ko:system/lib/modules/Si4709_driver.ko \
    device/samsung/nowplus/prebuilt/modules/vibrator.ko:system/lib/modules/vibrator.ko \
    device/samsung/nowplus/prebuilt/modules/vt_sensor.ko:system/lib/modules/vt_sensor.ko


# system/media/
#PRODUCT_COPY_FILES += \
#    device/samsung/nowplus/prebuilt/media/bootanimation.zip:system/media/bootanimation.zip
	
	
# permissions/ Install the features available on this device.
PRODUCT_COPY_FILES += \
    packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml \
    frameworks/base/data/etc/platform.xml:system/etc/permissions/platform.xml \
    frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.multitouch.distinct.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.distinct.xml \
    frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/base/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml
  

# Prebuilt kl keymaps
PRODUCT_COPY_FILES += \
    device/samsung/nowplus/TWL4030_Keypad.kl:system/usr/keylayout/TWL4030_Keypad.kl \
    device/samsung/nowplus/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl

# Generated kcm keymaps
PRODUCT_PACKAGES += \
        TWL4030_Keypad.kcm\
        gpio-keys.kcm
# Pick up audio package
#include frameworks/base/data/sounds/AudioPackage2.mk

#PRODUCT_LOCALES := hdpi en_GB de_DE fr_FR es_ES
CUSTOM_LOCALES += hdpi
PRODUCT_DEFAULT_LANGUAGE := en_GB

# Overrides
PRODUCT_SPECIFIC_DEFINES += TARGET_PRELINKER_MAP=$(TOP)/device/samsung/nowplus/prelink-linux-arm-nowplus.map

$(call inherit-product-if-exists, vendor/samsung/galaxysl/nopwlus-vendor.mk)
