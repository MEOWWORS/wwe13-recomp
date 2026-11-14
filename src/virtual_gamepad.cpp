#include <cfloat>
#include <chrono>
#include <cstdio>
#include "virtual_gamepad.h"

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
        const float dx = (touch.x - kLeftStickX) / kStickRadius;
        const float dy = (touch.y - kLeftStickY) / kStickRadius;
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
  if (user_index != 0 || !state_->gameplay_active.load()) {
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
  if (user_index != 0 || !state_->gameplay_active.load()) {
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
  return user_index == 0 && state_->gameplay_active.load()
             ? static_cast<rex::X_RESULT>(0)
             : static_cast<rex::X_RESULT>(0x0000048F);
}

rex::X_RESULT VirtualGamepadInputDriver::GetKeystroke(
    uint32_t user_index, uint32_t flags,
    rex::input::X_INPUT_KEYSTROKE* out_keystroke) {
  (void)flags;
  (void)out_keystroke;
  return user_index == 0 && state_->gameplay_active.load()
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
  if (!state_->gameplay_active.load() || !attached_window_) return;

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
      const Control control = HitTest(x, y);
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
