#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define ARRAY_COUNT(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

bool check_layer_support(const char **requested_layers, int requested_layer_count);

int main(int argc, char **argv) {
    if (!glfwInit()) {
        fprintf(stderr, "ERROR: Failed to intialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Explore Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Best Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

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
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // NOTE: Enable extensions that GLFW needs from Vulkan. On my machine right now:
    //       glfw_extensions[0] = VK_KHR_surface
    //       glfw_extensions[1] = VK_KHR_xcb_surface
    printf("\nINFO: Enumerating extensions GLFW needs from Vulkan:\n");
    for (uint32_t i = 0; i < glfw_extension_count; i++) {
        printf("INFO: glfw_extensions[%u] = %s\n", i, glfw_extensions[i]);
    }

    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    // NOTE: Validation layers
    const char *requested_layers[] = { "VK_LAYER_KHRONOS_validation" };

    if (!check_layer_support(requested_layers, ARRAY_COUNT(requested_layers))) {
        fprintf(stderr, "ERROR: Requested Vulkan layers are not available.");
        return -1;
    }

    create_info.enabledLayerCount = ARRAY_COUNT(requested_layers);
    create_info.ppEnabledLayerNames = requested_layers;

    // NOTE: Finally create VK instance
    /*
      VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
      const VkInstanceCreateInfo*                 pCreateInfo,
      const VkAllocationCallbacks*                pAllocator,
      VkInstance*                                 pInstance);
    */
    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to create Vulkan instance!\n");
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

bool check_layer_support(const char **requested_layers, int requested_layer_count) {
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
    for (uint32_t i = 0; i < available_layer_count; i++) {
        printf("INFO: available_layers[%d] = %s\n", i, available_layers[i].layerName);
    }

    bool layers_valid = true;
    for (int requested_layer_i = 0; requested_layer_i < requested_layer_count; requested_layer_i++) {
        bool layer_found = false;

        for (uint32_t available_layer_i = 0; available_layer_i < available_layer_count; available_layer_i++) {
            bool name_equals = strcmp(requested_layers[requested_layer_i],
                                      available_layers[available_layer_i].layerName) == 0;

            if (name_equals) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            layers_valid = false;
            break;
        }
    }

    free(available_layers);
    return layers_valid;
}
