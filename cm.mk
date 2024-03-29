# Copyright (C) 2015 The CyanogenMod Project
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

$(call inherit-product, device/dexp/mlte5/full_mlte5.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

PRODUCT_NAME := cm_mlte5
BOARD_VENDOR := dexp
PRODUCT_DEVICE := mlte5

PRODUCT_GMS_CLIENTID_BASE := android-dexp

PRODUCT_MANUFACTURER := DEXP
PRODUCT_MODEL := MLTE5

PRODUCT_BRAND := DEXP
TARGET_VENDOR := DEXP
TARGET_VENDOR_PRODUCT_NAME := MLTE5
TARGET_VENDOR_DEVICE_NAME := MLTE5
