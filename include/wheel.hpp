#pragma once
namespace flydemo {

void Wheel_Update(void* wheel, float dt);
bool Wheel_HasGroundContact(void* wheel);
void Wheel_SetRollRate(void* wheel, float rate);
void Wheel_SetSteeringAngle(void* wheel, float angle);
}
