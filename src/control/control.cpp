#include "control.hpp"
#include "memory.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace flydemo {
using flydemo::mem::field;

namespace {
std::uint8_t& codeState(void* object, int code) {
    return *(static_cast<std::uint8_t*>(object) +
             static_cast<std::ptrdiff_t>(control_off::CodeStateBias) + code);
}
const std::uint8_t& codeState(const void* object, int code) {
    return *(static_cast<const std::uint8_t*>(object) +
             static_cast<std::ptrdiff_t>(control_off::CodeStateBias) + code);
}
std::uint8_t& suppression(void* object, int code) {
    return *(static_cast<std::uint8_t*>(object) +
             static_cast<std::ptrdiff_t>(control_off::SuppressBias) + code);
}

std::uint16_t loadU16LE(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

std::int16_t bitsToI16(std::uint16_t bits) {
    std::int16_t out = 0;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

std::int8_t bitsToI8(std::uint8_t bits) {
    std::int8_t out = 0;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

void storeI16LE(std::uint8_t* p, std::int16_t value) {
    std::uint16_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    p[0] = static_cast<std::uint8_t>(bits & 0xffu);
    p[1] = static_cast<std::uint8_t>((bits >> 8) & 0xffu);
}

std::int16_t loadI16LE(const std::uint8_t* p) {
    return bitsToI16(loadU16LE(p));
}

std::int16_t addWrap16(std::int16_t a, std::uint16_t bBits) {
    std::uint16_t aBits = 0;
    std::memcpy(&aBits, &a, sizeof(aBits));
    return bitsToI16(static_cast<std::uint16_t>(aBits + bBits));
}

void setAxisPair(void* object, int negativeCode, std::int8_t value) {
    if (value < 0) {
        codeState(object, negativeCode) = 1;
        codeState(object, negativeCode + 1) = 0;
    } else if (value > 0) {
        codeState(object, negativeCode) = 0;
        codeState(object, negativeCode + 1) = 1;
    } else {
        codeState(object, negativeCode) = 0;
        codeState(object, negativeCode + 1) = 0;
    }
}

void releaseJoystickOnError(ControllerDeviceState& devices,
                                      ControlInput& backend) {
    if (devices.DIJoystick) {
        backend.ReleaseDevice(ControllerDevice::Joystick);
        devices.DIJoystick = false;
    }
}
}

bool Controller_StartKeyboard(ControllerDeviceState& devices,
                              ControlInput& backend) {

    assert(devices.DInputObj);
    assert(!devices.DIKeyb);
    if (!devices.DInputObj || devices.DIKeyb)
        return false;
    if (!backend.CreateDevice(ControllerDevice::Keyboard))
        return false;
    devices.DIKeyb = true;
    if (!backend.SetDataFormat(ControllerDevice::Keyboard))
        return false;
    if (!backend.SetCooperativeLevel(ControllerDevice::Keyboard,
                                     control_di::KeyboardCoopFlags))
        return false;
    return backend.Acquire(ControllerDevice::Keyboard);
}

bool Controller_StartMouse(void* object,
                           ControllerDeviceState& devices,
                           ControlInput& backend) {

    assert(devices.DInputObj);
    assert(!devices.DIMouse);
    if (!devices.DInputObj || devices.DIMouse)
        return false;
    if (!backend.CreateDevice(ControllerDevice::Mouse))
        return false;
    devices.DIMouse = true;
    if (!backend.SetDataFormat(ControllerDevice::Mouse))
        return false;
    if (!backend.SetCooperativeLevel(ControllerDevice::Mouse,
                                     control_di::MouseCooperativeFlags))
        return false;
    if (!backend.Acquire(ControllerDevice::Mouse))
        return false;
    Controller_MouseRange(object, 0, 640, 0, 480);
    return true;
}

bool Controller_StartJoystick(void* object,
                              ControllerDeviceState& devices,
                              ControlInput& backend) {

    assert(devices.DInputObj);
    assert(!devices.DIJoystick);
    field<std::uint8_t>(object, control_off::JoystickAvailable) = 0;
    field<std::uint8_t>(object, control_off::JoystickActive) = 0;
    if (!devices.DInputObj || devices.DIJoystick)
        return false;

    if (!backend.EnumerateJoystick())
        return true;
    devices.JoystickDevice = true;

    if (!backend.QueryJoystickDevice2()) {
        backend.ReleaseJoystick();
        devices.JoystickDevice = false;
        return false;
    }
    devices.DIJoystick = true;
    backend.ReleaseJoystick();
    devices.JoystickDevice = false;

    if (!backend.SetDataFormat(ControllerDevice::Joystick) ||
        !backend.SetCooperativeLevel(ControllerDevice::Joystick,
                                     control_di::JoystickCoopFlags) ||
        !backend.SetJoystickRange(ControllerAxis::X,
                                  control_di::JoystickAxisMin,
                                  control_di::JoystickAxisMax) ||
        !backend.SetJoystickRange(ControllerAxis::Y,
                                  control_di::JoystickAxisMin,
                                  control_di::JoystickAxisMax) ||
        !backend.SetJoystickDeadZone(ControllerAxis::X,
                                     control_di::JoystickDeadZoneNormal) ||
        !backend.SetJoystickDeadZone(ControllerAxis::Y,
                                     control_di::JoystickDeadZoneNormal) ||
        !backend.Acquire(ControllerDevice::Joystick)) {

        releaseJoystickOnError(devices, backend);
        return false;
    }

    field<std::uint8_t>(object, control_off::JoystickAvailable) = 1;
    return true;
}

bool Controller_StartInput(void* object,
                                 ControllerDeviceState& devices,
                                 ControlInput& backend) {

    assert(!devices.DInputObj && !devices.DIKeyb && !devices.DIMouse &&
           !devices.DIJoystick);
    if (devices.DInputObj || devices.DIKeyb || devices.DIMouse || devices.DIJoystick)
        return false;
    if (!backend.CreateDirectInput(control_di::Version))
        return false;
    devices.DInputObj = true;

    const bool keyboardOk = Controller_StartKeyboard(devices, backend);
    const bool mouseOk = Controller_StartMouse(object, devices, backend);
    const bool joystickOk = Controller_StartJoystick(object, devices, backend);
    return keyboardOk && mouseOk && joystickOk;
}

void Controller_Shutdown(void* object,
                                    ControllerDeviceState& devices,
                                    ControlInput& backend) {

    if (!devices.DInputObj)
        return;

    if (devices.DIKeyb) {
        backend.Unacquire(ControllerDevice::Keyboard);
        backend.ReleaseDevice(ControllerDevice::Keyboard);
        devices.DIKeyb = false;
    }
    if (devices.DIMouse) {
        backend.Unacquire(ControllerDevice::Mouse);
        backend.ReleaseDevice(ControllerDevice::Mouse);
        devices.DIMouse = false;
    }

    if (devices.DIJoystick &&
        field<std::uint8_t>(object, control_off::JoystickAvailable) != 0) {
        backend.Unacquire(ControllerDevice::Joystick);
        backend.ReleaseDevice(ControllerDevice::Joystick);
        devices.DIJoystick = false;
    }

    backend.ReleaseDirectInput();
    devices.DInputObj = false;
}

void Controller_PollKeyboard(void* object,
                                      const std::uint8_t snapshot[256]) {

    std::memcpy(static_cast<std::uint8_t*>(object) + control_off::KeyboardSnapshot,
                snapshot, 256);

    for (int code = control_off::KeyboardPollMaxCode; code >= 1; --code) {
        if ((snapshot[code] & 0x80u) != 0) {
            if (suppression(object, code) == 0) {
                codeState(object, code) = 1;
                field<std::int16_t>(object, control_off::LastCode) =
                    static_cast<std::int16_t>(code);
            }
        } else {
            codeState(object, code) = 0;
            suppression(object, code) = 0;
        }
    }
}

void Controller_SetMouseState(void* object,
                                   const std::uint8_t snapshot[16]) {

    std::memcpy(static_cast<std::uint8_t*>(object) + control_off::MouseSnapshot,
                snapshot, 16);

    if ((snapshot[12] & 0x80u) != 0) {
        if (field<std::uint8_t>(object, control_off::MousePrimarySuppressed) == 0)
            field<std::uint8_t>(object, control_off::MousePressed0) = 1;
    } else {
        field<std::uint8_t>(object, control_off::MousePrimarySuppressed) = 0;
        field<std::uint8_t>(object, control_off::MousePressed0) = 0;
    }

    field<std::uint8_t>(object, control_off::MousePressed1) =
        ((snapshot[13] & 0x80u) != 0) ? 1u : 0u;

    std::int16_t x = addWrap16(field<std::int16_t>(object, control_off::MouseX),
                               loadU16LE(snapshot + 0));
    std::int16_t y = addWrap16(field<std::int16_t>(object, control_off::MouseY),
                               loadU16LE(snapshot + 4));

    const auto minX = field<std::int16_t>(object, control_off::MouseMinX);
    const auto maxX = field<std::int16_t>(object, control_off::MouseMaxX);
    const auto minY = field<std::int16_t>(object, control_off::MouseMinY);
    const auto maxY = field<std::int16_t>(object, control_off::MouseMaxY);
    if (x < minX) x = minX;
    if (x > maxX) x = maxX;
    if (y < minY) y = minY;
    if (y > maxY) y = maxY;
    field<std::int16_t>(object, control_off::MouseX) = x;
    field<std::int16_t>(object, control_off::MouseY) = y;
}

void Controller_PollJoystick(void* object,
                                      const std::uint8_t snapshot[0x50]) {
    std::memcpy(static_cast<std::uint8_t*>(object) + control_off::JoystickSnapshot,
                snapshot, 0x50);

    const auto axisX = bitsToI8(snapshot[0]);
    const auto axisY = bitsToI8(snapshot[4]);
    field<std::uint8_t>(object, control_off::JoystickAxisX) = snapshot[0];
    field<std::uint8_t>(object, control_off::JoystickAxisY) = snapshot[4];

    for (int i = 0; i < 4; ++i) {
        const std::uint8_t pressed = (snapshot[0x30 + i] & 0x80u) ? 1u : 0u;
        field<std::uint8_t>(object,
            control_off::JoystickButton0 + static_cast<std::size_t>(i)) = pressed;

        if (pressed != 0)
            codeState(object, control_off::JoystickButtonCodeBase + i) = 1;
    }

    setAxisPair(object, control_off::JoystickAxisCodeBase + 0, axisX);
    setAxisPair(object, control_off::JoystickAxisCodeBase + 2, axisY);
}

void Controller_PollKeyboard(void* object,
                             ControllerDeviceState& devices,
                             ControlInput& backend) {
    if (!devices.DIKeyb)
        return;
    std::array<std::uint8_t, 256> snapshot{};
    if (!backend.GetKeyboardState(snapshot.data())) {

        backend.Acquire(ControllerDevice::Keyboard);
        return;
    }
    Controller_PollKeyboard(object, snapshot.data());
}

void Controller_PollMouse(void* object,
                          ControllerDeviceState& devices,
                          ControlInput& backend) {
    if (!devices.DIMouse)
        return;
    std::array<std::uint8_t, 16> snapshot{};
    if (!backend.GetMouseState(snapshot.data())) {
        backend.Acquire(ControllerDevice::Mouse);
        return;
    }
    Controller_SetMouseState(object, snapshot.data());
}

void Controller_PollJoystick(void* object,
                             ControllerDeviceState& devices,
                             ControlInput& backend) {
    assert(field<std::uint8_t>(object, control_off::JoystickAvailable) == 1);
    if (!devices.DIJoystick ||
        field<std::uint8_t>(object, control_off::JoystickAvailable) != 1)
        return;

    if (!backend.PollJoystick()) {
        backend.Acquire(ControllerDevice::Joystick);
        return;
    }

    std::array<std::uint8_t, 0x50> snapshot{};
    if (!backend.GetJoystickState(snapshot.data())) {

        backend.Acquire(ControllerDevice::Joystick);
        return;
    }
    Controller_PollJoystick(object, snapshot.data());
}

void Controller_Update(void* object,
                       ControllerDeviceState& devices,
                       ControlInput& backend) {

    Controller_PollKeyboard(object, devices, backend);
    Controller_PollMouse(object, devices, backend);
    if (field<std::uint8_t>(object, control_off::JoystickAvailable) != 0 &&
        field<std::uint8_t>(object, control_off::JoystickActive) != 0)
        Controller_PollJoystick(object, devices, backend);
    Controller_UpdateActions(object);
}

bool Controller_LoadConfig(void* object,
                                const std::uint8_t* bytes,
                                std::size_t size) {
    if (bytes == nullptr || size < control_off::ConfigSize)
        return false;

    std::size_t at = 0;
    const std::int16_t minX = loadI16LE(bytes + at); at += 2;
    const std::int16_t maxX = loadI16LE(bytes + at); at += 2;
    const std::int16_t minY = loadI16LE(bytes + at); at += 2;
    const std::int16_t maxY = loadI16LE(bytes + at); at += 2;
    const std::uint8_t useJoystick = bytes[at++];

    field<std::int16_t>(object, control_off::MouseMinX) = minX;
    field<std::int16_t>(object, control_off::MouseMaxX) = maxX;
    field<std::int16_t>(object, control_off::MouseMinY) = minY;
    field<std::int16_t>(object, control_off::MouseMaxY) = maxY;
    field<std::uint8_t>(object, control_off::JoystickActive) = useJoystick;
    Controller_MouseRange(object, minX, maxX, minY, maxY);
    Controller_SetJoystick(object, useJoystick);

    for (int event = 0; event < control_off::ActionCount; ++event) {
        field<std::int16_t>(object,
            control_off::ActionBinding + static_cast<std::size_t>(event) * 2u) =
            loadI16LE(bytes + at);
        at += 2;
    }
    return true;
}

std::array<std::uint8_t, control_off::ConfigSize>
Controller_SaveConfig(const void* object) {
    std::array<std::uint8_t, control_off::ConfigSize> bytes{};
    std::size_t at = 0;
    storeI16LE(bytes.data() + at,
               field<std::int16_t>(object, control_off::MouseMinX)); at += 2;
    storeI16LE(bytes.data() + at,
               field<std::int16_t>(object, control_off::MouseMaxX)); at += 2;
    storeI16LE(bytes.data() + at,
               field<std::int16_t>(object, control_off::MouseMinY)); at += 2;
    storeI16LE(bytes.data() + at,
               field<std::int16_t>(object, control_off::MouseMaxY)); at += 2;
    bytes[at++] = field<std::uint8_t>(object, control_off::JoystickActive);
    for (int event = 0; event < control_off::ActionCount; ++event) {
        storeI16LE(bytes.data() + at, field<std::int16_t>(object,
            control_off::ActionBinding + static_cast<std::size_t>(event) * 2u));
        at += 2;
    }
    return bytes;
}

bool Controller_LoadInputs(ControllerNameTables& names,
                               const std::string* entries,
                               std::size_t count) {
    if (entries == nullptr || count < names.inputNames.size())
        return false;
    std::copy_n(entries, names.inputNames.size(), names.inputNames.begin());
    return true;
}

bool Controller_LoadEvents(ControllerNameTables& names,
                               const std::string* entries,
                               std::size_t count) {
    if (entries == nullptr || count < names.eventNames.size())
        return false;
    std::copy_n(entries, names.eventNames.size(), names.eventNames.begin());
    return true;
}

std::string_view Controller_GetInputName(const ControllerNameTables& names,
                                         std::int16_t input) {
    assert(input >= 0 && input <= control_off::MaxCode);
    if (input < 0 || input > control_off::MaxCode)
        return {};
    if (input == 0)
        return "[UNDEFINED]";
    return names.inputNames[static_cast<std::size_t>(input - 1)];
}

std::string_view Controller_GetEventName(const ControllerNameTables& names,
                                         std::int8_t event) {
    assert(event >= 0 && event < control_off::ActionCount);
    if (event < 0 || event >= control_off::ActionCount)
        return {};
    return names.eventNames[static_cast<std::size_t>(event)];
}

void Controller_MouseButtons(void* object) {
    field<std::uint8_t>(object, control_off::MousePressed0) = 0;
    field<std::uint8_t>(object, control_off::MousePressed1) = 0;
    field<std::uint8_t>(object, control_off::MousePrimarySuppressed) = 1;
}

void Controller_UpdateActions(void* object) {
    for (int action = 0; action < control_off::ActionCount; ++action) {
        const auto code = field<std::int16_t>(
            object, control_off::ActionBinding + static_cast<std::size_t>(action) * 2u);
        const auto* raw = static_cast<const std::uint8_t*>(object);
        const std::ptrdiff_t index =
            static_cast<std::ptrdiff_t>(control_off::CodeStateBias) + code;
        field<std::uint8_t>(object,
                            control_off::ActionState + static_cast<std::size_t>(action)) =
            (raw[index] == 1) ? 1u : 0u;
    }
}

std::int16_t Controller_GetCode(const void* object) {
    return field<std::int16_t>(object, control_off::LastCode);
}

bool Controller_GetCodeState(const void* object, std::int16_t code) {
    assert(code >= 0 && code <= control_off::MaxCode);
    if (code == 0)
        return false;
    return codeState(object, code) != 0;
}

void Controller_SuppressCode(void* object, std::int16_t code) {
    assert(code >= 0 && code <= control_off::MaxCode);
    codeState(object, code) = 0;
    suppression(object, code) = 1;
}

bool Controller_ActionState(const void* object, std::int8_t action) {
    assert(action >= 0 && action < control_off::ActionCount);
    return field<std::uint8_t>(object,
        control_off::ActionState + static_cast<std::size_t>(action)) != 0;
}

std::int16_t Controller_ActionBinding(const void* object,
                                         std::int8_t action) {
    assert(action >= 0 && action < control_off::ActionCount);
    return field<std::int16_t>(object,
        control_off::ActionBinding + static_cast<std::size_t>(action) * 2u);
}

void Controller_BlockAction(void* object, std::int8_t action) {
    assert(action >= 0 && action < control_off::ActionCount);
    auto& active = field<std::uint8_t>(object,
        control_off::ActionState + static_cast<std::size_t>(action));
    if (active == 0)
        return;
    active = 0;
    const auto code = Controller_ActionBinding(object, action);
    codeState(object, code) = 0;
    suppression(object, code) = 1;
}

std::int8_t Controller_ActionValue(const void* object,
                                      std::int8_t action) {
    assert(action >= 0 && action < control_off::ActionCount);
    if (!Controller_ActionState(object, action))
        return 0;

    const auto code = Controller_ActionBinding(object, action);
    const std::uint8_t rawX = field<std::uint8_t>(object, control_off::JoystickAxisX);
    const std::uint8_t rawY = field<std::uint8_t>(object, control_off::JoystickAxisY);
    switch (code) {
    case 0xE2: return bitsToI8(static_cast<std::uint8_t>(0u - rawX));
    case 0xE3: return bitsToI8(rawX);
    case 0xE4: return bitsToI8(static_cast<std::uint8_t>(0u - rawY));
    case 0xE5: return bitsToI8(rawY);
    default:   return 100;
    }
}

std::int16_t Controller_BindFirst(void* object,
                                           std::int8_t event) {
    assert(event >= 0 && event < control_off::ActionCount);
    std::int16_t chosen = 0;
    for (int code = 1; code <= control_off::MaxCode; ++code) {
        if (Controller_GetCodeState(object, static_cast<std::int16_t>(code))) {
            chosen = static_cast<std::int16_t>(code);
            break;
        }
    }
    if (chosen == 0)
        return 0;

    field<std::int16_t>(object,
        control_off::ActionBinding + static_cast<std::size_t>(event) * 2u) = chosen;

    for (int i = 0; i <= control_off::ActionCount; ++i) {
        if (i == event)
            continue;
        auto& binding = field<std::int16_t>(object,
            control_off::ActionBinding + static_cast<std::size_t>(i) * 2u);
        if (binding == chosen)
            binding = 0;
    }
    return chosen;
}

bool Controller_RebindAction(void* object,
                                        std::int8_t event,
                                        ControllerDeviceState& devices,
                                        ControlInput& backend) {
    assert(event >= 0 && event < control_off::ActionCount);

    const bool joystickCapture =
        devices.DIJoystick && Controller_HasJoystick(object) &&
        Controller_JoystickOn(object);
    if (joystickCapture) {

        (void)backend.SetJoystickDeadZone(ControllerAxis::X,
                                          control_di::JoystickDeadZoneCapture);
        (void)backend.SetJoystickDeadZone(ControllerAxis::Y,
                                          control_di::JoystickDeadZoneCapture);
    }

    std::int16_t chosen = 0;
    for (;;) {
        Controller_EndFrame(object);
        Controller_Update(object, devices, backend);

        bool anyInput = Controller_GetCode(object) != 0;
        if (!anyInput) {

            constexpr std::int16_t probeOrder[] = {
                0xE2, 0xE3, 0xE4, 0xE5, 0xDE, 0xDF, 0xE0, 0xE1
            };
            for (const auto code : probeOrder) {
                if (Controller_GetCodeState(object, code)) {
                    anyInput = true;
                    break;
                }
            }
        }
        if (!anyInput)
            continue;

        if (Controller_GetCode(object) != 0)
            Controller_Update(object, devices, backend);
        chosen = Controller_BindFirst(object, event);
        break;
    }

    if (joystickCapture) {

        (void)backend.SetJoystickDeadZone(ControllerAxis::X,
                                          control_di::JoystickDeadZoneNormal);
        (void)backend.SetJoystickDeadZone(ControllerAxis::Y,
                                          control_di::JoystickDeadZoneNormal);
    }
    return chosen != 0;
}

bool Controller_MousePrimary(const void* object) {
    return field<std::uint8_t>(object, control_off::MousePressed0) != 0;
}

bool Controller_MouseButton2(const void* object) {
    return field<std::uint8_t>(object, control_off::MousePressed1) != 0;
}

std::int16_t Controller_GetMouseX(const void* object) {
    return field<std::int16_t>(object, control_off::MouseX);
}

std::int16_t Controller_GetMouseY(const void* object) {
    return field<std::int16_t>(object, control_off::MouseY);
}

void Controller_SetMousePos(void* object,
                                 std::int16_t x,
                                 std::int16_t y) {
    assert(x >= field<std::int16_t>(object, control_off::MouseMinX) &&
           x <= field<std::int16_t>(object, control_off::MouseMaxX));
    assert(y >= field<std::int16_t>(object, control_off::MouseMinY) &&
           y <= field<std::int16_t>(object, control_off::MouseMaxY));
    field<std::int16_t>(object, control_off::MouseX) = x;
    field<std::int16_t>(object, control_off::MouseY) = y;
}

void Controller_MouseRange(void* object,
                              std::int16_t minX,
                              std::int16_t maxX,
                              std::int16_t minY,
                              std::int16_t maxY) {
    assert(minX < maxX);
    assert(minY < maxY);
    field<std::int16_t>(object, control_off::MouseMinX) = minX;
    field<std::int16_t>(object, control_off::MouseMaxX) = maxX;
    field<std::int16_t>(object, control_off::MouseMinY) = minY;
    field<std::int16_t>(object, control_off::MouseMaxY) = maxY;

    const auto x = static_cast<std::int16_t>(
        (static_cast<std::int32_t>(minX) + static_cast<std::int32_t>(maxX)) / 2);
    const auto y = static_cast<std::int16_t>(
        (static_cast<std::int32_t>(minY) + static_cast<std::int32_t>(maxY)) / 2);
    Controller_SetMousePos(object, x, y);
}

bool Controller_HasJoystick(const void* object) {
    return field<std::uint8_t>(object, control_off::JoystickAvailable) != 0;
}

bool Controller_JoystickOn(const void* object) {
    return field<std::uint8_t>(object, control_off::JoystickActive) != 0;
}

void Controller_SetJoystick(void* object, std::uint8_t use) {
    assert(use == 0 || use == 1);
    if (Controller_HasJoystick(object) && use != 0)
        field<std::uint8_t>(object, control_off::JoystickActive) = 1;
    else
        field<std::uint8_t>(object, control_off::JoystickActive) = 0;
}

std::int8_t Controller_JoystickX(const void* object) {
    return bitsToI8(field<std::uint8_t>(object, control_off::JoystickAxisX));
}

std::int8_t Controller_JoystickY(const void* object) {
    return bitsToI8(field<std::uint8_t>(object, control_off::JoystickAxisY));
}

bool Controller_JoyButton(const void* object, unsigned button) {
    assert(button < 4);
    if (button >= 4)
        return false;
    return field<std::uint8_t>(object,
        control_off::JoystickButton0 + static_cast<std::size_t>(button)) != 0;
}

void Controller_EndFrame(void* object) {
    for (int code = 1; code <= control_off::MaxCode; ++code)
        codeState(object, code) = 0;

    field<std::int16_t>(object, control_off::LastCode) = 0;
    for (int action = 0; action < control_off::ActionCount; ++action)
        field<std::uint8_t>(object,
            control_off::ActionState + static_cast<std::size_t>(action)) = 0;

    field<std::uint8_t>(object, control_off::MousePressed0) = 0;
    field<std::uint8_t>(object, control_off::MousePressed1) = 0;
    field<std::uint8_t>(object, control_off::JoystickButton0) = 0;
    field<std::uint8_t>(object, control_off::JoystickButton1) = 0;
    field<std::uint8_t>(object, control_off::JoystickButton2) = 0;
    field<std::uint8_t>(object, control_off::JoystickButton3) = 0;
}

}
