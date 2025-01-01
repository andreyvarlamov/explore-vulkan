#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define array_count(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

enum { SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600 };

typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    VkImage *swapchain_images;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
} Swapchain_Etc;

typedef struct {
    VkDevice device;
    int graphics_queue_family_index;
} Logical_Device_Etc;

typedef struct {
    float position[2];
    float color[3];
} Vertex;

void exit_with_error(const char *msg, ...);
void trace_log(const char *msg, ...);
void *xmalloc(size_t bytes);

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

VkInstance create_instance();
bool check_layer_support(const char **requested_layers, int requested_layer_count);
VkPhysicalDevice find_suitable_physical_device(VkInstance instance);
Logical_Device_Etc create_logical_device(VkPhysicalDevice physical_device);

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window);
Swapchain_Etc create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice physical_device, Logical_Device_Etc logical_device);
VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_image_format);
VkImageView *create_image_views(VkDevice device, VkFormat swapchain_image_format, VkImage *swapchain_images, uint32_t image_count);
VkFramebuffer *create_framebuffers(VkDevice device,
                                   VkRenderPass render_pass,
                                   VkExtent2D swapchain_extent,
                                   VkImageView *swapchain_image_views,
                                   uint32_t image_count);

VkShaderModule create_shader_module(VkDevice device, const char *file_name);
VkPipelineLayout create_pipeline_layout(VkDevice device);
VkVertexInputBindingDescription get_binding_description();
VkVertexInputAttributeDescription *get_attribute_descriptions();
VkPipeline create_graphics_pipeline(VkDevice device, VkExtent2D swapchain_extent, VkRenderPass render_pass, VkPipelineLayout pipeline_layout);

int main() {
    if (!glfwInit()) exit_with_error("Failed to intialize GLFW");

    trace_log("Initialized GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Explore Vulkan", NULL, NULL);
    if (!window) {
        exit_with_error("Failed to initialize GLFW window");
    }

    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, keyboard_callback);

    VkInstance instance = create_instance();

    trace_log("Created Vulkan instance");

    VkPhysicalDevice physical_device = find_suitable_physical_device(instance);
    Logical_Device_Etc logical_device = create_logical_device(physical_device);

    // Surface <- Swapchain image <- image view <- framebuffer?
    VkSurfaceKHR surface = create_surface(instance, window);
    Swapchain_Etc swapchain_etc = create_swapchain(surface, physical_device, logical_device);
    VkRenderPass render_pass = create_render_pass(logical_device.device, swapchain_etc.swapchain_image_format);
    VkImageView *swapchain_image_views = create_image_views(logical_device.device,
                                                            swapchain_etc.swapchain_image_format,
                                                            swapchain_etc.swapchain_images,
                                                            swapchain_etc.swapchain_image_count);
    VkFramebuffer *swapchain_framebuffers = create_framebuffers(logical_device.device,
                                                                render_pass,
                                                                swapchain_etc.swapchain_extent,
                                                                swapchain_image_views,
                                                                swapchain_etc.swapchain_image_count);


    VkPipelineLayout pipeline_layout = create_pipeline_layout(logical_device.device);
    VkPipeline pipeline = create_graphics_pipeline(logical_device.device,
                                                   swapchain_etc.swapchain_extent,
                                                   render_pass,
                                                   pipeline_layout);


    trace_log("Entering main loop");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    trace_log("Exiting gracefully");

    vkDestroyPipeline(logical_device.device, pipeline, NULL);
    vkDestroyPipelineLayout(logical_device.device, pipeline_layout, NULL);
    for (uint32_t i = 0; i < swapchain_etc.swapchain_image_count; i++) {
        vkDestroyFramebuffer(logical_device.device, swapchain_framebuffers[i], NULL);
        vkDestroyImageView(logical_device.device, swapchain_image_views[i], NULL);
    }
    free(swapchain_framebuffers);
    free(swapchain_image_views);
    vkDestroyRenderPass(logical_device.device, render_pass, NULL);
    vkDestroySwapchainKHR(logical_device.device, swapchain_etc.swapchain, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyDevice(logical_device.device, NULL);
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

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window; (void)key; (void)scancode; (void)action; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        trace_log("Received ESC. Terminating...");
        glfwSetWindowShouldClose(window, true);
    }
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

    if (!check_layer_support(requested_layers, array_count(requested_layers))) {
        exit_with_error("Requested Vulkan layers are not available");
    }

    create_info.enabledLayerCount = array_count(requested_layers);
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

    VkPhysicalDevice physical_device = physical_devices[0];
    free(physical_devices);
    return physical_device;
}

Logical_Device_Etc create_logical_device(VkPhysicalDevice physical_device) {
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

    Logical_Device_Etc logical_device;
    logical_device.device = device;
    logical_device.graphics_queue_family_index = graphics_family_index;
    return logical_device;
}

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window) {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        exit_with_error("Failed to create Vulkan surface");
    }
    return surface;
}

Swapchain_Etc create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice physical_device, Logical_Device_Etc logical_device) {
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
    free(present_modes);

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

    uint32_t queue_family_indices[] = {logical_device.graphics_queue_family_index};
    /*
      typedef enum VkSharingMode {
          VK_SHARING_MODE_EXCLUSIVE = 0,
          VK_SHARING_MODE_CONCURRENT = 1,
          VK_SHARING_MODE_MAX_ENUM = 0x7FFFFFFF
      } VkSharingMode;
    */
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // ?
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = queue_family_indices; // ?
    swapchain_create_info.preTransform = surface_capabilities.currentTransform; // ?
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
    if (vkCreateSwapchainKHR(logical_device.device, &swapchain_create_info, NULL, &swapchain)) {
        exit_with_error("Failed to create swapchain");
    }

    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(logical_device.device, swapchain, &swapchain_image_count, NULL);
    /*
      VkImage is opaque pointer: VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImage)
    */
    VkImage *swapchain_images = xmalloc(sizeof(VkImage) * swapchain_image_count);
    vkGetSwapchainImagesKHR(logical_device.device, swapchain, &swapchain_image_count, swapchain_images);

    Swapchain_Etc result;
    result.swapchain = swapchain;
    result.swapchain_image_count = swapchain_image_count;
    result.swapchain_images = swapchain_images;
    result.swapchain_extent = extent;
    result.swapchain_image_format = surface_format.format;
    return result;
}

VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_image_format) {
    /*
      typedef struct VkAttachmentDescription {
           VkAttachmentDescriptionFlags    flags;
           VkFormat                        format;
           VkSampleCountFlagBits           samples;
           VkAttachmentLoadOp              loadOp;
           VkAttachmentStoreOp             storeOp;
           VkAttachmentLoadOp              stencilLoadOp;
           VkAttachmentStoreOp             stencilStoreOp;
           VkImageLayout                   initialLayout;
           VkImageLayout                   finalLayout;
       } VkAttachmentDescription;
    */
    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = swapchain_image_format;
    /*
      typedef enum VkSampleCountFlagBits {
          VK_SAMPLE_COUNT_1_BIT = 0x00000001,
          VK_SAMPLE_COUNT_2_BIT = 0x00000002,
          VK_SAMPLE_COUNT_4_BIT = 0x00000004,
          VK_SAMPLE_COUNT_8_BIT = 0x00000008,
          VK_SAMPLE_COUNT_16_BIT = 0x00000010,
          VK_SAMPLE_COUNT_32_BIT = 0x00000020,
          VK_SAMPLE_COUNT_64_BIT = 0x00000040,
          VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkSampleCountFlagBits;
      typedef VkFlags VkSampleCountFlags;
    */
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
    /*
      typedef enum VkAttachmentLoadOp {
          VK_ATTACHMENT_LOAD_OP_LOAD = 0,
          VK_ATTACHMENT_LOAD_OP_CLEAR = 1,
          VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2,
          VK_ATTACHMENT_LOAD_OP_NONE_KHR = 1000400000,
          VK_ATTACHMENT_LOAD_OP_NONE_EXT = VK_ATTACHMENT_LOAD_OP_NONE_KHR,
          VK_ATTACHMENT_LOAD_OP_MAX_ENUM = 0x7FFFFFFF
      } VkAttachmentLoadOp;
    */
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear the image at the start
    /*
      typedef enum VkAttachmentStoreOp {
          VK_ATTACHMENT_STORE_OP_STORE = 0,
          VK_ATTACHMENT_STORE_OP_DONT_CARE = 1,
          VK_ATTACHMENT_STORE_OP_NONE = 1000301000,
          VK_ATTACHMENT_STORE_OP_NONE_KHR = VK_ATTACHMENT_STORE_OP_NONE,
          VK_ATTACHMENT_STORE_OP_NONE_QCOM = VK_ATTACHMENT_STORE_OP_NONE,
          VK_ATTACHMENT_STORE_OP_NONE_EXT = VK_ATTACHMENT_STORE_OP_NONE,
          VK_ATTACHMENT_STORE_OP_MAX_ENUM = 0x7FFFFFFF
      } VkAttachmentStoreOp;
    */
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered image
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // No stencil, so no load op
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // No stencil, so no store op
    /*
      typedef enum VkImageLayout {
          VK_IMAGE_LAYOUT_UNDEFINED = 0,
          VK_IMAGE_LAYOUT_GENERAL = 1,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
          ...
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002,
          ...
      } VkImageLayout;
    */
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout before rendering
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Layout for presentation

    /*
      typedef struct VkAttachmentReference {
          uint32_t         attachment;
          VkImageLayout    layout;
      } VkAttachmentReference;
    */
    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0; // Index in the attachment array (in subpass)
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /*
      typedef struct VkSubpassDescription {
          VkSubpassDescriptionFlags       flags;
          VkPipelineBindPoint             pipelineBindPoint;
          uint32_t                        inputAttachmentCount;
          const VkAttachmentReference*    pInputAttachments;
          uint32_t                        colorAttachmentCount;
          const VkAttachmentReference*    pColorAttachments;
          const VkAttachmentReference*    pResolveAttachments;
          const VkAttachmentReference*    pDepthStencilAttachment;
          uint32_t                        preserveAttachmentCount;
          const uint32_t*                 pPreserveAttachments;
      } VkSubpassDescription;
    */
    VkSubpassDescription subpass = {0};
    /*
      typedef enum VkPipelineBindPoint {
          VK_PIPELINE_BIND_POINT_GRAPHICS = 0,
          VK_PIPELINE_BIND_POINT_COMPUTE = 1,
      #ifdef VK_ENABLE_BETA_EXTENSIONS
          VK_PIPELINE_BIND_POINT_EXECUTION_GRAPH_AMDX = 1000134000,
      #endif
          VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR = 1000165000,
          VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI = 1000369003,
          VK_PIPELINE_BIND_POINT_RAY_TRACING_NV = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
          VK_PIPELINE_BIND_POINT_MAX_ENUM = 0x7FFFFFFF
      } VkPipelineBindPoint;
    */
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    /*
      typedef struct VkSubpassDependency {
          uint32_t                srcSubpass;
          uint32_t                dstSubpass;
          VkPipelineStageFlags    srcStageMask;
          VkPipelineStageFlags    dstStageMask;
          VkAccessFlags           srcAccessMask;
          VkAccessFlags           dstAccessMask;
          VkDependencyFlags       dependencyFlags;
      } VkSubpassDependency;
    */
    VkSubpassDependency dependency = {0};
    // #define VK_SUBPASS_EXTERNAL               (~0U)
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // Before the render pass
    dependency.dstSubpass = 0; // First subpass
    /*
      typedef enum VkPipelineStageFlagBits {
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
          VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002,
          VK_PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004,
          VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008,
          VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x00000010,
          VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
          VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800,
          VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000,
          VK_PIPELINE_STAGE_HOST_BIT = 0x00004000,
          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT = 0x00008000,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000,
          VK_PIPELINE_STAGE_NONE = 0,
          VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT = 0x01000000,
          VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT = 0x00040000,
          VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR = 0x02000000,
          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR = 0x00200000,
          VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT = 0x00800000,
          VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR = 0x00400000,
          VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV = 0x00020000,
          VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT = 0x00080000,
          VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT = 0x00100000,
          VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
          VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
          VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV = VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT,
          VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV = VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
          VK_PIPELINE_STAGE_NONE_KHR = VK_PIPELINE_STAGE_NONE,
          VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkPipelineStageFlagBits;
      typedef VkFlags VkPipelineStageFlags;
    */
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    /*
      typedef enum VkAccessFlagBits {
          VK_ACCESS_INDIRECT_COMMAND_READ_BIT = 0x00000001,
          VK_ACCESS_INDEX_READ_BIT = 0x00000002,
          VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = 0x00000004,
          VK_ACCESS_UNIFORM_READ_BIT = 0x00000008,
          VK_ACCESS_INPUT_ATTACHMENT_READ_BIT = 0x00000010,
          VK_ACCESS_SHADER_READ_BIT = 0x00000020,
          VK_ACCESS_SHADER_WRITE_BIT = 0x00000040,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT = 0x00000080,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = 0x00000200,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400,
          VK_ACCESS_TRANSFER_READ_BIT = 0x00000800,
          VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000,
          VK_ACCESS_HOST_READ_BIT = 0x00002000,
          VK_ACCESS_HOST_WRITE_BIT = 0x00004000,
          VK_ACCESS_MEMORY_READ_BIT = 0x00008000,
          VK_ACCESS_MEMORY_WRITE_BIT = 0x00010000,
          VK_ACCESS_NONE = 0,
          VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT = 0x02000000,
          VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT = 0x04000000,
          VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT = 0x08000000,
          VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT = 0x00100000,
          VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT = 0x00080000,
          VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR = 0x00200000,
          VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR = 0x00400000,
          VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT = 0x01000000,
          VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR = 0x00800000,
          VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV = 0x00020000,
          VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV = 0x00040000,
          VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
          VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
          VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
          VK_ACCESS_NONE_KHR = VK_ACCESS_NONE,
          VK_ACCESS_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkAccessFlagBits;
      typedef VkFlags VkAccessFlags;
    */
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    /*
      typedef struct VkRenderPassCreateInfo {
          VkStructureType                   sType;
          const void*                       pNext;
          VkRenderPassCreateFlags           flags;
          uint32_t                          attachmentCount;
          const VkAttachmentDescription*    pAttachments;
          uint32_t                          subpassCount;
          const VkSubpassDescription*       pSubpasses;
          uint32_t                          dependencyCount;
          const VkSubpassDependency*        pDependencies;
      } VkRenderPassCreateInfo;
    */
    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    /*
      VkRenderPass is an opaque pointer: VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkRenderPass)
    */
    VkRenderPass render_pass;
    if (vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass) != VK_SUCCESS) {
        exit_with_error("Failed to create render pass");
    }
    return render_pass;
}

VkImageView *create_image_views(VkDevice device, VkFormat swapchain_image_format, VkImage *swapchain_images, uint32_t image_count) {
    /*
      VkImageView is an opaque pointer: VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImageView)
    */
    VkImageView *swapchain_image_views = xmalloc(sizeof(VkImageView) * image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        /*
          typedef struct VkImageViewCreateInfo {
              VkStructureType            sType;
              const void*                pNext;
              VkImageViewCreateFlags     flags;
              VkImage                    image;
              VkImageViewType            viewType;
              VkFormat                   format;
              VkComponentMapping         components;
              VkImageSubresourceRange    subresourceRange;
          } VkImageViewCreateInfo;
        */
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images[i];
        /*
          typedef enum VkImageViewType {
              VK_IMAGE_VIEW_TYPE_1D = 0,
              VK_IMAGE_VIEW_TYPE_2D = 1,
              VK_IMAGE_VIEW_TYPE_3D = 2,
              VK_IMAGE_VIEW_TYPE_CUBE = 3,
              VK_IMAGE_VIEW_TYPE_1D_ARRAY = 4,
              VK_IMAGE_VIEW_TYPE_2D_ARRAY = 5,
              VK_IMAGE_VIEW_TYPE_CUBE_ARRAY = 6,
              VK_IMAGE_VIEW_TYPE_MAX_ENUM = 0x7FFFFFFF
          } VkImageViewType;
        */
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_image_format;

        /*
          typedef struct VkComponentMapping {
              VkComponentSwizzle    r;
              VkComponentSwizzle    g;
              VkComponentSwizzle    b;
              VkComponentSwizzle    a;
          } VkComponentMapping;
         */
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        /*
          typedef struct VkImageSubresourceRange {
              VkImageAspectFlags    aspectMask;
              uint32_t              baseMipLevel;
              uint32_t              levelCount;
              uint32_t              baseArrayLayer;
              uint32_t              layerCount;
          } VkImageSubresourceRange;
        */
        /*
          typedef enum VkImageAspectFlagBits {
              VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001,
              VK_IMAGE_ASPECT_DEPTH_BIT = 0x00000002,
              VK_IMAGE_ASPECT_STENCIL_BIT = 0x00000004,
              VK_IMAGE_ASPECT_METADATA_BIT = 0x00000008,
              VK_IMAGE_ASPECT_PLANE_0_BIT = 0x00000010,
              VK_IMAGE_ASPECT_PLANE_1_BIT = 0x00000020,
              VK_IMAGE_ASPECT_PLANE_2_BIT = 0x00000040,
              VK_IMAGE_ASPECT_NONE = 0,
              VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT = 0x00000080,
              VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT = 0x00000100,
              VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT = 0x00000200,
              VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT = 0x00000400,
              VK_IMAGE_ASPECT_PLANE_0_BIT_KHR = VK_IMAGE_ASPECT_PLANE_0_BIT,
              VK_IMAGE_ASPECT_PLANE_1_BIT_KHR = VK_IMAGE_ASPECT_PLANE_1_BIT,
              VK_IMAGE_ASPECT_PLANE_2_BIT_KHR = VK_IMAGE_ASPECT_PLANE_2_BIT,
              VK_IMAGE_ASPECT_NONE_KHR = VK_IMAGE_ASPECT_NONE,
              VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
          } VkImageAspectFlagBits;
        */
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &view_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS) {
            exit_with_error("Failed to creat image view for swapchain image %d", i);
        }
    }

    return swapchain_image_views;
}

VkFramebuffer *create_framebuffers(VkDevice device,
                                   VkRenderPass render_pass,
                                   VkExtent2D swapchain_extent,
                                   VkImageView *swapchain_image_views,
                                   uint32_t image_count) {
    /*
      VkFramebuffer is an opaque pointer: VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFramebuffer)
    */
    VkFramebuffer *swapchain_framebuffers = xmalloc(sizeof(VkFramebuffer) * image_count);

    for (uint32_t i = 0; i < image_count; i++) {
        /*
          typedef struct VkFramebufferCreateInfo {
              VkStructureType             sType;
              const void*                 pNext;
              VkFramebufferCreateFlags    flags;
              VkRenderPass                renderPass;
              uint32_t                    attachmentCount;
              const VkImageView*          pAttachments;
              uint32_t                    width;
              uint32_t                    height;
              uint32_t                    layers;
          } VkFramebufferCreateInfo;
        */
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &swapchain_image_views[i];
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device, &framebuffer_info, NULL, &swapchain_framebuffers[i]) != VK_SUCCESS) {
            exit_with_error("Failed to create framebuffer for swapchain image %d", i);
        }
    }

    return swapchain_framebuffers;
}

VkShaderModule create_shader_module(VkDevice device, const char *file_name) {
    FILE *file = fopen(file_name, "rb");
    if (!file) exit_with_error("Failed to open SPIR-V file: %s", file_name);

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    uint32_t *code = malloc(file_size);
    fread(code, file_size, 1, file);
    fclose(file);

    /*
      typedef struct VkShaderModuleCreateInfo {
          VkStructureType              sType;
          const void*                  pNext;
          VkShaderModuleCreateFlags    flags;
          size_t                       codeSize;
          const uint32_t*              pCode;
      } VkShaderModuleCreateInfo;
    */
    VkShaderModuleCreateInfo shader_module_info = {0};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.codeSize = file_size;
    shader_module_info.pCode = code;

    /*
      VkShaderModule is an opaque pointer: VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkShaderModule)
    */
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &shader_module_info, NULL, &shader_module) != VK_SUCCESS) {
        exit_with_error("Failed to create shader module for file: %s", file_name);
    }

    free(code);
    return shader_module;
}

VkPipelineLayout create_pipeline_layout(VkDevice device) {
    // For now: empty layout as our shaders don't use external resources (no descriptor sets, no push consants)

    /*
      typedef struct VkPipelineLayoutCreateInfo {
          VkStructureType                 sType;
          const void*                     pNext;
          VkPipelineLayoutCreateFlags     flags;
          uint32_t                        setLayoutCount;
          const VkDescriptorSetLayout*    pSetLayouts;
          uint32_t                        pushConstantRangeCount;
          const VkPushConstantRange*      pPushConstantRanges;
      } VkPipelineLayoutCreateInfo;
    */
    VkPipelineLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = NULL;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = NULL;

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout) != VK_SUCCESS) {
        exit_with_error("Failed to create pipeline layout");
    }

    return pipeline_layout;
}


VkVertexInputBindingDescription get_binding_description() {
    /*
      typedef struct VkVertexInputBindingDescription {
          uint32_t             binding;
          uint32_t             stride;
          VkVertexInputRate    inputRate;
      } VkVertexInputBindingDescription;
    */
    VkVertexInputBindingDescription binding_description = {0};
    binding_description.binding = 0; // Binding index in the vertex buffer
    binding_description.stride = sizeof(Vertex);
    /*
      typedef enum VkVertexInputRate {
          VK_VERTEX_INPUT_RATE_VERTEX = 0,
          VK_VERTEX_INPUT_RATE_INSTANCE = 1,
          VK_VERTEX_INPUT_RATE_MAX_ENUM = 0x7FFFFFFF
      } VkVertexInputRate;
    */
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Per vertex data

    return binding_description;
}

VkVertexInputAttributeDescription *get_attribute_descriptions() {
    /*
      typedef struct VkVertexInputAttributeDescription {
          uint32_t    location;
          uint32_t    binding;
          VkFormat    format;
          uint32_t    offset;
      } VkVertexInputAttributeDescription;
    */
    static VkVertexInputAttributeDescription attribute_descriptions[2] = {0};

    // Position
    attribute_descriptions[0].binding = 0; // Binding index
    attribute_descriptions[0].location = 0; // Location in shader
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2 -- 2 floats
    attribute_descriptions[0].offset = offsetof(Vertex, position);

    // Color
    attribute_descriptions[1].binding = 0; // Binding index
    attribute_descriptions[1].location = 1; // Location in shader
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3 -- 3 floats
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    return attribute_descriptions;
}

VkPipeline create_graphics_pipeline(VkDevice device, VkExtent2D swapchain_extent, VkRenderPass render_pass, VkPipelineLayout pipeline_layout) {
    VkShaderModule vert_shader_module = create_shader_module(device, "../res/shaders/bin/basic.vert.spv");
    VkShaderModule frag_shader_module = create_shader_module(device, "../res/shaders/bin/basic.frag.spv");

    /*
      typedef struct VkPipelineShaderStageCreateInfo {
          VkStructureType                     sType;
          const void*                         pNext;
          VkPipelineShaderStageCreateFlags    flags;
          VkShaderStageFlagBits               stage;
          VkShaderModule                      module;
          const char*                         pName;
          const VkSpecializationInfo*         pSpecializationInfo;
      } VkPipelineShaderStageCreateInfo;
    */
    VkPipelineShaderStageCreateInfo vert_stage_info = {0};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    /*
      typedef enum VkShaderStageFlagBits {
          VK_SHADER_STAGE_VERTEX_BIT = 0x00000001,
          VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
          VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
          VK_SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
          VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
          VK_SHADER_STAGE_COMPUTE_BIT = 0x00000020,
          VK_SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
          VK_SHADER_STAGE_ALL = 0x7FFFFFFF,
          VK_SHADER_STAGE_RAYGEN_BIT_KHR = 0x00000100,
          VK_SHADER_STAGE_ANY_HIT_BIT_KHR = 0x00000200,
          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR = 0x00000400,
          VK_SHADER_STAGE_MISS_BIT_KHR = 0x00000800,
          VK_SHADER_STAGE_INTERSECTION_BIT_KHR = 0x00001000,
          VK_SHADER_STAGE_CALLABLE_BIT_KHR = 0x00002000,
          VK_SHADER_STAGE_TASK_BIT_EXT = 0x00000040,
          VK_SHADER_STAGE_MESH_BIT_EXT = 0x00000080,
          VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI = 0x00004000,
          VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI = 0x00080000,
          VK_SHADER_STAGE_RAYGEN_BIT_NV = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
          VK_SHADER_STAGE_ANY_HIT_BIT_NV = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
          VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
          VK_SHADER_STAGE_MISS_BIT_NV = VK_SHADER_STAGE_MISS_BIT_KHR,
          VK_SHADER_STAGE_INTERSECTION_BIT_NV = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
          VK_SHADER_STAGE_CALLABLE_BIT_NV = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
          VK_SHADER_STAGE_TASK_BIT_NV = VK_SHADER_STAGE_TASK_BIT_EXT,
          VK_SHADER_STAGE_MESH_BIT_NV = VK_SHADER_STAGE_MESH_BIT_EXT,
          VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkShaderStageFlagBits;
    */
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_shader_module;
    vert_stage_info.pName = "main"; // Entry point in the shader

    VkPipelineShaderStageCreateInfo frag_stage_info = {0};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_shader_module;
    frag_stage_info.pName = "main"; // Entry point in the shader

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

    // Vertex Input
    /*
      typedef struct VkPipelineVertexInputStateCreateInfo {
          VkStructureType                             sType;
          const void*                                 pNext;
          VkPipelineVertexInputStateCreateFlags       flags;
          uint32_t                                    vertexBindingDescriptionCount;
          const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
          uint32_t                                    vertexAttributeDescriptionCount;
          const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
      } VkPipelineVertexInputStateCreateInfo;
    */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription binding_description = get_binding_description();
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;

    VkVertexInputAttributeDescription *attribute_descriptions = get_attribute_descriptions();
    vertex_input_info.vertexAttributeDescriptionCount = 2;
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions;

    /*
      typedef struct VkPipelineInputAssemblyStateCreateInfo {
          VkStructureType                            sType;
          const void*                                pNext;
          VkPipelineInputAssemblyStateCreateFlags    flags;
          VkPrimitiveTopology                        topology;
          VkBool32                                   primitiveRestartEnable;
      } VkPipelineInputAssemblyStateCreateInfo;
    */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {0};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    /*
      typedef enum VkPrimitiveTopology {
          VK_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
          VK_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
          VK_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN = 5,
          VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
          VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
          VK_PRIMITIVE_TOPOLOGY_PATCH_LIST = 10,
          VK_PRIMITIVE_TOPOLOGY_MAX_ENUM = 0x7FFFFFFF
      } VkPrimitiveTopology;
    */
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    /*
      typedef struct VkViewport {
          float    x;
          float    y;
          float    width;
          float    height;
          float    minDepth;
          float    maxDepth;
      } VkViewport;
    */
    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain_extent.width;
    viewport.height = (float)swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    /*
      typedef struct VkOffset2D {
          int32_t    x;
          int32_t    y;
      } VkOffset2D;

      typedef struct VkExtent2D {
          uint32_t    width;
          uint32_t    height;
      } VkExtent2D;

      typedef struct VkRect2D {
          VkOffset2D    offset;
          VkExtent2D    extent;
      } VkRect2D;
    */
    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = swapchain_extent;

    /*
      typedef struct VkPipelineViewportStateCreateInfo {
          VkStructureType                       sType;
          const void*                           pNext;
          VkPipelineViewportStateCreateFlags    flags;
          uint32_t                              viewportCount;
          const VkViewport*                     pViewports;
          uint32_t                              scissorCount;
          const VkRect2D*                       pScissors;
      } VkPipelineViewportStateCreateInfo;
    */
    VkPipelineViewportStateCreateInfo viewport_state_info = {0};
    viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_info.viewportCount = 1;
    viewport_state_info.pViewports = &viewport;
    viewport_state_info.scissorCount = 1;
    viewport_state_info.pScissors = &scissor;

    /*
      typedef struct VkPipelineRasterizationStateCreateInfo {
          VkStructureType                            sType;
          const void*                                pNext;
          VkPipelineRasterizationStateCreateFlags    flags;
          VkBool32                                   depthClampEnable;
          VkBool32                                   rasterizerDiscardEnable;
          VkPolygonMode                              polygonMode;
          VkCullModeFlags                            cullMode;
          VkFrontFace                                frontFace;
          VkBool32                                   depthBiasEnable;
          float                                      depthBiasConstantFactor;
          float                                      depthBiasClamp;
          float                                      depthBiasSlopeFactor;
          float                                      lineWidth;
      } VkPipelineRasterizationStateCreateInfo;
    */
    VkPipelineRasterizationStateCreateInfo rasterization_state_info = {0};
    rasterization_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_info.depthClampEnable = VK_FALSE;
    rasterization_state_info.rasterizerDiscardEnable = VK_FALSE;

    /*
      typedef enum VkPolygonMode {
          VK_POLYGON_MODE_FILL = 0,
          VK_POLYGON_MODE_LINE = 1,
          VK_POLYGON_MODE_POINT = 2,
          VK_POLYGON_MODE_FILL_RECTANGLE_NV = 1000153000,
          VK_POLYGON_MODE_MAX_ENUM = 0x7FFFFFFF
      } VkPolygonMode;
    */
    rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_info.lineWidth = 1.0f;
    /*
      typedef enum VkCullModeFlagBits {
          VK_CULL_MODE_NONE = 0,
          VK_CULL_MODE_FRONT_BIT = 0x00000001,
          VK_CULL_MODE_BACK_BIT = 0x00000002,
          VK_CULL_MODE_FRONT_AND_BACK = 0x00000003,
          VK_CULL_MODE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkCullModeFlagBits;
    */
    rasterization_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
    /*
      typedef enum VkFrontFace {
          VK_FRONT_FACE_COUNTER_CLOCKWISE = 0,
          VK_FRONT_FACE_CLOCKWISE = 1,
          VK_FRONT_FACE_MAX_ENUM = 0x7FFFFFFF
      } VkFrontFace;
    */
    rasterization_state_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state_info.depthBiasEnable = VK_FALSE;

    // Multisampling (disabled for now)
    /*
      typedef struct VkPipelineMultisampleStateCreateInfo {
          VkStructureType                          sType;
          const void*                              pNext;
          VkPipelineMultisampleStateCreateFlags    flags;
          VkSampleCountFlagBits                    rasterizationSamples;
          VkBool32                                 sampleShadingEnable;
          float                                    minSampleShading;
          const VkSampleMask*                      pSampleMask;
          VkBool32                                 alphaToCoverageEnable;
          VkBool32                                 alphaToOneEnable;
      } VkPipelineMultisampleStateCreateInfo;
    */
    VkPipelineMultisampleStateCreateInfo multisample_state_info = {0};
    multisample_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_info.sampleShadingEnable = VK_FALSE;
    /*
      typedef enum VkSampleCountFlagBits {
          VK_SAMPLE_COUNT_1_BIT = 0x00000001,
          VK_SAMPLE_COUNT_2_BIT = 0x00000002,
          VK_SAMPLE_COUNT_4_BIT = 0x00000004,
          VK_SAMPLE_COUNT_8_BIT = 0x00000008,
          VK_SAMPLE_COUNT_16_BIT = 0x00000010,
          VK_SAMPLE_COUNT_32_BIT = 0x00000020,
          VK_SAMPLE_COUNT_64_BIT = 0x00000040,
          VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkSampleCountFlagBits;
    */
    multisample_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending (disabled for now)
    /*
      typedef struct VkPipelineColorBlendAttachmentState {
          VkBool32                 blendEnable;
          VkBlendFactor            srcColorBlendFactor;
          VkBlendFactor            dstColorBlendFactor;
          VkBlendOp                colorBlendOp;
          VkBlendFactor            srcAlphaBlendFactor;
          VkBlendFactor            dstAlphaBlendFactor;
          VkBlendOp                alphaBlendOp;
          VkColorComponentFlags    colorWriteMask;
      } VkPipelineColorBlendAttachmentState;
    */
    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    /*
      typedef enum VkColorComponentFlagBits {
          VK_COLOR_COMPONENT_R_BIT = 0x00000001,
          VK_COLOR_COMPONENT_G_BIT = 0x00000002,
          VK_COLOR_COMPONENT_B_BIT = 0x00000004,
          VK_COLOR_COMPONENT_A_BIT = 0x00000008,
          VK_COLOR_COMPONENT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
      } VkColorComponentFlagBits;
    */
    color_blend_attachment.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    color_blend_attachment.blendEnable = VK_FALSE;

    /*
      typedef struct VkPipelineColorBlendStateCreateInfo {
          VkStructureType                               sType;
          const void*                                   pNext;
          VkPipelineColorBlendStateCreateFlags          flags;
          VkBool32                                      logicOpEnable;
          VkLogicOp                                     logicOp;
          uint32_t                                      attachmentCount;
          const VkPipelineColorBlendAttachmentState*    pAttachments;
          float                                         blendConstants[4];
      } VkPipelineColorBlendStateCreateInfo;
    */
    VkPipelineColorBlendStateCreateInfo color_blend_state_info = {0};
    color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_info.logicOpEnable = VK_FALSE;
    color_blend_state_info.attachmentCount = 1;
    color_blend_state_info.pAttachments = &color_blend_attachment;

    /*
      typedef struct VkGraphicsPipelineCreateInfo {
          VkStructureType                                  sType;
          const void*                                      pNext;
          VkPipelineCreateFlags                            flags;
          uint32_t                                         stageCount;
          const VkPipelineShaderStageCreateInfo*           pStages;
          const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
          const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
          const VkPipelineTessellationStateCreateInfo*     pTessellationState;
          const VkPipelineViewportStateCreateInfo*         pViewportState;
          const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
          const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
          const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
          const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
          const VkPipelineDynamicStateCreateInfo*          pDynamicState;
          VkPipelineLayout                                 layout;
          VkRenderPass                                     renderPass;
          uint32_t                                         subpass;
          VkPipeline                                       basePipelineHandle;
          int32_t                                          basePipelineIndex;
      } VkGraphicsPipelineCreateInfo;
    */
    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_state_info;
    pipeline_info.pRasterizationState = &rasterization_state_info;
    pipeline_info.pMultisampleState = &multisample_state_info;
    pipeline_info.pColorBlendState = &color_blend_state_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; // TODO: Didn't we define this before?

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        exit_with_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, vert_shader_module, NULL);
    vkDestroyShaderModule(device, frag_shader_module, NULL);

    return pipeline;
}
