#pragma once

#include "preset_keybind.hpp"

bool save_preset_keybinds(std::filesystem::path& config_path, std::vector<preset_keybind>& preset_keybinds);
bool load_preset_keybinds(std::filesystem::path& config_path, std::vector<preset_keybind>& out);