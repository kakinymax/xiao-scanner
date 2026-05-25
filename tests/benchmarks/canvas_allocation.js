const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();

  const result = await page.evaluate(() => {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    const w = 600;
    const h = 400;

    // Benchmark 1: Reassigning width/height every frame
    const start1 = performance.now();
    for (let i = 0; i < 10000; i++) {
      canvas.width = w;
      canvas.height = h;
      ctx.fillRect(0, 0, w, h);
    }
    const end1 = performance.now();

    // Benchmark 2: Conditional assignment
    let lastW = 0, lastH = 0;
    const start2 = performance.now();
    for (let i = 0; i < 10000; i++) {
      if (lastW !== w || lastH !== h) {
        canvas.width = w;
        canvas.height = h;
        lastW = w;
        lastH = h;
      }
      ctx.fillRect(0, 0, w, h);
    }
    const end2 = performance.now();

    return {
      unoptimized: end1 - start1,
      optimized: end2 - start2
    };
  });

  console.log('Unoptimized (reallocating every frame):', result.unoptimized.toFixed(2), 'ms');
  console.log('Optimized (conditional assignment):', result.optimized.toFixed(2), 'ms');
  console.log('Improvement:', ((result.unoptimized - result.optimized) / result.unoptimized * 100).toFixed(2), '%');

  await browser.close();
})();
