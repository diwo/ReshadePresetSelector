#pragma once

#include <imgui.h>
#include <reshade.hpp>
#include <string>

bool reshade_key_input_box(const char *name, unsigned int key[4], reshade::api::effect_runtime *runtime, bool& is_input_active);
std::string reshade_key_name(const unsigned int key[4]);
static std::string reshade_key_name(unsigned int keycode);