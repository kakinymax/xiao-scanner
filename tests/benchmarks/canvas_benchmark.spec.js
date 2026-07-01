const { test, expect } = require('@playwright/test');
const path = require('path');

test('Canvas reallocation benchmark', async ({ page }) => {
  await page.goto(`file://${path.resolve(__dirname, '../../index.html')}`);

  const results = await page.evaluate(() => {
    return new Promise((resolve) => {
      const canvas = document.createElement('canvas');
      const ctx = canvas.getContext('2d', { willReadFrequently: true });
      const videoWidth = 640;
      const videoHeight = 480;
      const iterations = 500;

      // Benchmark 1: Unconditional allocation (current implementation)
      const start1 = performance.now();
      for (let i = 0; i < iterations; i++) {
        canvas.width = Math.min(videoWidth, 600);
        canvas.height = canvas.width * (videoHeight / videoWidth);
        ctx.fillRect(0, 0, 10, 10);
      }
      const end1 = performance.now();

      // Benchmark 2: Conditional allocation (optimized implementation)
      const start2 = performance.now();
      for (let i = 0; i < iterations; i++) {
        const targetWidth = Math.min(videoWidth, 600);
        const targetHeight = Math.floor(targetWidth * (videoHeight / videoWidth));
        if (canvas.width !== targetWidth) canvas.width = targetWidth;
        if (canvas.height !== targetHeight) canvas.height = targetHeight;
        ctx.fillRect(0, 0, 10, 10);
      }
      const end2 = performance.now();

      resolve({
        unconditionalMs: end1 - start1,
        conditionalMs: end2 - start2
      });
    });
  });

  console.log(`Benchmark Results:`);
  console.log(`Unconditional Assignment: ${results.unconditionalMs.toFixed(2)}ms`);
  console.log(`Conditional Assignment: ${results.conditionalMs.toFixed(2)}ms`);
  console.log(`Improvement: ${((results.unconditionalMs - results.conditionalMs) / results.unconditionalMs * 100).toFixed(2)}%`);
});