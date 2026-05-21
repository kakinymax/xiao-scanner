const { performance } = require('perf_hooks');

const history_count = 10000;
const ts = Math.floor(Date.now() / 1000);

function runInlinePad() {
  const start = performance.now();
  let result = [];
  for (let i = 0; i < history_count; i++) {
    let agoMins = (history_count - 1 - i) * 12;
    let pTime = new Date((ts - agoMins * 60) * 1000);
    let pad = (n) => ("0" + n).slice(-2);
    let shortTimeStr = `${pad(pTime.getHours())}:${pad(pTime.getMinutes())}`;
    result.push(shortTimeStr);
  }
  const end = performance.now();
  return end - start;
}

function runExtractedPad() {
  const start = performance.now();
  let result = [];
  let pad = (n) => ("0" + n).slice(-2);
  for (let i = 0; i < history_count; i++) {
    let agoMins = (history_count - 1 - i) * 12;
    let pTime = new Date((ts - agoMins * 60) * 1000);
    let shortTimeStr = `${pad(pTime.getHours())}:${pad(pTime.getMinutes())}`;
    result.push(shortTimeStr);
  }
  const end = performance.now();
  return end - start;
}

const inlineTime = runInlinePad();
const extractedTime = runExtractedPad();

console.log(`Inline pad function time: ${inlineTime.toFixed(2)} ms`);
console.log(`Extracted pad function time: ${extractedTime.toFixed(2)} ms`);
console.log(`Improvement: ${((inlineTime - extractedTime) / inlineTime * 100).toFixed(2)}%`);
