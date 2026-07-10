#ifndef KEYCODES_H
#define KEYCODES_H

// PicoCalc keyboard keycodes (uit register 0x09, high byte).
// Printbare toetsen leveren gewoon hun ASCII-waarde (0x20-0x7E).
// Onderstaande niet-ASCII codes zijn de speciale toetsen.
//
// [v] = op deze PicoCalc geverifieerd (2026-07-10)
// [f] = uit de officiele clockworkpi keyboard-firmware, niet los getest
//       (maar de firmwaretabel klopt: alle geteste codes matchen).

// --- Control chars (ASCII) ---
#define PC_KEY_BACKSPACE 0x08 // [v]
#define PC_KEY_TAB       0x09 // [v]
#define PC_KEY_ENTER     0x0A // [v]
#define PC_KEY_SPACE     0x20 // [v]

// --- F-toetsen (0x81-0x8A) ---
#define PC_KEY_F1 0x81 // [f]
#define PC_KEY_F2 0x82 // [f]
#define PC_KEY_F3 0x83 // [f]
#define PC_KEY_F4 0x84 // [f]
#define PC_KEY_F5 0x85 // [f]
#define PC_KEY_F6 0x86 // [f]
#define PC_KEY_F7 0x87 // [f]
#define PC_KEY_F8 0x88 // [f]
#define PC_KEY_F9 0x89 // [f]
#define PC_KEY_F10 0x8A // [f]

// --- Modifiers (0xA_) ---
#define PC_KEY_ALT       0xA1 // [v]
#define PC_KEY_SHIFT_L   0xA2 // [v]
#define PC_KEY_SHIFT_R   0xA3 // [f]
#define PC_KEY_SYM       0xA4 // [f]
#define PC_KEY_CTRL      0xA5 // [v]

// --- Navigatie / Esc (0xB_) ---
#define PC_KEY_ESC       0xB1 // [v]
#define PC_KEY_LEFT      0xB4 // [v]
#define PC_KEY_UP        0xB5 // [v]
#define PC_KEY_DOWN      0xB6 // [v]
#define PC_KEY_RIGHT     0xB7 // [v]

// --- Locks (0xC_) ---
#define PC_KEY_CAPS_LOCK 0xC1 // [v]

// --- Editing (0xD_) ---
#define PC_KEY_BREAK     0xD0 // [f]
#define PC_KEY_INSERT    0xD1 // [f]
#define PC_KEY_HOME      0xD2 // [v] (op deze layout via Shift bereikt)
#define PC_KEY_DEL       0xD4 // [v]
#define PC_KEY_END       0xD5 // [v] (via Shift)
#define PC_KEY_PAGE_UP   0xD6 // [f]
#define PC_KEY_PAGE_DOWN 0xD7 // [f]

#endif // KEYCODES_H
