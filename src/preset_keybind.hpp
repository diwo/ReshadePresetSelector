#pragma once

#include <filesystem>

enum keybind_action {
	change_preset = 0,
	take_screenshot = 1
};

const keybind_action keybind_actions[] = {
	change_preset,
	take_screenshot
};

struct preset_keybind
{
	std::filesystem::path preset;
	unsigned int keybind[4];
	keybind_action action;
	bool keybind_opened;
	bool remove;
};
