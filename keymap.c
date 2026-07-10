#include <stdio.h>
#include "keymap.h"
#include "keycodes.h"
#include "i2ckbd.h"
#include "machine.h"

// MSX PPI-matrix indices (rij*8 + kolom), afgeleid uit PPI.c / de matrix-tabel.
#define MSX_SHIFT 48
#define MSX_CTRL  49
#define MSX_GRAPH 50
#define MSX_CAPS  51
#define MSX_ENTER 63
#define MSX_BS    61
#define MSX_STOP  60
#define MSX_TAB   59
#define MSX_ESC   58
#define MSX_SPACE 64
#define MSX_DEL   67
#define MSX_LEFT  68
#define MSX_UP    69
#define MSX_DOWN  70
#define MSX_RIGHT 71
#define MSX_F1    53
#define MSX_F2    54
#define MSX_F3    55
#define MSX_F4    56
#define MSX_F5    57

// ASCII -> MSX-matrix. idx1 = matrixindex + 1 (0 = niet gemapt).
typedef struct { uint8_t idx1; uint8_t shift; } msx_key_t;

static const msx_key_t ascii_map[128] = {
    ['0'] = {0 + 1, 0}, ['1'] = {1 + 1, 0}, ['2'] = {2 + 1, 0}, ['3'] = {3 + 1, 0},
    ['4'] = {4 + 1, 0}, ['5'] = {5 + 1, 0}, ['6'] = {6 + 1, 0}, ['7'] = {7 + 1, 0},
    ['8'] = {8 + 1, 0}, ['9'] = {9 + 1, 0},

    ['a'] = {22 + 1, 0}, ['b'] = {23 + 1, 0}, ['c'] = {24 + 1, 0}, ['d'] = {25 + 1, 0},
    ['e'] = {26 + 1, 0}, ['f'] = {27 + 1, 0}, ['g'] = {28 + 1, 0}, ['h'] = {29 + 1, 0},
    ['i'] = {30 + 1, 0}, ['j'] = {31 + 1, 0}, ['k'] = {32 + 1, 0}, ['l'] = {33 + 1, 0},
    ['m'] = {34 + 1, 0}, ['n'] = {35 + 1, 0}, ['o'] = {36 + 1, 0}, ['p'] = {37 + 1, 0},
    ['q'] = {38 + 1, 0}, ['r'] = {39 + 1, 0}, ['s'] = {40 + 1, 0}, ['t'] = {41 + 1, 0},
    ['u'] = {42 + 1, 0}, ['v'] = {43 + 1, 0}, ['w'] = {44 + 1, 0}, ['x'] = {45 + 1, 0},
    ['y'] = {46 + 1, 0}, ['z'] = {47 + 1, 0},

    // hoofdletters: zelfde toets + MSX-shift
    ['A'] = {22 + 1, 1}, ['B'] = {23 + 1, 1}, ['C'] = {24 + 1, 1}, ['D'] = {25 + 1, 1},
    ['E'] = {26 + 1, 1}, ['F'] = {27 + 1, 1}, ['G'] = {28 + 1, 1}, ['H'] = {29 + 1, 1},
    ['I'] = {30 + 1, 1}, ['J'] = {31 + 1, 1}, ['K'] = {32 + 1, 1}, ['L'] = {33 + 1, 1},
    ['M'] = {34 + 1, 1}, ['N'] = {35 + 1, 1}, ['O'] = {36 + 1, 1}, ['P'] = {37 + 1, 1},
    ['Q'] = {38 + 1, 1}, ['R'] = {39 + 1, 1}, ['S'] = {40 + 1, 1}, ['T'] = {41 + 1, 1},
    ['U'] = {42 + 1, 1}, ['V'] = {43 + 1, 1}, ['W'] = {44 + 1, 1}, ['X'] = {45 + 1, 1},
    ['Y'] = {46 + 1, 1}, ['Z'] = {47 + 1, 1},

    [' '] = {64 + 1, 0},
    // symbolen die als losse matrixpositie bestaan (onzeker; verfijnen na test)
    ['-'] = {10 + 1, 0}, ['^'] = {11 + 1, 0}, ['@'] = {13 + 1, 0}, [';'] = {15 + 1, 0},
    [':'] = {16 + 1, 0}, [')'] = {17 + 1, 0}, [','] = {18 + 1, 0}, ['.'] = {19 + 1, 0},
    ['/'] = {20 + 1, 0}, ['*'] = {21 + 1, 0}, ['('] = {14 + 1, 0}, ['$'] = {12 + 1, 0},
};

// Speciale (niet-ASCII) PicoCalc-keycodes -> MSX-matrixindex, of -1.
static int special_index(uint8_t code)
{
    switch (code) {
    case PC_KEY_ENTER:     return MSX_ENTER;
    case PC_KEY_BACKSPACE: return MSX_BS;
    case PC_KEY_TAB:       return MSX_TAB;
    case PC_KEY_ESC:       return MSX_ESC;
    case PC_KEY_LEFT:      return MSX_LEFT;
    case PC_KEY_UP:        return MSX_UP;
    case PC_KEY_DOWN:      return MSX_DOWN;
    case PC_KEY_RIGHT:     return MSX_RIGHT;
    case PC_KEY_DEL:       return MSX_DEL;
    case PC_KEY_CTRL:      return MSX_CTRL;   // o.a. Ctrl+STOP (break)
    case PC_KEY_ALT:       return MSX_GRAPH;
    case PC_KEY_CAPS_LOCK: return MSX_CAPS;
    case PC_KEY_F1:        return MSX_F1;
    case PC_KEY_F2:        return MSX_F2;
    case PC_KEY_F3:        return MSX_F3;
    case PC_KEY_F4:        return MSX_F4;
    case PC_KEY_F5:        return MSX_F5;
    // De PicoCalc stuurt Shift+F1..F5 als F6..F10 (0x86..0x8A). Samen met de
    // vastgehouden Shift (matrix 48) geeft F1..F5 op de MSX dan F6..F10.
    case PC_KEY_F6:        return MSX_F1;
    case PC_KEY_F7:        return MSX_F2;
    case PC_KEY_F8:        return MSX_F3;
    case PC_KEY_F9:        return MSX_F4;
    case PC_KEY_F10:       return MSX_F5;
    default:               return -1;
    }
}

static bool pc_shift_held = false;

void keymap_handle(uint8_t state, uint8_t code)
{
    if (state == KBD_STATE_IDLE) return;
    bool down = (state == KBD_STATE_PRESSED || state == KBD_STATE_HOLD);

    // De PicoCalc Shift-toets rechtstreeks op MSX-Shift: nodig voor Shift+F-toets
    // (=> F6-F10) en alles wat de STM32 niet zelf in ASCII oplost.
    if (code == PC_KEY_SHIFT_L || code == PC_KEY_SHIFT_R) {
        pc_shift_held = down;
        if (down) machine_keydown(MSX_SHIFT);
        else      machine_keyup(MSX_SHIFT);
        return;
    }

    int idx;
    uint8_t shift = 0;

    int sp = special_index(code);
    if (sp >= 0) {
        idx = sp;
    } else if (code < 128 && ascii_map[code].idx1) {
        idx = ascii_map[code].idx1 - 1;
        shift = ascii_map[code].shift;
    } else {
        printf("[kbd] code=0x%02X state=%u -> UNMAPPED\n", code, state);
        return; // niet gemapt
    }

    if (down) {
        machine_keydown(idx);
        if (shift) machine_keydown(MSX_SHIFT);
    } else {
        machine_keyup(idx);
        // MSX-Shift alleen loslaten als de fysieke Shift ook niet (meer) staat,
        // anders valt shift weg terwijl je 'm nog vasthoudt.
        if (shift && !pc_shift_held) machine_keyup(MSX_SHIFT);
    }
}
