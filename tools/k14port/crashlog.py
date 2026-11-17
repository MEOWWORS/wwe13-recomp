# Test APK only: when the app starts, save Android's record of how the previous runs ended (a native crash includes
# its tombstone: signal, address and the call stack) into logs/, so it ends up in the WWE13-Logs.zip.
import sys
p = sys.argv[1]
s = open(p).read()
mark = "// WWE13: exit records"
code = '''
        // WWE13: exit records of earlier runs (crash stacks) into logs/ for the log zip.
        try {
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                android.app.ActivityManager am = (android.app.ActivityManager) getSystemService(ACTIVITY_SERVICE);
                java.io.File dir = new java.io.File(getExternalFilesDir(null), "logs");
                dir.mkdirs();
                for (android.app.ApplicationExitInfo e : am.getHistoricalProcessExitReasons(null, 0, 5)) {
                    java.io.File out = new java.io.File(dir, "wwe13-exit-" + e.getTimestamp() + ".txt");
                    if (out.exists()) continue;
                    java.io.FileOutputStream os = new java.io.FileOutputStream(out);
                    os.write((e.toString() + "\\n").getBytes());
                    java.io.InputStream in = e.getTraceInputStream();
                    if (in != null) {
                        byte[] b = new byte[65536];
                        int n;
                        while ((n = in.read(b)) > 0) os.write(b, 0, n);
                        in.close();
                    }
                    os.close();
                }
            }
        } catch (Throwable t) {
        }
'''
if mark not in s:
    a = "super.onCreate(savedInstanceState);\n"
    assert a in s, "SDLActivity: no super.onCreate"
    s = s.replace(a, a + code, 1)
    open(p, "w").write(s)
print("crash records: on")
