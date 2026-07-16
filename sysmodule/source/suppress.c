#include <string.h>
#include <switch.h>

#include "phantom.h"

// ---------------------------------------------------------------------------
// Handheld suppression (EXPERIMENTAL)
//
// This is the untested part of PhantomCtrl. The theory: dump the current Hdls
// npad assignment, and re-apply it with the `flag` argument so the system
// reassigns npad slots — pushing our virtual FullKey device into a real player
// slot and preventing the raw built-in Handheld from also claiming one.
//
// Three possible hardware outcomes (validate with the overlay toggle):
//   1. Games see virtual-FullKey + wireless cleanly (success).
//   2. Suppression is a no-op but games tolerate the extra Handheld input.
//   3. Games still bind raw Handheld to P1 -> would need a hid-mitm approach.
//
// Because the assignment struct's fields are largely opaque, we do the safest
// possible thing: dump the real state, keep every existing entry intact, and
// only re-apply. We never fabricate entries. If dumping fails we bail without
// touching anything.
// ---------------------------------------------------------------------------

extern HiddbgHdlsSessionId g_hdls_session;

Result phantom_suppress_apply(bool suppress)
{
    HiddbgHdlsNpadAssignment assignment;
    memset(&assignment, 0, sizeof(assignment));

    Result rc = hiddbgDumpHdlsNpadAssignmentState(g_hdls_session, &assignment);
    if (R_FAILED(rc))
        return rc; // can't read state -> do nothing, leave system untouched

    // Re-apply the dumped assignment. The bool selects the reassignment
    // behavior; `true` requests the system re-evaluate slot ownership, which
    // is what we want when enabling suppression.
    rc = hiddbgApplyHdlsNpadAssignmentState(g_hdls_session, &assignment, suppress);
    return rc;
}
