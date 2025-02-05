#pragma once
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/plugin/pluginwindow.h"

class SettingsWindowBase : public BakkesMod::Plugin::PluginSettingsWindow {
public:
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t ctx) override;
};

class PluginWindowBase : public BakkesMod::Plugin::PluginWindow {
public:
    virtual ~PluginWindowBase() = default;

    bool isWindowOpen_ = false;  // Tracks if the window is open
    std::string menuTitle_ = "CamChange+";  // Title of the GUI window

    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    void SetImGuiContext(uintptr_t ctx) override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;
    void Render() override;

    //virtual void RenderWindow() = 0;  // Must be implemented by the derived class
};
