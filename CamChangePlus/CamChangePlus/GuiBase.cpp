#include "pch.h"
#include "GuiBase.h"

std::string SettingsWindowBase::GetPluginName() {
    return "CamChange+";
}

void SettingsWindowBase::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

std::string PluginWindowBase::GetMenuName() {
    return "CamChange+";  // Unique menu name
}

std::string PluginWindowBase::GetMenuTitle() {
    return menuTitle_;
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

    // Call the user-implemented RenderWindow() directly without wrapping it in another ImGui::Begin/End
    //RenderWindow();  // This will now be responsible for rendering the entire GUI
}
