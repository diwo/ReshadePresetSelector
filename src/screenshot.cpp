#include "screenshot.hpp"

#include <reshade.hpp>
#include <fpng.h>
#include <string>
#include <queue>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <time.h>

static std::queue<std::unique_ptr<screenshot_stage>> g_screenshot_workloads;
static uint32_t g_last_frame;
static uint32_t g_last_effects_render_frame;

void screenshot_stage::initialize(reshade::api::effect_runtime *runtime) {
	if (!this->started)
	{
		this->start_work(runtime);
		started = true;
	}
}

void screenshot_change_preset_stage::start_work(reshade::api::effect_runtime *runtime)
{
	runtime->set_current_preset_path(preset.string().c_str());
	this->prev_effects_render_frame = g_last_effects_render_frame;
}

bool screenshot_change_preset_stage::is_completed()
{
	return g_last_effects_render_frame > this->prev_effects_render_frame;
}

void screenshot_wait_stage::start_work(reshade::api::effect_runtime *runtime)
{
	this->start_frame = g_last_frame;
}

bool screenshot_wait_stage::is_completed()
{
	return g_last_frame - this->start_frame >= this->frames_count;
}

void screenshot_capture_stage::start_work(reshade::api::effect_runtime *runtime)
{
	save_screenshot(runtime);
}

void queue_screenshot_workload(std::filesystem::path &preset, std::filesystem::path &original_preset)
{
	g_screenshot_workloads.push(std::make_unique<screenshot_change_preset_stage>(preset));
	g_screenshot_workloads.push(std::make_unique<screenshot_wait_stage>(5));
	g_screenshot_workloads.push(std::make_unique<screenshot_capture_stage>());
	g_screenshot_workloads.push(std::make_unique<screenshot_change_preset_stage>(original_preset));
}

bool process_screenshot_workload(reshade::api::effect_runtime *runtime)
{
	if (g_screenshot_workloads.empty()) return false;

	do
	{
		g_screenshot_workloads.front()->initialize(runtime);
		if (g_screenshot_workloads.front()->is_completed())
			g_screenshot_workloads.pop();
		else
			break;
	}
	while (!g_screenshot_workloads.empty());

	return true;
}

static std::string get_screenshot_filename()
{
	const auto now = std::chrono::system_clock::now();
	const auto now_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	const std::time_t now_time = std::chrono::system_clock::to_time_t(now_seconds);
	std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - now_seconds);

	struct tm tm;
	localtime_s(&tm, &now_time);
	char filename[30];
	sprintf(filename, "%.4d-%.2d-%.2d %.2d-%.2d-%.2d.%.3lld.png",
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());

	return filename;
}

static std::filesystem::path get_screenshot_path(reshade::api::effect_runtime *runtime)
{
	size_t size;
	reshade::get_reshade_base_path(nullptr, &size);
	char* reshade_path_str = new char[size];
	reshade::get_reshade_base_path(reshade_path_str, &size);

	reshade::get_config_value(runtime, "SCREENSHOT", "SavePath", nullptr, &size);
	char* screenshot_path_str = new char[size];
	reshade::get_config_value(runtime, "SCREENSHOT", "SavePath", screenshot_path_str, &size);

	std::filesystem::path screenshot_path = std::filesystem::u8path(reshade_path_str);
	screenshot_path /= std::filesystem::u8path(screenshot_path_str);
	screenshot_path /= get_screenshot_filename();

	return screenshot_path.lexically_normal();
}

void save_screenshot(reshade::api::effect_runtime *runtime)
{
	uint32_t width, height;
	runtime->get_screenshot_width_and_height(&width, &height);
	std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
	if (!runtime->capture_screenshot(pixels.data()))
	{
		reshade::log_message(reshade::log_level::error, "Failed to capture screenshot");
		return;
	}

	for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); ++i)
		*reinterpret_cast<uint32_t *>(pixels.data() + 3 * i) = *reinterpret_cast<const uint32_t *>(pixels.data() + 4 * i);

	std::vector<uint8_t> encoded_data;
	if (!fpng::fpng_encode_image_to_memory(pixels.data(), width, height, 3, encoded_data))
	{
		reshade::log_message(reshade::log_level::error, "Failed to encode screenshot to png");
		return;
	}

	std::filesystem::path path = get_screenshot_path(runtime);

	if (!std::filesystem::exists(path.parent_path()))
	{
		if (!std::filesystem::create_directories(path.parent_path()))
		{
			reshade::log_message(reshade::log_level::error, "Failed to create screenshot directory");
			return;
		}
	}

	std::ofstream file = std::ofstream(path, std::ios::binary | std::ios::trunc);
	file.write((const char*)encoded_data.data(), encoded_data.size());
	file.close();

	if (!file.good())
	{
		reshade::log_message(reshade::log_level::error, "Error while saving screenshot to file");
	}
}

void screenshot_notify_frame(uint32_t frame)
{
	g_last_frame = frame;
}

void screenshot_notify_effects_rendered(uint32_t frame)
{
	g_last_effects_render_frame = frame;
}
