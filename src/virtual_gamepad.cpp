#include <cfloat>
#include <chrono>
#include <cstdio>
#include "virtual_gamepad.h"

#include <cstdlib>
#include <vector>

#include <SDL3/SDL.h>
#include <rex/ui/imgui_drawer.h>

#include "controls_atlas_rects.h"
#include "launcher_embedded_images.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include <imgui.h>
#include <rex/input/input.h>
#include <rex/ui/window.h>

namespace wwe13 {
namespace {

using Control = VirtualGamepadState::Control;

// Touch style: "android" (artwork buttons, the default) or "xbox" (plain A/B/X/Y), from WWE13_TOUCH_STYLE.
// 0 = xbox, 1 = android, 2 = none (a real gamepad is used; nothing is drawn or touched).
int TouchStyle() {
  static const int style = [] {
    const char* value = std::getenv("WWE13_TOUCH_STYLE");
    if (value && std::strcmp(value, "xbox") == 0) return 0;
    if (value && std::strcmp(value, "none") == 0) return 2;
    return 1;
  }();
  return style;
}

bool AndroidStyle() { return TouchStyle() == 1; }

// The virtual pad only counts as a connected controller while it is shown.
bool PadActive(const VirtualGamepadState& state) {
  return state.gameplay_active.load() && TouchStyle() != 2;
}

// Screen width / height, updated on every touch and draw (the Android layout is in screen-height units).
float g_aspect = 16.0f / 9.0f;

// Android layout. All sizes are in units of the screen height, so buttons stay round on any screen.
// anchor: 0 = x from the left edge, 1 = x from the right edge, 2 = x offset from the centre.
struct PadItem {
  controls_atlas::Rect rect;
  int anchor;
  float x, y, half_width;
  Control control;
  bool round;
};
constexpr float kPadStickX = 0.25f;
constexpr float kPadStickY = 0.72f;
constexpr float kPadStickRadius = 0.19f;
constexpr float kPadStickThrow = kPadStickRadius * 0.62f;
constexpr PadItem kPadItems[] = {
    {controls_atlas::kStrike, 1, 0.31f, 0.36f, 0.075f, Control::kX, true},
    {controls_atlas::kFinisher, 1, 0.125f, 0.36f, 0.075f, Control::kY, true},
    {controls_atlas::kGrapple, 1, 0.325f, 0.545f, 0.077f, Control::kA, true},
    {controls_atlas::kReversal, 1, 0.13f, 0.545f, 0.077f, Control::kRightTrigger, true},
    {controls_atlas::kAction, 1, 0.42f, 0.73f, 0.066f, Control::kB, true},
    {controls_atlas::kRun, 1, 0.265f, 0.73f, 0.066f, Control::kLeftTrigger, true},
    {controls_atlas::kMenu, 1, 0.115f, 0.73f, 0.060f, Control::kBack, true},
    {controls_atlas::kL, 2, -0.26f, 0.94f, 0.085f, Control::kLeftShoulder, false},
    {controls_atlas::kR, 2, -0.07f, 0.94f, 0.085f, Control::kRightShoulder, false},
    {controls_atlas::kPause, 2, 0.085f, 0.94f, 0.038f, Control::kStart, true},
    {controls_atlas::kCamera, 2, 0.18f, 0.94f, 0.038f, Control::kDpadUp, true},
    {controls_atlas::kEye, 2, 0.27f, 0.94f, 0.035f, Control::kDpadDown, true},
};

// The item's centre x in screen-height units.
float PadItemX(const PadItem& item, float aspect) {
  switch (item.anchor) {
    case 1: return aspect - item.x;
    case 2: return aspect * 0.5f + item.x;
    default: return item.x;
  }
}

float PadItemHalfHeight(const PadItem& item) {
  return item.half_width * float(item.rect.h) / float(item.rect.w);
}

// x and y are 0..1 across the screen.
Control HitTestAndroid(float x, float y, float aspect) {
  const float xh = x * aspect;
  {
    const float dx = xh - kPadStickX;
    const float dy = y - kPadStickY;
    if (std::sqrt(dx * dx + dy * dy) <= kPadStickRadius) {
      return Control::kLeftStick;
    }
  }
  for (const PadItem& item : kPadItems) {
    const float dx = xh - PadItemX(item, aspect);
    const float dy = y - item.y;
    if (item.round) {
      if (std::sqrt(dx * dx + dy * dy) <= item.half_width * 1.08f) {
        return item.control;
      }
    } else if (std::abs(dx) <= item.half_width && std::abs(dy) <= PadItemHalfHeight(item) * 1.25f) {
      return item.control;
    }
  }
  return Control::kNone;
}

constexpr float kLeftStickX = 0.19f;
constexpr float kLeftStickY = 0.76f;
constexpr float kRightStickX = 0.76f;
constexpr float kRightStickY = 0.76f;
constexpr float kStickRadius = 0.13f;

float Distance(float x1, float y1, float x2, float y2) {
  const float dx = x1 - x2;
  const float dy = y1 - y2;
  return std::sqrt(dx * dx + dy * dy);
}

int16_t StickAxis(float value) {
  value = std::clamp(value, -1.0f, 1.0f);
  return static_cast<int16_t>(value * 32767.0f);
}

Control HitTest(float x, float y) {
  if (Distance(x, y, kLeftStickX, kLeftStickY) <= kStickRadius) {
    return Control::kLeftStick;
  }
  if (Distance(x, y, kRightStickX, kRightStickY) <= kStickRadius) {
    return Control::kRightStick;
  }

  // D-pad: four independently tracked directions.
  if (x < 0.17f && y > 0.35f && y < 0.63f) {
    const float dx = x - 0.085f;
    const float dy = y - 0.49f;
    if (std::abs(dx) > std::abs(dy)) {
      return dx < 0 ? Control::kDpadLeft : Control::kDpadRight;
    }
    return dy < 0 ? Control::kDpadUp : Control::kDpadDown;
  }

  // ABXY cluster.
  if (Distance(x, y, 0.94f, 0.50f) < 0.055f) return Control::kA;
  if (Distance(x, y, 0.875f, 0.58f) < 0.055f) return Control::kB;
  if (Distance(x, y, 0.875f, 0.42f) < 0.055f) return Control::kX;
  if (Distance(x, y, 0.81f, 0.50f) < 0.055f) return Control::kY;

  // Shoulder buttons and triggers.
  if (x < 0.25f && y < 0.17f) return Control::kLeftShoulder;
  if (x > 0.75f && y < 0.17f) return Control::kRightShoulder;
  if (x < 0.25f && y >= 0.17f && y < 0.30f) return Control::kLeftTrigger;
  if (x > 0.75f && y >= 0.17f && y < 0.30f) return Control::kRightTrigger;

  // Back and Start.
  if (x > 0.36f && x < 0.48f && y > 0.86f) return Control::kBack;
  if (x >= 0.52f && x < 0.64f && y > 0.86f) return Control::kStart;

  return Control::kNone;
}

void RebuildState(VirtualGamepadState& state) {
  state.buttons = 0;
  state.left_trigger = 0;
  state.right_trigger = 0;
  state.left_x = 0;
  state.left_y = 0;
  state.right_x = 0;
  state.right_y = 0;

  for (const auto& [pointer_id, touch] : state.touches) {
    (void)pointer_id;
    switch (touch.control) {
      case Control::kLeftStick: {
        const bool pad_style = AndroidStyle();
        const float dx = pad_style ? (touch.x * g_aspect - kPadStickX) / kPadStickThrow
                                   : (touch.x - kLeftStickX) / kStickRadius;
        const float dy = pad_style ? (touch.y - kPadStickY) / kPadStickThrow
                                   : (touch.y - kLeftStickY) / kStickRadius;
        const float magnitude = std::sqrt(dx * dx + dy * dy);
        const float scale = magnitude > 1.0f ? 1.0f / magnitude : 1.0f;
        state.left_x = StickAxis(dx * scale);
        state.left_y = StickAxis(-dy * scale);
        break;
      }
      case Control::kRightStick: {
        const float dx = (touch.x - kRightStickX) / kStickRadius;
        const float dy = (touch.y - kRightStickY) / kStickRadius;
        const float magnitude = std::sqrt(dx * dx + dy * dy);
        const float scale = magnitude > 1.0f ? 1.0f / magnitude : 1.0f;
        state.right_x = StickAxis(dx * scale);
        state.right_y = StickAxis(-dy * scale);
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
      case Control::kLeftShoulder:
        state.buttons |= rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER;
        break;
      case Control::kRightShoulder:
        state.buttons |= rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER;
        break;
      case Control::kLeftTrigger: state.left_trigger = 255; break;
      case Control::kRightTrigger: state.right_trigger = 255; break;
      case Control::kStart: state.buttons |= rex::input::X_INPUT_GAMEPAD_START; break;
      case Control::kBack: state.buttons |= rex::input::X_INPUT_GAMEPAD_BACK; break;
      case Control::kNone: break;
    }
  }
}

void DrawButton(ImDrawList* draw, ImVec2 center, float radius,
                const char* label, ImU32 color, bool pressed) {
  const float offset = pressed ? 1.5f : 0.0f;
  center.y += offset;

  // Deep shadow and metallic outer rim.
  draw->AddCircleFilled(ImVec2(center.x, center.y + 3.0f), radius + 2.0f,
                        IM_COL32(0, 0, 0, 190), 40);
  draw->AddCircleFilled(center, radius,
                        pressed ? IM_COL32(170, 190, 215, 245)
                                : IM_COL32(190, 205, 225, 225), 40);
  draw->AddCircleFilled(center, radius - 2.5f,
                        IM_COL32(13, 18, 27, 245), 40);

  // Raised button face with a subtle inner highlight.
  draw->AddCircleFilled(center, radius - 5.0f,
                        pressed ? IM_COL32(75, 105, 145, 250)
                                : color, 40);
  draw->AddCircle(center, radius - 5.0f,
                  IM_COL32(225, 235, 250, pressed ? 240 : 155), 40, 1.5f);
  draw->AddCircle(center, radius - 9.0f,
                  IM_COL32(255, 255, 255, pressed ? 95 : 45), 40, 1.0f);

  const float font_size = std::max(ImGui::GetFontSize() * 1.35f,
                                   radius * 0.58f);
  const ImVec2 text_size = ImGui::CalcTextSize(label);
  const float scale = font_size / ImGui::GetFontSize();
  const ImVec2 scaled_size(text_size.x * scale, text_size.y * scale);
  const ImVec2 text_pos(center.x - scaled_size.x * 0.5f,
                        center.y - scaled_size.y * 0.5f);

  draw->AddText(ImGui::GetFont(), font_size,
                ImVec2(text_pos.x + 1.0f, text_pos.y + 2.0f),
                IM_COL32(0, 0, 0, 210), label);
  draw->AddText(ImGui::GetFont(), font_size, text_pos,
                IM_COL32(255, 255, 255, 250), label);
}

static ImVec2 g_knob(0.0f, 0.0f);

void DrawStick(ImDrawList* draw, ImVec2 center, float radius,
               const char* label, bool pressed) {
  draw->AddCircleFilled(ImVec2(center.x, center.y + 4.0f), radius + 3.0f,
                        IM_COL32(0, 0, 0, 165), 48);
  draw->AddCircleFilled(center, radius,
                        IM_COL32(165, 180, 200, 210), 48);
  draw->AddCircleFilled(center, radius - 3.0f,
                        IM_COL32(16, 22, 32, 235), 48);
  draw->AddCircle(center, radius - 3.0f,
                  IM_COL32(240, 245, 255, 190), 48, 2.0f);

  const ImVec2 knob(center.x + g_knob.x, center.y + g_knob.y);
  const float knob_radius = radius * 0.43f;
  draw->AddCircleFilled(ImVec2(knob.x, knob.y + 3.0f), knob_radius,
                        IM_COL32(0, 0, 0, 180), 40);
  draw->AddCircleFilled(knob, knob_radius,
                        pressed ? IM_COL32(145, 175, 215, 250)
                                : IM_COL32(75, 90, 112, 245), 40);
  draw->AddCircle(knob, knob_radius,
                  IM_COL32(255, 255, 255, 220), 40, 2.0f);
  draw->AddCircle(knob, knob_radius - 5.0f,
                  IM_COL32(255, 255, 255, 65), 40, 1.0f);

  const float font_size = ImGui::GetFontSize() * 1.25f;
  const ImVec2 text_size = ImGui::CalcTextSize(label);
  const float scale = font_size / ImGui::GetFontSize();
  draw->AddText(ImGui::GetFont(), font_size,
                ImVec2(center.x - text_size.x * scale * 0.5f,
                       center.y + radius + 5.0f),
                IM_COL32(255, 255, 255, 240), label);
}


void DrawStick(ImDrawList* draw, ImVec2 center, float radius,
               const char* label) {
  draw->AddCircleFilled(center, radius, IM_COL32(25, 28, 34, 115), 40);
  draw->AddCircle(center, radius, IM_COL32(235, 235, 235, 150), 40, 2.0f);
  draw->AddCircleFilled(ImVec2(center.x + g_knob.x, center.y + g_knob.y), radius * 0.40f, IM_COL32(100, 110, 125, 190), 32);
  draw->AddCircle(ImVec2(center.x + g_knob.x, center.y + g_knob.y), radius * 0.40f, IM_COL32(255, 255, 255, 180), 32, 2.0f);
  const ImVec2 text_size = ImGui::CalcTextSize(label);
  draw->AddText(ImVec2(center.x - text_size.x * 0.5f,
                       center.y + radius + 4.0f),
                IM_COL32(255, 255, 255, 210), label);
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
      const Control control = AndroidStyle() ? HitTestAndroid(x, y, g_aspect) : HitTest(x, y);
      if (control == Control::kNone) return;
      state_->touches[pointer_id] = {control, x, y};
      event.set_handled(true);
      break;
    }
    case rex::ui::TouchEvent::Action::kMove:
      if (it == state_->touches.end()) return;
      it->second.x = x;
      it->second.y = y;
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

void VirtualGamepadOverlay::DrawAndroidStyle(ImGuiIO& io) {
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const float h = io.DisplaySize.y;
  const float aspect = io.DisplaySize.x / h;
  g_aspect = aspect;
  const ImTextureID texture = reinterpret_cast<ImTextureID>(atlas_.get());
  const float atlas_w = float(controls_atlas::kWidth);
  const float atlas_h = float(controls_atlas::kHeight);

  uint32_t pressed = 0;
  int16_t stick_x = 0, stick_y = 0;
  {
    std::lock_guard lock(state_->mutex);
    for (const auto& [pointer_id, touch] : state_->touches) {
      (void)pointer_id;
      pressed |= uint32_t(1) << uint32_t(touch.control);
    }
    stick_x = state_->left_x;
    stick_y = state_->left_y;
  }
  const auto is_pressed = [pressed](Control control) {
    return (pressed & (uint32_t(1) << uint32_t(control))) != 0;
  };
  const auto draw_sprite = [&](const controls_atlas::Rect& rect, float cx, float cy, float half_w,
                               float half_h, bool down) {
    const float scale = down ? 0.93f : 1.0f;
    const ImVec2 p0((cx - half_w * scale) * h, (cy - half_h * scale) * h);
    const ImVec2 p1((cx + half_w * scale) * h, (cy + half_h * scale) * h);
    const ImVec2 uv0(float(rect.x) / atlas_w, float(rect.y) / atlas_h);
    const ImVec2 uv1(float(rect.x + rect.w) / atlas_w, float(rect.y + rect.h) / atlas_h);
    draw->AddImage(texture, p0, p1, uv0, uv1,
                   down ? IM_COL32(190, 215, 255, 215) : IM_COL32(255, 255, 255, 118));
  };

  // Movement pad (left stick), with a soft marker that follows the thumb.
  const controls_atlas::Rect& pad = controls_atlas::kDpad;
  draw_sprite(pad, kPadStickX, kPadStickY, kPadStickRadius,
              kPadStickRadius * float(pad.h) / float(pad.w), false);
  if (is_pressed(Control::kLeftStick)) {
    const ImVec2 knob((kPadStickX + kPadStickThrow * float(stick_x) / 32767.0f) * h,
                      (kPadStickY - kPadStickThrow * float(stick_y) / 32767.0f) * h);
    draw->AddCircleFilled(knob, kPadStickRadius * 0.34f * h, IM_COL32(255, 255, 255, 70), 40);
    draw->AddCircle(knob, kPadStickRadius * 0.34f * h, IM_COL32(255, 255, 255, 190), 40, 2.0f);
  }

  for (const PadItem& item : kPadItems) {
    draw_sprite(item.rect, PadItemX(item, aspect), item.y, item.half_width, PadItemHalfHeight(item),
                is_pressed(item.control));
  }
}

VirtualGamepadOverlay::VirtualGamepadOverlay(
    rex::ui::ImGuiDrawer* drawer,
    std::shared_ptr<VirtualGamepadState> state)
    : ImGuiDialog(drawer), state_(std::move(state)) {}

void VirtualGamepadOverlay::OnDraw(ImGuiIO& io) {
  if (!state_->gameplay_active.load() || io.DisplaySize.x <= 0.0f ||
      io.DisplaySize.y <= 0.0f) {
    return;
  }

  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const float w = io.DisplaySize.x;
  const float h = io.DisplaySize.y;
  const auto point = [w, h](float x, float y) {
    return ImVec2(w * x, h * y);
  };

  {
    using clk = std::chrono::steady_clock;
    static auto fps_t0 = clk::now();
    static int fps_frames = 0;
    static float fps_value = 0.0f;
    ++fps_frames;
    const float fps_secs = std::chrono::duration<float>(clk::now() - fps_t0).count();
    if (fps_secs >= 0.5f) {
      fps_value = fps_frames / fps_secs;
      fps_frames = 0;
      fps_t0 = clk::now();
    }
    char fps_text[40];
    std::snprintf(fps_text, sizeof(fps_text), "%.0f FPS  %.1f ms", fps_value,
                  fps_value > 0.0f ? 1000.0f / fps_value : 0.0f);
    const float fps_size = h * 0.035f;
    const ImVec2 fps_sz = ImGui::GetFont()->CalcTextSizeA(fps_size, FLT_MAX, 0.0f, fps_text);
    const ImVec2 fps_pos(w * 0.5f - fps_sz.x * 0.5f, h * 0.012f);
    draw->AddRectFilled(ImVec2(fps_pos.x - 8.0f, fps_pos.y - 3.0f),
                        ImVec2(fps_pos.x + fps_sz.x + 8.0f, fps_pos.y + fps_sz.y + 3.0f),
                        IM_COL32(0, 0, 0, 150), 6.0f);
    draw->AddText(ImGui::GetFont(), fps_size, fps_pos, IM_COL32(120, 255, 160, 255), fps_text);
  }
  if (TouchStyle() == 2) {
    return;  // no on-screen controls
  }
  if (AndroidStyle() && EnsureAtlas()) {
    DrawAndroidStyle(io);
    return;
  }

  const ImU32 button_color = IM_COL32(43, 57, 78, 235);
  const ImU32 utility_color = IM_COL32(37, 48, 65, 225);

  bool left_pressed = false;
  bool right_pressed = false;
  bool a_pressed = false, b_pressed = false;
  bool x_pressed = false, y_pressed = false;
  bool up_pressed = false, down_pressed = false;
  bool left_dpad_pressed = false, right_dpad_pressed = false;
  bool lb_pressed = false, rb_pressed = false;
  bool lt_pressed = false, rt_pressed = false;
  bool back_pressed = false, start_pressed = false;

  {
    std::lock_guard lock(state_->mutex);
    for (const auto& [pointer_id, touch] : state_->touches) {
      (void)pointer_id;
      switch (touch.control) {
        case Control::kLeftStick: left_pressed = true; break;
        case Control::kRightStick: right_pressed = true; break;
        case Control::kA: a_pressed = true; break;
        case Control::kB: b_pressed = true; break;
        case Control::kX: x_pressed = true; break;
        case Control::kY: y_pressed = true; break;
        case Control::kDpadUp: up_pressed = true; break;
        case Control::kDpadDown: down_pressed = true; break;
        case Control::kDpadLeft: left_dpad_pressed = true; break;
        case Control::kDpadRight: right_dpad_pressed = true; break;
        case Control::kLeftShoulder: lb_pressed = true; break;
        case Control::kRightShoulder: rb_pressed = true; break;
        case Control::kLeftTrigger: lt_pressed = true; break;
        case Control::kRightTrigger: rt_pressed = true; break;
        case Control::kBack: back_pressed = true; break;
        case Control::kStart: start_pressed = true; break;
        case Control::kNone: break;
      }
    }
  }

  g_knob = ImVec2(h * kStickRadius * 0.6f * state_->left_x / 32767.0f,
                  -h * kStickRadius * 0.6f * state_->left_y / 32767.0f);
  DrawStick(draw, point(kLeftStickX, kLeftStickY),
            h * kStickRadius, "L", left_pressed);

  g_knob = ImVec2(h * kStickRadius * 0.6f * state_->right_x / 32767.0f,
                  -h * kStickRadius * 0.6f * state_->right_y / 32767.0f);
  DrawStick(draw, point(kRightStickX, kRightStickY),
            h * kStickRadius, "R", right_pressed);

  DrawButton(draw, point(0.085f, 0.42f), h * 0.055f, "^",
             utility_color, up_pressed);
  DrawButton(draw, point(0.085f, 0.56f), h * 0.055f, "v",
             utility_color, down_pressed);
  DrawButton(draw, point(0.035f, 0.49f), h * 0.055f, "<",
             utility_color, left_dpad_pressed);
  DrawButton(draw, point(0.135f, 0.49f), h * 0.055f, ">",
             utility_color, right_dpad_pressed);

  DrawButton(draw, point(0.94f, 0.50f), h * 0.065f, "A",
             button_color, a_pressed);
  DrawButton(draw, point(0.875f, 0.58f), h * 0.065f, "B",
             button_color, b_pressed);
  DrawButton(draw, point(0.875f, 0.42f), h * 0.065f, "X",
             button_color, x_pressed);
  DrawButton(draw, point(0.81f, 0.50f), h * 0.065f, "Y",
             button_color, y_pressed);

  DrawButton(draw, point(0.12f, 0.10f), h * 0.055f, "LB",
             utility_color, lb_pressed);
  DrawButton(draw, point(0.88f, 0.10f), h * 0.055f, "RB",
             utility_color, rb_pressed);
  DrawButton(draw, point(0.12f, 0.235f), h * 0.055f, "LT",
             utility_color, lt_pressed);
  DrawButton(draw, point(0.88f, 0.235f), h * 0.055f, "RT",
             utility_color, rt_pressed);

  DrawButton(draw, point(0.42f, 0.91f), h * 0.035f, "BACK",
             utility_color, back_pressed);
  DrawButton(draw, point(0.58f, 0.91f), h * 0.035f, "START",
             utility_color, start_pressed);
}

}  // namespace wwe13
