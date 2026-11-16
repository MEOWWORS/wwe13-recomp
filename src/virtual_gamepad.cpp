#include "virtual_gamepad.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <rex/input/input.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/window.h>

#include "controls_atlas_rects.h"
#include "launcher_embedded_images.h"
#include "touch_layout.h"

namespace wwe13 {

namespace {

using Control = VirtualGamepadState::Control;

// Touch style from WWE13_TOUCH_STYLE: "xbox", "android" (the artwork, default) or "none" (a real gamepad is
// used; nothing is drawn or touched).
int TouchStyle() {
  static const int style = [] {
    const char* value = std::getenv("WWE13_TOUCH_STYLE");
    if (value && std::strcmp(value, "xbox") == 0) return 0;
    if (value && std::strcmp(value, "none") == 0) return 2;
    return 1;
  }();
  return style;
}

// The virtual pad only counts as a connected controller while it is shown.
bool PadActive(const VirtualGamepadState& state) {
  return state.gameplay_active.load() && TouchStyle() != 2;
}

// Size and opacity chosen in the launcher (Controls page): WWE13_TOUCH_SCALE and WWE13_TOUCH_ALPHA, in percent.
float TouchScale() {
  static const float scale = [] {
    const char* value = std::getenv("WWE13_TOUCH_SCALE");
    const int percent = value ? std::atoi(value) : 100;
    return (percent < 60 || percent > 150) ? 1.0f : float(percent) / 100.0f;
  }();
  return scale;
}

int TouchAlpha() {
  static const int alpha = [] {
    const char* value = std::getenv("WWE13_TOUCH_ALPHA");
    const int percent = value ? std::atoi(value) : 46;
    return (percent < 15 || percent > 100) ? 118 : percent * 255 / 100;
  }();
  return alpha;
}

// The layout for the current screen shape: the player's arrangement from the launcher's editor
// (WWE13_TOUCH_LAYOUT), at the chosen size. Rebuilt when the screen shape changes.
std::mutex g_layout_mutex;
float g_layout_aspect = 0.0f;
bool g_layout_android = true;
std::vector<touch::Item> g_layout;

// Callers hold g_layout_mutex.
const std::vector<touch::Item>& LayoutFor(float aspect, bool android) {
  if (g_layout.empty() || std::abs(aspect - g_layout_aspect) > 0.02f || android != g_layout_android) {
    const char* file = std::getenv("WWE13_TOUCH_LAYOUT");
    g_layout = touch::LoadLayout(android ? touch::Style::kAndroid : touch::Style::kXbox, aspect,
                                 file ? file : "", TouchScale());
    g_layout_aspect = aspect;
    g_layout_android = android;
  }
  return g_layout;
}

// The Android artwork could not be loaded: the drawn Xbox 360 buttons are used instead.
std::atomic_bool g_android_art_failed{false};
bool UseAndroidLayout() { return TouchStyle() == 1 && !g_android_art_failed.load(); }

float g_aspect = 16.0f / 9.0f;

int16_t StickAxis(float value) {
  value = std::clamp(value, -1.0f, 1.0f);
  return static_cast<int16_t>(value * 32767.0f);
}

// Callers hold state.mutex.
void RebuildState(VirtualGamepadState& state) {
  state.buttons = 0;
  state.left_trigger = 0;
  state.right_trigger = 0;
  state.left_x = 0;
  state.left_y = 0;
  state.right_x = 0;
  state.right_y = 0;

  std::lock_guard layout_lock(g_layout_mutex);
  const std::vector<touch::Item>& items = LayoutFor(g_aspect, UseAndroidLayout());
  for (const auto& [pointer_id, touch] : state.touches) {
    (void)pointer_id;
    switch (touch.control) {
      case Control::kLeftStick:
      case Control::kRightStick: {
        if (touch.item < 0 || touch.item >= int(items.size())) break;
        float dx = 0.0f, dy = 0.0f;
        touch::StickDeflection(items[touch.item], touch.x, touch.y, g_aspect, &dx, &dy);
        if (touch.control == Control::kLeftStick) {
          state.left_x = StickAxis(dx);
          state.left_y = StickAxis(-dy);
        } else {
          state.right_x = StickAxis(dx);
          state.right_y = StickAxis(-dy);
        }
        break;
      }
      case Control::kA: state.buttons |= rex::input::X_INPUT_GAMEPAD_A; break;
      case Control::kB: state.buttons |= rex::input::X_INPUT_GAMEPAD_B; break;
      case Control::kX: state.buttons |= rex::input::X_INPUT_GAMEPAD_X; break;
      case Control::kY: state.buttons |= rex::input::X_INPUT_GAMEPAD_Y; break;
      case Control::kDpadUp: state.buttons |= rex::input::X_INPUT_GAMEPAD_DPAD_UP; break;
      case Control::kDpadDown: state.buttons |= rex::input::X_INPUT_GAMEPAD_DPAD_DOWN; break;
      case Control::kDpadLeft: state.buttons |= rex::input::X_INPUT_GAMEPAD_DPAD_LEFT; break;
      case Control::kDpadRight: state.buttons |= rex::input::X_INPUT_GAMEPAD_DPAD_RIGHT; break;
      case Control::kLeftShoulder: state.buttons |= rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER; break;
      case Control::kRightShoulder: state.buttons |= rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER; break;
      case Control::kLeftTrigger: state.left_trigger = 255; break;
      case Control::kRightTrigger: state.right_trigger = 255; break;
      case Control::kStart: state.buttons |= rex::input::X_INPUT_GAMEPAD_START; break;
      case Control::kBack: state.buttons |= rex::input::X_INPUT_GAMEPAD_BACK; break;
      case Control::kLeftThumb: state.buttons |= rex::input::X_INPUT_GAMEPAD_LEFT_THUMB; break;
      case Control::kRightThumb: state.buttons |= rex::input::X_INPUT_GAMEPAD_RIGHT_THUMB; break;
      case Control::kNone: break;
    }
  }
}

}  // namespace

VirtualGamepadInputDriver::VirtualGamepadInputDriver(
    std::shared_ptr<VirtualGamepadState> state)
    : InputDriver(nullptr, 1000), state_(std::move(state)) {}

VirtualGamepadInputDriver::~VirtualGamepadInputDriver() {
  if (attached_window_) {
    attached_window_->RemoveInputListener(this);
    attached_window_->RemoveListener(this);
    attached_window_ = nullptr;
  }
  ReleaseAllTouches();
}

rex::X_STATUS VirtualGamepadInputDriver::Setup() {
  return static_cast<rex::X_STATUS>(0);
}

rex::X_RESULT VirtualGamepadInputDriver::GetCapabilities(
    uint32_t user_index, uint32_t flags,
    rex::input::X_INPUT_CAPABILITIES* out_caps) {
  (void)flags;
  if (user_index != 0 || !PadActive(*state_)) {
    return static_cast<rex::X_RESULT>(0x0000048F);
  }
  if (out_caps) {
    std::memset(out_caps, 0, sizeof(*out_caps));
    out_caps->type = 1;
    out_caps->sub_type = 1;
    out_caps->gamepad.buttons = 0xFFFF;
    out_caps->gamepad.left_trigger = 0xFF;
    out_caps->gamepad.right_trigger = 0xFF;
    out_caps->gamepad.thumb_lx = 0x7FFF;
    out_caps->gamepad.thumb_ly = 0x7FFF;
    out_caps->gamepad.thumb_rx = 0x7FFF;
    out_caps->gamepad.thumb_ry = 0x7FFF;
    out_caps->vibration.left_motor_speed = 0xFFFF;
    out_caps->vibration.right_motor_speed = 0xFFFF;
  }
  return static_cast<rex::X_RESULT>(0);
}

rex::X_RESULT VirtualGamepadInputDriver::GetState(
    uint32_t user_index, rex::input::X_INPUT_STATE* out_state) {
  if (user_index != 0 || !PadActive(*state_)) {
    return static_cast<rex::X_RESULT>(0x0000048F);
  }

  std::lock_guard lock(state_->mutex);
  ++state_->packet_number;
  if (out_state) {
    std::memset(out_state, 0, sizeof(*out_state));
    out_state->packet_number = state_->packet_number;
    out_state->gamepad.buttons = state_->buttons;
    out_state->gamepad.left_trigger = state_->left_trigger;
    out_state->gamepad.right_trigger = state_->right_trigger;
    out_state->gamepad.thumb_lx = state_->left_x;
    out_state->gamepad.thumb_ly = state_->left_y;
    out_state->gamepad.thumb_rx = state_->right_x;
    out_state->gamepad.thumb_ry = state_->right_y;
  }
  return static_cast<rex::X_RESULT>(0);
}

rex::X_RESULT VirtualGamepadInputDriver::SetState(
    uint32_t user_index, rex::input::X_INPUT_VIBRATION* vibration) {
  (void)vibration;
  return user_index == 0 && PadActive(*state_)
             ? static_cast<rex::X_RESULT>(0)
             : static_cast<rex::X_RESULT>(0x0000048F);
}

rex::X_RESULT VirtualGamepadInputDriver::GetKeystroke(
    uint32_t user_index, uint32_t flags,
    rex::input::X_INPUT_KEYSTROKE* out_keystroke) {
  (void)flags;
  (void)out_keystroke;
  return user_index == 0 && PadActive(*state_)
             ? static_cast<rex::X_RESULT>(0x000010D2)
             : static_cast<rex::X_RESULT>(0x0000048F);
}

void VirtualGamepadInputDriver::OnWindowAvailable(rex::ui::Window* window) {
  if (!window || attached_window_ == window) return;
  if (attached_window_) {
    attached_window_->RemoveInputListener(this);
    attached_window_->RemoveListener(this);
  }
  attached_window_ = window;
  attached_window_->AddInputListener(this, window_z_order());
  attached_window_->AddListener(this);
}

void VirtualGamepadInputDriver::OnClosing(rex::ui::UIEvent& event) {
  (void)event;
  if (attached_window_) {
    attached_window_->RemoveInputListener(this);
    attached_window_->RemoveListener(this);
    attached_window_ = nullptr;
  }
  ReleaseAllTouches();
}

void VirtualGamepadInputDriver::ReleaseAllTouches() {
  std::lock_guard lock(state_->mutex);
  state_->touches.clear();
  RebuildState(*state_);
}

void VirtualGamepadInputDriver::OnTouchEvent(rex::ui::TouchEvent& event) {
  if (!PadActive(*state_) || !attached_window_) return;

  const float width = static_cast<float>(attached_window_->GetActualPhysicalWidth());
  const float height = static_cast<float>(attached_window_->GetActualPhysicalHeight());
  if (width <= 0.0f || height <= 0.0f) return;

  const uint32_t pointer_id = event.pointer_id();
  const float x = std::clamp(event.x() / width, 0.0f, 1.0f);
  const float y = std::clamp(event.y() / height, 0.0f, 1.0f);

  std::lock_guard lock(state_->mutex);
  auto it = state_->touches.find(pointer_id);

  switch (event.action()) {
    case rex::ui::TouchEvent::Action::kDown: {
      g_aspect = width / height;
      Control control = Control::kNone;
      int item = -1;
      {
        std::lock_guard layout_lock(g_layout_mutex);
        item = touch::HitTest(LayoutFor(g_aspect, UseAndroidLayout()), x, y, g_aspect, &control);
      }
      if (item < 0 || control == Control::kNone) return;
      VirtualGamepadState::Touch touch;
      touch.control = control;
      touch.x = x;
      touch.y = y;
      touch.item = item;
      state_->touches[pointer_id] = touch;
      event.set_handled(true);
      break;
    }
    case rex::ui::TouchEvent::Action::kMove:
      if (it == state_->touches.end()) return;
      it->second.x = x;
      it->second.y = y;
      {
        // A d-pad follows the thumb from one direction to the next.
        std::lock_guard layout_lock(g_layout_mutex);
        const std::vector<touch::Item>& items = LayoutFor(g_aspect, UseAndroidLayout());
        const int item = it->second.item;
        if (item >= 0 && item < int(items.size()) && items[item].shape == touch::Shape::kDpad) {
          it->second.control = touch::DpadDirection(items[item], x, y, g_aspect);
        }
      }
      event.set_handled(true);
      break;
    case rex::ui::TouchEvent::Action::kUp:
    case rex::ui::TouchEvent::Action::kCancel:
      if (it == state_->touches.end()) return;
      state_->touches.erase(it);
      event.set_handled(true);
      break;
  }

  RebuildState(*state_);
}

VirtualGamepadOverlay::VirtualGamepadOverlay(
    rex::ui::ImGuiDrawer* drawer,
    std::shared_ptr<VirtualGamepadState> state)
    : ImGuiDialog(drawer), state_(std::move(state)) {}

VirtualGamepadOverlay::~VirtualGamepadOverlay() {
  // The texture belongs to the immediate drawer, which may already be gone: never destroy it from here.
  (void)atlas_.release();
}

bool VirtualGamepadOverlay::EnsureAtlas() {
  rex::ui::ImmediateDrawer* drawer = imgui_drawer() ? imgui_drawer()->immediate_drawer() : nullptr;
  if (!drawer) {
    return false;
  }
  if (drawer != atlas_drawer_) {
    (void)atlas_.release();  // made by the previous drawer
    atlas_drawer_ = drawer;
    atlas_failed_ = false;
  }
  if (atlas_) {
    return true;
  }
  if (atlas_failed_) {
    return false;
  }
  atlas_failed_ = true;
  SDL_IOStream* io = SDL_IOFromConstMem(kControlsAtlasPng, static_cast<size_t>(kControlsAtlasPngSize));
  if (!io) {
    return false;
  }
  SDL_Surface* loaded = SDL_LoadPNG_IO(io, true);
  if (!loaded) {
    return false;
  }
  SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(loaded);
  if (!rgba) {
    return false;
  }
  const uint32_t width = static_cast<uint32_t>(rgba->w);
  const uint32_t height = static_cast<uint32_t>(rgba->h);
  std::vector<uint8_t> pixels(size_t(width) * size_t(height) * 4);
  for (uint32_t row = 0; row < height; ++row) {
    std::memcpy(pixels.data() + size_t(row) * width * 4,
                static_cast<const uint8_t*>(rgba->pixels) + size_t(row) * size_t(rgba->pitch),
                size_t(width) * 4);
  }
  SDL_DestroySurface(rgba);
  atlas_ = drawer->CreateTexture(width, height, rex::ui::ImmediateTextureFilter::kLinear, false,
                                 pixels.data());
  if (!atlas_) {
    return false;
  }
  atlas_failed_ = false;
  return true;
}

void VirtualGamepadOverlay::DrawAndroidStyle(ImGuiIO& io) { (void)io; }

void VirtualGamepadOverlay::OnDraw(ImGuiIO& io) {
  if (!state_->gameplay_active.load() || io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f ||
      TouchStyle() == 2) {
    return;
  }
  ImTextureID atlas{};
  if (TouchStyle() == 1) {
    if (EnsureAtlas()) {
      atlas = reinterpret_cast<ImTextureID>(atlas_.get());
    } else {
      g_android_art_failed.store(true);
    }
  }

  touch::DrawState draw_state;
  {
    std::lock_guard lock(state_->mutex);
    for (const auto& [pointer_id, touch] : state_->touches) {
      (void)pointer_id;
      draw_state.pressed |= touch::ActionBit(touch.control);
    }
    draw_state.left_x = float(state_->left_x) / 32767.0f;
    draw_state.left_y = -float(state_->left_y) / 32767.0f;
    draw_state.right_x = float(state_->right_x) / 32767.0f;
    draw_state.right_y = -float(state_->right_y) / 32767.0f;
  }
  std::lock_guard layout_lock(g_layout_mutex);
  const float aspect = io.DisplaySize.x / io.DisplaySize.y;
  const std::vector<touch::Item>& items = LayoutFor(aspect, UseAndroidLayout());
  touch::Draw(ImGui::GetForegroundDrawList(), io.DisplaySize, items, atlas, TouchAlpha(), draw_state,
              ImGui::GetFont());
}

}  // namespace wwe13
