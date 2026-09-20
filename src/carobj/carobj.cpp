#include <cassert>
#include <cmath>
#include <cstdint>
#include "types.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include "car_object.hpp"
#include "car_color.hpp"
#include "car_specs.hpp"
#include <string>

namespace flydemo {
using namespace flydemo;
using flydemo::mem::field;
using flydemo::mem::pointer32;
using flydemo::mem::store_pointer32;

extern CD3DVECTOR TransformVector(const void* matrix, const CD3DVECTOR& local);
extern void DynamicObject_Update(void* object, float dt);
extern bool Wheel_HasGroundContact(void* wheel);
extern const CD3DVECTOR* Object_GetForward(void* object);
extern float Car_GetStageSpeed(void* car, int stage);
extern float Car_GetDriveCoefficient(void* car, int stage);
extern float Car_ReverseBrake(void* car);
extern CD3DVECTOR Car_GetWheelMount(const void* car, int wheel);
extern void Wheel_SetRollRate(void* wheel, float rate);
extern void Wheel_SetSteeringAngle(void* wheel, float angle);
extern void Object_SetPosition(void* object, const CD3DVECTOR& position);
extern void Dynamic_CopyOrientation(void* object,
                                    const void* transform);

static CD3DVECTOR ConstructorWheelMount(const CarSpecs& specs, unsigned wheel) {
    CD3DVECTOR mount = wheel < 2 ? specs.upperWheelMount : specs.lowerWheelMount;

    if (wheel == 1 || wheel == 2)
        mount.x = -mount.x;
    return mount;
}

static std::string ChildName(std::string_view carName, std::string_view suffix) {
    std::string name(carName);
    name.append(suffix.data(), suffix.size());
    return name;
}

void Car_Construct(void* rawCar, std::string_view objectName,
                               std::string_view carFile,
                               const CarCreateParams& options,
                               CarCreateIO& backend) {
    auto* self = static_cast<CD3DCAROBJECT*>(rawCar);

    backend.ConstructBaseCar(rawCar, objectName, carFile);

    CarSpecs specs;
    backend.LoadSpecs(rawCar, carFile, specs);
    ApplyCarSpecsToRaw(rawCar, specs);

    if (options.spoiler != 0 && options.spoiler != 1)
        backend.InvalidSpoilerSelector(options.spoiler);
    if (options.minigun != 0 && options.minigun != 1)
        backend.InvalidMinigunSelector(options.minigun);

    field<float>(self, off::CONDITION) = 100.0f;
    field<std::uint32_t>(self, off::RESERVED_1128) = 0;
    field<std::int8_t>(self, off::CURRENT_GEAR) = 0;
    field<float>(self, off::SPEED) = 0.0f;
    field<std::uint8_t>(self, off::FORWARD_ACTIVE) = 0;
    field<std::uint8_t>(self, off::STEERING_ACTIVE) = 0;
    field<std::uint8_t>(self, off::REVERSE_ACTIVE) = 0;
    field<std::uint8_t>(self, off::BRAKE_ACTIVE) = 0;
    const std::uint8_t missileMode = static_cast<std::uint8_t>(options.missileMode);
    field<std::uint8_t>(self, off::MISSILE_MODE) = missileMode;

    field<std::uint8_t>(self, off::MISSILE_COUNT) =
        missileMode != 0 ? field<std::uint8_t>(self, off::MISSILE_CAPACITY) : 0;
    store_pointer32(self, off::SPOILER_PTR, nullptr);
    store_pointer32(self, off::MINIGUN_PTR, nullptr);
    store_pointer32(self, off::AFTERBURNER_PTR, nullptr);

    const CD3DVECTOR carOrigin = field<CD3DVECTOR>(self, off::OBJECT_BOUNDS_CENTER);
    for (unsigned wheel = 0; wheel < 4; ++wheel) {
        const unsigned slot = off::WHEEL_PTR_BASE + wheel * off::WHEEL_SLOT_STRIDE;
        std::string wheelName = ChildName(objectName, "_wheel");
        wheelName.push_back(static_cast<char>('1' + wheel));
        const bool mirrored = wheel == 0 || wheel == 2;
        void* wheelObject = backend.CreateWheel(CarObjectSizes::Wheel,
                                                wheelName,
                                                specs.resourceStrings[1],
                                                rawCar,
                                                mirrored);
        store_pointer32(self, slot, wheelObject);
        if (wheelObject != nullptr) {
            backend.SetObjectPosition(wheelObject, carOrigin);
            backend.TranslateObject(wheelObject, ConstructorWheelMount(specs, wheel));
        }
        field<float>(self, slot + off::WHEEL_STEER_IN_SLOT) = 0.0f;
        field<float>(self, slot + off::WHEEL_SPEED_IN_SLOT) = 0.0f;
        field<float>(self, slot + off::WHEEL_OFFSET_IN_SLOT) = specs.wheelOffset;
    }

    if (options.spoiler == 1) {
        void* spoiler = backend.CreateSpoiler(CarObjectSizes::Spoiler,
                                              ChildName(objectName, "_spoiler"),
                                              specs.resourceStrings[2]);
        store_pointer32(self, off::SPOILER_PTR, spoiler);
        if (spoiler != nullptr) {
            backend.SetObjectPosition(spoiler, carOrigin);
            backend.TranslateObject(spoiler, specs.spoilerMount);
        }
    }

    if (options.minigun == 1) {
        const int side = !(specs.minigunMount.x < 0.0f) ? 1 : 0;
        void* minigun = backend.CreateMinigun(CarObjectSizes::Minigun,
                                              ChildName(objectName, "_minigun"),
                                              rawCar, side);
        store_pointer32(self, off::MINIGUN_PTR, minigun);
        if (minigun != nullptr) {
            backend.SetObjectPosition(minigun, carOrigin);
            backend.TranslateObject(minigun, specs.minigunMount);
        }
    }

    const std::uint8_t afterburnerByte =
        static_cast<std::uint8_t>(options.afterburnerVariant);
    if (afterburnerByte != 0xffu) {
        void* afterburner = backend.CreateAfterburner(
            CarObjectSizes::Afterburner,
            ChildName(objectName, "_afterburner"),
            static_cast<std::int8_t>(afterburnerByte));
        store_pointer32(self, off::AFTERBURNER_PTR, afterburner);
    }

    field<std::uint8_t>(self, off::LIGHTS_ACTIVE) = 0;
    const int totalLights = static_cast<int>(specs.lightGroup0Count) +
                            static_cast<int>(specs.lightGroup1Count);
    CarLightState* states = nullptr;
    if (totalLights >= 0)
        states = backend.AllocateLightStates(static_cast<std::size_t>(totalLights));
    store_pointer32(self, off::LIGHT_STATE_TABLE, states);
    for (int i = 0; i < totalLights; ++i) {
        states[i].baseColor = backend.MaterialBaseColor(rawCar,
                                                        static_cast<std::size_t>(i));
        states[i].intensity = 0.0f;
    }

    if (specs.canChangeColors)
        self->SetColor(options.color);
}

void Car_Destruct(void* rawCar, CarDestroyIO& backend) {

    if (void* colorArray = pointer32(rawCar, off::COLOR_TEXTURE_ARRAY)) {
        backend.DestroyColorTextureArray(colorArray);
        store_pointer32(rawCar, off::COLOR_TEXTURE_ARRAY, nullptr);
    }
    if (void* lights = pointer32(rawCar, off::LIGHT_STATE_TABLE)) {
        backend.DestroyLightStateTable(lights);
        store_pointer32(rawCar, off::LIGHT_STATE_TABLE, nullptr);
    }

    for (int offset = static_cast<int>(off::SPECS_STRING_5);
         offset >= static_cast<int>(off::CAR_FILE_STRING); offset -= 0x10)
        backend.DestroyEmbeddedString(rawCar, static_cast<unsigned>(offset));

    backend.DestroyBaseCar(rawCar);
}

namespace {
CarColorIO* gCarColorBackend = nullptr;
}

void Car_SetColorIO(CarColorIO* backend) {
    gCarColorBackend = backend;
}

CarColorIO* Car_GetColorIO() {
    return gCarColorBackend;
}

void CD3DCAROBJECT::SetColor(std::uint32_t color) {

    field<std::uint32_t>(this, off::CURRENT_COLOR) = 0x00ffffffu;
    if (field<std::uint8_t>(this, off::CAN_CHANGE_COLORS) == 0)
        return;
    field<std::uint32_t>(this, off::CURRENT_COLOR) = color;

    CarColorIO* backend = Car_GetColorIO();
    if (backend == nullptr)
        return;

    const std::string carName(backend->CarName(this));
    const std::uint8_t count = field<std::uint8_t>(this, off::COLOR_TEXTURE_COUNT);
    for (std::uint8_t i = 0; i < count; ++i) {
        const std::string source(backend->ColorTextureName(this, i));

        std::string alias = carName;
        alias += '_';
        alias += source;

        backend->LoadTextureAs(source, alias);

        backend->ReplaceObjectTexture(this, source, alias);

        if (void* spoiler = pointer32(this, off::SPOILER_PTR))
            backend->ReplaceObjectTexture(spoiler, source, alias);

        backend->RecolorTexture(alias, color);
    }
}

static unsigned WheelSlotOffset(unsigned wheel) {
    assert(wheel < 4);
    return off::WHEEL_PTR_BASE + wheel * off::WHEEL_SLOT_STRIDE;
}

static void Scale(CD3DVECTOR& value, float scalar) {
    value.x *= scalar;
    value.y *= scalar;
    value.z *= scalar;
}

static void Add(CD3DVECTOR& value, const CD3DVECTOR& rhs) {
    value.x += rhs.x;
    value.y += rhs.y;
    value.z += rhs.z;
}

static float Dot(const CD3DVECTOR& lhs, const CD3DVECTOR& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

static CD3DVECTOR VelocityOrForward(CD3DCAROBJECT* self) {
    CD3DVECTOR direction = field<CD3DVECTOR>(self, off::LINEAR_VELOCITY);

    if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f)
        direction = field<CD3DVECTOR>(self, off::OBJECT_FORWARD);

    const float length = std::sqrt(Dot(direction, direction));
    Scale(direction, 1.0f / length);
    return direction;
}

static void UpdateWheelOffsets(CD3DCAROBJECT* self) {

    for (unsigned wheel = 0; wheel < 4; ++wheel) {
        const unsigned slot = WheelSlotOffset(wheel);
        CD3DVECTOR scratch = field<CD3DVECTOR>(self, off::OBJECT_UP);
        Scale(scratch, -field<float>(self, slot + off::WHEEL_OFFSET_IN_SLOT));
        Scale(scratch, 1000.0f);
        (void)scratch;
    }
}

static void ResetFrameActions(CD3DCAROBJECT* self) {
    field<std::uint8_t>(self, off::FORWARD_ACTIVE) = 0;
    field<std::uint8_t>(self, off::STEERING_ACTIVE) = 0;
    field<std::uint8_t>(self, off::REVERSE_ACTIVE) = 0;
    field<std::uint8_t>(self, off::BRAKE_ACTIVE) = 0;
}

void CD3DCAROBJECT::Update(float dt) {

    UpdateWheelOffsets(this);
    Car_ApplyTyrePhysics(this);

    const bool ordinaryPath =
        field<std::uint8_t>(this, off::OBJECT_STATE_0090) == 0 ||
        field<std::uint8_t>(this, off::PHYS_ACTIVE) == 1;

    DynamicObject_Update(this, dt);

    if (!ordinaryPath) {
        UpdateWheels(dt);
        UpdateSpoilerMount();
        UpdateMinigunMount();
        UpdateLightMaterials(dt);
    }

    if (field<std::uint8_t>(this, off::PHYS_ACTIVE) != 0) {
        UpdateWheelSpeeds(dt);
        UpdateGear();
        UpdateWheels(dt);
        UpdateSpoilerMount();
        UpdateMinigunMount();
        UpdateLightMaterials(dt);

        if (field<float>(this, off::CONDITION) == 0.0f)
            field<std::uint8_t>(this, off::LIGHTS_ACTIVE) = 0;
        ResetFrameActions(this);
        return;
    }

    field<std::int8_t>(this, off::CURRENT_GEAR) = 0;
    field<std::uint8_t>(this, off::LIGHTS_ACTIVE) = 0;
    ResetFrameActions(this);
    field<float>(this, off::SPEED) = 0.0f;
}

static bool WheelsOnGround(CD3DCAROBJECT* self,
                                      unsigned first,
                                      unsigned second) {
    return Wheel_HasGroundContact(pointer32(self, WheelSlotOffset(first))) ||
           Wheel_HasGroundContact(pointer32(self, WheelSlotOffset(second)));
}

static void MeasureWheelPairSpeed(CD3DCAROBJECT* self,
                                  unsigned first,
                                  unsigned second,
                                  const CD3DVECTOR& normalizedVelocity) {
    const CD3DVECTOR velocity = field<CD3DVECTOR>(self, off::LINEAR_VELOCITY);
    const float horizontalMagnitude =
        std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);

    void* referenceWheel = pointer32(self, WheelSlotOffset(first));
    const float signedSpeed =
        horizontalMagnitude * Dot(normalizedVelocity, *Object_GetForward(referenceWheel));

    field<float>(self, WheelSlotOffset(first) + off::WHEEL_SPEED_IN_SLOT) = signedSpeed;
    field<float>(self, WheelSlotOffset(second) + off::WHEEL_SPEED_IN_SLOT) = signedSpeed;
}

void CD3DCAROBJECT::UpdateWheelSpeeds(float dt) {

    const CD3DVECTOR direction = VelocityOrForward(this);
    const unsigned pair0Left = WheelSlotOffset(0) + off::WHEEL_SPEED_IN_SLOT;
    const unsigned pair0Right = WheelSlotOffset(1) + off::WHEEL_SPEED_IN_SLOT;
    const unsigned pair1Left = WheelSlotOffset(2) + off::WHEEL_SPEED_IN_SLOT;
    const unsigned pair1Right = WheelSlotOffset(3) + off::WHEEL_SPEED_IN_SLOT;

    if (WheelsOnGround(this, 0, 1)) {
        MeasureWheelPairSpeed(this, 0, 1, direction);
    } else if (field<std::uint8_t>(this, off::FORWARD_ACTIVE) == 1) {
        const int topGear = static_cast<std::int8_t>(
            field<std::uint8_t>(this, off::NUM_GEARS));
        float& left = field<float>(this, pair0Left);
        if (Car_GetStageSpeed(this, topGear) > left) {
            const int gear = field<std::int8_t>(this, off::CURRENT_GEAR);
            left += Car_GetDriveCoefficient(this, gear) * dt * 2.0f;
            field<float>(this, pair0Right) = left;
        }
    } else if (field<std::uint8_t>(this, off::REVERSE_ACTIVE) == 1) {
        float& left = field<float>(this, pair0Left);
        if (Car_GetStageSpeed(this, -1) < left) {
            left += Car_ReverseBrake(this) * dt * 2.0f;
            field<float>(this, pair0Right) = left;
        }
    } else {

        const float decay = 1.0f / (1.0f + dt * 2.0f);
        field<float>(this, pair0Left) *= decay;
        field<float>(this, pair0Right) *= decay;
    }

    if (WheelsOnGround(this, 2, 3)) {
        MeasureWheelPairSpeed(this, 2, 3, direction);
    } else {
        const float decay = 1.0f / (1.0f + dt * 2.0f);
        field<float>(this, pair1Left) *= decay;
        field<float>(this, pair1Right) *= decay;
    }

    if (field<std::uint8_t>(this, off::BRAKE_ACTIVE) == 1) {
        field<float>(this, pair1Left) = 0.0f;
        field<float>(this, pair1Right) = 0.0f;
    }

    float& speed = field<float>(this, off::SPEED);
    speed = field<float>(this, pair0Left);
    if (speed > -0.1 && speed < 0.1 &&
        field<std::uint8_t>(this, off::FORWARD_ACTIVE) == 0 &&
        field<std::uint8_t>(this, off::REVERSE_ACTIVE) == 0) {
        speed = 0.0f;
    }
}

void CD3DCAROBJECT::UpdateGear() {

    const float speed = field<float>(this, off::SPEED);
    std::int8_t& gear = field<std::int8_t>(this, off::CURRENT_GEAR);

    if (speed == 0.0f) {
        gear = 0;
        return;
    }
    if (speed < 0.0f) {
        gear = -1;
        return;
    }

    const int numGears = field<std::int8_t>(this, off::NUM_GEARS);
    gear = 1;
    while (gear <= numGears &&
           Car_GetStageSpeed(this, gear) <= speed) {
        ++gear;
    }
    if (gear > numGears)
        gear = static_cast<std::int8_t>(numGears);
}

void CD3DCAROBJECT::UpdateWheels(float dt) {

    if (field<std::uint8_t>(this, off::STEERING_ACTIVE) == 0) {
        const float decay = 1.0f / (1.0f + dt * 10.0f);
        field<float>(this, off::STEER_LEFT) *= decay;
        field<float>(this, off::STEER_RIGHT) *= decay;
    }

    const void* carTransform =
        reinterpret_cast<const std::uint8_t*>(this) + off::PHYS_MATRIX;
    const CD3DVECTOR carOrigin = field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER);
    const CD3DVECTOR carUp = field<CD3DVECTOR>(this, off::OBJECT_UP);

    for (unsigned wheelIndex = 0; wheelIndex < 4; ++wheelIndex) {
        const unsigned slot = WheelSlotOffset(wheelIndex);
        void* wheel = pointer32(this, slot);

        CD3DVECTOR worldPosition =
            TransformVector(carTransform, Car_GetWheelMount(this, wheelIndex));
        Add(worldPosition, carOrigin);

        CD3DVECTOR verticalOffset = carUp;
        Scale(verticalOffset, -field<float>(this, slot + off::WHEEL_OFFSET_IN_SLOT));
        Add(worldPosition, verticalOffset);

        Wheel_SetSteeringAngle(wheel,
            field<float>(this, slot + off::WHEEL_STEER_IN_SLOT));

        const float diameter = field<float>(wheel, off::OBJECT_SIZE_Y);
        const double rollRate =
            static_cast<double>(field<float>(this, slot + off::WHEEL_SPEED_IN_SLOT)) /
            static_cast<double>(diameter * 0.5f) *
            0.15915495087284554 * 256.0;
        Wheel_SetRollRate(wheel, static_cast<float>(rollRate));

        Object_SetPosition(wheel, worldPosition);
        Dynamic_CopyOrientation(wheel, carTransform);
    }
}

static void UpdateAttachedObject(CD3DCAROBJECT* self,
                                 unsigned pointerOffset,
                                 unsigned mountOffset) {
    void* child = pointer32(self, pointerOffset);
    if (!child)
        return;

    const void* carTransform =
        reinterpret_cast<const std::uint8_t*>(self) + off::PHYS_MATRIX;
    CD3DVECTOR worldPosition =
        TransformVector(carTransform, field<CD3DVECTOR>(self, mountOffset));
    Add(worldPosition, field<CD3DVECTOR>(self, off::OBJECT_BOUNDS_CENTER));
    Object_SetPosition(child, worldPosition);
    Dynamic_CopyOrientation(child, carTransform);
}

void CD3DCAROBJECT::UpdateSpoilerMount() {
    UpdateAttachedObject(this, off::SPOILER_PTR, off::SPOILER_MOUNT);
}

void CD3DCAROBJECT::UpdateMinigunMount() {
    UpdateAttachedObject(this, off::MINIGUN_PTR, off::MINIGUN_MOUNT);
}

void CD3DCAROBJECT::UpdateLightMaterials(float dt) {

    CarLightState* states =
        pointer32<CarLightState>(this, off::LIGHT_STATE_TABLE);
    const auto group0Count = field<std::int16_t>(this, off::LIGHT_GROUP0_COUNT);
    const auto group1Count = field<std::int16_t>(this, off::LIGHT_GROUP1_COUNT);
    const int totalCount = static_cast<int>(group0Count) + static_cast<int>(group1Count);

    Car_UpdateLightStates(
        states, group0Count, group1Count,
        field<std::uint8_t>(this, off::LIGHTS_ACTIVE) == 1,
        field<std::uint8_t>(this, off::REVERSE_ACTIVE) == 1,
        field<float>(this, off::SPEED), dt);

    void* materials = pointer32(this, off::OBJECT_MATERIAL_ARRAY);
    for (int i = 0; i < totalCount; ++i) {
        constexpr unsigned kMaterialStride = 0x38;
        constexpr unsigned kMaterialRgb = 0x24;
        field<std::uint32_t>(materials,
            static_cast<unsigned>(i) * kMaterialStride + kMaterialRgb) =
                Car_ScaleLight(states[i].baseColor, states[i].intensity);
    }
}

void* CD3DCAROBJECT::GetWheel(int wheel) {
    assert(wheel >= 0 && wheel < 4);
    return pointer32(this, off::WHEEL_PTR_BASE
                              + static_cast<unsigned>(wheel) * off::WHEEL_SLOT_STRIDE);
}

CD3DVECTOR CD3DCAROBJECT::GetCameraPosition(CarCamera cam) const {
    assert(cam == COCKPIT || cam == REAR || cam == CUSTOM1 || cam == CUSTOM2);

    const CD3DVECTOR local = field<CD3DVECTOR>(this, off::CAR_CAMERA_BASE
                                        + static_cast<unsigned>(cam) * 12u);
    CD3DVECTOR world = TransformVector(reinterpret_cast<const std::uint8_t*>(this)
                                     + off::PHYS_MATRIX,
                                 local);
    const CD3DVECTOR origin = field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER);
    world.x += origin.x;
    world.y += origin.y;
    world.z += origin.z;
    return world;
}

std::uint32_t CD3DCAROBJECT::GetColor() const {
    return field<std::uint32_t>(this, off::CURRENT_COLOR);
}

std::uint8_t CD3DCAROBJECT::GetMissileMode() const {
    return field<std::uint8_t>(this, off::MISSILE_MODE);
}

float CD3DCAROBJECT::GetCondition() const {
    return field<float>(this, off::CONDITION);
}

std::uint8_t CD3DCAROBJECT::GetMissileCount() const {
    return field<std::uint8_t>(this, off::MISSILE_COUNT);
}

void CD3DCAROBJECT::SetMissileCount(std::uint8_t count) {
    field<std::uint8_t>(this, off::MISSILE_COUNT) = count;
}

bool CD3DCAROBJECT::GetLightsState() const {
    return field<std::uint8_t>(this, off::LIGHTS_ACTIVE) != 0;
}

void* CD3DCAROBJECT::GetSpoiler() const {
    return pointer32(this, off::SPOILER_PTR);
}

void* CD3DCAROBJECT::GetMinigun() const {
    return pointer32(this, off::MINIGUN_PTR);
}

void* CD3DCAROBJECT::GetAfterburner() const {
    return pointer32(this, off::AFTERBURNER_PTR);
}

}
