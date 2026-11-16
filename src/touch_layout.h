// WWE '13 Android port: on-screen controls shared by the game (virtual_gamepad.cpp) and the launcher's layout
// editor. Two styles: Xbox 360 (drawn buttons) and Android (the artwork in controls_atlas.png). Positions are
// fractions of the screen (x of its width, y of its height), so a layout fits any phone; sizes are in units of the
// screen height, so round buttons stay round. Moved controls are kept in a small text file (WWE13_TOUCH_LAYOUT).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

namespace wwe13::touch {

enum class Style { kXbox, kAndroid };

// What a control sends (same order as before; the thumb-stick clicks were added at the end).
enum class Action : uint8_t {
  kNone,
  kLeftStick,
  kRightStick,
  kA, kB, kX, kY,
  kDpadUp, kDpadDown, kDpadLeft, kDpadRight,
  kLeftShoulder, kRightShoulder,
  kLeftTrigger, kRightTrigger,
  kStart, kBack,
  kLeftThumb, kRightThumb,
};

enum class Shape : uint8_t { kStick, kDpad, kRound, kPill, kArt };

struct Item {
  const char* id;     // key in the layout file
  Shape shape;
  Action action;      // for a d-pad: kDpadUp (the direction comes from the touch)
  const char* label;  // Xbox style text
  ImU32 accent;       // Xbox style ring and letter colour (0 = grey)
  int art;            // Android artwork index, -1 = none
  bool round;         // hit area: circle (else rectangle)
  float x, y;         // centre, 0..1 of the screen width / height
  float half_w, half_h;
};

// The default layout for a screen of this width / height.
std::vector<Item> DefaultLayout(Style style, float aspect);
// The layout the player arranged (defaults for controls never moved), at the given size (1 = 100%).
std::vector<Item> LoadLayout(Style style, float aspect, const std::string& file, float scale);
// Saves the positions of this style; the other style's lines in the file stay.
bool SaveLayout(Style style, const std::vector<Item>& items, const std::string& file);

// The control under a touch (x, y in 0..1); -1 when none. For a d-pad, *direction gets the pressed direction.
int HitTest(const std::vector<Item>& items, float x, float y, float aspect, Action* direction);
Action DpadDirection(const Item& item, float x, float y, float aspect);
// How far a stick is pushed, -1..1 each way (y down), for a touch at (x, y).
void StickDeflection(const Item& item, float x, float y, float aspect, float* dx, float* dy);

struct DrawState {
  uint64_t pressed = 0;  // bit per Action
  float left_x = 0.0f, left_y = 0.0f, right_x = 0.0f, right_y = 0.0f;  // stick knobs, -1..1 (y down)
  int highlight = -1;    // editor: the control being dragged
};
inline uint64_t ActionBit(Action action) { return uint64_t(1) << uint32_t(action); }

// Draws the controls over a screen of this size. atlas: the Android artwork (needed for kArt items).
// alpha 0..255 sets how see-through they are.
void Draw(ImDrawList* draw, ImVec2 size, const std::vector<Item>& items, ImTextureID atlas, int alpha,
          const DrawState& state, ImFont* font);

}  // namespace wwe13::touch
