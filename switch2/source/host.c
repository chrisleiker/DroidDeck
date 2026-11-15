#include "host.h"

#include <stdio.h>
#include <string.h>

static DdCheck *add_check(DdHost *host, const char *title) {
    DdCheck *check = &host->checks[host->check_count++];
    check->title = title;
    check->state = DD_UNKNOWN;
    check->detail[0] = 0;
    return check;
}

#define DETAIL(check, ...) snprintf((check)->detail, sizeof((check)->detail), __VA_ARGS__)

static void check_vulkan(DdHost *host) {
    DdCheck *check = add_check(host, "Vulkan");
    char value[128], api[32];
    dd_vkinfo_query(host->vkinfo, sizeof(host->vkinfo));
    if (dd_report_get(host->vkinfo, "error", value, sizeof(value))) {
        check->state = DD_FAIL;
        DETAIL(check, "%s", value);
        return;
    }
    dd_report_get(host->vkinfo, "device", host->gpu, sizeof(host->gpu));
    if (!dd_report_get(host->vkinfo, "api", api, sizeof(api))) api[0] = 0;
    static const char *const dxvk[] = { "ext.VK_EXT_robustness2", "ext.VK_EXT_transform_feedback", "ext.VK_KHR_dynamic_rendering" };
    const char *missing = NULL;
    for (size_t i = 0; i < sizeof(dxvk) / sizeof(*dxvk); i++)
        if (!dd_report_get(host->vkinfo, dxvk[i], value, sizeof(value)) || strcmp(value, "yes")) missing = dxvk[i] + 4;
    check->state = missing ? DD_FAIL : DD_PASS;
    if (missing) DETAIL(check, "%s, Vulkan %s. DXVK needs %s.", host->gpu, api, missing);
    else DETAIL(check, "%s, Vulkan %s", host->gpu, api);
}

#ifdef __SWITCH__

#include <switch.h>
#include <kaihen.h>
#include <sys/statvfs.h>

static PadState pad;
static PlFontData font;
static bool font_loaded;

void dd_host_init(void) {
    romfsInit();
    plInitialize(PlServiceType_User);
    font_loaded = R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard));
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
}

void dd_host_exit(void) {
    plExit();
    romfsExit();
}

bool dd_host_running(void) {
    return appletMainLoop();
}

uint32_t dd_host_keys_down(void) {
    padUpdate(&pad);
    u64 down = padGetButtonsDown(&pad);
    uint32_t keys = 0;
    if (down & HidNpadButton_AnyUp) keys |= DD_KEY_UP;
    if (down & HidNpadButton_AnyDown) keys |= DD_KEY_DOWN;
    if (down & HidNpadButton_AnyLeft) keys |= DD_KEY_LEFT;
    if (down & HidNpadButton_AnyRight) keys |= DD_KEY_RIGHT;
    if (down & HidNpadButton_A) keys |= DD_KEY_A;
    if (down & HidNpadButton_B) keys |= DD_KEY_B;
    if (down & HidNpadButton_L) keys |= DD_KEY_L;
    if (down & HidNpadButton_R) keys |= DD_KEY_R;
    if (down & HidNpadButton_Plus) keys |= DD_KEY_QUIT;
    return keys;
}

const void *dd_host_font(size_t *size) {
    if (!font_loaded) return NULL;
    *size = font.size;
    return font.address;
}

const char *dd_host_asset(const char *name, char *path, size_t cap) {
    snprintf(path, cap, "romfs:/%s", name);
    return path;
}

static void check_kaihen(DdHost *host) {
    DdCheck *check = add_check(host, "kaiHEN");
    Service ro, hen;
    HenStatus status;
    Result rc = kaihenGetRoSession(&ro);
    if (R_FAILED(rc)) {
        check->state = DD_FAIL;
        DETAIL(check, "Not started from hbloader (0x%x)", rc);
        return;
    }
    rc = kaihenOpenHomebrewSession(&ro, &hen);
    if (R_SUCCEEDED(rc)) {
        rc = henGetStatus(&hen, &status);
        serviceClose(&hen);
    }
    if (R_FAILED(rc)) {
        check->state = DD_FAIL;
        DETAIL(check, "No homebrew session (0x%x)", rc);
        return;
    }
    check->state = R_SUCCEEDED(status.initialization_result) ? DD_PASS : DD_FAIL;
    DETAIL(check, "Protocol %u, startup result 0x%x", status.protocol_version, status.initialization_result);
}

// FEX translates x86 code at run time, so it needs memory it can write and then execute.
static void check_jit(DdHost *host) {
    DdCheck *check = add_check(host, "JIT for FEX");
    static const u32 code[] = { 0x52800540, 0xd65f03c0 }; // mov w0, #42; ret
    Jit jit;
    Result rc = jitCreate(&jit, 0x10000);
    if (R_SUCCEEDED(rc)) {
        rc = jitTransitionToWritable(&jit);
        if (R_SUCCEEDED(rc)) {
            memcpy(jitGetRwAddr(&jit), code, sizeof(code));
            rc = jitTransitionToExecutable(&jit);
        }
        if (R_SUCCEEDED(rc)) {
            int (*entry)(void) = (int (*)(void)) jitGetRxAddr(&jit);
            int result = entry();
            check->state = result == 42 ? DD_PASS : DD_FAIL;
            DETAIL(check, result == 42 ? "Generated code runs" : "Generated code returned %d", result);
        }
        jitClose(&jit);
    }
    if (R_FAILED(rc)) {
        check->state = DD_FAIL;
        DETAIL(check, "No executable memory (0x%x)", rc);
    }
}

static void check_storage(DdHost *host) {
    DdCheck *check = add_check(host, "Storage");
    struct statvfs vfs;
    if (statvfs("sdmc:/", &vfs) != 0) {
        check->state = DD_FAIL;
        DETAIL(check, "No SD card");
        return;
    }
    unsigned long long free_bytes = (unsigned long long) vfs.f_bavail * vfs.f_frsize;
    check->state = free_bytes >= DD_RUNTIME_BYTES ? DD_PASS : DD_FAIL;
    DETAIL(check, "%.1f GB free on the SD card. The runtime needs 3 GB.", free_bytes / 1073741824.0);
}

static void check_memory(DdHost *host) {
    DdCheck *check = add_check(host, "Memory");
    u64 total = heapGetTotalSize();
    check->state = total >= (2ull << 30) ? DD_PASS : DD_FAIL;
    DETAIL(check, "%llu MB heap for this program", (unsigned long long) (total >> 20));
}

void dd_host_probe(DdHost *host) {
    memset(host, 0, sizeof(*host));
    SetSysFirmwareVersion fw;
    if (R_SUCCEEDED(setsysInitialize())) {
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) snprintf(host->firmware, sizeof(host->firmware), "%s", fw.display_version);
        setsysExit();
    }
    check_kaihen(host);
    check_jit(host);
    check_vulkan(host);
    check_storage(host);
    check_memory(host);
    DdCheck *linux_abi = add_check(host, "Linux runtime");
    linux_abi->state = DD_FAIL;
    DETAIL(linux_abi, "proot, FEX, Proton and Steam are Linux programs. Horizon has no Linux system calls, fork, exec or ptrace.");
    host->runtime_can_start = false;
}

#else

#include <SDL2/SDL.h>

void dd_host_init(void) {}
void dd_host_exit(void) {}

static bool quit;

bool dd_host_running(void) {
    return !quit;
}

uint32_t dd_host_keys_down(void) {
    uint32_t keys = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) quit = true;
        if (event.type != SDL_KEYDOWN) continue;
        switch (event.key.keysym.sym) {
            case SDLK_UP: keys |= DD_KEY_UP; break;
            case SDLK_DOWN: keys |= DD_KEY_DOWN; break;
            case SDLK_LEFT: keys |= DD_KEY_LEFT; break;
            case SDLK_RIGHT: keys |= DD_KEY_RIGHT; break;
            case SDLK_RETURN: keys |= DD_KEY_A; break;
            case SDLK_BACKSPACE: keys |= DD_KEY_B; break;
            case SDLK_q: keys |= DD_KEY_L; break;
            case SDLK_e: keys |= DD_KEY_R; break;
            case SDLK_ESCAPE: keys |= DD_KEY_QUIT; break;
        }
    }
    return keys;
}

const void *dd_host_font(size_t *size) {
    (void) size;
    return NULL;
}

const char *dd_host_asset(const char *name, char *path, size_t cap) {
    snprintf(path, cap, "romfs/%s", name);
    return path;
}

void dd_host_probe(DdHost *host) {
    memset(host, 0, sizeof(*host));
    snprintf(host->firmware, sizeof(host->firmware), "desktop preview");
    static const char *const titles[] = { "kaiHEN", "JIT for FEX", NULL, "Storage", "Memory" };
    for (size_t i = 0; i < sizeof(titles) / sizeof(*titles); i++) {
        if (!titles[i]) { check_vulkan(host); continue; }
        DdCheck *check = add_check(host, titles[i]);
        DETAIL(check, "Checked on the console only");
    }
    DdCheck *linux_abi = add_check(host, "Linux runtime");
    linux_abi->state = DD_FAIL;
    DETAIL(linux_abi, "proot, FEX, Proton and Steam are Linux programs. Horizon has no Linux system calls, fork, exec or ptrace.");
}

#endif
