# Jump tables: the v0.10 converter read some `switch` tables shorter than the old converter did, and a case it missed
# traps (`__builtin_trap(); // Switch case out of range`). Read every switch from the old converted code, compare it
# with the new one, and write each table that differs to config/switch_tables.toml ([[switch_tables]], which the new
# converter uses instead of its own guess).
import glob, re, sys
OLD, NEW = "/workspaces/wwe13-recomp/generated", "/workspaces/wwe13-k14/generated"
func_re = re.compile(r"^(?:PPC_FUNC_IMPL\(__imp__sub_|DEFINE_REX_FUNC\(sub_|REX_FUNC_IMPL\(__imp__sub_)([0-9A-F]{8})\)")
label_re = re.compile(r"^loc_([0-9A-F]{8}):")
insn_re = re.compile(r"^\s*// (\S+)")
switch_re = re.compile(r"^\s*switch \(ctx\.r(\d+)\.u32\) \{")
case_re = re.compile(r"^\s*case (\d+):")
goto_re = re.compile(r"^\s*goto loc_([0-9A-F]{8});")

def scan(root):
    tables = {}
    for path in glob.glob(root + "/**/*recomp*.cpp", recursive=True):
        base, count, pending = None, 0, None
        lines = open(path, errors="replace").read().split("\n")
        i = 0
        while i < len(lines):
            line = lines[i]
            m = func_re.match(line) or label_re.match(line)
            if m:
                base, count, pending = int(m.group(1), 16), 0, None
            elif base is not None:
                m = insn_re.match(line)
                if m:
                    pending = base + 4 * count if m.group(1) == "bctr" else None
                    count += 1
                elif pending is not None:
                    m = switch_re.match(line)
                    if m:
                        reg, targets, case = int(m.group(1)), {}, None
                        i += 1
                        while i < len(lines) and "default:" not in lines[i]:
                            c = case_re.match(lines[i]); g = goto_re.match(lines[i])
                            if c: case = int(c.group(1))
                            elif g and case is not None: targets[case] = int(g.group(1), 16)
                            i += 1
                        if targets and sorted(targets) == list(range(len(targets))):
                            tables[pending] = (reg, [targets[k] for k in range(len(targets))])
                        pending = None
            i += 1
    return tables

old, new = scan(OLD), scan(NEW)
print("switches: old", len(old), "new", len(new), "both", len(set(old) & set(new)))
assert 0x83364414 in old, "the crashing switch (bctr 0x83364414) was not found in the old code"
# Only switches both converters found: a bctr the new converter does not see as a switch is left alone.
diff = {a: old[a] for a in old if a in new and new[a] != old[a]}
shorter = sum(1 for a in diff if a in new and len(new[a][1]) < len(old[a][1]))
print("differ:", len(diff), "(shorter in new:", shorter, ", missing in new:", sum(1 for a in diff if a not in new), ")")
for a in sorted(diff)[:8]:
    print("  bctr %08X r%d: old %d cases, new %s" % (a, diff[a][0], len(diff[a][1]), len(new[a][1]) if a in new else "-"))
with open("/workspaces/wwe13-k14/config/switch_tables.toml", "w") as f:
    f.write("# Jump tables taken from the old converter's output (switchfix.py): the v0.10 converter read these differently.\n")
    for a in sorted(diff):
        reg, targets = diff[a]
        f.write("\n[[switch_tables]]\naddress = 0x%08X\nregister = %d\nlabels = [%s]\n" % (a, reg, ", ".join("0x%08X" % t for t in targets)))
m = "/workspaces/wwe13-k14/wwe13_manifest.toml"
s = open(m).read()
if "config/switch_tables.toml" not in s:
    a = 'includes = ["config/functions.toml", "config/hooks.toml"]'
    assert s.count(a) == 1
    open(m, "w").write(s.replace(a, 'includes = ["config/functions.toml", "config/hooks.toml", "config/switch_tables.toml"]'))
print("written: config/switch_tables.toml (%d tables)" % len(diff))
