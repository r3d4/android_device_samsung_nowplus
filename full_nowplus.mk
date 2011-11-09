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

#
# This file is the build configuration for a full Android
# build for panda hardware. This cleanly combines a set of
# device-specific aspects (drivers) with a device-agnostic
# product configuration (apps).
#

# Inherit AOSP device configuration
$(call inherit-product, device/samsung/nowplus/nowplus.mk)

# Inherit from those products. Most specific first.
#$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

$(call inherit-product, $(SRC_TARGET_DIR)/product/full.mk)

# Include GSM stuff
$(call inherit-product, vendor/cyanogen/products/gsm.mk)

# Inherit some common cyanogenmod stuff.
#$(call inherit-product, vendor/cyanogen/products/common_full.mk)
#$(call inherit-product, vendor/cyanogen/products/themes_common.mk)

# Discard inherited values and use our own instead.

PRODUCT_NAME := nowplus
PRODUCT_DEVICE := nowplus
PRODUCT_MODEL := GT-I8320
PRODUCT_BRAND := samsung
PRODUCT_MANUFACTURER := samsung

#PRODUCT_BUILD_PROP_OVERRIDES := BUILD_ID=FRG83G PRODUCT_NAME=voles TARGET_DEVICE=sholes BUILD_FINGERPRINT=verizon/voles/sholes/sholes:2.2.2/FRG83G/91102:user/release-keys PRODUCT_BRAND=verizon PRIVATE_BUILD_DESC="voles-user 2.2.2 FRG83G 91102 release-keys" BUILD_NUMBER=91102 BUILD_UTC_DATE=1294972140 TARGET_BUILD_TYPE=user BUILD_VERSION_TAGS=release-keys USER=android-build


# Release name and versioning
PRODUCT_RELEASE_NAME := nowplus
PRODUCT_VERSION_DEVICE_SPECIFIC :=
-include vendor/cyanogen/products/common_versions.mk

# Common CM overlay
PRODUCT_PACKAGE_OVERLAYS += vendor/cyanogen/overlay/common
