#pragma once

#include <reshade.hpp>
#include <queue>
#include <memory>
#include <filesystem>

struct screenshot_stage {
	bool started;

	screenshot_stage() : started(false) {}
	void initialize(reshade::api::effect_runtime *runtime);
	virtual void start_work(reshade::api::effect_runtime *runtime) {}
	virtual bool is_completed() { return true; }
};

struct screenshot_change_preset_stage : screenshot_stage {
	std::filesystem::path preset;
	uint32_t prev_effects_render_frame;

	screenshot_change_preset_stage(std::filesystem::path &preset) :
		screenshot_stage(), preset(preset) {};
	
	void start_work(reshade::api::effect_runtime *runtime);
	bool is_completed();
};

struct screenshot_wait_stage : screenshot_stage {
	uint8_t frames_count;
	uint32_t start_frame;

	screenshot_wait_stage(uint8_t frames_count) :
		screenshot_stage(), frames_count(frames_count) {};

	void start_work(reshade::api::effect_runtime *runtime);
	bool is_completed();
};

struct screenshot_capture_stage : screenshot_stage {
	screenshot_capture_stage() : screenshot_stage() {};

	void start_work(reshade::api::effect_runtime *runtime);
};

void queue_screenshot_workload(std::filesystem::path &preset, std::filesystem::path &original_preset);
bool process_screenshot_workload(reshade::api::effect_runtime *runtime);
void save_screenshot(reshade::api::effect_runtime *runtime);
void screenshot_notify_frame(uint32_t frame);
void screenshot_notify_effects_rendered(uint32_t frame);
