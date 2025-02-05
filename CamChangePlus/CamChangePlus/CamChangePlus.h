#pragma once

#include <chrono> // For timing
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "GuiBase.h"  // ✅ Ensure this is included
#include "imgui/imgui.h" // Core ImGui functions
#include "imgui/imgui_internal.h" // Internal ImGui features, if needed
// Include necessary headers for game interactions
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/WrapperStructs.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/VehicleWrapper.h"

// Include car movement components (for jump & dodge detection)
#include "bakkesmod/wrappers/GameObject/CarComponent/JumpComponentWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/DoubleJumpComponentWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/DodgeComponentWrapper.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

struct ActionMapping {
    std::string eventName;
    std::string actionName;
    float delay;
    float customValue; // Custom value (e.g., swivel speed, FOV change)
};

class CamChangePlus : public BakkesMod::Plugin::BakkesModPlugin, public PluginWindowBase, public SettingsWindowBase {
public:
    virtual void onLoad() override;
    virtual void onUnload() override;

    std::vector<ActionMapping> eventActions;

private:
    bool isWindowOpen_ = false;
    std::string menuTitle_ = "CamChangePlus";

    // ===========================
    //        GUI
    //
    
    void RenderSettings();
    void ExecuteAction(const std::string& action, float value);
    void ProcessEventActions(const std::string& event);
    void ScheduleAction(const std::string& action, float delay, float value);
    void StartSequencePlayback();
    void StopSequencePlayback();
    void ResetToDefault();

    // ===========================
    //        Game Hooks
    // ===========================
    void HookGameEvents();

    // ===========================
    //        Event Handlers
    // ===========================
    void OnBallTouch();
    void OnExplosion();
    void OnJump();
    void OnDoubleJump();
    void OnFlip();

    // ===========================
    //      Camera Controls
    // ===========================
    void ToggleReverseCam();
    void ToggleBallCam(bool enable);
    void AdjustCameraYaw(float yaw);

    // ===========================
    //    Console Commands (For Testing)
    // ===========================
    void RegisterCommands();

    // ===========================
    //    Internal State Variables
    // ===========================

    float storedYaw = 0.0f;
    float lastLoggedYaw = 0.0f;
    bool isUsingBehindView = false;
    bool hasJumped = false;
    bool hasDoubleJumped = false;
    bool hasFlipped = false;
    bool wasOnGround = true;
    bool tasRunning = false;  // Track whether TAS mode is active
    size_t currentTasIndex = 0; // Track the current action being executed
    bool prevJumpState = false;  // Tracks the previous state of GetbJumped()
    bool prevOnGround = true;

    // Ball touch cooldown tracking
    std::chrono::steady_clock::time_point lastBallTouchTime;
    constexpr static double ballTouchCooldown = 0.2; // 200ms cooldown
};
