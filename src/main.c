#include <GLFW/glfw3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define ARRAY_COUNT(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

enum { SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600 };

void exit_with_error(const char *msg, ...);
void trace_log(const char *msg, ...);
void *xmalloc(size_t bytes);

bool check_layer_support(const char **requested_layers, int requested_layer_count);
void enumerate_physical_devices(VkInstance instance);

int main(int argc, char **argv) {
    if (!glfwInit()) exit_with_error("Failed to intialize GLFW");

    trace_log("Initialized GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Explore Vulkan", NULL, NULL);
    if (!window) {
        exit_with_error("Failed to initialize GLFW window");
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
    trace_log("Enumerating extensions GLFW needs from Vulkan:");
    for (uint32_t i = 0; i < glfw_extension_count; i++) {
        trace_log("  glfw_extensions[%u] = %s", i, glfw_extensions[i]);
    }

    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    // NOTE: Validation layers
    const char *requested_layers[] = { "VK_LAYER_KHRONOS_validation" };

    if (!check_layer_support(requested_layers, ARRAY_COUNT(requested_layers))) {
        exit_with_error("Requested Vulkan layers are not available");
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
        exit_with_error("Failed to create Vulkan instance");
    }

    trace_log("Created Vulkan instance");

    enumerate_physical_devices(instance);

    trace_log("Entering main loop");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    trace_log("Exiting gracefully");
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void exit_with_error(const char *msg, ...) {
    fprintf(stderr, "FATAL: ");
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    glfwTerminate();
    exit(1);
}

void trace_log(const char *msg, ...) {
    printf("INFO: ");
    va_list ap;
    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);
    printf("\n");
}

void *xmalloc(size_t bytes) {
    void *d = malloc(bytes);
    if (d == NULL) exit_with_error("Failed to malloc");
    return d;
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
    VkLayerProperties *available_layers = xmalloc(sizeof(VkLayerProperties) * available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    // NOTE: Available layers on my system right now:
    //       available_layers[0] = VK_LAYER_MESA_device_select
    //       available_layers[1] = VK_LAYER_KHRONOS_validation
    //       available_layers[2] = VK_LAYER_INTEL_nullhw
    //       available_layers[3] = VK_LAYER_MESA_overlay
    trace_log("Enumarating available Vulkan layers:");
    for (uint32_t i = 0; i < available_layer_count; i++) {
        trace_log("  available_layers[%d] = %s", i, available_layers[i].layerName);
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

void enumerate_physical_devices(VkInstance instance) {
    uint32_t device_count = 0;

    /*
      VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
      VkInstance                                  instance,
      uint32_t*                                   pPhysicalDeviceCount,
      VkPhysicalDevice*                           pPhysicalDevices);
    */
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    if (device_count == 0) {
        exit_with_error("Failed to find GPUs that support Vulkan");
    }

    /*
      VkPhysicalDevice is an opaque pointer in vulkan_core.h:
      VK_DEFINE_HANDLE(VkPhysicalDevice)
      #define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
      Actual definition is by each implementation.
    */
    VkPhysicalDevice *physical_devices = xmalloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices);

    trace_log("Enumerating found physical_devices:");
    for (uint32_t i = 0; i < device_count; i++) {
        /*
          typedef struct VkPhysicalDeviceProperties {
          uint32_t                            apiVersion;
          uint32_t                            driverVersion;
          uint32_t                            vendorID;
          uint32_t                            deviceID;
          VkPhysicalDeviceType                deviceType;
          char                                deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
          uint8_t                             pipelineCacheUUID[VK_UUID_SIZE];
          VkPhysicalDeviceLimits              limits;
          VkPhysicalDeviceSparseProperties    sparseProperties;
          } VkPhysicalDeviceProperties;
        */
        VkPhysicalDeviceProperties device_properties;
        /*
          VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
          VkPhysicalDevice                            physicalDevice,
          VkPhysicalDeviceProperties*                 pProperties);
        */
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_properties);

        trace_log("Device %d: %s", i, device_properties.deviceName);
    }
}
