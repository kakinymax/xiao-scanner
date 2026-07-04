const { test, expect } = require('@playwright/test');

test('Canvas rendering performance benchmark', async ({ page }) => {
  // Mock performance environment
  await page.evaluate(() => {
    window.benchmarkLogs = [];
    window.benchmarkResults = {};

    // Mock video properties
    const video = { videoWidth: 1920, videoHeight: 1080 };
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');

    // Scenario 1: Unconditional Assignment (Before Optimization)
    let startTimeUnconditional = performance.now();
    for (let i = 0; i < 1000; i++) {
      canvas.width = Math.min(video.videoWidth, 600);
      canvas.height = canvas.width * (video.videoHeight / video.videoWidth);
      ctx.fillRect(0, 0, canvas.width, canvas.height); // simulate drawing
    }
    let durationUnconditional = performance.now() - startTimeUnconditional;
    window.benchmarkResults.unconditional = durationUnconditional;

    // Reset canvas to some default state
    canvas.width = 0;
    canvas.height = 0;

    // Scenario 2: Conditional Assignment (After Optimization)
    let startTimeConditional = performance.now();
    for (let i = 0; i < 1000; i++) {
      const targetWidth = Math.floor(Math.min(video.videoWidth, 600));
      const targetHeight = Math.floor(targetWidth * (video.videoHeight / video.videoWidth));
      if (canvas.width !== targetWidth) canvas.width = targetWidth;
      if (canvas.height !== targetHeight) canvas.height = targetHeight;
      ctx.fillRect(0, 0, canvas.width, canvas.height); // simulate drawing
    }
    let durationConditional = performance.now() - startTimeConditional;
    window.benchmarkResults.conditional = durationConditional;
  });

  const results = await page.evaluate(() => window.benchmarkResults);

  console.log(`Unconditional assignment (1000 iterations): ${results.unconditional.toFixed(2)} ms`);
  console.log(`Conditional assignment (1000 iterations): ${results.conditional.toFixed(2)} ms`);
  console.log(`Improvement: ${((results.unconditional - results.conditional) / results.unconditional * 100).toFixed(2)}% faster`);

  // Assert that conditional is generally faster (allowing some variance, but it should be noticeably better or at least not worse in extreme edge cases, usually it's way faster because it skips reallocation)
  expect(results.conditional).toBeLessThan(results.unconditional);
});
