const { test, expect } = require('@playwright/test');
const fs = require('fs');
const path = require('path');

test.describe('Canvas Resize Performance Benchmark', () => {
  test('measures canvas rendering performance with conditional vs continuous assignment', async ({ page }) => {
    // 1. Get original logic (Continuous Assignment) vs Optimized logic

    // Test HTML to run the benchmarks in browser context
    const testHtml = `
      <!DOCTYPE html>
      <html>
      <head><title>Benchmark</title></head>
      <body>
        <canvas id="canvas1"></canvas>
        <canvas id="canvas2"></canvas>
        <script>
          function runContinuous(iterations) {
            const canvas = document.getElementById("canvas1");
            const ctx = canvas.getContext("2d");
            const start = performance.now();
            let sum = 0;

            for (let i = 0; i < iterations; i++) {
              const newWidth = Math.min(1920, 600);
              const newHeight = newWidth * (1080 / 1920);

              // Old way: continuous assignment
              canvas.width = newWidth;
              canvas.height = newHeight;

              ctx.fillStyle = "red";
              ctx.fillRect(0, 0, canvas.width, canvas.height);
              sum += canvas.width; // prevent optimization
            }
            return performance.now() - start;
          }

          function runOptimized(iterations) {
            const canvas = document.getElementById("canvas2");
            const ctx = canvas.getContext("2d");
            const start = performance.now();
            let sum = 0;

            for (let i = 0; i < iterations; i++) {
              const newWidth = Math.floor(Math.min(1920, 600));
              const newHeight = Math.floor(newWidth * (1080 / 1920));

              // New way: conditional assignment
              if (canvas.width !== newWidth || canvas.height !== newHeight) {
                canvas.width = newWidth;
                canvas.height = newHeight;
              }

              ctx.fillStyle = "blue";
              ctx.fillRect(0, 0, canvas.width, canvas.height);
              sum += canvas.width; // prevent optimization
            }
            return performance.now() - start;
          }
        </script>
      </body>
      </html>
    `;

    await page.setContent(testHtml);

    const iterations = 5000;

    // Warmup
    await page.evaluate((iters) => runContinuous(Math.min(100, iters)), iterations);
    await page.evaluate((iters) => runOptimized(Math.min(100, iters)), iterations);

    const continuousTime = await page.evaluate((iters) => runContinuous(iters), iterations);
    const optimizedTime = await page.evaluate((iters) => runOptimized(iters), iterations);

    console.log(`Benchmark results (${iterations} iterations):`);
    console.log(`Continuous Assignment: ${continuousTime.toFixed(2)} ms`);
    console.log(`Conditional Assignment: ${optimizedTime.toFixed(2)} ms`);

    const improvement = ((continuousTime - optimizedTime) / continuousTime) * 100;
    console.log(`Improvement: ${improvement.toFixed(2)}%`);

    // Verify it is faster
    expect(optimizedTime).toBeLessThan(continuousTime);
  });
});
