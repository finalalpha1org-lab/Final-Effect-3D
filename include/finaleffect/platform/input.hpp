// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <variant>

namespace finaleffect::platform {

enum class KeyCode : std::uint16_t {
    Unknown = 0,
    Escape,
    Delete,
    Backspace,
    Space,
    F,
    Z,
    Y,
    W,
    E,
    R,
    A,
    S,
    D,
    Tab,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    LeftShift,
    LeftControl,
    LeftAlt,
};

enum class MouseButton : std::uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
};

enum class KeyAction : std::uint8_t {
    Press = 0,
    Release = 1,
    Repeat = 2,
};

struct KeyModifiers {
    bool shift{false};
    bool ctrl{false};
    bool alt{false};
    bool super{false};
};

struct KeyEvent {
    KeyCode key{KeyCode::Unknown};
    KeyAction action{KeyAction::Press};
    KeyModifiers mods{};
};

struct MouseButtonEvent {
    MouseButton button{MouseButton::Left};
    KeyAction action{KeyAction::Press};
    double x{0.0};
    double y{0.0};
    KeyModifiers mods{};
};

struct CursorMoveEvent {
    double x{0.0};
    double y{0.0};
};

struct ScrollEvent {
    double delta_x{0.0};
    double delta_y{0.0};
    KeyModifiers mods{};
};

using InputEvent = std::variant<KeyEvent, MouseButtonEvent, CursorMoveEvent, ScrollEvent>;

} // namespace finaleffect::platform
