#include "pch.h"
#include "GuiBase.h"

std::string SettingsWindowBase::GetPluginName() {
    return "CamChangePlus";  // Make sure it's a clean name
}

void SettingsWindowBase::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

std::string PluginWindowBase::GetMenuName() {
    return "camchangeplus";  // Matches `togglemenu camchangeplus`
}

std::string PluginWindowBase::GetMenuTitle() {
    return "CamChangePlus - Camera Automation";
}

void PluginWindowBase::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool PluginWindowBase::ShouldBlockInput() {
    return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

bool PluginWindowBase::IsActiveOverlay() {
    return true;
}

void PluginWindowBase::OnOpen() {
    isWindowOpen_ = true;
}

void PluginWindowBase::OnClose() {
    isWindowOpen_ = false;
}

void PluginWindowBase::Render() {
    if (!isWindowOpen_) {
        return;  // Do not render if the window is not open
    }

    ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 350), ImVec2(1300, 800));

    if (ImGui::Begin("CamChangePlus - Advanced Camera Control", &isWindowOpen_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        RenderWindow();  // This will now properly render the menu
    }

    ImGui::End();
}
