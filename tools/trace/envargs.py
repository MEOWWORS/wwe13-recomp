# Settings > Extra arguments: besides "--name=value" (game options), accept "WWE13_NAME=value" words as test switches
# for the game (environment variables), so experiments need no new APK. The frame trace is now one of them
# (WWE13_FRAME_TRACE=1) instead of always on.
p = "/workspaces/wwe13-recomp/launcher2/core/launch.cpp"
s = open(p).read()
pairs = [
 ('      {"WWE13_FRAME_TRACE", "1"},  // test build: one traced frame every 10 s in the log\n', ''),
 ('  // Extra arguments typed in Settings: each "--name=value" word is passed on as it is.\n  {\n',
  '  // Extra arguments typed in Settings: each "--name=value" word is passed on as it is; each\n'
  '  // "WWE13_NAME=value" word sets that test switch for the game.\n'
  '  std::vector<std::pair<std::string, std::string>> extra_environment;\n  {\n'),
 ('        if (word.size() > 2 && word.rfind("--", 0) == 0) plan.arguments.push_back(word);\n',
  '        if (word.size() > 2 && word.rfind("--", 0) == 0) {\n'
  '          plan.arguments.push_back(word);\n'
  '        } else if (word.rfind("WWE13_", 0) == 0 && word.find(\'=\') != std::string::npos) {\n'
  '          extra_environment.emplace_back(word.substr(0, word.find(\'=\')), word.substr(word.find(\'=\') + 1));\n'
  '        }\n'),
 ('                            : effective.touch_controls == TouchControls::kNone ? "none" : "android"}};\n',
  '                            : effective.touch_controls == TouchControls::kNone ? "none" : "android"}};\n'
  '  for (const auto& [name, value] : extra_environment) {\n'
  '    auto it = std::find_if(plan.environment.begin(), plan.environment.end(),\n'
  '                           [&name](const auto& entry) { return entry.first == name; });\n'
  '    if (it != plan.environment.end()) {\n      it->second = value;\n    } else {\n'
  '      plan.environment.emplace_back(name, value);\n    }\n  }\n'),
]
for a, b in pairs:
    if b and b in s:
        continue
    if not b and a not in s:
        continue
    assert s.count(a) == 1, (a[:60], s.count(a))
    s = s.replace(a, b)
open(p, "w").write(s)
print("extra arguments: WWE13_ test switches ok")
# Recommended setting: Faster 2x anti-aliasing (measured: about 4 fps over Original 4x on the Nothing Phone 3a).
g = "/workspaces/wwe13-recomp/launcher2/core/gpus.cpp"
t = open(g).read()
a = "  recommendation.settings.anti_aliasing = AntiAliasing::kOriginal4x;\n"
if a in t:
    open(g, "w").write(t.replace(a, "  recommendation.settings.anti_aliasing = AntiAliasing::kFaster2x;  // measured faster on phones\n"))
print("recommended anti-aliasing: Faster 2x")
