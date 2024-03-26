#include "preset_keybind.hpp"
#include "key_input_box.hpp"
#include "ini_file.hpp"

#include <imgui.h>
#include <reshade.hpp>
#include <imfilebrowser.h>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <optional>

#define PRESET_DISPLAY_MAX_LENGTH 40

static std::vector<preset_keybind> g_preset_keybinds;
static std::filesystem::path g_config_path;
static ImGui::FileBrowser g_file_browser;
static std::optional<int> g_browse_idx;
static bool g_is_key_input_box_active;

static void draw_overlay(reshade::api::effect_runtime *runtime)
{
	bool updated = false;
	g_is_key_input_box_active = false;

	if (ImGui::CollapsingHeader("Preset Keybindings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int i = 0; i < g_preset_keybinds.size(); ++i)
		{
			ImGui::PushID(i);

			const char* preset_display;
			if (g_preset_keybinds[i].preset.empty())
			{
				preset_display = "None";
			} else {
				std::string str = g_preset_keybinds[i].preset.string();
				if (str.length()  > PRESET_DISPLAY_MAX_LENGTH)
				{
					std::stringstream ss;
					ss << "..." << str.substr(str.length()-PRESET_DISPLAY_MAX_LENGTH, PRESET_DISPLAY_MAX_LENGTH);
					str = ss.str();
				}
				preset_display = str.c_str();
			}
			ImGui::Text("%s", preset_display);
			ImGui::SameLine();

			if (ImGui::Button("Browse..."))
			{
				std::filesystem::path preset_path;
				if (g_preset_keybinds[i].preset.empty())
				{
					size_t path_size;
					runtime->get_current_preset_path(nullptr, &path_size);
					char* current_preset = new char[path_size];
					runtime->get_current_preset_path(current_preset, &path_size);
					preset_path = std::filesystem::path(current_preset);
				} else {
					preset_path = g_preset_keybinds[i].preset;
				}
				g_file_browser.SetPwd(preset_path.parent_path());
				g_file_browser.Open();
				g_browse_idx.emplace(i);
			}
			ImGui::SameLine();

			if (!g_preset_keybinds[i].preset.empty())
			{
				const char* button_text;
				if (g_preset_keybinds[i].keybind[0] != 0)
				{
					ImGui::Text("%s", reshade_key_name(g_preset_keybinds[i].keybind).c_str());
					ImGui::SameLine();
					button_text = "Change";
				} else {
					button_text = "Keybind";
				}
				if (ImGui::Button(button_text))
				{
					g_preset_keybinds[i].keybind_opened = true;
				}
				ImGui::SameLine();
			}

			if (ImGui::Button("Remove"))
			{
				g_preset_keybinds[i].remove = true;
				updated = true;
			}

			if (g_preset_keybinds[i].keybind_opened)
			{
				ImGui::Separator();
				ImGui::Text("%s", "Key Shortcut");
				ImGui::SameLine();
				bool active;
				if (reshade_key_input_box("", g_preset_keybinds[i].keybind, runtime, active))
				{
					updated = true;
				}
				g_is_key_input_box_active |= active;
				if (ImGui::Button("Done"))
				{
					g_preset_keybinds[i].keybind_opened = false;
				}
				ImGui::Separator();
			}

			ImGui::PopID();
		}

		if (g_preset_keybinds.empty() || !g_preset_keybinds.back().preset.empty())
		{
			ImGui::Separator();
			if (ImGui::Button("Add"))
			{
				preset_keybind pkb = {};
				g_preset_keybinds.push_back(std::move(pkb));
			}
		}

		g_file_browser.Display();
		if (g_file_browser.HasSelected() && g_browse_idx.has_value())
		{
			g_preset_keybinds[g_browse_idx.value()].preset = g_file_browser.GetSelected();
			g_file_browser.ClearSelected();
			g_browse_idx = {};
			updated = true;
		}

		g_preset_keybinds.erase(
			std::remove_if(
				g_preset_keybinds.begin(),
				g_preset_keybinds.end(),
				[](preset_keybind const & pkb) { return pkb.remove; }
			),
			g_preset_keybinds.end()
		);
	}

	if (updated)
	{
		bool success = save_preset_keybinds(g_config_path, g_preset_keybinds);
		std::stringstream ss;
		if (success)
		{
			ss << "Config saved to: " << g_config_path.string();
			reshade::log_message(reshade::log_level::info, ss.str().c_str());
		} else {
			ss << "Config failed to save: " << g_config_path.string();
			reshade::log_message(reshade::log_level::error, ss.str().c_str());
		}
	}
}

static void handle_select_preset_keypress(reshade::api::effect_runtime *runtime)
{
	unsigned int ctrl_down = runtime->is_key_down(0x11);
	unsigned int shift_down = runtime->is_key_down(0x10);
	unsigned int alt_down = runtime->is_key_down(0x12);

	for (auto& pkb : g_preset_keybinds)
	{
		if (!g_is_key_input_box_active &&
			pkb.keybind[0] != 0 && runtime->is_key_pressed(pkb.keybind[0]) &&
			(ctrl_down == pkb.keybind[1]) && (shift_down == pkb.keybind[2]) && (alt_down == pkb.keybind[3]))
		{
			runtime->set_current_preset_path(pkb.preset.string().c_str());
		}
	}
}

// https://github.com/crosire/reshade/blob/v6.0.0/source/dll_main.cpp#L115
std::filesystem::path get_module_path(HMODULE module)
{
	WCHAR buf[4096];
	return GetModuleFileNameW(module, buf, ARRAYSIZE(buf)) ? buf : std::filesystem::path();
}

extern "C" __declspec(dllexport) const char *NAME = "Preset Selector";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Bind specific presets to keyboard shortcuts.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;

		g_config_path = get_module_path(hModule).replace_extension(".ini");
		load_preset_keybinds(g_config_path, g_preset_keybinds);

		g_file_browser.SetTitle("Select preset");
		g_file_browser.SetTypeFilters({ ".ini", ".txt" });

		reshade::register_overlay(nullptr, &draw_overlay);
		reshade::register_event<reshade::addon_event::reshade_present>(&handle_select_preset_keypress);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_overlay(nullptr, &draw_overlay);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}