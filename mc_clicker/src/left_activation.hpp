#pragma once

// Click Only always requires both a physical LMB hold and a marked slot.
// The legacy auto mode can optionally restrict itself to marked slots.
inline bool left_activation_allowed(bool click_only, bool auto_restrict_slots,
                                    bool physical_lmb_down, bool slot_marked) {
    if (click_only) return physical_lmb_down && slot_marked;
    return !auto_restrict_slots || slot_marked;
}
