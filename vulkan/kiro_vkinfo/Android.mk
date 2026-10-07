# KiroOS test tool for the loader's Vulkan 1.1 layer: built on request (make kiro_vkinfo),
# never in a product.
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := kiro_vkinfo
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := kiro_vkinfo.cpp
LOCAL_C_INCLUDES := frameworks/native/vulkan/include
LOCAL_CFLAGS := -DVK_USE_PLATFORM_ANDROID_KHR -Wno-unused-parameter
LOCAL_CPPFLAGS := -std=c++14
LOCAL_SHARED_LIBRARIES := libvulkan
LOCAL_MULTILIB := 64
include $(BUILD_EXECUTABLE)

# the swapchain on a real surface, through the system loader
include $(CLEAR_VARS)
LOCAL_MODULE := kiro_vkswap
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := kiro_vkswap.cpp
LOCAL_C_INCLUDES := frameworks/native/vulkan/include
LOCAL_CFLAGS := -DVK_USE_PLATFORM_ANDROID_KHR -Wno-unused-parameter
LOCAL_CPPFLAGS := -std=c++14
LOCAL_SHARED_LIBRARIES := libvulkan libgui libui libutils libbinder libcutils
LOCAL_MULTILIB := 64
include $(BUILD_EXECUTABLE)
