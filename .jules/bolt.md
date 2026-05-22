## 2024-05-22 - [Avoid unnecessary Canvas resize]
**Learning:** In a requestAnimationFrame loop, repeatedly assigning `canvas.width = Math.min(video.videoWidth, 600)` forces the browser to reallocate the backing store and clear the canvas context on every frame, even if the width is identical. This adds significant overhead to DOM manipulations.
**Action:** Always cache the calculated dimensions and only update `canvas.width` and `canvas.height` when the source dimensions actually change. Also, move function definitions out of tight loops (like `pad` function) to prevent repeated allocations.
