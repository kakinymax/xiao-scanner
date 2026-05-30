const { test, expect } = require('@playwright/test');
const path = require('path');

test('Canvas resizing performance', async ({ page }) => {
  const htmlPath = path.resolve(__dirname, '../../index.html');
  await page.goto(`file://${htmlPath}`);

  // Inject a mock video and simulate tick() 1000 times
  const duration = await page.evaluate(() => {
    return new Promise((resolve) => {
      let mockVideo = {
        readyState: 4, // HAVE_ENOUGH_DATA
        videoWidth: 1920,
        videoHeight: 1080
      };

      const canvas = document.getElementById("canvas");
      const ctx = canvas.getContext("2d");

      const start = performance.now();
      for (let i = 0; i < 1000; i++) {
        // Original logic
        canvas.width = Math.min(mockVideo.videoWidth, 600);
        canvas.height = canvas.width * (mockVideo.videoHeight / mockVideo.videoWidth);
        // ctx.drawImage(mockVideo, 0, 0, canvas.width, canvas.height); // skip drawImage for now since mockVideo is fake
      }
      const end = performance.now();

      const origDuration = end - start;

      const startOpt = performance.now();
      for (let i = 0; i < 1000; i++) {
        // Optimized logic
        const newWidth = Math.floor(Math.min(mockVideo.videoWidth, 600));
        const newHeight = Math.floor(newWidth * (mockVideo.videoHeight / mockVideo.videoWidth));
        if (canvas.width !== newWidth || canvas.height !== newHeight) {
          canvas.width = newWidth;
          canvas.height = newHeight;
        }
      }
      const endOpt = performance.now();

      const optDuration = endOpt - startOpt;

      resolve({ origDuration, optDuration });
    });
  });

  console.log(`Original duration (1000 ticks): ${duration.origDuration.toFixed(2)}ms`);
  console.log(`Optimized duration (1000 ticks): ${duration.optDuration.toFixed(2)}ms`);
});
