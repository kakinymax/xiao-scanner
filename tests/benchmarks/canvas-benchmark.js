const { chromium } = require('playwright');
const path = require('path');

async function runBenchmark() {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

  page.on('console', msg => console.log('BROWSER:', msg.text()));

  const filePath = `file://${path.resolve(__dirname, '../../index.html')}`;
  await page.goto(filePath);

  const result = await page.evaluate(async () => {
    // Mock dependencies
    window.jsQR = () => null;
    window.scanning = true;
    window.isComplete = false;

    const video = document.getElementById('video');
    const mockCanvas = document.createElement('canvas');
    mockCanvas.width = 640;
    mockCanvas.height = 480;
    document.body.appendChild(mockCanvas);
    mockCanvas.style.display = 'none';

    const mockCtx = mockCanvas.getContext('2d');
    mockCtx.fillStyle = 'red';
    mockCtx.fillRect(0, 0, 640, 480);

    // Use captureStream to mock a real video stream
    const stream = mockCanvas.captureStream(30);
    video.srcObject = stream;
    await video.play();

    // Wait until video has enough data
    await new Promise(resolve => {
        const checkReady = () => {
            if (video.readyState === 4 /* HAVE_ENOUGH_DATA */) {
                resolve();
            } else {
                setTimeout(checkReady, 50);
            }
        };
        checkReady();
    });

    console.log("Video is ready. Dimensions:", video.videoWidth, "x", video.videoHeight);

    // Disable requestAnimationFrame inside tick to prevent infinite recursion
    const originalRaf = window.requestAnimationFrame;
    window.requestAnimationFrame = () => {};

    const ITERS = 10000;

    // Extract JUST the render portion of tick to isolate the canvas allocation bottleneck.
    // jsQR executes so quickly when mocked that the true DOM allocation cost is otherwise masked
    // or we end up timing the mock function call overhead.
    const runCoreRenderLoop = () => {
        const canvas = document.getElementById('canvas');
        const ctx = canvas.getContext('2d');

        // This is the problematic pattern from index.html we are benchmarking
        canvas.width = Math.min(video.videoWidth, 600);
        canvas.height = canvas.width * (video.videoHeight / video.videoWidth);
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

        // Force evaluation
        ctx.getImageData(0, 0, canvas.width, canvas.height);
    };

    // Warmup
    for (let i = 0; i < 100; i++) {
        runCoreRenderLoop();
    }

    const start = performance.now();
    for (let i = 0; i < ITERS; i++) {
        runCoreRenderLoop();
    }
    const end = performance.now();

    window.requestAnimationFrame = originalRaf;

    return {
      durationMs: end - start,
      avgMs: (end - start) / ITERS
    };
  });

  console.log(`Benchmark completed:`);
  console.log(`Total time for 10000 iterations: ${result.durationMs.toFixed(2)}ms`);
  console.log(`Average time per iteration: ${result.avgMs.toFixed(3)}ms`);

  await browser.close();
}

runBenchmark().catch(console.error);