# 1) SDL: the 2K14 SDK switches OpenGL ES off further down the file, which left our launcher on the slow software
#    renderer. Keep OpenGL ES on for Android.
# 2) Launcher: v0.10 reads the game folder from --game_data_root (the old SDK took it as the first plain argument).
new = {}
def edit(p, a, b):
    s = new.get(p) or open(p).read()
    if b not in s:
        assert s.count(a) == 1, (p, a[:60], s.count(a))
        s = s.replace(a, b)
    new[p] = s
edit("/workspaces/k14/thirdparty/CMakeLists.txt",
     'set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)\n',
     'if(ANDROID)\n'
     '    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)  # WWE13: the launcher draws with OpenGL ES 2\n'
     'else()\n'
     '    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)\n'
     'endif()\n')
edit("/workspaces/wwe13-k14/launcher2/core/launch.cpp",
     "  plan.arguments.push_back(internal::PathToUtf8(game_folder));\n",
     '  plan.arguments.push_back("--game_data_root=" + internal::PathToUtf8(game_folder));  // 2K14 SDK (v0.10)\n')
for p, t in new.items():
    open(p, "w").write(t)
print("fix4 ok")
