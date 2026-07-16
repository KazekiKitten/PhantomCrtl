#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "phantom.h"

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x100000

#ifdef __cplusplus
extern "C" {
#endif

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Two FS sessions: one for the module, headroom for config IO.
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = hidInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    rc = hiddbgInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
    fsdevMountSdmc();

    smExit();
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    hiddbgExit();
    hidExit();
}

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// PhantomCtrl runtime
// ---------------------------------------------------------------------------

static PhantomState g_state = {
    .enabled       = true,   // feed the virtual pad by default
    .virt_attached = false,
    .wireless_count = 0,
    .virt_handle   = 0,
};

HiddbgHdlsSessionId g_hdls_session; // non-static: used for HDLS calls
static alignas(0x1000) u8  g_hdls_workbuffer[PHANTOM_HDLS_WORKBUFFER_SIZE];
static bool g_hdls_ready = false;

// Implemented in config.c
bool phantom_config_load(PhantomState *st);
void phantom_config_save(const PhantomState *st);

// Build the device descriptor for our virtual controller: a Pro-Controller-style
// FullKey pad over Bluetooth, so games treat it like a real paired controller.
static void phantom_make_device_info(HiddbgHdlsDeviceInfo *info)
{
    memset(info, 0, sizeof(*info));
    info->deviceType        = HidDeviceType_FullKey15;
    info->npadInterfaceType = HidNpadInterfaceType_Bluetooth;
    info->singleColorBody    = 0xFF2D2D2D; // dark grey body
    info->singleColorButtons = 0xFFE6E6E6; // light buttons
    info->colorLeftGrip      = 0xFF2D2D2D;
    info->colorRightGrip     = 0xFF2D2D2D;
}

static Result phantom_attach_virtual(void)
{
    HiddbgHdlsDeviceInfo info;
    phantom_make_device_info(&info);

    HiddbgHdlsHandle handle = {0};
    Result rc = hiddbgAttachHdlsVirtualDevice(&handle, &info);
    if (R_SUCCEEDED(rc)) {
        g_state.virt_handle   = handle.handle;
        g_state.virt_attached = true;
    }
    return rc;
}

static Result phantom_detach_virtual(void)
{
    if (!g_state.virt_attached)
        return 0;
    HiddbgHdlsHandle handle = { .handle = g_state.virt_handle };
    Result rc = hiddbgDetachHdlsVirtualDevice(handle);
    g_state.virt_attached = false;
    g_state.virt_handle   = 0;
    return rc;
}

// Count real (non-handheld, non-virtual) controllers currently connected.
// Used only for overlay status display.
static u32 phantom_count_wireless(void)
{
    u32 count = 0;
    // Player slots No1..No8. A slot with a live styleset that isn't our
    // virtual handheld source counts as a real wireless pad.
    for (int i = HidNpadIdType_No1; i <= HidNpadIdType_No8; i++) {
        u32 style = hidGetNpadStyleSet((HidNpadIdType)i);
        if (style != 0)
            count++;
    }
    return count;
}

int main(int argc, char* argv[])
{
    // Acquire the Hdls work buffer once. session_id is threaded through all
    // later Hdls calls (required on [13.0.0+]).
    Result rc = hiddbgAttachHdlsWorkBuffer(&g_hdls_session, g_hdls_workbuffer,
                                           sizeof(g_hdls_workbuffer));
    g_hdls_ready = R_SUCCEEDED(rc);

    // Configure input reading for the built-in Handheld source.
    hidInitializeNpad();
    const HidNpadIdType ids[] = { HidNpadIdType_Handheld, HidNpadIdType_No1 };
    hidSetSupportedNpadIdType(ids, sizeof(ids)/sizeof(ids[0]));

    // Load persisted knobs at boot (if the overlay wrote them previously).
    phantom_config_load(&g_state);

    // Attach exactly once at startup. Toggling only controls what we feed.
    // This avoids handle-leak crashes when "detach during toggle" is attempted.
    if (g_hdls_ready)
        phantom_attach_virtual();

    // Config sync cadence: poll disk / write status every ~200ms, not every
    // 5ms input frame. Counter derives the divisor from the intervals.
    const u32 sync_every = (u32)(200000000ULL / PHANTOM_POLL_INTERVAL_NS);
    u32 tick = 0;

    while (true) {
        if (g_hdls_ready && g_state.virt_attached) {
            HiddbgHdlsState st;
            memset(&st, 0, sizeof(st));

            st.battery_level = 4;            // full
            st.flags         = 1;            // BIT(0) IsPowered - stays "powered"

            if (g_state.enabled) {
                HidNpadHandheldState hh;
                size_t n = hidGetNpadStatesHandheld(HidNpadIdType_Handheld, &hh, 1);
                if (n > 0) {
                    st.buttons        = hh.buttons & ~(HidNpadButton_StickLDown | HidNpadButton_StickLRight);
                    st.analog_stick_l = hh.analog_stick_l;
                    st.analog_stick_r = hh.analog_stick_r;
                }
            }
            // else: leave buttons/sticks zeroed - pad stays attached, reports nothing.

            HiddbgHdlsHandle handle = { .handle = g_state.virt_handle };
            hiddbgSetHdlsState(handle, &st);
        }

        // Low-frequency: sync the overlay's toggles in, push status out.
        if (++tick >= sync_every) {
            tick = 0;
            phantom_config_load(&g_state);
            g_state.wireless_count = phantom_count_wireless();
            phantom_config_save(&g_state);
        }

        svcSleepThread(PHANTOM_POLL_INTERVAL_NS);
    }

    // Unreachable in normal operation; kept for correctness.
    phantom_detach_virtual();
    if (g_hdls_ready)
        hiddbgReleaseHdlsWorkBuffer(g_hdls_session);
    return 0;
}
