// Neutral input event abstraction shared by all platform backends
// (Sokol and SDL). Pressure-aware so a graphics-tablet pen or touchscreen
// finger can drive pressure-sensitive sketching.
#pragma once
#include <cstdint>

namespace Input {

enum class Type {
    Move, Down, Up, Scroll, Resize,
    KeyDown, KeyUp, Char,
    PenMove, PenDown, PenUp,
    FingerDown, FingerMove, FingerUp
};

enum class Pointer { Mouse, Pen, Touch };

// Key codes are backend-independent; see KeyCodes.hpp for the sokol/SDL maps.
enum class Key : int {
    NoKey = 0,
    Escape, Enter, KPEnter, Backspace, Tab, Space,
    Left, Right, Up, Down,
    Equal, KPAdd, Minus, KPSubtract,
    A, B, E, G, O, P, Q, S, U, W, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9
};

enum Mod : int {
    Shift  = 1 << 0,
    Ctrl   = 1 << 1,
    Alt    = 1 << 2,
    Super  = 1 << 3
};

struct Event {
    Type type = Type::Move;
    float x = 0.0f, y = 0.0f, dx = 0.0f, dy = 0.0f;
    float pressure = 1.0f;       // pen/finger pressure, 0..1 (1.0 for mouse)
    int button = 0;
    int key = 0;                 // Input::Key
    int mods = 0;                // Input::Mod bits
    uint32_t codepoint = 0;      // UTF-32 for Char events
    Pointer pointer = Pointer::Mouse;

    bool isPress() const { return type == Type::Down || type == Type::PenDown || type == Type::FingerDown; }
    bool isRelease() const { return type == Type::Up || type == Type::PenUp || type == Type::FingerUp; }
    bool isMove() const { return type == Type::Move || type == Type::PenMove || type == Type::FingerMove; }
};

} // namespace Input

// Sokol -> neutral key/mod mapping (needs sokol_app.h for the SAPP_KEYCODE_* #defines).
#include "sokol_app.h"
namespace Input {
inline Key keyFromSokol(uint32_t k) {
    switch (k) {
        case SAPP_KEYCODE_ESCAPE: return Key::Escape;
        case SAPP_KEYCODE_ENTER: return Key::Enter;
        case SAPP_KEYCODE_KP_ENTER: return Key::KPEnter;
        case SAPP_KEYCODE_BACKSPACE: return Key::Backspace;
        case SAPP_KEYCODE_TAB: return Key::Tab;
        case SAPP_KEYCODE_SPACE: return Key::Space;
        case SAPP_KEYCODE_LEFT: return Key::Left;
        case SAPP_KEYCODE_RIGHT: return Key::Right;
        case SAPP_KEYCODE_UP: return Key::Up;
        case SAPP_KEYCODE_DOWN: return Key::Down;
        case SAPP_KEYCODE_EQUAL: case SAPP_KEYCODE_KP_ADD: return Key::Equal;
        case SAPP_KEYCODE_MINUS: case SAPP_KEYCODE_KP_SUBTRACT: return Key::Minus;
        case SAPP_KEYCODE_A: return Key::A;
        case SAPP_KEYCODE_B: return Key::B;
        case SAPP_KEYCODE_E: return Key::E;
        case SAPP_KEYCODE_G: return Key::G;
        case SAPP_KEYCODE_O: return Key::O;
        case SAPP_KEYCODE_P: return Key::P;
        case SAPP_KEYCODE_Q: return Key::Q;
        case SAPP_KEYCODE_S: return Key::S;
        case SAPP_KEYCODE_U: return Key::U;
        case SAPP_KEYCODE_W: return Key::W;
        case SAPP_KEYCODE_Y: return Key::Y;
        case SAPP_KEYCODE_Z: return Key::Z;
        case SAPP_KEYCODE_0: return Key::Digit0;
        case SAPP_KEYCODE_1: return Key::Digit1;
        case SAPP_KEYCODE_2: return Key::Digit2;
        case SAPP_KEYCODE_3: return Key::Digit3;
        case SAPP_KEYCODE_4: return Key::Digit4;
        case SAPP_KEYCODE_5: return Key::Digit5;
        case SAPP_KEYCODE_6: return Key::Digit6;
        case SAPP_KEYCODE_7: return Key::Digit7;
        case SAPP_KEYCODE_8: return Key::Digit8;
        case SAPP_KEYCODE_9: return Key::Digit9;
        default: return Key::NoKey;
    }
}
inline int modsFromSokol(uint32_t m) {
    int r = 0;
    if (m & SAPP_MODIFIER_SHIFT) r |= (int)Mod::Shift;
    if (m & SAPP_MODIFIER_CTRL)  r |= (int)Mod::Ctrl;
    if (m & SAPP_MODIFIER_ALT)   r |= (int)Mod::Alt;
    if (m & SAPP_MODIFIER_SUPER) r |= (int)Mod::Super;
    return r;
}
} // namespace Input
