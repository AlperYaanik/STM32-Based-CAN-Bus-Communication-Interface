# Media

Not part of the build — this folder holds the images the root README links to. Add
files here with exactly these names and the links above already work; no other edit
needed.

| File | Used in | What to capture |
|---|---|---|
| `hardware-setup.jpg` | root README, `## Hardware` | Both boards, the two transceivers and the wiring between them, in one frame. Should make the block diagram in `## Architecture` legible as "this is that, physically." Landscape, well-lit, in focus on the wiring — a phone photo is fine. |
| `demo.gif` | root README, `## Result` | A few seconds of the receiver's terminal output scrolling — `ACCEL`/`GYRO` lines at minimum; the `[rx]`/`[os]`/`[cpu]` report lines if they fit without the capture dragging on. Proves the numbers in the README came from a running system, not just the source. Keep it short (5–10 s) and under a few MB so it loads inline on GitHub; a terminal screen recording converted to GIF (e.g. with ScreenToGif, or `ffmpeg -i in.mp4 -vf "fps=10,scale=800:-1" demo.gif`) works well. |

Both are optional — the README reads fine without them, the images just make the
result concrete for someone skimming rather than reading.
