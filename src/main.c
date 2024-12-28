#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define ARRAY_COUNT(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

enum { SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600 };

void exit_with_error(const char *msg, ...);
void trace_log(const char *msg, ...);
void *xmalloc(size_t bytes);

VkInstance create_instance();
bool check_layer_support(const char **requested_layers, int requested_layer_count);
VkPhysicalDevice find_suitable_physical_device(VkInstance instance);
VkDevice create_logical_device(VkPhysicalDevice physical_device);
VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window);
VkSwapchainKHR create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice physical_device, VkDevice device);

static int g_graphics_family_index; // TODO: What?

int main(int argc, char **argv) {
    if (!glfwInit()) exit_with_error("Failed to intialize GLFW");

    trace_log("Initialized GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Explore Vulkan", NULL, NULL);
    if (!window) {
        exit_with_error("Failed to initialize GLFW window");
    }

    glfwMakeContextCurrent(window);

    VkInstance instance = create_instance();

    trace_log("Created Vulkan instance");

    VkPhysicalDevice physical_device = find_suitable_physical_device(instance);
    VkDevice device = create_logical_device(physical_device);

    VkSurfaceKHR surface = create_surface(instance, window);

    VkSwapchainKHR swapchain = create_swapchain(surface, physical_device, device);

    trace_log("Entering main loop");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    trace_log("Exiting gracefully");
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyDevice(device, NULL);
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

VkInstance create_instance() {
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

    return instance;
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

VkPhysicalDevice find_suitable_physical_device(VkInstance instance) {
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

    // NOTE: Example with llvm pipe:
    //       INFO: Device 0: llvmpipe (LLVM 19.1.0, 256 bits)
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

    VkPhysicalDevice physical_device = {0};
    if (device_count > 0) {
        physical_device  = physical_devices[0];
    }
    free(physical_devices);
    return physical_device;
}

VkDevice create_logical_device(VkPhysicalDevice physical_device) {
    uint32_t queue_family_count = 0;

    /*
      VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
      VkPhysicalDevice                            physicalDevice,
      uint32_t*                                   pQueueFamilyPropertyCount,
      VkQueueFamilyProperties*                    pQueueFamilyProperties);
    */
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    /*
      typedef struct VkQueueFamilyProperties {
      VkQueueFlags    queueFlags;
      uint32_t        queueCount;
      uint32_t        timestampValidBits;
      VkExtent3D      minImageTransferGranularity;
      } VkQueueFamilyProperties;
    */
    VkQueueFamilyProperties *queue_families = xmalloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    int graphics_family_index = -1;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        /*
          typedef enum VkQueueFlagBits {
              VK_QUEUE_GRAPHICS_BIT = 0x00000001,
              VK_QUEUE_COMPUTE_BIT = 0x00000002,
              VK_QUEUE_TRANSFER_BIT = 0x00000004,
              VK_QUEUE_SPARSE_BINDING_BIT = 0x00000008,
              VK_QUEUE_PROTECTED_BIT = 0x00000010,
              VK_QUEUE_VIDEO_DECODE_BIT_KHR = 0x00000020,
              VK_QUEUE_VIDEO_ENCODE_BIT_KHR = 0x00000040,
              VK_QUEUE_OPTICAL_FLOW_BIT_NV = 0x00000100,
              VK_QUEUE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
          } VkQueueFlagBits;
          typedef VkFlags VkQueueFlags;
          typedef VkFlags VkDeviceCreateFlags;
         */
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_index = i;
            break;
        }
    }

    if (graphics_family_index == -1) {
        exit_with_error("Failed to find a suitable queue family");
    }

    g_graphics_family_index = graphics_family_index;

    float queue_priority = 1.0f;
    /*
      typedef struct VkDeviceQueueCreateInfo {
          VkStructureType             sType;
          const void*                 pNext;
          VkDeviceQueueCreateFlags    flags;
          uint32_t                    queueFamilyIndex;
          uint32_t                    queueCount;
          const float*                pQueuePriorities;
      } VkDeviceQueueCreateInfo;
    */
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = graphics_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    /*
      typedef struct VkDeviceCreateInfo {
          VkStructureType                    sType;
          const void*                        pNext;
          VkDeviceCreateFlags                flags;
          uint32_t                           queueCreateInfoCount;
          const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
          uint32_t                           enabledLayerCount;
          const char* const*                 ppEnabledLayerNames;
          uint32_t                           enabledExtensionCount;
          const char* const*                 ppEnabledExtensionNames;
          const VkPhysicalDeviceFeatures*    pEnabledFeatures;
      } VkDeviceCreateInfo;
    */
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    const char *device_extensions[] = { "VK_KHR_swapchain" };
    device_create_info.enabledExtensionCount = 1;
    device_create_info.ppEnabledExtensionNames = device_extensions;

    /*
      Opaque pointer: VK_DEFINE_HANDLE(VkDevice)
    */
    VkDevice device;
    if (vkCreateDevice(physical_device, &device_create_info, NULL, &device) != VK_SUCCESS) {
        exit_with_error("Failed to create logical device");
    }

    trace_log("Logical device created successfully");
    free(queue_families);
    return device;
}

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window) {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        exit_with_error("Failed to create Vulkan surface");
    }
    return surface;
}

VkSwapchainKHR create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice physical_device, VkDevice device) {
    /*
      typedef struct VkSurfaceCapabilitiesKHR {
          uint32_t                         minImageCount;
          uint32_t                         maxImageCount;
          VkExtent2D                       currentExtent;
          VkExtent2D                       minImageExtent;
          VkExtent2D                       maxImageExtent;
          uint32_t                         maxImageArrayLayers;
          VkSurfaceTransformFlagsKHR       supportedTransforms;
          VkSurfaceTransformFlagBitsKHR    currentTransform;
          VkCompositeAlphaFlagsKHR         supportedCompositeAlpha;
          VkImageUsageFlags                supportedUsageFlags;
      } VkSurfaceCapabilitiesKHR;
    */
    VkSurfaceCapabilitiesKHR surface_capabilities;

    /*
      VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          VkPhysicalDevice                            physicalDevice,
          VkSurfaceKHR                                surface,
          VkSurfaceCapabilitiesKHR*                   pSurfaceCapabilities);
     */
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, NULL);
    VkSurfaceFormatKHR *formats = xmalloc(sizeof(VkSurfaceFormatKHR) * format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats);

    /*
      typedef struct VkSurfaceFormatKHR {
          VkFormat           format;
          VkColorSpaceKHR    colorSpace;
      } VkSurfaceFormatKHR;
    */
    VkSurfaceFormatKHR surface_format = formats[0];
    free(formats);

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
    VkPresentModeKHR *present_modes = xmalloc(sizeof(VkPresentModeKHR) * present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);

    /*
      typedef enum VkPresentModeKHR {
          VK_PRESENT_MODE_IMMEDIATE_KHR = 0,
          VK_PRESENT_MODE_MAILBOX_KHR = 1,
          VK_PRESENT_MODE_FIFO_KHR = 2,
          VK_PRESENT_MODE_FIFO_RELAXED_KHR = 3,
          VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR = 1000111000,
          VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR = 1000111001,
          VK_PRESENT_MODE_MAX_ENUM_KHR = 0x7FFFFFFF
      } VkPresentModeKHR;
     */
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Always available... so why did we do the previous?

    /*
      typedef struct VkExtent2D {
          uint32_t    width;
          uint32_t    height;
      } VkExtent2D;
    */
    VkExtent2D extent = surface_capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = SCREEN_WIDTH;
        extent.height = SCREEN_HEIGHT;
    }

    /*
      typedef struct VkSwapchainCreateInfoKHR {
          VkStructureType                  sType;
          const void*                      pNext;
          VkSwapchainCreateFlagsKHR        flags;
          VkSurfaceKHR                     surface;
          uint32_t                         minImageCount;
          VkFormat                         imageFormat;
          VkColorSpaceKHR                  imageColorSpace;
          VkExtent2D                       imageExtent;
          uint32_t                         imageArrayLayers;
          VkImageUsageFlags                imageUsage;
          VkSharingMode                    imageSharingMode;
          uint32_t                         queueFamilyIndexCount;
          const uint32_t*                  pQueueFamilyIndices;
          VkSurfaceTransformFlagBitsKHR    preTransform;
          VkCompositeAlphaFlagBitsKHR      compositeAlpha;
          VkPresentModeKHR                 presentMode;
          VkBool32                         clipped;
          VkSwapchainKHR                   oldSwapchain;
      } VkSwapchainCreateInfoKHR;
    */
    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = surface_capabilities.minImageCount;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageArrayLayers = 1;
    /*
      typedef enum VkImageUsageFlagBits {
           VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
           VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
           VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004,
           VK_IMAGE_USAGE_STORAGE_BIT = 0x00000008,
           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010,
           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020,
           VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT = 0x00000040,
           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT = 0x00000080,
           VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR = 0x00000400,
           VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR = 0x00000800,
           VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR = 0x00001000,
           VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT = 0x00000200,
           VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR = 0x00000100,
           VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT = 0x00400000,
           VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR = 0x00002000,
           VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR = 0x00004000,
           VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR = 0x00008000,
           VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT = 0x00080000,
           VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI = 0x00040000,
           VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM = 0x00100000,
           VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM = 0x00200000,
           VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
           VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      }  VkImageUsageFlagBits;
    */
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = {g_graphics_family_index};
    /*
      typedef enum VkSharingMode {
          VK_SHARING_MODE_EXCLUSIVE = 0,
          VK_SHARING_MODE_CONCURRENT = 1,
          VK_SHARING_MODE_MAX_ENUM = 0x7FFFFFFF
      } VkSharingMode;
    */
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = queue_family_indices;

    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    /*
      typedef enum VkCompositeAlphaFlagBitsKHR {
          VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001,
          VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR = 0x00000002,
          VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR = 0x00000004,
          VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR = 0x00000008,
          VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR = 0x7FFFFFFF
      } VkCompositeAlphaFlagBitsKHR;
    */
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain)) {
        exit_with_error("Failed to create swapchain");
    }

    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);
    VkImage *swapchain_images = xmalloc(sizeof(VkImage) * swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images);

    return swapchain;
}
