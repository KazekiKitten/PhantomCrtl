#pragma once
#include <switch.h>

// PhantomCtrl shared definitions.
// Program Title ID: 0100000000504843  ("PHC" in the low bytes).

#define PHANTOM_TITLE_ID 0x0100000000504843ULL

// Hdls work buffer size. sys-con uses 0x1000; the state list is small.
#define PHANTOM_HDLS_WORKBUFFER_SIZE 0x1000

// Poll period. 60Hz-ish. Games sample npad at their own rate; 5ms keeps
// virtual-pad latency below one display frame without pinning the core.
#define PHANTOM_POLL_INTERVAL_NS (5ULL * 1000000ULL) // 5 ms

// Persisted settings path on the SD card.
#define PHANTOM_CONFIG_PATH "/config/phantomctrl/config.ini"

// Runtime state shared between the worker thread and (later) the IPC service.
typedef struct {
    bool enabled;          // master on/off: feed the virtual pad
    bool virt_attached;    // is our virtual FullKey device currently attached
    u32  wireless_count;   // number of real (non-handheld) pads seen
    u64  virt_handle;      // HiddbgHdlsHandle.handle of our virtual device
} PhantomState;
