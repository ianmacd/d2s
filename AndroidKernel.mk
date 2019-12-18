#Android makefile to build kernel as a part of Android Build

ifeq ($(KERNEL_DEFCONFIG),)
$(error KERNEL_DEFCONFIG must be set as environment variable)
endif

ifeq ($(INSTALLED_KERNEL_TARGET),)
INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
INSTALLED_DTBOIMAGE_TARGET := $(PRODUCT_OUT)/dtbo.img
BOARD_PREBUILT_DTBOIMAGE := $(PRODUCT_OUT)/prebuilt_dtbo.img
INSTALLED_DTB_TARGET := $(PRODUCT_OUT)/dtb.img
endif

TARGET_KERNEL_ARCH := $(strip $(TARGET_KERNEL_ARCH))
ifeq ($(TARGET_KERNEL_ARCH),)
KERNEL_ARCH := arm64
else
KERNEL_ARCH := $(TARGET_KERNEL_ARCH)
endif

ifeq ($(CROSS_COMPILE),)
KERNEL_CROSS_COMPILE := aarch64-linux-android-
else
KERNEL_CROSS_COMPILE := $(CROSS_COMPILE)
endif

SOONG_GLOBAL_CONFIG := build/soong/cc/config/global.go
CLANG_VERSION := $(shell  grep "ClangDefaultVersion" $(SOONG_GLOBAL_CONFIG) | grep -o "clang-[0-9][0-9]*")
CLANG_PATH := prebuilts/clang/host/linux-x86
CC :=$(PWD)/$(CLANG_PATH)/$(CLANG_VERSION)/bin/clang

ifeq ($(CLANG_TRIPLE),)
CLANG_TRIPLE := aarch64-linux-gnu-
else
CLANG_TRIPLE := $(CLANG_TRIPLE)
endif

ifeq ($(TARGET_PREBUILT_KERNEL),)

TARGET_KERNEL_SOURCE := kernel/$(TARGET_KERNEL)
KERNEL_CONFIG := $(TARGET_KERNEL_SOURCE)/.config
KERNEL_BOOT := $(TARGET_KERNEL_SOURCE)/arch/$(KERNEL_ARCH)/boot
KERNEL_BIN := $(KERNEL_BOOT)/Image
KERNEL_DTB_DIR := $(KERNEL_BOOT)/dts/exynos
ifeq ($(TARGET_USE_EVT0),true)
KERNEL_DTB := $(KERNEL_DTB_DIR)/$(TARGET_SOC)_evt0.dtb
KERNEL_DTBO_CFG := $(KERNEL_DTB_DIR)/$(TARGET_SOC)_evt0_dtboimg.cfg
else
KERNEL_DTB := $(KERNEL_DTB_DIR)/$(TARGET_SOC).dtb
KERNEL_DTBO_CFG := $(KERNEL_DTB_DIR)/$(TARGET_SOC)_dtboimg.cfg
endif

MKDTIMG := $(HOST_OUT_EXECUTABLES)/mkdtimg

KERNEL_MERGE_CONFIG := $(TARGET_KERNEL_SOURCE)/scripts/kconfig/merge_config.sh
KERNEL_CONFIG_BASE := $(TARGET_KERNEL_SOURCE)/arch/$(KERNEL_ARCH)/configs
KERNEL_DEFCONFIG_PATH := $(KERNEL_CONFIG_BASE)/$(KERNEL_DEFCONFIG)
KERNEL_USER_CFG := $(KERNEL_CONFIG_BASE)/$(TARGET_SOC)_user.cfg
KERNEL_USERDEBUG_CFG := $(KERNEL_CONFIG_BASE)/$(TARGET_SOC)_userdebug.cfg

ifeq ($(KERNEL_DEFCONFIG),)
$(error Kernel configuration not defined, cannot build kernel)
else

ifeq ($(TARGET_BUILD_VARIANT),eng)
MAKE_CONFIG_CMD := $(MAKE) -C $(TARGET_KERNEL_SOURCE) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) $(KERNEL_DEFCONFIG)
else
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
MAKE_CONFIG_CMD := ARCH=$(KERNEL_ARCH) $(KERNEL_MERGE_CONFIG) -m -O $(TARGET_KERNEL_SOURCE) $(KERNEL_DEFCONFIG_PATH) $(KERNEL_USERDEBUG_CFG);
MAKE_CONFIG_CMD += $(MAKE) -C $(TARGET_KERNEL_SOURCE) ARCH=$(KERNEL_ARCH) KCONFIG_ALLCONFIG=.config alldefconfig
else
MAKE_CONFIG_CMD := ARCH=$(KERNEL_ARCH) $(KERNEL_MERGE_CONFIG) -m -O $(TARGET_KERNEL_SOURCE) $(KERNEL_DEFCONFIG_PATH) $(KERNEL_USER_CFG);
MAKE_CONFIG_CMD += $(MAKE) -C $(TARGET_KERNEL_SOURCE) ARCH=$(KERNEL_ARCH) KCONFIG_ALLCONFIG=.config alldefconfig
endif
endif

ifeq ($(N_KERNEL_BUILD_THREAD),)
N_KERNEL_BUILD_THREAD := 1
endif

TARGET_PREBUILT_KERNEL := $(KERNEL_BIN)

.PHONY: phony-rebuild

.PHONY: kernel
kernel: $(KERNEL_BIN)

.PHONY: kernel-distclean
kernel-distclean:
	$(MAKE) -C $(TARGET_KERNEL_SOURCE) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) distclean

$(KERNEL_CONFIG): phony-rebuild
	$(hide) echo "make $(KERNEL_DEFCONFIG)"
	$(MAKE_CONFIG_CMD)

$(KERNEL_BIN): $(KERNEL_CONFIG)
	$(hide) echo "Building kernel..."
	$(MAKE) -C $(TARGET_KERNEL_SOURCE) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) CLANG_TRIPLE=$(CLANG_TRIPLE) CC=$(CC) -j$(N_KERNEL_BUILD_THREAD)

$(INSTALLED_KERNEL_TARGET): $(INSTALLED_DTBOIMAGE_TARGET)
	cp $(KERNEL_BIN) $(INSTALLED_KERNEL_TARGET)
	cp $(KERNEL_DTB) $(INSTALLED_DTB_TARGET)

$(BOARD_PREBUILT_DTBOIMAGE): $(MKDTIMG) $(KERNEL_DTBO_CFG) $(KERNEL_BIN)
	$(hide) echo "Building DTBOIMAGE..."
	ln -sf $(TARGET_KERNEL_SOURCE)/arch
	$(MKDTIMG) cfg_create $@ $(KERNEL_DTBO_CFG)
	rm -f arch

endif #TARGET_PREBUILT_KERNEL
endif #KERNEL_DEFCONFIG
