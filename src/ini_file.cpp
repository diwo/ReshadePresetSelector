#define MINI_CASE_SENSITIVE

#include "preset_keybind.hpp"
#include "ini_file.hpp"

#include <reshade.hpp>
#include <ini.h>

static const char* SECTION_PRESET_KEYBINDS = "PresetKeybinds";
static const char* KEY_PREFIX_PRESET = "Preset_";
static const char* KEY_PREFIX_KEYBIND = "Keybind_";
static const char* KEY_PREFIX_ACTION = "Action_";

static std::string to_string(unsigned int keybind[4])
{
    std::stringstream ss;
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
        {
            ss << ",";
        }
        ss << keybind[i];
    }
    return ss.str();
}

static void parse_keybind(std::string& str, unsigned int out[4])
{
    std::stringstream ss(str);
    std::string s;
    int idx = 0;
    while (std::getline(ss, s, ',') && idx < 4)
    {
        out[idx] = std::stoi(s);
        ++idx;
    }
}

static std::string convert_key(std::string key, const char* from_prefix, const char* to_prefix)
{
    std::stringstream ss;
    ss << to_prefix;
    ss << key.substr(strlen(from_prefix), key.size()-strlen(from_prefix));
    return ss.str();
}

bool save_preset_keybinds(std::filesystem::path& config_path, std::vector<preset_keybind>& preset_keybinds)
{
    mINI::INIFile config_file(config_path.string());
    mINI::INIStructure ini;

    int idx = 0;
    for (auto& pkb : preset_keybinds)
    {
        std::stringstream preset_prop, keybind_prop, action_prop;
        preset_prop << KEY_PREFIX_PRESET << idx;
        keybind_prop << KEY_PREFIX_KEYBIND << idx;
        action_prop << KEY_PREFIX_ACTION << idx;
        ++idx;

        ini[SECTION_PRESET_KEYBINDS][preset_prop.str()] = pkb.preset.string();
        ini[SECTION_PRESET_KEYBINDS][keybind_prop.str()] = to_string(pkb.keybind);
        ini[SECTION_PRESET_KEYBINDS][action_prop.str()] = std::to_string(pkb.action);
    }

    return config_file.generate(ini);
}

bool load_preset_keybinds(std::filesystem::path& config_path, std::vector<preset_keybind>& out)
{
    mINI::INIFile config_file(config_path.string());
    mINI::INIStructure ini;
    if (!config_file.read(ini))
    {
        return false;
    }

    out.clear();
    for (auto const& it : ini[SECTION_PRESET_KEYBINDS])
    {
        std::string key = it.first;
        if (key.rfind(KEY_PREFIX_PRESET, 0) == 0)
        {
            preset_keybind pkb = {};

            pkb.preset = it.second;

            std::string keybind_key = convert_key(key, KEY_PREFIX_PRESET, KEY_PREFIX_KEYBIND);
            if (ini[SECTION_PRESET_KEYBINDS].has(keybind_key))
            {
                std::string keybind_str = ini[SECTION_PRESET_KEYBINDS][keybind_key];
                parse_keybind(keybind_str, pkb.keybind);
            }

            std::string action_key = convert_key(key, KEY_PREFIX_PRESET, KEY_PREFIX_ACTION);
            if (ini[SECTION_PRESET_KEYBINDS].has(action_key))
            {
                std::string action_str = ini[SECTION_PRESET_KEYBINDS][action_key];
                pkb.action = static_cast<keybind_action>(stoi(action_str));
            }

            out.push_back(std::move(pkb));
        }
    }

    return true;
}