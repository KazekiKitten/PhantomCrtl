#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "phantom.h"

// ---------------------------------------------------------------------------
// File-based state channel (overlay <-> sysmodule)
//
// The overlay writes /config/phantomctrl/config.ini; the sysmodule polls it.
// Format is a handful of `key=value` lines, so we parse by hand rather than
// pull in a portlib. Failure to read leaves the in-memory state untouched.
//
// Chosen over a custom IPC service deliberately: a malformed file just gets
// ignored, whereas a buggy IPC session server can hang the system's service
// manager. Stability first; IPC can come after hardware validation.
// ---------------------------------------------------------------------------

#define PHANTOM_CONFIG_DIR "/config/phantomctrl"

static bool parse_bool(const char *v)
{
    return v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y';
}

// Read enabled/suppress from disk into *st. Returns true if the file existed
// and was read. Only the two writable knobs are consumed; status fields
// (virt_attached, handle, counts) are owned by the sysmodule and never read.
bool phantom_config_load(PhantomState *st)
{
    FILE *f = fopen(PHANTOM_CONFIG_PATH, "r");
    if (!f)
        return false;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strncmp(key, "enabled", 7) == 0)
            st->enabled = parse_bool(val);
        else if (strncmp(key, "suppress", 8) == 0)
            st->suppress = parse_bool(val);
    }
    fclose(f);
    return true;
}

// Write the full current state back so the overlay can render live status.
void phantom_config_save(const PhantomState *st)
{
    mkdir(PHANTOM_CONFIG_DIR, 0777);

    FILE *f = fopen(PHANTOM_CONFIG_PATH, "w");
    if (!f)
        return;

    // Writable knobs first, then read-only status the overlay displays.
    fprintf(f, "enabled=%d\n",        st->enabled ? 1 : 0);
    fprintf(f, "suppress=%d\n",       st->suppress ? 1 : 0);
    fprintf(f, "virt_attached=%d\n",  st->virt_attached ? 1 : 0);
    fprintf(f, "virt_handle=%016lx\n", st->virt_handle);
    fprintf(f, "wireless_count=%u\n", st->wireless_count);
    fclose(f);
}
