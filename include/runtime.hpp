#pragma once
#include "types.hpp"

namespace flydemo {

void Host_SetFrameDelta(float dt);
float Renderer_GetFrameDelta();
const CD3DVECTOR* Object_GetForward(void* object);
const CD3DVECTOR* Object_GetPosition(void* object);
void Object_SetPosition(void* object, const CD3DVECTOR& position);

}
