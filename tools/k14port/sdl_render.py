# The 2K14 SDK builds SDL without its 2D renderer (the game draws with Vulkan directly), but our launcher draws its
# menus with SDL's OpenGL ES 2 renderer. Turn the renderer on for Android builds only.
p = "/workspaces/k14/thirdparty/CMakeLists.txt"
s = open(p).read()
a = 'set(SDL_RENDER OFF CACHE BOOL "" FORCE)\n'
b = ('if(ANDROID)\n'
     '    # WWE13: the in-app launcher draws its menus with SDL\'s renderer (OpenGL ES 2).\n'
     '    set(SDL_RENDER ON CACHE BOOL "" FORCE)\n'
     '    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)\n'
     'else()\n'
     '    set(SDL_RENDER OFF CACHE BOOL "" FORCE)\n'
     'endif()\n')
if b not in s:
    assert s.count(a) == 1, ("SDL_RENDER line", s.count(a))
    s = s.replace(a, b)
    open(p, "w").write(s)
print("sdl renderer: on for Android")
