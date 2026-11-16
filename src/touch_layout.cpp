// WWE '13 Android port: on-screen controls (see touch_layout.h).

#include "touch_layout.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "controls_atlas_rects.h"

namespace wwe13::touch {
namespace {

constexpr controls_atlas::Rect kArt[] = {
    controls_atlas::kDpad,     controls_atlas::kStrike, controls_atlas::kFinisher, controls_atlas::kGrapple,
    controls_atlas::kReversal, controls_atlas::kAction, controls_atlas::kRun,      controls_atlas::kMenu,
    controls_atlas::kL,        controls_atlas::kR,      controls_atlas::kPause,    controls_atlas::kCamera,
    controls_atlas::kEye,
};

const char* StyleKey(Style style) { return style == Style::kAndroid ? "android" : "xbox"; }

// Android artwork: x in screen-height units, measured from the left (0), the right (1) or the centre (2).
struct ArtDefault {
  const char* id;
  int art;
  int anchor;
  float x, y, half_w;
  Action action;
  bool round;
};
constexpr ArtDefault kAndroidDefaults[] = {
    {"pad", 0, 0, 0.25f, 0.72f, 0.19f, Action::kLeftStick, true},
    {"strike", 1, 1, 0.31f, 0.36f, 0.075f, Action::kX, true},
    {"finisher", 2, 1, 0.125f, 0.36f, 0.075f, Action::kY, true},
    {"grapple", 3, 1, 0.325f, 0.545f, 0.077f, Action::kA, true},
    {"reversal", 4, 1, 0.13f, 0.545f, 0.077f, Action::kRightTrigger, true},
    {"action", 5, 1, 0.42f, 0.73f, 0.066f, Action::kB, true},
    {"run", 6, 1, 0.265f, 0.73f, 0.066f, Action::kLeftTrigger, true},
    {"menu", 7, 1, 0.115f, 0.73f, 0.060f, Action::kBack, true},
    {"l", 8, 2, -0.26f, 0.94f, 0.085f, Action::kLeftShoulder, false},
    {"r", 9, 2, -0.07f, 0.94f, 0.085f, Action::kRightShoulder, false},
    {"pause", 10, 2, 0.085f, 0.94f, 0.038f, Action::kStart, true},
    {"camera", 11, 2, 0.18f, 0.94f, 0.038f, Action::kDpadUp, true},
    {"eye", 12, 2, 0.27f, 0.94f, 0.035f, Action::kDpadDown, true},
};

ImU32 WithAlpha(ImU32 color, int alpha) {
  const int a = int((color >> 24) & 0xFF) * std::clamp(alpha, 0, 255) / 255;
  return (color & 0x00FFFFFFu) | (ImU32(a) << 24);
}

void CenteredText(ImDrawList* draw, ImFont* font, float px, ImVec2 center, ImU32 color, const char* text) {
  if (!font) font = ImGui::GetFont();
  const ImVec2 size = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
  draw->AddText(font, px, ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f), color, text);
}

}  // namespace

std::vector<Item> DefaultLayout(Style style, float aspect) {
  std::vector<Item> items;
  if (style == Style::kAndroid) {
    for (const ArtDefault& d : kAndroidDefaults) {
      const controls_atlas::Rect& rect = kArt[d.art];
      const float xh = d.anchor == 1 ? aspect - d.x : d.anchor == 2 ? aspect * 0.5f + d.x : d.x;
      items.push_back({d.id, Shape::kArt, d.action, "", 0, d.art, d.round, xh / aspect, d.y, d.half_w,
                       d.half_w * float(rect.h) / float(rect.w)});
    }
    return items;
  }
  // Xbox 360 style, laid out like a controller: shoulders on top, sticks, d-pad, face buttons, stick clicks.
  const ImU32 grey = 0;
  items = {
      {"lt", Shape::kPill, Action::kLeftTrigger, "LT", grey, -1, false, 0.054f, 0.20f, 0.088f, 0.058f},
      {"lb", Shape::kPill, Action::kLeftShoulder, "LB", grey, -1, false, 0.181f, 0.20f, 0.088f, 0.058f},
      {"rb", Shape::kPill, Action::kRightShoulder, "RB", grey, -1, false, 0.819f, 0.20f, 0.088f, 0.058f},
      {"rt", Shape::kPill, Action::kRightTrigger, "RT", grey, -1, false, 0.946f, 0.20f, 0.088f, 0.058f},
      {"view", Shape::kPill, Action::kBack, "VIEW", grey, -1, false, 0.441f, 0.08f, 0.088f, 0.048f},
      {"start", Shape::kPill, Action::kStart, "START", grey, -1, false, 0.558f, 0.08f, 0.088f, 0.048f},
      {"ls", Shape::kStick, Action::kLeftStick, "", grey, -1, true, 0.122f, 0.47f, 0.148f, 0.148f},
      {"dpad", Shape::kDpad, Action::kDpadUp, "", grey, -1, true, 0.271f, 0.739f, 0.144f, 0.144f},
      {"rs", Shape::kStick, Action::kRightStick, "", grey, -1, true, 0.729f, 0.739f, 0.148f, 0.148f},
      {"y", Shape::kRound, Action::kY, "Y", IM_COL32(200, 165, 45, 255), -1, true, 0.878f, 0.344f, 0.072f, 0.072f},
      {"x", Shape::kRound, Action::kX, "X", IM_COL32(45, 115, 200, 255), -1, true, 0.821f, 0.47f, 0.072f, 0.072f},
      {"b", Shape::kRound, Action::kB, "B", IM_COL32(205, 62, 52, 255), -1, true, 0.934f, 0.47f, 0.072f, 0.072f},
      {"a", Shape::kRound, Action::kA, "A", IM_COL32(85, 165, 65, 255), -1, true, 0.878f, 0.594f, 0.072f, 0.072f},
      {"l3", Shape::kRound, Action::kLeftThumb, "L3", grey, -1, true, 0.059f, 0.859f, 0.064f, 0.064f},
      {"r3", Shape::kRound, Action::kRightThumb, "R3", grey, -1, true, 0.941f, 0.859f, 0.064f, 0.064f},
  };
  // Narrower screens than the 20:9 this was drawn for: keep everything inside the screen.
  for (Item& item : items) {
    const float margin = item.half_w / aspect;
    item.x = std::clamp(item.x, margin, 1.0f - margin);
  }
  return items;
}

std::vector<Item> LoadLayout(Style style, float aspect, const std::string& file, float scale) {
  std::vector<Item> items = DefaultLayout(style, aspect);
  if (!file.empty()) {
    std::ifstream in(file);
    const std::string prefix = std::string(StyleKey(style)) + ".";
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind(prefix, 0) != 0) continue;
      const size_t equals = line.find('=');
      const size_t comma = line.find(',', equals);
      if (equals == std::string::npos || comma == std::string::npos) continue;
      const std::string id = line.substr(prefix.size(), equals - prefix.size());
      const float x = std::strtof(line.c_str() + equals + 1, nullptr);
      const float y = std::strtof(line.c_str() + comma + 1, nullptr);
      if (!(x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f)) continue;
      for (Item& item : items) {
        if (id == item.id) {
          item.x = x;
          item.y = y;
        }
      }
    }
  }
  if (scale > 0.0f && scale != 1.0f) {
    for (Item& item : items) {
      item.half_w *= scale;
      item.half_h *= scale;
    }
  }
  return items;
}

bool SaveLayout(Style style, const std::vector<Item>& items, const std::string& file) {
  const std::string prefix = std::string(StyleKey(style)) + ".";
  std::ostringstream out;
  {
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.rfind(prefix, 0) != 0) out << line << "\n";
    }
  }
  char buffer[96];
  for (const Item& item : items) {
    std::snprintf(buffer, sizeof(buffer), "%s%s=%.4f,%.4f\n", prefix.c_str(), item.id, item.x, item.y);
    out << buffer;
  }
  const std::string temporary = file + ".tmp";
  {
    std::ofstream file_out(temporary, std::ios::binary | std::ios::trunc);
    file_out << out.str();
    if (!file_out) return false;
  }
  return std::rename(temporary.c_str(), file.c_str()) == 0;
}

Action DpadDirection(const Item& item, float x, float y, float aspect) {
  const float dx = (x - item.x) * aspect;
  const float dy = y - item.y;
  if (std::abs(dx) > std::abs(dy)) return dx < 0.0f ? Action::kDpadLeft : Action::kDpadRight;
  return dy < 0.0f ? Action::kDpadUp : Action::kDpadDown;
}

int HitTest(const std::vector<Item>& items, float x, float y, float aspect, Action* direction) {
  int best = -1;
  float best_distance = FLT_MAX;
  for (size_t i = 0; i < items.size(); ++i) {
    const Item& item = items[i];
    const float dx = (x - item.x) * aspect;  // in screen-height units
    const float dy = y - item.y;
    bool inside;
    if (item.round) {
      inside = std::sqrt(dx * dx + dy * dy) <= item.half_w * 1.08f;
    } else {
      inside = std::abs(dx) <= item.half_w * 1.05f && std::abs(dy) <= item.half_h * 1.25f;
    }
    const float distance = dx * dx + dy * dy;
    if (inside && distance < best_distance) {
      best = int(i);
      best_distance = distance;
    }
  }
  if (best >= 0 && direction) {
    *direction = items[best].shape == Shape::kDpad ? DpadDirection(items[best], x, y, aspect) : items[best].action;
  }
  return best;
}

void StickDeflection(const Item& item, float x, float y, float aspect, float* dx, float* dy) {
  const float reach = item.half_w * 0.62f;
  float ox = (x - item.x) * aspect / reach;
  float oy = (y - item.y) / reach;
  const float length = std::sqrt(ox * ox + oy * oy);
  if (length > 1.0f) {
    ox /= length;
    oy /= length;
  }
  *dx = ox;
  *dy = oy;
}

void Draw(ImDrawList* draw, ImVec2 size, const std::vector<Item>& items, ImTextureID atlas, int alpha,
          const DrawState& state, ImFont* font) {
  const float h = size.y;
  const float line = std::max(1.5f, h * 0.0045f);  // outline thickness
  const ImU32 ring = WithAlpha(IM_COL32(168, 168, 170, 255), alpha);
  const ImU32 fill = WithAlpha(IM_COL32(24, 25, 29, 235), alpha);
  const ImU32 text = WithAlpha(IM_COL32(178, 178, 182, 255), alpha);
  for (size_t i = 0; i < items.size(); ++i) {
    const Item& item = items[i];
    const ImVec2 c(item.x * size.x, item.y * h);
    const float rw = item.half_w * h, rh = item.half_h * h;
    const bool down = item.shape == Shape::kDpad
                          ? (state.pressed & (ActionBit(Action::kDpadUp) | ActionBit(Action::kDpadDown) |
                                              ActionBit(Action::kDpadLeft) | ActionBit(Action::kDpadRight))) != 0
                          : (state.pressed & ActionBit(item.action)) != 0;
    const bool lit = down || int(i) == state.highlight;
    switch (item.shape) {
      case Shape::kArt: {
        if (!atlas || item.art < 0) break;
        const controls_atlas::Rect& rect = kArt[item.art];
        const float s = down ? 0.93f : 1.0f;
        const ImVec2 uv0(float(rect.x) / controls_atlas::kWidth, float(rect.y) / controls_atlas::kHeight);
        const ImVec2 uv1(float(rect.x + rect.w) / controls_atlas::kWidth, float(rect.y + rect.h) / controls_atlas::kHeight);
        draw->AddImage(atlas, ImVec2(c.x - rw * s, c.y - rh * s), ImVec2(c.x + rw * s, c.y + rh * s), uv0, uv1,
                       lit ? IM_COL32(190, 215, 255, 225) : IM_COL32(255, 255, 255, std::clamp(alpha, 0, 255)));
        if (item.action == Action::kLeftStick && down) {
          const ImVec2 knob(c.x + state.left_x * rw * 0.62f, c.y + state.left_y * rw * 0.62f);
          draw->AddCircleFilled(knob, rw * 0.34f, IM_COL32(255, 255, 255, 70), 40);
          draw->AddCircle(knob, rw * 0.34f, IM_COL32(255, 255, 255, 190), 40, 2.0f);
        }
        break;
      }
      case Shape::kPill: {
        const float round = rh * 0.62f;
        const ImVec2 p0(c.x - rw, c.y - rh), p1(c.x + rw, c.y + rh);
        draw->AddRectFilled(p0, p1, lit ? WithAlpha(IM_COL32(70, 72, 80, 245), alpha) : fill, round);
        draw->AddRect(p0, p1, ring, round, 0, line * 2.0f);
        const float inset = line * 3.2f;
        draw->AddRect(ImVec2(p0.x + inset, p0.y + inset), ImVec2(p1.x - inset, p1.y - inset), ring,
                      std::max(1.0f, round - inset), 0, line * 1.2f);
        CenteredText(draw, font, rh * 0.62f, c, lit ? WithAlpha(IM_COL32(255, 255, 255, 255), alpha) : text,
                     item.label);
        break;
      }
      case Shape::kRound: {
        const ImU32 accent = item.accent ? WithAlpha(item.accent, alpha) : ring;
        draw->AddCircleFilled(c, rw, lit ? WithAlpha(item.accent ? (item.accent & 0x00FFFFFFu) | 0x80000000u
                                                                 : IM_COL32(70, 72, 80, 245),
                                                     alpha)
                                         : fill, 48);
        draw->AddCircle(c, rw, ring, 48, line * 2.0f);
        draw->AddCircle(c, rw - line * 3.6f, accent, 48, line * 1.8f);
        CenteredText(draw, font, rw * (item.label[1] ? 0.62f : 0.95f), c,
                     item.accent ? accent : (lit ? WithAlpha(IM_COL32(255, 255, 255, 255), alpha) : text), item.label);
        break;
      }
      case Shape::kStick: {
        const float kx = item.action == Action::kRightStick ? state.right_x : state.left_x;
        const float ky = item.action == Action::kRightStick ? state.right_y : state.left_y;
        draw->AddCircleFilled(c, rw, WithAlpha(IM_COL32(52, 53, 57, 225), alpha), 64);
        draw->AddCircle(c, rw, ring, 64, line * 2.0f);
        draw->AddCircle(c, rw - line * 3.6f, WithAlpha(IM_COL32(120, 120, 124, 255), alpha), 64, line);
        const ImVec2 knob(c.x + (down ? kx : 0.0f) * rw * 0.62f, c.y + (down ? ky : 0.0f) * rw * 0.62f);
        draw->AddCircleFilled(knob, rw * 0.42f, WithAlpha(lit ? IM_COL32(150, 152, 158, 255) : IM_COL32(118, 119, 123, 245), alpha), 48);
        draw->AddCircle(knob, rw * 0.42f, WithAlpha(IM_COL32(205, 205, 208, 255), alpha), 48, line * 1.6f);
        break;
      }
      case Shape::kDpad: {
        draw->AddCircle(c, rw, ring, 64, line * 2.0f);
        const float arm = rw * 0.30f;    // half the width of a cross arm
        const float reach = rw * 0.92f;  // tip of an arm
        const ImU32 cell = WithAlpha(IM_COL32(17, 17, 19, 240), alpha);
        const ImU32 cell_down = WithAlpha(IM_COL32(95, 97, 105, 250), alpha);
        struct Arm { Action action; ImVec2 p0, p1; };
        const Arm arms[] = {
            {Action::kDpadUp, ImVec2(c.x - arm, c.y - reach), ImVec2(c.x + arm, c.y - arm)},
            {Action::kDpadDown, ImVec2(c.x - arm, c.y + arm), ImVec2(c.x + arm, c.y + reach)},
            {Action::kDpadLeft, ImVec2(c.x - reach, c.y - arm), ImVec2(c.x - arm, c.y + arm)},
            {Action::kDpadRight, ImVec2(c.x + arm, c.y - arm), ImVec2(c.x + reach, c.y + arm)},
        };
        draw->AddRectFilled(ImVec2(c.x - arm, c.y - arm), ImVec2(c.x + arm, c.y + arm), cell);
        for (const Arm& a : arms) {
          const bool pressed = (state.pressed & ActionBit(a.action)) != 0 || int(i) == state.highlight;
          draw->AddRectFilled(a.p0, a.p1, pressed ? cell_down : cell);
          draw->AddRect(a.p0, a.p1, ring, 0.0f, 0, line * 1.3f);
        }
        break;
      }
    }
  }
}

}  // namespace wwe13::touch
