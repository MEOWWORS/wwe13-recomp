#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <rex/input/input_driver.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/window_listener.h>
#include <rex/input/device.h>

#include <vector>

#include "touch_layout.h"

namespace wwe13 {

struct VirtualGamepadState {
  std::atomic_bool gameplay_active{false};
  std::mutex mutex;
  uint16_t buttons = 0;
  uint8_t left_trigger = 0;
  uint8_t right_trigger = 0;
  int16_t left_x = 0;
  int16_t left_y = 0;
  int16_t right_x = 0;
  int16_t right_y = 0;
  uint32_t packet_number = 0;

  // What each touch presses (touch_layout.h; the on-screen layout is shared with the launcher's editor).
  using Control = touch::Action;

  struct Touch {
    Control control = Control::kNone;
    float x = 0.0f;
    float y = 0.0f;
    int item = -1;  // index of the control in the layout
  };

  std::unordered_map<uint32_t, Touch> touches;
};

class VirtualGamepadInputDriver final
    : public rex::input::InputDriver,
      public rex::ui::WindowInputListener,
      public rex::ui::WindowListener {
 public:
  explicit VirtualGamepadInputDriver(
      std::shared_ptr<VirtualGamepadState> state);
  ~VirtualGamepadInputDriver() override;

  rex::X_STATUS Setup() override;
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override;
  rex::X_RESULT GetDeviceCapabilities(
      rex::input::DeviceId device, uint32_t flags,
      rex::input::X_INPUT_CAPABILITIES* out_caps) override;
  rex::X_RESULT GetDeviceState(
      rex::input::DeviceId device,
      rex::input::X_INPUT_STATE* out_state) override;
  rex::X_RESULT SetDeviceVibration(
      rex::input::DeviceId device,
      rex::input::X_INPUT_VIBRATION* vibration) override;
  rex::X_RESULT GetDeviceKeystroke(
      rex::input::DeviceId device, uint32_t flags,
      rex::input::X_INPUT_KEYSTROKE* out_keystroke) override;

  void OnWindowAvailable(rex::ui::Window* window) override;
  void OnTouchEvent(rex::ui::TouchEvent& event) override;
  void OnClosing(rex::ui::UIEvent& event) override;

 private:
  void ReleaseAllTouches();

  std::shared_ptr<VirtualGamepadState> state_;
  rex::ui::Window* attached_window_ = nullptr;
};

class VirtualGamepadOverlay final : public rex::ui::ImGuiDialog {
 public:
  VirtualGamepadOverlay(
      rex::ui::ImGuiDrawer* drawer,
      std::shared_ptr<VirtualGamepadState> state);
  ~VirtualGamepadOverlay();

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  // Loads the button artwork once per immediate drawer. False = draw the plain buttons instead.
  bool EnsureAtlas();
  void DrawAndroidStyle(ImGuiIO& io);

  std::shared_ptr<VirtualGamepadState> state_;
  std::unique_ptr<rex::ui::ImmediateTexture> atlas_;
  rex::ui::ImmediateDrawer* atlas_drawer_ = nullptr;
  bool atlas_failed_ = false;
};

}  // namespace wwe13
