#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static bool enable_validation_layers = true;
static const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

bool check_validation_layer_support(const char **layers, int layer_count);

int main(int argc, char **argv) {
    if (!glfwInit()) {
        fprintf(stderr, "ERROR: Failed to intialize GLFW\n");
        return -1;
    }

    GLFWwindow *window = glfwCreateWindow(800, 600, "Hello world", NULL, NULL);
    if (!window) {
        fprintf(stderr, "ERROR: Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    VkInstance instance;

    /*
      typedef struct VkApplicationInfo {
      VkStructureType    sType;
      const void*        pNext;
      const char*        pApplicationName;
      uint32_t           applicationVersion;
      const char*        pEngineName;
      uint32_t           engineVersion;
      uint32_t           apiVersion;
      } VkApplicationInfo;
    */
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Explore Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Best Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    /*
      typedef struct VkInstanceCreateInfo {
      VkStructureType             sType;
      const void*                 pNext;
      VkInstanceCreateFlags       flags;
      const VkApplicationInfo*    pApplicationInfo;
      uint32_t                    enabledLayerCount;
      const char* const*          ppEnabledLayerNames;
      uint32_t                    enabledExtensionCount;
      const char* const*          ppEnabledExtensionNames;
      } VkInstanceCreateInfo;
    */
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // NOTE: Enable extensions that GLFW needs from Vulkan. On my machine right now:
    //       glfwExtensions[0] = VK_KHR_surface
    //       glfwExtensions[1] = VK_KHR_xcb_surface
    printf("\nINFO: Enumerating extensions GLFW needs from Vulkan:\n");
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        printf("INFO: glfwExtensions[%u] = %s\n", i, glfwExtensions[i]);
    }

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    // NOTE: Enable validation layers
    if (enable_validation_layers && !check_validation_layer_support(validation_layers, 1)) {
        fprintf(stderr, "ERROR: Validation layers requested but not available!\n");
    }

    // NOTE: Finally create VK instance
    /*
      VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
      const VkInstanceCreateInfo*                 pCreateInfo,
      const VkAllocationCallbacks*                pAllocator,
      VkInstance*                                 pInstance);
    */
    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        printf("ERROR: Failed to create Vulkan instance!\n");
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

bool check_validation_layer_support(const char **layers, int layer_count) {
    uint32_t available_layer_count;

    /*
      VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
      uint32_t*                                   pPropertyCount,
      VkLayerProperties*                          pProperties);
    */
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);

    /*
      typedef struct VkLayerProperties {
      char        layerName[VK_MAX_EXTENSION_NAME_SIZE];
      uint32_t    specVersion;
      uint32_t    implementationVersion;
      char        description[VK_MAX_DESCRIPTION_SIZE];
      } VkLayerProperties;a
    */
    VkLayerProperties *available_layers = malloc(sizeof(VkLayerProperties) * available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    // NOTE: Available layers on my system right now:
    //       available_layers[0] = VK_LAYER_MESA_device_select
    //       available_layers[1] = VK_LAYER_KHRONOS_validation
    //       available_layers[2] = VK_LAYER_INTEL_nullhw
    //       available_layers[3] = VK_LAYER_MESA_overlay
    printf("\nINFO: Enumarating available Vulkan layers:\n");
    for (int i = 0; i < available_layer_count; i++) {
        printf("INFO: available_layers[%d] = %s\n", i, available_layers[i].layerName);
    }

    free(available_layers);
    return false;
}
