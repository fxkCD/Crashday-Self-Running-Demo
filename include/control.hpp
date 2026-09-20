#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace flydemo {

namespace control_off {
constexpr std::size_t HInstance32 = 0x0000;
constexpr std::size_t HWnd32 = 0x0004;
constexpr std::size_t DirectInputObject32 = 0x0008;
constexpr std::size_t KeyboardDevice32 = 0x000C;
constexpr std::size_t MouseDevice32 = 0x0010;
constexpr std::size_t JoystickDevice32 = 0x0014;

constexpr std::size_t MouseMinX = 0x0018;
constexpr std::size_t MouseMaxX = 0x001A;
constexpr std::size_t MouseMinY = 0x001C;
constexpr std::size_t MouseMaxY = 0x001E;
constexpr std::size_t MouseX = 0x0020;
constexpr std::size_t MouseY = 0x0022;
constexpr std::size_t MousePressed0 = 0x0024;
constexpr std::size_t MousePressed1 = 0x0025;
constexpr std::size_t MousePrimarySuppressed = 0x0026;
constexpr std::size_t JoystickAvailable = 0x0028;
constexpr std::size_t JoystickActive = 0x0029;
constexpr std::size_t JoystickAxisX = 0x002A;
constexpr std::size_t JoystickAxisY = 0x002B;
constexpr std::size_t JoystickButton0 = 0x002C;
constexpr std::size_t JoystickButton1 = 0x002D;
constexpr std::size_t JoystickButton2 = 0x002E;
constexpr std::size_t JoystickButton3 = 0x002F;

constexpr std::size_t CodeStateBias = 0x002F;
constexpr int MaxCode = 229;
constexpr int KeyboardPollMaxCode = 221;
constexpr int JoystickButtonCodeBase = 222;
constexpr int JoystickAxisCodeBase = 226;

constexpr std::size_t SuppressBias = 0x0F67;
constexpr std::size_t LastCode = 0x104E;
constexpr std::size_t ActionState = 0x1050;
constexpr int ActionCount = 14;
constexpr std::size_t ActionBinding = 0x105E;
constexpr std::size_t InputNameArray = 0x0118;
constexpr std::size_t EventNameArray = 0x107C;
constexpr std::size_t KeyboardSnapshot = 0x115C;
constexpr std::size_t MouseSnapshot = 0x125C;
constexpr std::size_t JoystickSnapshot = 0x126C;
constexpr std::size_t ObjectStateSpan = 0x12BC;
constexpr std::size_t ConfigSize = 37;
}

namespace control_di {
constexpr std::uint32_t Version = 0x0500;
constexpr std::uint32_t KeyboardCoopFlags = 0x00000006;
constexpr std::uint32_t MouseCooperativeFlags = 0x00000005;
constexpr std::uint32_t JoystickCoopFlags = 0x00000006;
constexpr std::int32_t JoystickAxisMin = -100;
constexpr std::int32_t JoystickAxisMax = 100;
constexpr std::uint32_t JoystickDeadZoneNormal = 2000;
constexpr std::uint32_t JoystickDeadZoneCapture = 8000;
}

enum class ControllerDevice : std::uint8_t { Keyboard, Mouse, Joystick };
enum class ControllerAxis : std::uint8_t { X, Y };

class ControlInput {
public:
    virtual ~ControlInput() = default;

    virtual bool CreateDirectInput(std::uint32_t version) = 0;
    virtual void ReleaseDirectInput() = 0;

    virtual bool CreateDevice(ControllerDevice device) = 0;
    virtual bool SetDataFormat(ControllerDevice device) = 0;
    virtual bool SetCooperativeLevel(ControllerDevice device,
                                     std::uint32_t flags) = 0;
    virtual bool Acquire(ControllerDevice device) = 0;
    virtual void Unacquire(ControllerDevice device) = 0;
    virtual void ReleaseDevice(ControllerDevice device) = 0;

    virtual bool EnumerateJoystick() = 0;
    virtual bool QueryJoystickDevice2() = 0;
    virtual void ReleaseJoystick() = 0;

    virtual bool SetJoystickRange(ControllerAxis axis,
                                  std::int32_t minimum,
                                  std::int32_t maximum) = 0;
    virtual bool SetJoystickDeadZone(ControllerAxis axis,
                                     std::uint32_t value) = 0;
    virtual bool PollJoystick() = 0;

    virtual bool GetKeyboardState(std::uint8_t out[256]) = 0;
    virtual bool GetMouseState(std::uint8_t out[16]) = 0;
    virtual bool GetJoystickState(std::uint8_t out[0x50]) = 0;
};

struct ControllerDeviceState {
    bool DInputObj = false;
    bool DIKeyb = false;
    bool DIMouse = false;
    bool DIJoystick = false;
    bool JoystickDevice = false;
};

bool Controller_StartInput(void* object,
                                 ControllerDeviceState& devices,
                                 ControlInput& backend);
void Controller_Shutdown(void* object,
                                    ControllerDeviceState& devices,
                                    ControlInput& backend);
bool Controller_StartKeyboard(ControllerDeviceState& devices,
                              ControlInput& backend);
bool Controller_StartMouse(void* object,
                           ControllerDeviceState& devices,
                           ControlInput& backend);
bool Controller_StartJoystick(void* object,
                              ControllerDeviceState& devices,
                              ControlInput& backend);

void Controller_PollKeyboard(void* object,
                             ControllerDeviceState& devices,
                             ControlInput& backend);
void Controller_PollMouse(void* object,
                          ControllerDeviceState& devices,
                          ControlInput& backend);
void Controller_PollJoystick(void* object,
                             ControllerDeviceState& devices,
                             ControlInput& backend);
void Controller_Update(void* object,
                       ControllerDeviceState& devices,
                       ControlInput& backend);

void Controller_PollKeyboard(void* object,
                                      const std::uint8_t snapshot[256]);
void Controller_SetMouseState(void* object,
                                   const std::uint8_t snapshot[16]);
void Controller_PollJoystick(void* object,
                                      const std::uint8_t snapshot[0x50]);

bool Controller_LoadConfig(void* object,
                                const std::uint8_t* bytes,
                                std::size_t size);
std::array<std::uint8_t, control_off::ConfigSize>
Controller_SaveConfig(const void* object);

struct ControllerNameTables {
    std::array<std::string, control_off::MaxCode> inputNames{};
    std::array<std::string, control_off::ActionCount> eventNames{};
};
bool Controller_LoadInputs(ControllerNameTables& names,
                               const std::string* entries,
                               std::size_t count);
bool Controller_LoadEvents(ControllerNameTables& names,
                               const std::string* entries,
                               std::size_t count);
std::string_view Controller_GetInputName(const ControllerNameTables& names,
                                         std::int16_t input);
std::string_view Controller_GetEventName(const ControllerNameTables& names,
                                         std::int8_t event);

std::int16_t Controller_GetCode(const void* object);
bool Controller_GetCodeState(const void* object, std::int16_t code);
void Controller_SuppressCode(void* object, std::int16_t code);
bool Controller_ActionState(const void* object, std::int8_t action);
std::int16_t Controller_ActionBinding(const void* object,
                                         std::int8_t action);
void Controller_BlockAction(void* object, std::int8_t action);
std::int8_t Controller_ActionValue(const void* object,
                                      std::int8_t action);

std::int16_t Controller_BindFirst(void* object,
                                           std::int8_t event);

bool Controller_RebindAction(void* object,
                                        std::int8_t event,
                                        ControllerDeviceState& devices,
                                        ControlInput& backend);

bool Controller_MousePrimary(const void* object);
bool Controller_MouseButton2(const void* object);
std::int16_t Controller_GetMouseX(const void* object);
std::int16_t Controller_GetMouseY(const void* object);
void Controller_SetMousePos(void* object,
                                 std::int16_t x,
                                 std::int16_t y);
void Controller_MouseRange(void* object,
                              std::int16_t minX,
                              std::int16_t maxX,
                              std::int16_t minY,
                              std::int16_t maxY);

bool Controller_HasJoystick(const void* object);
bool Controller_JoystickOn(const void* object);
void Controller_SetJoystick(void* object, std::uint8_t use);
std::int8_t Controller_JoystickX(const void* object);
std::int8_t Controller_JoystickY(const void* object);
bool Controller_JoyButton(const void* object, unsigned button);

void Controller_MouseButtons(void* object);
void Controller_UpdateActions(void* object);
void Controller_EndFrame(void* object);

}
