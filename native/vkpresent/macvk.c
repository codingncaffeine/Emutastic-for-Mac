// macvk — offscreen Vulkan hardware-render backend for 3D libretro cores on macOS (via MoltenVK).
//
// Vulkan analog of native/machwgl (CGL). It implements libretro's Vulkan HW-render interface so cores
// like mupen64plus-next's ParaLLEl-RDP render on Apple Silicon (RetroArch does the same via MoltenVK).
// We create the VkInstance; the core's negotiation interface (if provided) creates the VkDevice with the
// exact features it needs (falling back to our own device otherwise). The core renders into its own
// VkImage each frame and hands it to us via set_image; vkp_hw_readback copies that image to a host buffer
// as top-down BGRA for the normal present path. Exports a small C ABI to C# (HwVkContext.cs).
//
// Threading: lives on the emu/worker thread; the core may submit to our queue from any thread, guarded by
// lock_queue/unlock_queue (a mutex). Synchronous readback (copy + fence wait) — simple + correct first;
// async ring can come later. MoltenVK 1.4 on Apple M4 verified to enumerate the device.
#define VK_USE_PLATFORM_METAL_EXT 1   // VkMetalSurfaceCreateInfoEXT / vkCreateMetalSurfaceEXT
#include <vulkan/vulkan.h>
#include <objc/runtime.h>             // create a CAMetalLayer for cores that require a real VkSurfaceKHR
#include <objc/message.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

// ── libretro Vulkan interface (inlined from libretro_vulkan.h, exact layout) ────────────────────────
#define RETRO_HW_RENDER_INTERFACE_VULKAN 0
#define RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION 5
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN 0

struct retro_vulkan_image { VkImageView image_view; VkImageLayout image_layout; VkImageViewCreateInfo create_info; };

typedef void (*retro_vulkan_set_image_t)(void*, const struct retro_vulkan_image*, uint32_t, const VkSemaphore*, uint32_t);
typedef uint32_t (*retro_vulkan_get_sync_index_t)(void*);
typedef uint32_t (*retro_vulkan_get_sync_index_mask_t)(void*);
typedef void (*retro_vulkan_set_command_buffers_t)(void*, uint32_t, const VkCommandBuffer*);
typedef void (*retro_vulkan_wait_sync_index_t)(void*);
typedef void (*retro_vulkan_lock_queue_t)(void*);
typedef void (*retro_vulkan_unlock_queue_t)(void*);
typedef void (*retro_vulkan_set_signal_semaphore_t)(void*, VkSemaphore);
typedef const VkApplicationInfo* (*retro_vulkan_get_application_info_t)(void);

struct retro_vulkan_context {
    VkPhysicalDevice gpu; VkDevice device; VkQueue queue; uint32_t queue_family_index;
    VkQueue presentation_queue; uint32_t presentation_queue_family_index;
};
typedef bool (*retro_vulkan_create_device_t)(struct retro_vulkan_context*, VkInstance, VkPhysicalDevice,
    VkSurfaceKHR, PFN_vkGetInstanceProcAddr, const char**, unsigned, const char**, unsigned, const VkPhysicalDeviceFeatures*);
typedef void (*retro_vulkan_destroy_device_t)(void);

struct retro_hw_render_context_negotiation_interface_vulkan {
    int interface_type; unsigned interface_version;
    retro_vulkan_get_application_info_t get_application_info;
    retro_vulkan_create_device_t create_device;
    retro_vulkan_destroy_device_t destroy_device;
    /* v2 fields follow; ignored for v1 */
};

struct retro_hw_render_interface_vulkan {
    int interface_type; unsigned interface_version;
    void *handle;
    VkInstance instance; VkPhysicalDevice gpu; VkDevice device;
    PFN_vkGetDeviceProcAddr get_device_proc_addr;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    VkQueue queue; unsigned queue_index;
    retro_vulkan_set_image_t set_image;
    retro_vulkan_get_sync_index_t get_sync_index;
    retro_vulkan_get_sync_index_mask_t get_sync_index_mask;
    retro_vulkan_set_command_buffers_t set_command_buffers;
    retro_vulkan_wait_sync_index_t wait_sync_index;
    retro_vulkan_lock_queue_t lock_queue;
    retro_vulkan_unlock_queue_t unlock_queue;
    retro_vulkan_set_signal_semaphore_t set_signal_semaphore;
};

// ── shim state ──────────────────────────────────────────────────────────────────────────────────────
#define SYNC_RING 4
#define RB_RING   3          // readback pipeline depth: copy frame N while returning an earlier frame's
                             // data. 3 gives the GPU two frames of slack so a busy frame doesn't stall us.
static struct {
    VkInstance instance;
    VkSurfaceKHR surface;    // offscreen Metal surface — some cores (Dolphin) require a real one
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue queue;
    uint32_t qfi;            // graphics+compute queue family index
    VkCommandPool pool;
    // Pipelined readback ring: each frame we submit the image→host copy into slot rbWrite (no wait) and
    // return the OTHER slot's already-finished pixels. This moves the GPU copy + fence wait off the emu
    // thread's critical path (it overlaps the core's next-frame render), instead of stalling ~5ms/frame.
    VkCommandBuffer rbCmd[RB_RING]; VkFence rbFence[RB_RING];
    VkBuffer rbBuf[RB_RING]; VkDeviceMemory rbMem[RB_RING]; void *rbMap[RB_RING]; VkDeviceSize rbCap[RB_RING];
    int rbW[RB_RING], rbH[RB_RING]; VkFormat rbFmt[RB_RING]; int rbPending[RB_RING];
    int rbWrite;
    // Downscale-before-readback: the core renders at the (huge) upscaled resolution for quality, but we
    // only need display-resolution pixels. Blitting the core image down to the window size first makes the
    // copy + CPU read cost ~constant regardless of upscaling (8x readback ≈ 2x) — and 8x→display is a
    // gorgeous supersample. tgtW/tgtH = window pixel size from C# (0 = full-res readback). One scratch
    // image per ring slot holds the downscaled result before the host copy.
    int tgtW, tgtH;
    VkImage rbImg[RB_RING]; VkDeviceMemory rbImgMem[RB_RING]; int rbImgW[RB_RING], rbImgH[RB_RING]; VkFormat rbImgFmt[RB_RING];
    pthread_mutex_t qlock;
    int ownDevice;           // 1 if we (not the core) created the device
    uint32_t syncIndex;

    // latest set_image
    VkImage curImage; VkImageLayout curLayout; VkFormat curFormat;
    VkSemaphore waitSems[8]; uint32_t numWait; uint32_t srcQF;

    const struct retro_hw_render_context_negotiation_interface_vulkan *neg;
    struct retro_hw_render_interface_vulkan iface;
    char info[256];
    double issue_ms;

    // perf instrumentation (accumulated per ~300 readbacks, dumped to stderr)
    unsigned long dbg_n, dbg_waitCalls;
    double dbg_waitMs;     // time in cb_wait_sync_index (the device-idle)
    double dbg_submitMs;   // record+submit the copy
    double dbg_fenceMs;    // wait for the copy fence (GPU done)
    double dbg_mapMs;      // map+swizzle to BGRA
} V;

static double now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec*1000.0+ts.tv_nsec/1e6; }

// ── interface callbacks the core invokes ────────────────────────────────────────────────────────────
static void cb_set_image(void *h, const struct retro_vulkan_image *img, uint32_t nsem, const VkSemaphore *sems, uint32_t srcQF) {
    (void)h;
    if (!img) { V.curImage = VK_NULL_HANDLE; return; }
    V.curImage  = img->create_info.image;   // the VkImage backing the view
    V.curLayout = img->image_layout;
    V.curFormat = img->create_info.format;
    V.srcQF = srcQF;
    V.numWait = (nsem > 8) ? 8 : nsem;
    for (uint32_t i = 0; i < V.numWait; i++) V.waitSems[i] = sems[i];
}
static uint32_t cb_get_sync_index(void *h){ (void)h; return V.syncIndex; }
static uint32_t cb_get_sync_index_mask(void *h){ (void)h; return (1u << SYNC_RING) - 1u; }
static void cb_set_command_buffers(void *h, uint32_t n, const VkCommandBuffer *c){ (void)h;(void)n;(void)c; /* set_image path used */ }
static void cb_wait_sync_index(void *h){
    (void)h;
    // Our readback (in Video_cb) is fully synchronous and completes before retro_video_refresh returns,
    // so by the time the core recycles a sync index (RING frames later) we are long done with that image
    // — a vkDeviceWaitIdle here is redundant and serializes the whole GPU. Time it (no-op now) to confirm.
    double t0 = now_ms();
    V.dbg_waitCalls++;
    V.dbg_waitMs += now_ms() - t0;
}
static void cb_lock_queue(void *h){ (void)h; pthread_mutex_lock(&V.qlock); }
static void cb_unlock_queue(void *h){ (void)h; pthread_mutex_unlock(&V.qlock); }
static void cb_set_signal_semaphore(void *h, VkSemaphore s){ (void)h;(void)s; /* single-image recycle unused */ }

static uint32_t find_qfi(VkPhysicalDevice gpu) {
    uint32_t n = 0; vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, NULL);
    VkQueueFamilyProperties props[16]; if (n > 16) n = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, props);
    for (uint32_t i = 0; i < n; i++)
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) return i;
    for (uint32_t i = 0; i < n; i++) if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    return 0;
}

// Default device creation when the core provides no negotiation interface: enable everything available.
static int create_default_device(void) {
    V.qfi = find_qfi(V.gpu);
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = V.qfi; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkPhysicalDeviceFeatures feats; vkGetPhysicalDeviceFeatures(V.gpu, &feats);
    const char *devExt[] = { "VK_KHR_portability_subset" };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = devExt;
    dci.pEnabledFeatures = &feats;
    if (vkCreateDevice(V.gpu, &dci, NULL, &V.device) != VK_SUCCESS) { fprintf(stderr, "[macvk] vkCreateDevice (default) failed\n"); return 0; }
    V.ownDevice = 1;
    vkGetDeviceQueue(V.device, V.qfi, 0, &V.queue);
    return 1;
}

// Called from C# when the core supplies SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE (before init).
void vkp_hw_set_negotiation(const void *neg) {
    V.neg = (const struct retro_hw_render_context_negotiation_interface_vulkan*)neg;
}

int vkp_hw_init(int ctx_type, int major, int minor, int want_depth, int want_stencil, int maxw, int maxh) {
    (void)ctx_type; (void)want_depth; (void)want_stencil; (void)maxw; (void)maxh;
    pthread_mutex_init(&V.qlock, NULL);

    // apiVersion. For Vulkan HW-render, libretro passes the core's MINIMUM required Vulkan version
    // in `major` (retro_hw_render_callback.version_major), VK_MAKE_VERSION-encoded — e.g. ParaLLEl-RDP
    // sends VK_MAKE_VERSION(1,1,0)=4198400. So treat a large `major` as already-encoded; a small one as
    // a plain major.minor pair. Floor at 1.1 (ParaLLEl-RDP needs it) and clamp to the loader's max.
    uint32_t loaderMax = VK_API_VERSION_1_0;
    {
        PFN_vkEnumerateInstanceVersion eiv =
            (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
        if (eiv) eiv(&loaderMax);
    }
    uint32_t wantApi;
    if (major <= 0)         wantApi = VK_API_VERSION_1_1;
    else if (major < 1000)  wantApi = VK_MAKE_API_VERSION(0, (uint32_t)major, (uint32_t)minor, 0);
    else                    wantApi = (uint32_t)major;                 // already VK_MAKE_VERSION-encoded
    if (wantApi < VK_API_VERSION_1_1) wantApi = VK_API_VERSION_1_1;
    if (wantApi > loaderMax)          wantApi = loaderMax;

    VkApplicationInfo defApp = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    defApp.pApplicationName = "Emutastic"; defApp.pEngineName = "Emutastic";
    defApp.apiVersion = wantApi;
    const VkApplicationInfo *app = &defApp;
    if (V.neg && V.neg->get_application_info) { const VkApplicationInfo *a = V.neg->get_application_info(); if (a) app = a; }

    // Enable only the instance extensions actually advertised (MoltenVK lists all of these, but be
    // defensive). VK_KHR_surface + VK_EXT_metal_surface let us build a real VkSurfaceKHR — cores like
    // Dolphin assert on a non-NULL surface in their negotiation create_device even though they fake the
    // swapchain into set_image. portability_enumeration is needed for MoltenVK device enumeration.
    uint32_t navail = 0; vkEnumerateInstanceExtensionProperties(NULL, &navail, NULL);
    VkExtensionProperties avail[256]; if (navail > 256) navail = 256;
    vkEnumerateInstanceExtensionProperties(NULL, &navail, avail);
    const char *want[] = { "VK_KHR_get_physical_device_properties2", "VK_KHR_portability_enumeration",
                           "VK_KHR_surface", "VK_EXT_metal_surface" };
    const char *enabled[8]; uint32_t nen = 0; int hasPortability = 0, hasMetalSurface = 0;
    for (uint32_t w = 0; w < sizeof(want)/sizeof(want[0]); w++)
        for (uint32_t i = 0; i < navail; i++)
            if (strcmp(want[w], avail[i].extensionName) == 0) {
                enabled[nen++] = want[w];
                if (strcmp(want[w], "VK_KHR_portability_enumeration") == 0) hasPortability = 1;
                if (strcmp(want[w], "VK_EXT_metal_surface") == 0)          hasMetalSurface = 1;
                break;
            }

    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    if (hasPortability) ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    ici.pApplicationInfo = app;
    ici.enabledExtensionCount = nen; ici.ppEnabledExtensionNames = enabled;
    VkResult ir = vkCreateInstance(&ici, NULL, &V.instance);
    if (ir != VK_SUCCESS) {
        fprintf(stderr, "[macvk] vkCreateInstance failed: VkResult=%d (apiVersion=0x%x loaderMax=0x%x exts=%u portability=%d)\n",
                ir, app->apiVersion, loaderMax, nen, hasPortability);
        return 0;
    }

    // Build an offscreen Metal surface (CAMetalLayer). Dolphin's create_device requires it; ParaLLEl-RDP
    // ignores it. We never present through it — the core's "swapchain" hands images to our set_image.
    if (hasMetalSurface) {
        Class cls = objc_getClass("CAMetalLayer");
        id layer = cls ? ((id(*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("layer")) : NULL;
        if (layer) ((id(*)(id, SEL))objc_msgSend)(layer, sel_registerName("retain"));   // outlive autorelease
        PFN_vkCreateMetalSurfaceEXT createMetal =
            (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(V.instance, "vkCreateMetalSurfaceEXT");
        if (layer && createMetal) {
            VkMetalSurfaceCreateInfoEXT mi = { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };
            mi.pLayer = (const void *)layer;
            if (createMetal(V.instance, &mi, NULL, &V.surface) != VK_SUCCESS) V.surface = VK_NULL_HANDLE;
        }
        fprintf(stderr, "[macvk] Metal surface %s\n", V.surface ? "created" : "FAILED (Dolphin-class cores need it)");
    }

    uint32_t ndev = 0; vkEnumeratePhysicalDevices(V.instance, &ndev, NULL);
    if (ndev == 0) { fprintf(stderr, "[macvk] no Vulkan devices\n"); return 0; }
    VkPhysicalDevice devs[8]; if (ndev > 8) ndev = 8;
    vkEnumeratePhysicalDevices(V.instance, &ndev, devs);
    V.gpu = devs[0];

    // Device: prefer the core's negotiation create_device (it enables the features it needs).
    int made = 0;
    if (V.neg && V.neg->create_device) {
        struct retro_vulkan_context ctx; memset(&ctx, 0, sizeof ctx);
        VkPhysicalDeviceFeatures feats; vkGetPhysicalDeviceFeatures(V.gpu, &feats);
        if (V.neg->create_device(&ctx, V.instance, V.gpu, V.surface, vkGetInstanceProcAddr, NULL, 0, NULL, 0, &feats)
            && ctx.device != VK_NULL_HANDLE) {
            V.gpu = ctx.gpu; V.device = ctx.device; V.queue = ctx.queue; V.qfi = ctx.queue_family_index; V.ownDevice = 0;
            made = 1;
        } else {
            fprintf(stderr, "[macvk] core create_device failed — falling back to default device\n");
        }
    }
    if (!made && !create_default_device()) return 0;

    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pci.queueFamilyIndex = V.qfi;
    if (vkCreateCommandPool(V.device, &pci, NULL, &V.pool) != VK_SUCCESS) { fprintf(stderr, "[macvk] command pool failed\n"); return 0; }
    // One command buffer + fence per readback-ring slot (staging buffers are sized lazily on first use).
    VkCommandBufferAllocateInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbi.commandPool = V.pool; cbi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbi.commandBufferCount = 1;
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    for (int i = 0; i < RB_RING; i++) {
        vkAllocateCommandBuffers(V.device, &cbi, &V.rbCmd[i]);
        vkCreateFence(V.device, &fci, NULL, &V.rbFence[i]);
    }

    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(V.gpu, &props);
    snprintf(V.info, sizeof V.info, "renderer=%s api=%u.%u driver=MoltenVK ownDevice=%d",
             props.deviceName, VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), V.ownDevice);
    fprintf(stderr, "[macvk] Vulkan HW-render ready: %s\n", V.info);

    // Fill the interface the core fetches via GET_HW_RENDER_INTERFACE.
    memset(&V.iface, 0, sizeof V.iface);
    V.iface.interface_type = RETRO_HW_RENDER_INTERFACE_VULKAN;
    V.iface.interface_version = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
    V.iface.handle = &V;
    V.iface.instance = V.instance; V.iface.gpu = V.gpu; V.iface.device = V.device;
    V.iface.get_device_proc_addr = vkGetDeviceProcAddr;
    V.iface.get_instance_proc_addr = vkGetInstanceProcAddr;
    V.iface.queue = V.queue; V.iface.queue_index = V.qfi;
    V.iface.set_image = cb_set_image;
    V.iface.get_sync_index = cb_get_sync_index;
    V.iface.get_sync_index_mask = cb_get_sync_index_mask;
    V.iface.set_command_buffers = cb_set_command_buffers;
    V.iface.wait_sync_index = cb_wait_sync_index;
    V.iface.lock_queue = cb_lock_queue;
    V.iface.unlock_queue = cb_unlock_queue;
    V.iface.set_signal_semaphore = cb_set_signal_semaphore;
    return 1;
}

const void* vkp_hw_interface(void) { return &V.iface; }
const char* vkp_hw_info(void) { return V.info; }
void vkp_hw_readback_times(double *issue, double *map){ if (issue) *issue = V.issue_ms; if (map) *map = 0; }
void vkp_hw_readback_times2(double *a, double *b){ if (a) *a = 0; if (b) *b = 0; }
void vkp_hw_set_present_target(int w, int h){ V.tgtW = w; V.tgtH = h; }
unsigned int vkp_hw_fbo(void){ return 0; }
void vkp_hw_make_current(void){ /* Vulkan has no current-context */ }

static uint32_t find_mem(uint32_t typeBits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(V.gpu, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}
static uint32_t find_host_mem(uint32_t typeBits, VkMemoryPropertyFlags want) { return find_mem(typeBits, want); }

// The downscaled readback size for a core frame of cur_w x cur_h: the core's aspect scaled to FIT the
// window (tgtW x tgtH), never upscaling. tgt 0 (or already smaller) → full-res readback.
static void readback_size(int cur_w, int cur_h, int *rw, int *rh) {
    if (V.tgtW <= 0 || V.tgtH <= 0 || (cur_w <= V.tgtW && cur_h <= V.tgtH)) { *rw = cur_w; *rh = cur_h; return; }
    double s = (double)V.tgtW / cur_w; double sh = (double)V.tgtH / cur_h; if (sh < s) s = sh;
    int w = (int)(cur_w * s + 0.5), h = (int)(cur_h * s + 0.5);
    *rw = w < 1 ? 1 : w; *rh = h < 1 ? 1 : h;
}

// Ensure slot `i`'s scratch downscale image is w x h, format fmt (device-local, TRANSFER_DST+SRC).
static int ensure_slot_img(int i, int w, int h, VkFormat fmt) {
    if (V.rbImg[i] && V.rbImgW[i] == w && V.rbImgH[i] == h && V.rbImgFmt[i] == fmt) return 1;
    if (V.rbImg[i])    { vkDestroyImage(V.device, V.rbImg[i], NULL); V.rbImg[i] = VK_NULL_HANDLE; }
    if (V.rbImgMem[i]) { vkFreeMemory(V.device, V.rbImgMem[i], NULL); V.rbImgMem[i] = VK_NULL_HANDLE; }
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D; ici.format = fmt; ici.extent.width = w; ici.extent.height = h; ici.extent.depth = 1;
    ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT; ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE; ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(V.device, &ici, NULL, &V.rbImg[i]) != VK_SUCCESS) return 0;
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(V.device, V.rbImg[i], &mr);
    uint32_t mt = find_mem(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mt == UINT32_MAX) mt = find_mem(mr.memoryTypeBits, 0);
    if (mt == UINT32_MAX) return 0;
    VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO }; mai.allocationSize = mr.size; mai.memoryTypeIndex = mt;
    if (vkAllocateMemory(V.device, &mai, NULL, &V.rbImgMem[i]) != VK_SUCCESS) return 0;
    vkBindImageMemory(V.device, V.rbImg[i], V.rbImgMem[i], 0);
    V.rbImgW[i] = w; V.rbImgH[i] = h; V.rbImgFmt[i] = fmt;
    return 1;
}

// Ensure readback-ring slot `i`'s staging buffer holds `size` bytes; (re)create + persistently map on growth.
static int ensure_slot(int i, VkDeviceSize size) {
    if (V.rbBuf[i] && V.rbCap[i] >= size) return 1;
    if (V.rbMap[i]) { vkUnmapMemory(V.device, V.rbMem[i]); V.rbMap[i] = NULL; }
    if (V.rbBuf[i]) { vkDestroyBuffer(V.device, V.rbBuf[i], NULL); V.rbBuf[i] = VK_NULL_HANDLE; }
    if (V.rbMem[i]) { vkFreeMemory(V.device, V.rbMem[i], NULL); V.rbMem[i] = VK_NULL_HANDLE; }
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = size; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(V.device, &bci, NULL, &V.rbBuf[i]) != VK_SUCCESS) return 0;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(V.device, V.rbBuf[i], &mr);
    uint32_t mt = find_host_mem(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mt == UINT32_MAX) return 0;
    VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO }; mai.allocationSize = mr.size; mai.memoryTypeIndex = mt;
    if (vkAllocateMemory(V.device, &mai, NULL, &V.rbMem[i]) != VK_SUCCESS) return 0;
    vkBindBufferMemory(V.device, V.rbBuf[i], V.rbMem[i], 0);
    if (vkMapMemory(V.device, V.rbMem[i], 0, VK_WHOLE_SIZE, 0, &V.rbMap[i]) != VK_SUCCESS) return 0;  // persistent (HOST_COHERENT)
    V.rbCap[i] = size;
    return 1;
}

static void img_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
                        VkAccessFlags srcA, VkAccessFlags dstA, uint32_t srcQ, uint32_t dstQ) {
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = from; b.newLayout = to; b.srcAccessMask = srcA; b.dstAccessMask = dstA;
    b.srcQueueFamilyIndex = srcQ; b.dstQueueFamilyIndex = dstQ; b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; b.subresourceRange.levelCount = 1; b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}

// Record + submit (no wait) the readback of the core's current image into ring slot `slot`. When rw/rh is
// smaller than the core frame (cur_w x cur_h), GPU-blit it down to the slot's scratch image first (linear =
// supersample) so only display-res pixels are copied to host — keeps high upscaling cheap. Else copy direct.
static void rb_submit(int slot, int cur_w, int cur_h, int rw, int rh) {
    int down = (rw != cur_w || rh != cur_h);
    uint32_t sQ = (V.srcQF == VK_QUEUE_FAMILY_IGNORED || V.srcQF == V.qfi) ? VK_QUEUE_FAMILY_IGNORED : V.srcQF;
    uint32_t coreAcqSrc = sQ, coreAcqDst = (sQ == VK_QUEUE_FAMILY_IGNORED) ? VK_QUEUE_FAMILY_IGNORED : V.qfi;
    VkCommandBuffer cb = V.rbCmd[slot];

    pthread_mutex_lock(&V.qlock);
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);

    // Core image → TRANSFER_SRC (acquiring queue ownership if the core used a different family).
    img_barrier(cb, V.curImage, V.curLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, coreAcqSrc, coreAcqDst);

    VkImage copySrc = V.curImage;
    if (down) {
        img_barrier(cb, V.rbImg[slot], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
        VkImageBlit blit; memset(&blit, 0, sizeof blit);
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; blit.srcSubresource.layerCount = 1;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; blit.dstSubresource.layerCount = 1;
        blit.srcOffsets[1].x = cur_w; blit.srcOffsets[1].y = cur_h; blit.srcOffsets[1].z = 1;
        blit.dstOffsets[1].x = rw;    blit.dstOffsets[1].y = rh;    blit.dstOffsets[1].z = 1;
        vkCmdBlitImage(cb, V.curImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, V.rbImg[slot], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        img_barrier(cb, V.rbImg[slot], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
        copySrc = V.rbImg[slot];
    }

    VkBufferImageCopy region; memset(&region, 0, sizeof region);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; region.imageSubresource.layerCount = 1;
    region.imageExtent.width = rw; region.imageExtent.height = rh; region.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(cb, copySrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, V.rbBuf[slot], 1, &region);

    // Restore the core image's layout + release queue ownership back to it.
    img_barrier(cb, V.curImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, V.curLayout,
                VK_ACCESS_TRANSFER_READ_BIT, 0, coreAcqDst, coreAcqSrc);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    VkPipelineStageFlags stages[8];
    if (V.numWait > 0) {
        si.waitSemaphoreCount = V.numWait; si.pWaitSemaphores = V.waitSems;
        for (uint32_t i = 0; i < V.numWait; i++) stages[i] = VK_PIPELINE_STAGE_TRANSFER_BIT; si.pWaitDstStageMask = stages;
    }
    vkResetFences(V.device, 1, &V.rbFence[slot]);
    vkQueueSubmit(V.queue, 1, &si, V.rbFence[slot]);
    V.numWait = 0;   // semaphores are consumed once
    pthread_mutex_unlock(&V.qlock);
    V.rbPending[slot] = 1; V.rbW[slot] = rw; V.rbH[slot] = rh; V.rbFmt[slot] = V.curFormat;
}

// Pipelined readback: submit a copy of the CURRENT image, then return the PREVIOUS frame's finished pixels
// (top-down opaque BGRA). The GPU copy + its fence wait thus overlap the core's next-frame render instead
// of stalling the emu thread. 1-frame latency. Returns 1 if `out` was filled (0 only while priming).
int vkp_hw_readback(void *out, int cur_w, int cur_h, int bottom_left, int *out_w, int *out_h) {
    (void)bottom_left;   // Vulkan is top-left origin; no flip.
    if (!V.device || !V.curImage || !out || cur_w <= 0 || cur_h <= 0) return 0;
    double t0 = now_ms();
    int slot = V.rbWrite;
    int rw, rh; readback_size(cur_w, cur_h, &rw, &rh);     // downscale to the window size (supersample)
    VkDeviceSize sz = (VkDeviceSize)rw * rh * 4;

    // Slot reuse safety: its prior copy is RB_RING frames old, so its fence is signalled — but guard anyway.
    if (V.rbPending[slot]) { vkWaitForFences(V.device, 1, &V.rbFence[slot], VK_TRUE, 1000000000ull); V.rbPending[slot] = 0; }
    if (!ensure_slot(slot, sz)) return 0;
    if ((rw != cur_w || rh != cur_h) && !ensure_slot_img(slot, rw, rh, V.curFormat)) return 0;

    double ts0 = now_ms();
    rb_submit(slot, cur_w, cur_h, rw, rh);  // async — no wait (GPU-downscales first when rw<cur_w)
    double ts1 = now_ms();

    // Return the OTHER slot — the frame submitted last call, whose copy has had a full frame to finish.
    int read = (slot + 1) % RB_RING;
    int ret = 0;
    double tf0 = now_ms(), tm0 = tf0;
    if (V.rbPending[read]) {
        vkWaitForFences(V.device, 1, &V.rbFence[read], VK_TRUE, 1000000000ull);   // ~0: a frame old already
        tm0 = now_ms();
        const uint32_t *s32 = (const uint32_t*)V.rbMap[read];
        uint32_t *o32 = (uint32_t*)out;
        VkFormat fmt = V.rbFmt[read];
        long n = (long)V.rbW[read] * V.rbH[read];
        // Convert the core's pixel format to opaque BGRA8 (presented as GL_BGRA: byte order B,G,R,0xFF).
        // 32-bit-at-a-time (auto-vectorizes to NEON on arm64).
        if (fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
            // Dolphin renders to 10-bit on Apple displays (its surface-format preference picks RGB10_A2
            // over RGBA8). Packed LSB→MSB: R[0-9] G[10-19] B[20-29] A[30-31]; take the top 8 bits of each.
            for (long i = 0; i < n; i++) { uint32_t p = s32[i];
                o32[i] = 0xFF000000u | (((p >> 2) & 0xFFu) << 16) | (((p >> 12) & 0xFFu) << 8) | ((p >> 22) & 0xFFu); }
        } else if (fmt == VK_FORMAT_R8G8B8A8_UNORM || fmt == VK_FORMAT_R8G8B8A8_SRGB) {
            // RGBA in memory (0xAABBGGRR) → swap R/B, force alpha.
            for (long i = 0; i < n; i++) { uint32_t p = s32[i]; o32[i] = 0xFF000000u | ((p & 0x000000FFu) << 16) | (p & 0x0000FF00u) | ((p >> 16) & 0x000000FFu); }
        } else {
            // BGRA already (or close enough) → just force opaque alpha.
            for (long i = 0; i < n; i++) o32[i] = s32[i] | 0xFF000000u;
        }
        static VkFormat s_loggedFmt = (VkFormat)0;
        if (fmt != s_loggedFmt) { s_loggedFmt = fmt; fprintf(stderr, "[macvk] readback pixel format = %d\n", (int)fmt); }
        int rw = V.rbW[read], rh = V.rbH[read];
        V.rbPending[read] = 0;
        if (out_w) *out_w = rw; if (out_h) *out_h = rh;
        ret = 1;
    }
    V.rbWrite = read;                        // next frame writes the slot we just consumed
    V.syncIndex = (V.syncIndex + 1) % SYNC_RING;
    V.issue_ms += 0.05 * ((now_ms() - t0) - V.issue_ms);

    // perf split (submit vs prev-frame fence wait vs map/swizzle), dumped per 300 frames when
    // EMUTASTIC_VK_PERF is set in the environment (off by default — keeps normal runs quiet).
    V.dbg_submitMs += ts1 - ts0; V.dbg_fenceMs += tm0 - tf0; V.dbg_mapMs += now_ms() - tm0;
    if (++V.dbg_n >= 300) {
        if (getenv("EMUTASTIC_VK_PERF"))
            fprintf(stderr, "[macvk-perf] core %dx%d → readback %dx%d over %lu: submit=%.2fms prevWait=%.2fms map=%.2fms\n",
                    cur_w, cur_h, rw, rh, V.dbg_n, V.dbg_submitMs/V.dbg_n, V.dbg_fenceMs/V.dbg_n, V.dbg_mapMs/V.dbg_n);
        V.dbg_n = 0; V.dbg_submitMs = V.dbg_fenceMs = V.dbg_mapMs = 0; V.dbg_waitCalls = 0; V.dbg_waitMs = 0;
    }
    return ret;
}

void vkp_hw_destroy(void) {
    if (V.device) {
        vkDeviceWaitIdle(V.device);
        for (int i = 0; i < RB_RING; i++) {
            if (V.rbMap[i])    vkUnmapMemory(V.device, V.rbMem[i]);
            if (V.rbBuf[i])    vkDestroyBuffer(V.device, V.rbBuf[i], NULL);
            if (V.rbMem[i])    vkFreeMemory(V.device, V.rbMem[i], NULL);
            if (V.rbImg[i])    vkDestroyImage(V.device, V.rbImg[i], NULL);
            if (V.rbImgMem[i]) vkFreeMemory(V.device, V.rbImgMem[i], NULL);
            if (V.rbFence[i])  vkDestroyFence(V.device, V.rbFence[i], NULL);
        }
        if (V.pool)       vkDestroyCommandPool(V.device, V.pool, NULL);
        if (V.ownDevice) {
            // We created both the device and the instance → we own the full teardown.
            vkDestroyDevice(V.device, NULL);
            if (V.instance) vkDestroyInstance(V.instance, NULL);
        }
        // else: the CORE created the device via its negotiation create_device, and tears the device
        // down itself at process exit through a static C++ Context destructor (mupen64plus-next /
        // parallel-rdp). That destructor calls vkDeviceWaitIdle, which needs a LIVE VkInstance. If we
        // call destroy_device or vkDestroyInstance here, that later destructor dereferences a dangling
        // device/instance → SIGSEGV at exit (observed in MVKDevice::waitIdle). The game-host is a
        // single-shot process (it exits when the game closes), so we leave the core-owned device and
        // our instance for the core's exit teardown / the OS to reclaim — no leak that outlives the run.
    }
    pthread_mutex_destroy(&V.qlock);
    memset(&V, 0, sizeof V);
}
