// Port of app/src/main/cpp/deviceinfo/vkinfo.c. The report format is unchanged.
// The Android version loads libvulkan.so and returns a Java string; here the
// loader is linked statically (OunceVK) and the report goes to a buffer.
#include "host.h"

#include <stdio.h>
#include <string.h>

#ifdef __SWITCH__
#include <vulkan/vulkan.h>

#define LINE(...) do { if (used < cap) used += snprintf(out + used, cap - used, __VA_ARGS__); } while (0)

static const char *const EXTENSIONS[] = {
    "VK_EXT_robustness2", "VK_EXT_transform_feedback", "VK_EXT_custom_border_color",
    "VK_KHR_dynamic_rendering", "VK_EXT_extended_dynamic_state3", "VK_EXT_descriptor_indexing",
    "VK_KHR_timeline_semaphore", "VK_EXT_mesh_shader", "VK_KHR_ray_query",
};

static const char *type_name(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
        default: return "other";
    }
}

void dd_vkinfo_query(char *out, size_t cap) {
    size_t used = 0;
    out[0] = 0;
    uint32_t loader = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&loader);
    LINE("loader=%u.%u.%u\n", VK_API_VERSION_MAJOR(loader), VK_API_VERSION_MINOR(loader), VK_API_VERSION_PATCH(loader));

    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "DroidDeck device info",
                              .apiVersion = loader >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0 };
    VkInstanceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&info, NULL, &instance) != VK_SUCCESS) { LINE("error=vkCreateInstance failed\n"); return; }

    uint32_t count = 1;
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkResult listed = vkEnumeratePhysicalDevices(instance, &count, &device);
    if ((listed != VK_SUCCESS && listed != VK_INCOMPLETE) || count == 0 || !device) {
        LINE("error=no Vulkan device\n");
    } else {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(device, &p);
        LINE("device=%s\n", p.deviceName);
        LINE("type=%s\n", type_name(p.deviceType));
        LINE("api=%u.%u.%u\n", VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion));
        LINE("driver_version=%u.%u.%u\n", p.driverVersion >> 22, (p.driverVersion >> 12) & 0x3ff, p.driverVersion & 0xfff);
        LINE("vendor_id=0x%04x\ndevice_id=0x%08x\n", p.vendorID, p.deviceID);
        LINE("max_image_2d=%u\n", p.limits.maxImageDimension2D);
        LINE("max_push_constants=%u\n", p.limits.maxPushConstantsSize);

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, NULL);
        static VkExtensionProperties exts[512];
        if (extCount > 512) extCount = 512;
        if (extCount) vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, exts);
        int driverProps = p.apiVersion >= VK_API_VERSION_1_2;
        for (uint32_t i = 0; i < extCount; i++)
            if (!strcmp(exts[i].extensionName, "VK_KHR_driver_properties")) driverProps = 1;
        if (driverProps && loader >= VK_API_VERSION_1_1) {
            VkPhysicalDeviceDriverProperties driver = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
            VkPhysicalDeviceProperties2 p2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver };
            vkGetPhysicalDeviceProperties2(device, &p2);
            LINE("driver_name=%s\n", driver.driverName);
            LINE("driver_info=%s\n", driver.driverInfo);
            LINE("conformance=%u.%u.%u.%u\n", driver.conformanceVersion.major, driver.conformanceVersion.minor,
                 driver.conformanceVersion.subminor, driver.conformanceVersion.patch);
        }
        LINE("extension_count=%u\n", extCount);
        for (size_t e = 0; e < sizeof(EXTENSIONS) / sizeof(*EXTENSIONS); e++) {
            int has = 0;
            for (uint32_t i = 0; i < extCount && !has; i++) has = !strcmp(exts[i].extensionName, EXTENSIONS[e]);
            LINE("ext.%s=%s\n", EXTENSIONS[e], has ? "yes" : "no");
        }
        VkPhysicalDeviceFeatures f;
        vkGetPhysicalDeviceFeatures(device, &f);
        LINE("geometry_shader=%s\ntessellation_shader=%s\ntexture_bc=%s\n",
             f.geometryShader ? "yes" : "no", f.tessellationShader ? "yes" : "no", f.textureCompressionBC ? "yes" : "no");
        LINE("shader_int64=%s\n", f.shaderInt64 ? "yes" : "no");
        VkPhysicalDeviceMemoryProperties m;
        vkGetPhysicalDeviceMemoryProperties(device, &m);
        VkDeviceSize largest = 0;
        for (uint32_t i = 0; i < m.memoryHeapCount; i++)
            if ((m.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && m.memoryHeaps[i].size > largest) largest = m.memoryHeaps[i].size;
        LINE("device_heap_mb=%llu\n", (unsigned long long) (largest >> 20));
    }
    vkDestroyInstance(instance, NULL);
}

#else

void dd_vkinfo_query(char *out, size_t cap) {
    snprintf(out, cap, "error=no Vulkan loader\n");
}

#endif

const char *dd_report_get(const char *report, const char *key, char *value, size_t cap) {
    size_t klen = strlen(key);
    for (const char *line = report; line && *line; ) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t) (end - line) : strlen(line);
        if (len > klen && !strncmp(line, key, klen) && line[klen] == '=') {
            size_t vlen = len - klen - 1;
            if (vlen >= cap) vlen = cap - 1;
            memcpy(value, line + klen + 1, vlen);
            value[vlen] = 0;
            return value;
        }
        line = end ? end + 1 : NULL;
    }
    return NULL;
}
