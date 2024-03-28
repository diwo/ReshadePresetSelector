#include "preset_keybind.hpp"
#include "key_input_box.hpp"
#include "ini_file.hpp"
#include "screenshot.hpp"

#include <imgui.h>
#include <reshade.hpp>
#include <imfilebrowser.h>
#include <fpng.h>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <optional>

#define PRESET_DISPLAY_MAX_LENGTH 30

static std::vector<preset_keybind> g_preset_keybinds;
static std::filesystem::path g_config_path;
static ImGui::FileBrowser g_file_browser;
static std::optional<int> g_browse_idx;
static bool g_is_key_input_box_active;
static uint32_t g_frame;

static std::map<keybind_action, const char*> KEYBIND_ACTION_LABELS = {
	{ keybind_action::change_preset, "Change preset" },
	{ keybind_action::take_screenshot, "Take Screenshot" },
};

static std::filesystem::path get_current_preset_path(reshade::api::effect_runtime *runtime)
{
	size_t path_size;
	runtime->get_current_preset_path(nullptr, &path_size);
	char* current_preset = new char[path_size];
	runtime->get_current_preset_path(current_preset, &path_size);
	return std::filesystem::path(current_preset);
}

static void draw_overlay(reshade::api::effect_runtime *runtime)
{
	bool updated = false;
	g_is_key_input_box_active = false;

	if (ImGui::CollapsingHeader("Preset Keybindings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (uint8_t i = 0; i < g_preset_keybinds.size(); ++i)
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
					preset_path = std::filesystem::path(get_current_preset_path(runtime));
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
					ImGui::Text("%s | %s",
						reshade_key_name(g_preset_keybinds[i].keybind).c_str(),
						KEYBIND_ACTION_LABELS[g_preset_keybinds[i].action]);
					ImGui::SameLine();
					button_text = "Edit";
				} else {
					button_text = "Add Keybind";
				}
				if (ImGui::Button(button_text))
				{
					g_preset_keybinds[i].keybind_opened = !g_preset_keybinds[i].keybind_opened;
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
				ImGui::Text("%s", "Key Shortcut");
				ImGui::SameLine();
				bool active;
				if (reshade_key_input_box("", g_preset_keybinds[i].keybind, runtime, active))
				{
					updated = true;
				}

				ImGui::Text("%s", "Action");
				ImGui::SameLine();
				if (ImGui::BeginCombo("##action", KEYBIND_ACTION_LABELS[g_preset_keybinds[i].action]))
				{
					for (auto action : keybind_actions)
					{
						if (ImGui::Selectable(KEYBIND_ACTION_LABELS[action], g_preset_keybinds[i].action == action))
						{
							g_preset_keybinds[i].action = action;
							updated = true;
						}
					}
					ImGui::EndCombo();
				}
				g_is_key_input_box_active |= active;
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		if (g_preset_keybinds.empty() || !g_preset_keybinds.back().preset.empty())
		{
			if (ImGui::Button("+ Add"))
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

static void handle_keypress(reshade::api::effect_runtime *runtime)
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
			if (pkb.action == change_preset)
			{
				runtime->set_current_preset_path(pkb.preset.string().c_str());
			}
			else if (pkb.action == take_screenshot)
			{
				std::filesystem::path original_preset = get_current_preset_path(runtime);
				queue_screenshot_workload(pkb.preset, original_preset);
			}
		}
	}
}

static void on_reshade_overlay(reshade::api::effect_runtime *runtime)
{
	++g_frame;
	screenshot_notify_frame(g_frame);

	if (process_screenshot_workload(runtime)) return;

	handle_keypress(runtime);
}

static void on_reshade_begin_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb)
{
	screenshot_notify_effects_rendered(g_frame);
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

		fpng::fpng_init();

		g_config_path = get_module_path(hModule).replace_extension(".ini");
		load_preset_keybinds(g_config_path, g_preset_keybinds);

		g_file_browser.SetTitle("Select preset");
		g_file_browser.SetTypeFilters({ ".ini", ".txt" });

		reshade::register_overlay(nullptr, &draw_overlay);
		reshade::register_event<reshade::addon_event::reshade_overlay>(&on_reshade_overlay);
		reshade::register_event<reshade::addon_event::reshade_begin_effects>(&on_reshade_begin_effects);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_overlay(nullptr, &draw_overlay);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}