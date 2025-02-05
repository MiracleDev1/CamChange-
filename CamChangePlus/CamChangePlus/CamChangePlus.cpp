#include "pch.h"
#include "CamChangePlus.h"
#include "GuiBase.h"

BAKKESMOD_PLUGIN(CamChangePlus, "Camera control based on in-game events", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void CamChangePlus::onLoad() {
    // Initialize GUI context explicitly
    SettingsWindowBase::SetImGuiContext(reinterpret_cast<uintptr_t>(ImGui::GetCurrentContext()));
    PluginWindowBase::SetImGuiContext(reinterpret_cast<uintptr_t>(ImGui::GetCurrentContext()));

    cvarManager->log("[CamChangePlus] Plugin Loaded!");
    HookGameEvents();
    RegisterCommands();
}

void CamChangePlus::onUnload() {
    cvarManager->log("[CamChangePlus] Plugin Unloaded.");
}

void CamChangePlus::HookGameEvents() {
    // Hook into in-game events
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnHitBall", [this](...) { OnBallTouch(); });
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored", [this](...) { OnExplosion(); });
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnJumpPressed", [this](...) { OnJump(); });
    gameWrapper->HookEvent("Function CarComponent_DoubleJump_TA.Active.BeginState", [this](...) { OnDoubleJump(); });
    gameWrapper->HookEvent("Function TAGame.CarComponent_Dodge_TA.EventActivateDodge", [this](...) { OnFlip(); });
}

void CamChangePlus::OnBallTouch() {
    auto now = std::chrono::steady_clock::now();
    double timeSinceLastTouch = std::chrono::duration<double>(now - lastBallTouchTime).count();

    if (timeSinceLastTouch >= ballTouchCooldown) {
        lastBallTouchTime = now;
        cvarManager->log("[CamChangePlus] Ball Touch Detected!");
        ProcessEventActions("Ball Touch");
    }
}

void CamChangePlus::OnExplosion() {
    cvarManager->log("[CamChangePlus] Goal Explosion Detected!");
    ProcessEventActions("Explosion");
}

void CamChangePlus::OnJump() {
    auto car = gameWrapper->GetLocalCar();
    if (!car || car.IsNull()) return;

    bool onGround = car.GetbOnGround();

    // If onGround == 1, it means a jump happened
    if (onGround) {
        cvarManager->log("[CamChangePlus] Jump Detected!");
        ProcessEventActions("Jump");
    }
}

void CamChangePlus::OnDoubleJump() {
    auto car = gameWrapper->GetLocalCar();
    if (!car || car.IsNull()) return;

    bool onGround = car.GetbOnGround();

    // If onGround == 0, it means a double jump happened
    if (!onGround) {
        cvarManager->log("[CamChangePlus] Double Jump Detected!");
        ProcessEventActions("Double Jump");
    }
}

void CamChangePlus::OnFlip() {
    if (hasFlipped) return;

    cvarManager->log("[CamChangePlus] Flip/Dodge Detected!");
    ProcessEventActions("Flip");
}

void CamChangePlus::ToggleReverseCam() {
    gameWrapper->Execute([this](GameWrapper* gw) {
        auto playerController = gw->GetPlayerController();
        if (!playerController) return;

        isUsingBehindView = !isUsingBehindView;
        playerController.SetUsingBehindView(isUsingBehindView);
        cvarManager->log("[CamChangePlus] Reverse Cam: " + std::string(isUsingBehindView ? "Enabled" : "Disabled"));
        });
}

void CamChangePlus::ToggleBallCam(bool enable) {
    gameWrapper->Execute([this, enable](GameWrapper* gw) {
        auto playerController = gw->GetPlayerController();
        if (!playerController) return;

        playerController.SetUsingSecondaryCamera(enable);
        cvarManager->log("[CamChangePlus] Ball Cam: " + std::string(enable ? "Enabled" : "Disabled"));
        });
}

void CamChangePlus::AdjustCameraYaw(float yawPercentage) {
    // Clamp the input between -100 and 100
    yawPercentage = std::max(-100.0f, std::min(100.0f, yawPercentage));

    // Convert percentage to actual yaw range (-23500 to 23500)
    float mappedYaw = (yawPercentage / 100.0f) * 23500.0f;

    // If 0, stop forcing yaw (restore default swivel behavior)
    if (yawPercentage == 0.0f) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            gameWrapper->UnhookEvent("Function TAGame.Camera_TA.ApplySwivel");
            cvarManager->log("[CamChangePlus] Restored normal camera swivel (yaw = 0%).");
            });
        return;
    }

    // Store the mapped yaw value
    storedYaw = mappedYaw;

    gameWrapper->Execute([this, yawPercentage](GameWrapper* gw) {
        auto playerController = gw->GetPlayerController();
        if (!playerController || playerController.IsNull()) {
            cvarManager->log("[CamChangePlus] Error: PlayerController is null.");
            return;
        }

        // Unhook previous event to prevent stacking
        gameWrapper->UnhookEvent("Function TAGame.Camera_TA.ApplySwivel");

        // Hook into ApplySwivel (constant yaw enforcement)
        gameWrapper->HookEvent("Function TAGame.Camera_TA.ApplySwivel", [this, yawPercentage](...) {
            gameWrapper->Execute([this, yawPercentage](GameWrapper* gw) {
                auto camera = gw->GetCamera();
                if (!camera || camera.IsNull()) {
                    cvarManager->log("[CamChangePlus] Error: Camera is null.");
                    return;
                }

                // Get the current swivel settings
                auto swivel = camera.GetCurrentSwivel();

                // Apply the mapped yaw
                swivel.Yaw = storedYaw;

                // Set the new swivel
                camera.SetCurrentSwivel(swivel);

                // Log only when yaw changes
                if (storedYaw != lastLoggedYaw) {
                    cvarManager->log("[CamChangePlus] Applied Camera Yaw: " + std::to_string(swivel.Yaw) +
                        " (Percentage: " + std::to_string(yawPercentage) + "%)");
                    lastLoggedYaw = storedYaw;
                }
                });
            });
        });

    // Log the change when the command is used
    cvarManager->log("[CamChangePlus] Updated camera yaw to: " + std::to_string(yawPercentage) +
        "% (Mapped value: " + std::to_string(storedYaw) + ")");
}

void CamChangePlus::RegisterCommands() {
    // Command to manually adjust camera yaw
    cvarManager->registerNotifier("camchange_yaw", [this](std::vector<std::string> args) {
        if (args.size() < 2) {
            cvarManager->log("[CamChangePlus] Error: Please specify a yaw value.");
            return;
        }

        try {
            float yawValue = std::stof(args[1]);  // Convert the input to a float
            AdjustCameraYaw(yawValue);  // Adjust the camera yaw
            cvarManager->log("[CamChangePlus] Adjusting camera yaw by: " + std::to_string(yawValue));
        }
        catch (const std::exception& e) {
            cvarManager->log("[CamChangePlus] Error: Invalid yaw value. Please enter a valid number.");
        }
        }, "Manually adjust camera yaw", PERMISSION_ALL);

    // Command to toggle reverse camera view
    cvarManager->registerNotifier("camchange_reversecam", [this](std::vector<std::string> args) {
        ToggleReverseCam();  // Toggle the reverse camera
        cvarManager->log("[CamChangePlus] Reverse camera toggled.");
        }, "Toggle reverse camera", PERMISSION_ALL);

    // Command to toggle ball camera
    cvarManager->registerNotifier("camchange_ballcam", [this](std::vector<std::string> args) {
        if (args.size() < 2) {
            cvarManager->log("[CamChangePlus] Error: Please specify a 1 or 0 to enable/disable ball cam.");
            return;
        }

        try {
            bool enable = std::stoi(args[1]) != 0;  // Convert input to a boolean
            ToggleBallCam(enable);  // Toggle the ball camera
            cvarManager->log("[CamChangePlus] Ball camera " + std::string(enable ? "enabled" : "disabled"));
        }
        catch (const std::exception& e) {
            cvarManager->log("[CamChangePlus] Error: Invalid input for ball camera. Please use 1 (enable) or 0 (disable).");
        }
        }, "Toggle ball camera", PERMISSION_ALL);
}

void CamChangePlus::ProcessEventActions(const std::string& event) {
    if (!tasRunning || currentTasIndex >= eventActions.size()) {
        return;
    }

    const auto& currentAction = eventActions[currentTasIndex];

    // Ensure that only the correct action in order gets executed
    if (currentAction.eventName == event) {
        ScheduleAction(currentAction.actionName, currentAction.delay, currentAction.customValue);
        currentTasIndex++;  // Move to the next action in sequence

        // If TAS has finished executing all actions, reset
        if (currentTasIndex >= eventActions.size()) {
            ResetToDefault();
        }
    }
}

void CamChangePlus::ExecuteAction(const std::string& action, float value = 50.0f) {
    if (action == "Enable Reverse Cam") {
        ToggleReverseCam();
    }
    else if (action == "Disable Reverse Cam") {
        ToggleReverseCam();
    }
    else if (action == "Swivel Right") {
        AdjustCameraYaw(value);
    }
    else if (action == "Swivel Left") {
        AdjustCameraYaw(-value);
    }
    cvarManager->log("[CamChangePlus] Executed Action: " + action + " with value: " + std::to_string(value));
}

void CamChangePlus::ScheduleAction(const std::string& action, float delay, float value) {
    gameWrapper->SetTimeout([this, action, value](GameWrapper* gw) {
        ExecuteAction(action, value);
        }, delay);
}

void CamChangePlus::ResetToDefault() {
    cvarManager->log("[CamChangePlus] Resetting to default settings...");

    // Turn off Reverse Cam if it was enabled
    if (isUsingBehindView) {
        ToggleReverseCam();
    }

    // Reset Camera Yaw to 0
    AdjustCameraYaw(0.0f);

    // Stop TAS execution
    tasRunning = false;
    currentTasIndex = 0;

    cvarManager->log("[CamChangePlus] TAS reset complete.");
}

void CamChangePlus::StartSequencePlayback() {
    if (eventActions.empty()) {
        cvarManager->log("[CamChangePlus] No actions mapped!");
        return;
    }

    tasRunning = true;
    currentTasIndex = 0;
    cvarManager->log("[CamChangePlus] TAS Started!");

    // Start monitoring for TAS completion
    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        if (tasRunning && currentTasIndex >= eventActions.size()) {
            ResetToDefault();
        }
        }, 1.0f); // Check for completion every second
}

void CamChangePlus::StopSequencePlayback() {
    cvarManager->log("[CamChangePlus] TAS Stopped by user.");
    ResetToDefault();
}

void CamChangePlus::RenderSettings() {
    ImGui::Text("CamChangePlus Plugin Settings");

    static std::vector<const char*> events = { "Jump", "Flip", "Ball Touch", "Explosion" };
    static std::vector<const char*> actions = { "Enable Reverse Cam", "Disable Reverse Cam", "Swivel Right", "Swivel Left" };

    static int selectedEvent = 0;
    static int selectedAction = 0;
    static float selectedDelay = 0.0f;
    static float customValue = 50.0f;  // Default swivel percentage

    // Dropdowns for selecting event and action
    ImGui::Combo("Event", &selectedEvent, events.data(), events.size());
    ImGui::Combo("Action", &selectedAction, actions.data(), actions.size());
    ImGui::InputFloat("Delay (seconds)", &selectedDelay, 0.1f, 1.0f, "%.1f");

    // If the action is Swivel Right or Swivel Left, allow the user to set how far it swivels
    if (strcmp(actions[selectedAction], "Swivel Right") == 0 || strcmp(actions[selectedAction], "Swivel Left") == 0) {
        ImGui::SliderFloat("Swivel Percentage", &customValue, -100.0f, 100.0f, "%.1f%%");
    }

    if (ImGui::Button("Add Mapping")) {
        eventActions.push_back({ events[selectedEvent], actions[selectedAction], selectedDelay, customValue });
    }

    ImGui::Separator();
    ImGui::Text("Configured Mappings (Drag to Reorder):");

    // Drag-and-Drop Reordering
    for (size_t i = 0; i < eventActions.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        // Drag source
        if (ImGui::Selectable(eventActions[i].eventName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                int swapIndex = (ImGui::GetMouseDragDelta().y < 0) ? i - 1 : i + 1;
                if (swapIndex >= 0 && swapIndex < eventActions.size()) {
                    std::swap(eventActions[i], eventActions[swapIndex]);
                    ImGui::ResetMouseDragDelta();
                }
            }
        }

        // Display the action details
        ImGui::SameLine();
        ImGui::Text("-> %s (Delay: %.1fs)", eventActions[i].actionName.c_str(), eventActions[i].delay);

        if (strcmp(eventActions[i].actionName.c_str(), "Swivel Right") == 0 || strcmp(eventActions[i].actionName.c_str(), "Swivel Left") == 0) {
            ImGui::SameLine();
            ImGui::Text("(Swivel: %.1f%%)", eventActions[i].customValue);
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            eventActions.erase(eventActions.begin() + i);
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    if (ImGui::Button("Start TAS")) {
        StartSequencePlayback();
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop TAS")) {
        StopSequencePlayback();
    }
}
