# Touch on the 2K14 SDK: its SDL window passes mouse, keys and text on to the game but drops finger events, so the touch
# controls never heard a touch. Pass fingers on as touch events (what our touch controls and the in-game ImGui read),
# and stop SDL from also turning them into mouse clicks once the game runs (the keyboard/mouse driver would read them).
K, G = "/workspaces/k14", "/workspaces/wwe13-k14"
new = {}
def edit(p, a, b):
    s = new.get(p) or open(p).read()
    if b not in s:
        assert s.count(a) == 1, (p, a[:60], s.count(a))
        s = s.replace(a, b)
    new[p] = s
edit(K + "/include/rex/ui/window_sdl.h", "  void HandleMouseEvent(SDL_Event& event);\n",
     "  void HandleMouseEvent(SDL_Event& event);\n  void HandleTouchEvent(SDL_Event& event);  // WWE13 (Android touch controls)\n")
edit(K + "/src/ui/window_sdl.cpp", "    default:\n      break;\n  }\n}\n\n}  // namespace rex::ui",
     "    default:\n      break;\n  }\n}\n\n"
     "void WindowSDL::HandleTouchEvent(SDL_Event& event) {\n"
     "  // WWE13: fingers reach the touch controls and ImGui as touch events, in physical pixels.\n"
     "  TouchEvent::Action action;\n"
     "  switch (event.type) {\n"
     "    case SDL_EVENT_FINGER_DOWN: action = TouchEvent::Action::kDown; break;\n"
     "    case SDL_EVENT_FINGER_UP: action = TouchEvent::Action::kUp; break;\n"
     "    case SDL_EVENT_FINGER_MOTION: action = TouchEvent::Action::kMove; break;\n"
     "    case SDL_EVENT_FINGER_CANCELED: action = TouchEvent::Action::kCancel; break;\n"
     "    default: return;\n"
     "  }\n"
     "  WindowDestructionReceiver destruction_receiver(this);\n"
     "  TouchEvent e(this, uint32_t(event.tfinger.fingerID), action,\n"
     "               event.tfinger.x * float(GetActualPhysicalWidth()),\n"
     "               event.tfinger.y * float(GetActualPhysicalHeight()));\n"
     "  OnTouchEvent(e, destruction_receiver);\n"
     "}\n\n}  // namespace rex::ui")
edit(K + "/src/ui/windowed_app_context_sdl.cpp", "    case SDL_EVENT_DROP_FILE: {\n",
     "    case SDL_EVENT_FINGER_DOWN:\n    case SDL_EVENT_FINGER_UP:\n"
     "    case SDL_EVENT_FINGER_MOTION:\n    case SDL_EVENT_FINGER_CANCELED: {\n"
     "      if (WindowSDL* window = GetWindow(event.tfinger.windowID)) {\n"
     "        window->HandleTouchEvent(event);\n      }\n      break;\n    }\n"
     "    case SDL_EVENT_DROP_FILE: {\n")
edit(K + "/src/ui/windowed_app_main_sdl.cpp",
     "  int result = RunWindowedApp(argc, argv);\n  if (sdl_held)",
     "  // The launcher above taps through SDL's touch-as-mouse; the game reads fingers as touch events instead.\n"
     "  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, \"0\");\n"
     "  int result = RunWindowedApp(argc, argv);\n  if (sdl_held)")
# A device id of its own (SDL numbers its pads from 1, which the touch pad used).
edit(G + "/src/virtual_gamepad.cpp", "constexpr rex::input::DeviceId kTouchPad{1};",
     "constexpr rex::input::DeviceId kTouchPad{0x544F5543};  // 'TOUC'")
for p, t in new.items():
    open(p, "w").write(t)
print("fix6 ok: touch events")
