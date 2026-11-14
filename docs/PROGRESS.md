## 2026-11-14
- Fullscreen flag added (game + launcher); landscape hint and cutout theme not done.
- frame_step_fix added in SDK; the '13 frame-rate block is NOT confirmed found. Check log for "frame step block".
- Gamepad buttons enlarged (0.035->0.043, 0.038->0.05); sticks not yet.
- Untested on device. Open: SIGILL in libmain.so on AK::IOThread, match-start freeze (async shader compile untested).
