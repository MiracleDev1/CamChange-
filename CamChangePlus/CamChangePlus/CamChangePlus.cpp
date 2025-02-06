#include "pch.h"
#include "CamChangePlus.h"
#include "GuiBase.h"

BAKKESMOD_PLUGIN(CamChangePlus, "Camera control based on in-game events", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void CamChangePlus::onLoad() {
    cvarManager->log("[CamChangePlus] Plugin Loaded.");

    // Hook game events
    HookGameEvents();

    // Register commands
    RegisterCommands();

    // Bind F1 key to toggle menu
    cvarManager->setBind("F1", "togglemenu CamChangePlus");

    // Initialize GUI context
    if (ImGui::GetCurrentContext() == nullptr) {
        ImGui::CreateContext();
    }
    SettingsWindowBase::SetImGuiContext(reinterpret_cast<uintptr_t>(ImGui::GetCurrentContext()));
    PluginWindowBase::SetImGuiContext(reinterpret_cast<uintptr_t>(ImGui::GetCurrentContext()));

    // Ensure the window is open when loaded
    isWindowOpen_ = true;

    cvarManager->log("[CamChangePlus] Initialization complete.");
}

void CamChangePlus::onUnload() {
    cvarManager->log("[CamChangePlus] Plugin Unloaded.");
}

void CamChangePlus::HookGameEvents() {
    cvarManager->log("[CamChangePlus] Hooking game events...");

    gameWrapper->HookEvent("Function TAGame.Car_TA.OnHitBall", [this](...) { OnBallTouch(); });
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored", [this](...) { OnExplosion(); });
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnJumpPressed", [this](...) { OnJump(); });
    gameWrapper->HookEvent("Function CarComponent_DoubleJump_TA.Active.BeginState", [this](...) { OnDoubleJump(); });
    gameWrapper->HookEvent("Function TAGame.CarComponent_Dodge_TA.EventActivateDodge", [this](...) { OnFlip(); });

    cvarManager->log("[CamChangePlus] Game events hooked successfully.");
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
    static bool yawDirectionRight = true; // true = right, false = left

    if (action == "Enable Reverse Cam") {
        if (!isUsingBehindView) ToggleReverseCam();
    }
    else if (action == "Disable Reverse Cam") {
        if (isUsingBehindView) ToggleReverseCam();
    }
    else if (action == "Toggle Reverse Cam") {
        ToggleReverseCam();
    }
    else if (action == "Toggle Swivel Direction") {
        yawDirectionRight = !yawDirectionRight;
        cvarManager->log("[CamChangePlus] Yaw direction set to " + std::string(yawDirectionRight ? "Right" : "Left"));
    }
    else if (action == "Adjust Camera Yaw") {
        float yawValue = yawDirectionRight ? value : -value; // Use toggled direction
        AdjustCameraYaw(yawValue);
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

void CamChangePlus::SaveMappingsToFile() {
    json j;
    std::string filepath = gameWrapper->GetDataFolder().string() + "/CamChangePlus_shots.json";

    // Load existing file if it exists
    std::ifstream fileRead(filepath);
    if (fileRead.is_open()) {
        fileRead >> j;
        fileRead.close();
    }

    // Store current sequence
    j["shots"][currentSequenceName] = json::array();
    for (const auto& action : eventActions) {
        j["shots"][currentSequenceName].push_back({
            {"eventName", action.eventName},
            {"actionName", action.actionName},
            {"delay", action.delay},
            {"customValue", action.customValue}
            });
    }

    // Save back to file
    std::ofstream fileWrite(filepath);
    if (fileWrite.is_open()) {
        fileWrite << j.dump(4);
        fileWrite.close();
        cvarManager->log("[CamChangePlus] Saved sequence: " + currentSequenceName);
    }
    else {
        cvarManager->log("[CamChangePlus] Error: Could not save file.");
    }
}

void CamChangePlus::LoadMappingsFromFile(const std::string& sequenceName) {
    json j;
    std::string filepath = gameWrapper->GetDataFolder().string() + "/CamChangePlus_shots.json";

    std::ifstream file(filepath);
    if (!file.is_open()) {
        cvarManager->log("[CamChangePlus] Error: No saved shots found.");
        return;
    }

    file >> j;
    file.close();

    if (j.contains("shots") && j["shots"].contains(sequenceName)) {
        eventActions.clear();
        for (const auto& mapping : j["shots"][sequenceName]) {
            eventActions.push_back({
                mapping["eventName"],
                mapping["actionName"],
                mapping["delay"],
                mapping["customValue"]
                });
        }
        currentSequenceName = sequenceName;
        cvarManager->log("[CamChangePlus] Loaded sequence: " + sequenceName);
    }
    else {
        cvarManager->log("[CamChangePlus] Error: Sequence not found.");
    }
}

void CamChangePlus::LogDebugToFile(const std::string& message) {  // Use CamChangePlus::
    try {
        std::string desktopPath = std::filesystem::path(std::getenv("USERPROFILE")).string() + "/Desktop";
        std::string logFilePath = desktopPath + "/CamChangePlus_DebugLog.txt";

        std::ofstream logFile(logFilePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        }
    }
    catch (const std::exception& e) {
        std::cout << "[CamChangePlus] Exception: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "[CamChangePlus] Unknown error occurred while logging to file." << std::endl;
    }
}

void CamChangePlus::RenderSettings() {
    if (ImGui::Button("Open CamChangePlus", ImVec2(200, 40))) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            _globalCvarManager->executeCommand("togglemenu " + GetMenuName());
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Bind this menu in BakkesMod!");
    }
}


void CamChangePlus::RenderWindow() {
    if (!isWindowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(850, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 300), ImVec2(1200, 800));

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 3.0f;
    style.WindowRounding = 3.0f;
    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(8, 4);

    static bool showErrorPopup = false;
    static bool showDuplicateNamePopup = false;
    static bool showDeleteLastSequencePopup = false;
    static bool tasRunning = false;

    if (ImGui::Begin("CamChangePlus - Shot Sequence Builder", &isWindowOpen_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        static float leftPanelWidth = 220.0f;
        static int selectedMapping = -1;

        // Sidebar for managing sequences
        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0), true);
        ImGui::Text("Sequences");
        ImGui::Separator();

        static char sequenceName[256] = "";
        ImGui::InputText("##SequenceName", sequenceName, IM_ARRAYSIZE(sequenceName));
        ImGui::SameLine();
        if (ImGui::Button("Add", ImVec2(50, 25))) {
            if (strlen(sequenceName) > 0) {
                bool nameExists = false;
                for (const auto& seq : eventMappings) {
                    if (seq.first == sequenceName) {
                        nameExists = true;
                        break;
                    }
                }
                if (nameExists) {
                    showDuplicateNamePopup = true;
                }
                else {
                    eventMappings.emplace_back(sequenceName, std::vector<std::pair<std::string, std::string>>());
                }
            }
            else {
                showErrorPopup = true;
            }
        }

        ImGui::Separator();

        // List of existing sequences
        for (size_t i = 0; i < eventMappings.size(); ++i) {
            if (ImGui::Selectable(eventMappings[i].first.c_str(), selectedMapping == i)) {
                selectedMapping = i;
            }
        }

        if (selectedMapping >= 0 && eventMappings.size() > 1) {
            if (ImGui::Button("Delete Sequence", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
                eventMappings.erase(eventMappings.begin() + selectedMapping);
                selectedMapping = -1;
            }
        }
        else if (selectedMapping >= 0 && eventMappings.size() == 1) {
            if (ImGui::Button("Delete Sequence", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
                showDeleteLastSequencePopup = true;
            }
        }

        ImGui::EndChild();
        ImGui::SameLine();

        // Main panel for editing sequences
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

        if (selectedMapping >= 0 && selectedMapping < eventMappings.size()) {
            ImGui::Text("Editing Sequence: %s", eventMappings[selectedMapping].first.c_str());
            ImGui::Separator();

            static const char* availableEvents[] = { "Ball Touch", "Jump", "Double Jump", "Flip", "Explosion" };
            static const char* availableActions[] = { "Toggle Reverse Cam", "Set Yaw", "Enable Ball Cam" };

            static int selectedEvent = 0;
            static int selectedAction = 0;
            static float customYaw = 0.0f;
            static float delay = 0.0f;

            ImGui::Text("Add New Mapping");
            ImGui::Combo("##Event", &selectedEvent, availableEvents, IM_ARRAYSIZE(availableEvents));
            ImGui::SameLine();
            ImGui::Text("Event");

            ImGui::Combo("##Action", &selectedAction, availableActions, IM_ARRAYSIZE(availableActions));
            ImGui::SameLine();
            ImGui::Text("Action");

            if (selectedAction == 1) {  // Set Yaw
                ImGui::SliderFloat("Yaw %", &customYaw, -100.0f, 100.0f, "%.1f%%");
            }

            ImGui::InputFloat("Delay (s)", &delay, 0.1f, 1.0f, "%.2f");

            if (ImGui::Button("Add Mapping", ImVec2(150, 25))) {
                std::string actionDetail = availableActions[selectedAction];
                if (selectedAction == 1) {
                    actionDetail += " (" + std::to_string(customYaw) + "%)";
                }

                // Ensure the selected sequence exists
                if (selectedMapping >= 0 && selectedMapping < eventMappings.size()) {
                    eventMappings[selectedMapping].second.push_back(
                        std::make_pair(
                            std::string(availableEvents[selectedEvent]),
                            "→ " + actionDetail + " (Delay: " + std::to_string(delay) + "s)"
                        )
                    );
                }
            }

            ImGui::Separator();
            ImGui::Text("Current Mappings:");

            ImGui::BeginChild("MappingsList", ImVec2(0, 150), true);
            for (size_t i = 0; i < eventMappings[selectedMapping].second.size(); ++i) {
                ImGui::Text("%s %s",
                    eventMappings[selectedMapping].second[i].first.c_str(),
                    eventMappings[selectedMapping].second[i].second.c_str());

                ImGui::SameLine();
                if (ImGui::Button(("Delete##" + std::to_string(i)).c_str(), ImVec2(50, 20))) {
                    eventMappings[selectedMapping].second.erase(eventMappings[selectedMapping].second.begin() + i);
                    break;
                }

                // Move Up Button
                if (i > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button(("▲##MoveUp" + std::to_string(i)).c_str(), ImVec2(20, 20))) {
                        std::swap(eventMappings[selectedMapping].second[i], eventMappings[selectedMapping].second[i - 1]);
                    }
                }
                // Move Down Button
                if (i < eventMappings[selectedMapping].second.size() - 1) {
                    ImGui::SameLine();
                    if (ImGui::Button(("▼##MoveDown" + std::to_string(i)).c_str(), ImVec2(20, 20))) {
                        std::swap(eventMappings[selectedMapping].second[i], eventMappings[selectedMapping].second[i + 1]);
                    }
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("Save")) SaveMappingsToFile();
            ImGui::SameLine();
            if (ImGui::Button("Load")) LoadMappingsFromFile(sequenceName);
        }
        else {
            ImGui::Text("Select or create a sequence to edit.");
        }

        ImGui::EndChild();
        ImGui::End();
    }

    // Error Popup: Empty Sequence Name
    if (showErrorPopup) {
        ImGui::OpenPopup("Error");
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Sequence name cannot be empty!");
            if (ImGui::Button("OK", ImVec2(100, 30))) {
                showErrorPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Error Popup: Duplicate Sequence Name
    if (showDuplicateNamePopup) {
        ImGui::OpenPopup("Error");
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Sequence name already exists! Choose a different name.");
            if (ImGui::Button("OK", ImVec2(100, 30))) {
                showDuplicateNamePopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}
