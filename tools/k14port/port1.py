import os, shutil, sys
K, OLD = sys.argv[1], sys.argv[2]  # K: new SDK (k14), OLD: our v0.7.3 fork with the pieces to bring over
new = {}
def load(p):
    return new[p] if p in new else open(p).read()
def edit(p, pairs):
    s = load(p)
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:70], s.count(a))
    for a, b in pairs:
        s = s.replace(a, b)
    new[p] = s
def append(p, text):
    s = load(p)
    if text.strip() not in s:
        s = s.rstrip("\n") + "\n" + text
    new[p] = s

# ---------------------------------------------------------------- files that only exist in our fork
copies = [
    ("include/rex/filesystem/overlay_device.h", "include/rex/filesystem/overlay_device.h"),
    ("src/filesystem/devices/overlay_device.cpp", "src/filesystem/devices/overlay_device.cpp"),
    ("include/rex/diagnostics/freeze_logger.h", "include/rex/diagnostics/freeze_logger.h"),
    ("include/rex/diagnostics/rate_census.h", "include/rex/diagnostics/rate_census.h"),
    ("src/core/freeze_logger.cpp", "src/core/freeze_logger.cpp"),
    ("src/core/rate_census.cpp", "src/core/rate_census.cpp"),
    ("include/rex/input/text_entry.h", "include/rex/input/text_entry.h"),
]
for src, dst in copies:
    assert os.path.isfile(os.path.join(OLD, src)), ("missing in our SDK", src)
    assert not os.path.exists(os.path.join(K, dst)), ("already in the new SDK", dst)
append(K + "/src/filesystem/CMakeLists.txt",
       "\n# WWE13: the game folder layered over a disc image (title update + extracted files over the .iso).\n"
       "target_sources(rexfilesystem PRIVATE devices/overlay_device.cpp)\n")
append(K + "/src/core/CMakeLists.txt",
       "\n# WWE13: freeze reports and the frame-rate census (the game's diagnostics read them).\n"
       "target_sources(rexcore PRIVATE freeze_logger.cpp rate_census.cpp)\n")

# ---------------------------------------------------------------- host path redirect (arena crowd copies)
edit(K + "/include/rex/filesystem/devices/host_path_device.h", [
 ("  bool allow_share_delete_;\n};\n\n}  // namespace rex::filesystem",
  "  bool allow_share_delete_;\n};\n\n"
  "// Optional: maps a game file's host path to the file to open instead (generated copies, mods).\n"
  "// Returns the path to open (the same path = no change). Set once at startup, before the game runs.\n"
  "using HostPathRedirect = std::filesystem::path (*)(const std::filesystem::path& host_path);\n"
  "void SetHostPathRedirect(HostPathRedirect redirect);\n\n"
  "}  // namespace rex::filesystem"),
])
E = K + "/src/filesystem/devices/host_path_entry.cpp"
edit(E, [
 ("#include <rex/string.h>\n\nnamespace rex::filesystem {\n",
  "#include <rex/string.h>\n\nnamespace rex::filesystem {\n\n"
  "namespace {\n"
  "HostPathRedirect g_host_path_redirect = nullptr;\n"
  "std::filesystem::path Redirected(const std::filesystem::path& host_path) {\n"
  "  return g_host_path_redirect ? g_host_path_redirect(host_path) : host_path;\n"
  "}\n"
  "}  // namespace\n\n"
  "void SetHostPathRedirect(HostPathRedirect redirect) { g_host_path_redirect = redirect; }\n"),
 ("  auto file_handle = rex::filesystem::FileHandle::OpenExisting(\n"
  "      host_path_, desired_access, static_cast<HostPathDevice*>(device_)->allow_share_delete());\n",
  "  const bool for_write = (desired_access & (FileAccess::kFileWriteData | FileAccess::kFileAppendData)) != 0;\n"
  "  const std::filesystem::path open_path = for_write ? host_path_ : Redirected(host_path_);\n"
  "  auto file_handle = rex::filesystem::FileHandle::OpenExisting(\n"
  "      open_path, desired_access, static_cast<HostPathDevice*>(device_)->allow_share_delete());\n"
  "  if (file_handle && open_path != host_path_) {\n"
  "    // A generated copy can differ in size from the file listed: report the copy's.\n"
  "    std::error_code size_error;\n"
  "    const auto copy_size = std::filesystem::file_size(open_path, size_error);\n"
  "    if (!size_error) {\n"
  "      size_ = copy_size;\n"
  "      allocation_size_ = rex::round_up(copy_size, device_->bytes_per_sector());\n"
  "    }\n"
  "  }\n"),
 ("  return memory::MappedMemory::Open(host_path_, mode, offset, length);\n",
  "  return memory::MappedMemory::Open(mode == memory::MappedMemory::Mode::kRead ? Redirected(host_path_) : host_path_,\n"
  "                                    mode, offset, length);\n"),
])

# ---------------------------------------------------------------- play from a disc image under the game folder
R = K + "/src/system/runtime.cpp"
edit(R, [
 ("#include <rex/filesystem/devices/disc_image_device.h>\n",
  "#include <rex/filesystem/devices/disc_image_device.h>\n#include <rex/filesystem/overlay_device.h>\n"),
 ('REXCVAR_DEFINE_STRING(metadata_root, "", "Runtime", "Override metadata path");\n',
  'REXCVAR_DEFINE_STRING(metadata_root, "", "Runtime", "Override metadata path");\n'
  'REXCVAR_DEFINE_STRING(game_disc_image, "", "Runtime",\n'
  '                      "WWE13: play from an Xbox 360 disc image (a path, or /proc/self/fd/N from the Android "\n'
  '                      "file picker). Files in game_data_root (default.xexp, extracted files) take priority.");\n'),
 ("        mount_path, abs_game_root, !REXCVAR_GET(allow_game_relative_writes));\n  }\n",
  "        mount_path, abs_game_root, !REXCVAR_GET(allow_game_relative_writes));\n  }\n"
  "  const std::string under_disc = REXCVAR_GET(game_disc_image);\n"
  "  if (!disc_image && !under_disc.empty()) {\n"
  "    // WWE13: the disc image underneath; the game folder (title update, any extracted files) on top of it.\n"
  "    auto disc = std::make_unique<rex::filesystem::DiscImageDevice>(mount_path, std::filesystem::path(under_disc));\n"
  "    device = std::make_unique<rex::filesystem::OverlayDevice>(mount_path, std::move(device), std::move(disc));\n"
  "    REXSYS_INFO(\"  Disc image under the game folder: {}\", under_disc);\n"
  "  }\n"),
])

# ---------------------------------------------------------------- Android start: our launcher, landscape, picked ISO
M = K + "/src/ui/windowed_app_main_sdl.cpp"
old_main = '''extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char* argv[]) {
  if (const char* files_dir = std::getenv("WWE2K14_FILES_DIR"); files_dir && *files_dir) {
    chdir(files_dir);
    setenv("HOME", files_dir, 1);
  }
  if (!rex::InitializeAndroidSystemForApplicationContext()) {
    return EXIT_FAILURE;
  }
  int result = RunWindowedApp(argc, argv);
  rex::ShutdownAndroidSystem();
  return result;
}
'''
new_main = '''// WWE13: the game's in-app launcher (Play / Settings / Import ISO). Optional; returns true when Play was pressed and
// fills in the game's arguments and environment.
bool WWE13RunAndroidLauncher(std::vector<std::string>& arguments,
                             std::vector<std::pair<std::string, std::string>>& environment)
    __attribute__((weak));

namespace {
// A disc image picked in Android's file picker is a content:// URI. SDL asks Android for a file descriptor; the
// runtime then maps /proc/self/fd/N like any other file.
std::string ContentUriAsPath(const std::string& uri) {
  SDL_IOStream* io = SDL_IOFromFile(uri.c_str(), "rb");
  if (!io) {
    return uri;
  }
  FILE* fp = static_cast<FILE*>(
      SDL_GetPointerProperty(SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, nullptr));
  const int fd = fp ? ::dup(fileno(fp)) : -1;
  SDL_CloseIO(io);
  return fd >= 0 ? "/proc/self/fd/" + std::to_string(fd) : uri;
}
}  // namespace

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char* argv[]) {
  // WWE13: landscape only, for the launcher and the game (SDL otherwise allows portrait).
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  std::string files_dir;
  if (const char* dir = std::getenv("WWE2K14_FILES_DIR"); dir && *dir) {
    files_dir = dir;
  } else if (const char* external = SDL_GetAndroidExternalStoragePath(); external && *external) {
    files_dir = external;
  } else if (const char* internal = SDL_GetAndroidInternalStoragePath(); internal && *internal) {
    files_dir = internal;
  }
  if (!files_dir.empty()) {
    chdir(files_dir.c_str());
    setenv("HOME", files_dir.c_str(), 1);
  }
  if (!rex::InitializeAndroidSystemForApplicationContext()) {
    return EXIT_FAILURE;
  }
  std::vector<std::string> arg_storage;
  std::vector<char*> arg_ptrs;
  bool sdl_held = false;
  if (WWE13RunAndroidLauncher) {
    sdl_held = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    std::vector<std::string> launch_args;
    std::vector<std::pair<std::string, std::string>> launch_env;
    if (!WWE13RunAndroidLauncher(launch_args, launch_env)) {
      if (sdl_held) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
      rex::ShutdownAndroidSystem();
      return EXIT_SUCCESS;
    }
    for (const auto& kv : launch_env) setenv(kv.first.c_str(), kv.second.c_str(), 1);
    arg_storage.push_back(argc > 0 && argv[0] ? argv[0] : "app_process");
    static constexpr std::string_view kDisc = "--game_disc_image=";
    for (auto& a : launch_args) {
      if (a.starts_with(kDisc) && a.compare(kDisc.size(), 10, "content://") == 0) {
        a = std::string(kDisc) + ContentUriAsPath(a.substr(kDisc.size()));
      }
      arg_storage.push_back(std::move(a));
    }
    for (auto& a : arg_storage) arg_ptrs.push_back(a.data());
    arg_ptrs.push_back(nullptr);
    argc = static_cast<int>(arg_storage.size());
    argv = arg_ptrs.data();
  }
  int result = RunWindowedApp(argc, argv);
  if (sdl_held) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  rex::ShutdownAndroidSystem();
  return result;
}
'''
edit(M, [
 ("#include <unistd.h>\n\n#include <rex/system.h>\n#endif\n",
  "#include <unistd.h>\n\n#include <cstdio>\n#include <string_view>\n#include <utility>\n\n"
  "#include <SDL3/SDL.h>\n\n#include <rex/system.h>\n#endif\n"),
 (old_main, new_main),
])

# ---------------------------------------------------------------- write (only after every check passed)
for src, dst in copies:
    os.makedirs(os.path.dirname(os.path.join(K, dst)), exist_ok=True)
    shutil.copyfile(os.path.join(OLD, src), os.path.join(K, dst))
for p, text in new.items():
    with open(p, "w") as f:
        f.write(text)
print("port ok:", len(copies), "files copied,", len(new), "files edited")
