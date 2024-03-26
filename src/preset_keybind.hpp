#pragma once

#include <filesystem>

struct preset_keybind
{
	std::filesystem::path preset;
	unsigned int keybind[4];
	bool keybind_opened;
	bool remove;
};
