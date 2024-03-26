#include "key_input_box.hpp"

#include <imgui.h>
#include <reshade.hpp>
#include <string>

// https://github.com/crosire/reshade/blob/v6.0.0/source/imgui_widgets.cpp#L240
bool reshade_key_input_box(const char *name, unsigned int key[4], reshade::api::effect_runtime *runtime, bool& is_input_active)
{
	bool res = false;

	char buf[48]; buf[0] = '\0';
	if (key[0] || key[1] || key[2] || key[3])
		buf[reshade_key_name(key).copy(buf, sizeof(buf) - 1)] = '\0';

	// ImGui::BeginDisabled(ImGui::GetCurrentContext()->NavInputSource == ImGuiInputSource_Gamepad);

	ImGui::InputTextWithHint(name, "Click to set keyboard shortcut", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

	is_input_active = ImGui::IsItemActive();

	if (is_input_active)
	{
		const unsigned int last_key_pressed = runtime->last_key_pressed();
		if (last_key_pressed != 0)
		{
			unsigned int new_key[4];
			bool is_set = false;

			if (last_key_pressed == 0x08) // Backspace
			{
				new_key[0] = 0;
				new_key[1] = 0;
				new_key[2] = 0;
				new_key[3] = 0;
				is_set = true;
			}
			else if (last_key_pressed < 0x10 || last_key_pressed > 0x12) // Exclude modifier keys
			{
				new_key[0] = last_key_pressed;
				new_key[1] = runtime->is_key_down(0x11); // Ctrl
				new_key[2] = runtime->is_key_down(0x10); // Shift
				new_key[3] = runtime->is_key_down(0x12); // Alt
				is_set = true;
			}

			if (is_set && memcmp(key, new_key, sizeof(new_key)) != 0)
			{
				std::memcpy(key, new_key, sizeof(new_key));
				res = true;
			}
		}
	}
	else if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
	{
		ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key or press backspace to remove the shortcut.");
	}

	// ImGui::EndDisabled();

	return res;
}

// https://github.com/crosire/reshade/blob/v6.0.0/source/input.cpp#L417
static std::string reshade_key_name(unsigned int keycode)
{
	if (keycode >= 256)
		return std::string();

	if (keycode == VK_HOME &&
		LOBYTE(GetKeyboardLayout(0)) == LANG_GERMAN)
		return "Pos1";

	static const char *keyboard_keys[256] = {
		"", "Left Mouse", "Right Mouse", "Cancel", "Middle Mouse", "X1 Mouse", "X2 Mouse", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
		"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
		"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
		"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
		"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "Apps", "", "Sleep",
		"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
		"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
		"Num Lock", "Scroll Lock", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"Left Shift", "Right Shift", "Left Control", "Right Control", "Left Menu", "Right Menu", "Browser Back", "Browser Forward", "Browser Refresh", "Browser Stop", "Browser Search", "Browser Favorites", "Browser Home", "Volume Mute", "Volume Down", "Volume Up",
		"Next Track", "Previous Track", "Media Stop", "Media Play/Pause", "Mail", "Media Select", "Launch App 1", "Launch App 2", "", "", "OEM ;", "OEM +", "OEM ,", "OEM -", "OEM .", "OEM /",
		"OEM ~", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "", "", "", "", "", "OEM [", "OEM \\", "OEM ]", "OEM '", "OEM 8",
		"", "", "OEM <", "", "", "", "", "", "", "", "", "", "", "", "", "",
		"", "", "", "", "", "", "Attn", "CrSel", "ExSel", "Erase EOF", "Play", "Zoom", "", "PA1", "OEM Clear", ""
	};

	return keyboard_keys[keycode];
}
std::string reshade_key_name(const unsigned int key[4])
{
	assert(key[0] != VK_CONTROL && key[0] != VK_SHIFT && key[0] != VK_MENU);

	return (key[1] ? "Ctrl + " : std::string()) + (key[2] ? "Shift + " : std::string()) + (key[3] ? "Alt + " : std::string()) + reshade_key_name(key[0]);
}
