const { test, expect } = require('@playwright/test');

test('Canvas Resize Overhead Benchmark', async ({ page }) => {
  await page.goto('about:blank');

  const result = await page.evaluate(async () => {
    // Setup a canvas element
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');

    // Simulate a video feed source dimension
    const videoWidth = 1920;
    const videoHeight = 1080;
    const iterations = 10000;

    let unoptimizedTime = 0;
    let optimizedTime = 0;

    // Unoptimized: Assigning width/height unconditionally on every frame
    const startUnoptimized = performance.now();
    for (let i = 0; i < iterations; i++) {
      canvas.width = Math.min(videoWidth, 600);
      canvas.height = canvas.width * (videoHeight / videoWidth);
      ctx.fillRect(0, 0, canvas.width, canvas.height); // Minimal draw
    }
    const endUnoptimized = performance.now();
    unoptimizedTime = endUnoptimized - startUnoptimized;

    // Reset canvas for a fair start
    canvas.width = 0;
    canvas.height = 0;

    // Optimized: Calculate integer target dimensions and assign only if changed
    const startOptimized = performance.now();
    for (let i = 0; i < iterations; i++) {
      const targetWidth = Math.floor(Math.min(videoWidth, 600));
      const targetHeight = Math.floor(targetWidth * (videoHeight / videoWidth));

      if (canvas.width !== targetWidth) canvas.width = targetWidth;
      if (canvas.height !== targetHeight) canvas.height = targetHeight;

      ctx.fillRect(0, 0, canvas.width, canvas.height); // Minimal draw
    }
    const endOptimized = performance.now();
    optimizedTime = endOptimized - startOptimized;

    return {
      unoptimizedTime: unoptimizedTime.toFixed(2),
      optimizedTime: optimizedTime.toFixed(2),
      improvementRatio: (unoptimizedTime / optimizedTime).toFixed(2),
      improvementPercentage: (((unoptimizedTime - optimizedTime) / unoptimizedTime) * 100).toFixed(2)
    };
  });

  console.log('--- Benchmark Results ---');
  console.log(`Unoptimized Time (10k iterations): ${result.unoptimizedTime} ms`);
  console.log(`Optimized Time (10k iterations): ${result.optimizedTime} ms`);
  console.log(`Improvement Ratio: ${result.improvementRatio}x faster`);
  console.log(`Improvement Percentage: ${result.improvementPercentage}% time saved`);
  console.log('---------------------------');

  expect(parseFloat(result.optimizedTime)).toBeLessThan(parseFloat(result.unoptimizedTime));
});
