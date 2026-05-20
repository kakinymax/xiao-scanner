const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

async function runBenchmark() {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

  // ファイルパスを絶対パスで指定
  const filePath = `file://${path.resolve(__dirname, '../../index.html')}`;

  // 現在の最適化済みコードでのパフォーマンス計測
  console.log("Running benchmark on OPTIMIZED code...");
  await page.goto(filePath);

  // カメラやビデオのモックを設定して、tick関数を強制的に回す準備
  const optimizedResult = await page.evaluate(async () => {
    // 必要なモック
    window.scanning = true;
    window.isComplete = false;

    // HTMLCanvasElementのwidth/heightセッターをフックして呼ばれた回数をカウント
    let setterCallCount = 0;
    const canvas = document.getElementById('canvas');
    const originalWidthSetter = Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'width').set;
    const originalHeightSetter = Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'height').set;

    Object.defineProperty(canvas, 'width', {
        set(val) {
            setterCallCount++;
            originalWidthSetter.call(this, val);
        },
        get() { return Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'width').get.call(this); }
    });
    Object.defineProperty(canvas, 'height', {
        set(val) {
            setterCallCount++;
            originalHeightSetter.call(this, val);
        },
        get() { return Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'height').get.call(this); }
    });

    const video = document.getElementById('video');
    // モック用のプロパティ
    Object.defineProperty(video, 'readyState', { value: 4 }); // HAVE_ENOUGH_DATA
    Object.defineProperty(video, 'videoWidth', { value: 1920 });
    Object.defineProperty(video, 'videoHeight', { value: 1080 });

    // tick関数をN回実行して時間を計測
    const ITERATIONS = 1000;
    const startTime = performance.now();

    for (let i = 0; i < ITERATIONS; i++) {
        // tick関数内で参照される変数が期待通り動くようにする
        // 元のtick関数は requestAnimationFrame を呼ぶので、直接呼ばずに
        // 内部ロジックをシミュレートするか、tick関数自体を1回ずつ呼んでrequestAnimationFrameをキャンセルする
    }

    // tick関数のコアロジックを抽出して直接実行する方が確実
    const tickCore = () => {
          // ビデオのサイズが変わった時（または初回）のみサイズを再計算
          if (
            video.videoWidth !== window.cachedVideoWidth ||
            video.videoHeight !== window.cachedVideoHeight
          ) {
            window.cachedVideoWidth = video.videoWidth;
            window.cachedVideoHeight = video.videoHeight;
            window.cachedNewWidth = Math.min(window.cachedVideoWidth, 600);
            window.cachedNewHeight = Math.floor(
              window.cachedNewWidth * (window.cachedVideoHeight / window.cachedVideoWidth),
            );
          }

          if (canvas.width !== window.cachedNewWidth) canvas.width = window.cachedNewWidth;
          if (canvas.height !== window.cachedNewHeight)
            canvas.height = window.cachedNewHeight;

          const ctx = canvas.getContext('2d');
          ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
    };

    window.cachedVideoWidth = 0;
    window.cachedVideoHeight = 0;

    const startCoreTime = performance.now();
    for (let i = 0; i < ITERATIONS; i++) {
        tickCore();
    }
    const endCoreTime = performance.now();

    return {
        duration: endCoreTime - startCoreTime,
        setterCalls: setterCallCount,
        iterations: ITERATIONS
    };
  });

  console.log(`Optimized Result: ${optimizedResult.duration.toFixed(2)} ms for ${optimizedResult.iterations} iterations.`);
  console.log(`Canvas width/height setter calls: ${optimizedResult.setterCalls}`);


  // 古いコード（毎回代入）のパフォーマンス計測
  console.log("\nRunning benchmark on UNOPTIMIZED code...");
  await page.reload();

  const unoptimizedResult = await page.evaluate(async () => {
    // HTMLCanvasElementのwidth/heightセッターをフック
    let setterCallCount = 0;
    const canvas = document.getElementById('canvas');
    const originalWidthSetter = Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'width').set;
    const originalHeightSetter = Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'height').set;

    Object.defineProperty(canvas, 'width', {
        set(val) {
            setterCallCount++;
            originalWidthSetter.call(this, val);
        },
        get() { return Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'width').get.call(this); }
    });
    Object.defineProperty(canvas, 'height', {
        set(val) {
            setterCallCount++;
            originalHeightSetter.call(this, val);
        },
        get() { return Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype, 'height').get.call(this); }
    });

    const video = document.getElementById('video');
    Object.defineProperty(video, 'readyState', { value: 4 });
    Object.defineProperty(video, 'videoWidth', { value: 1920 });
    Object.defineProperty(video, 'videoHeight', { value: 1080 });

    const ITERATIONS = 1000;

    // 古いコードのロジック
    const tickUnoptimizedCore = () => {
        canvas.width = Math.min(video.videoWidth, 600);
        canvas.height = canvas.width * (video.videoHeight / video.videoWidth);
        const ctx = canvas.getContext('2d');
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
    };

    const startCoreTime = performance.now();
    for (let i = 0; i < ITERATIONS; i++) {
        tickUnoptimizedCore();
    }
    const endCoreTime = performance.now();

    return {
        duration: endCoreTime - startCoreTime,
        setterCalls: setterCallCount,
        iterations: ITERATIONS
    };
  });

  console.log(`Unoptimized Result: ${unoptimizedResult.duration.toFixed(2)} ms for ${unoptimizedResult.iterations} iterations.`);
  console.log(`Canvas width/height setter calls: ${unoptimizedResult.setterCalls}`);

  const improvement = ((unoptimizedResult.duration - optimizedResult.duration) / unoptimizedResult.duration) * 100;
  console.log(`\nPerformance Improvement: ${improvement.toFixed(2)}% faster`);

  await browser.close();
}

runBenchmark().catch(console.error);
