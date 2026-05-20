const { TextEncoder, TextDecoder } = require('util');
global.TextEncoder = TextEncoder;
global.TextDecoder = TextDecoder;

const fs = require('fs');
const path = require('path');
const { JSDOM } = require('jsdom');

describe('initBlocks', () => {
  let dom;
  let window;
  let document;

  beforeEach(() => {
    const html = fs.readFileSync(path.resolve(__dirname, '../index.html'), 'utf8');
    dom = new JSDOM(html, {
      runScripts: "dangerously",
      // Suppress JSDOM canvas error logging since we don't need canvas for initBlocks
      virtualConsole: new (require('jsdom')).VirtualConsole().sendTo(console, { omitJSDOMErrors: true })
    });
    window = dom.window;
    document = window.document;
  });

  it('initBlocks is exposed to window', () => {
    expect(typeof window.initBlocks).toBe('function');
  });

  it('getTestState is exposed to window', () => {
    expect(typeof window.getTestState).toBe('function');
  });

  describe('behavior', () => {
    it('creates correct number of blocks and sets state', () => {
      window.initBlocks(5);

      const state = window.getTestState();
      expect(state.expectedPages).toBe(5);
      expect(state.receivedPayloads).toHaveLength(5);
      expect(state.receivedPayloads.every(x => x === null)).toBe(true);

      const blocksEl = document.getElementById("blocks");
      expect(blocksEl.children.length).toBe(5);
      expect(blocksEl.children[0].className).toBe('block');
      expect(blocksEl.children[0].id).toBe('block-0');
      expect(blocksEl.children[4].id).toBe('block-4');
    });

    it('clears blocks if total is 0', () => {
      // First set up some blocks
      window.initBlocks(3);
      expect(document.getElementById("blocks").children.length).toBe(3);

      // Now set to 0
      window.initBlocks(0);

      const state = window.getTestState();
      expect(state.expectedPages).toBe(0);
      expect(state.receivedPayloads).toHaveLength(0);

      const blocksEl = document.getElementById("blocks");
      expect(blocksEl.children.length).toBe(0);
    });

    it('returns early if total matches expectedPages (idempotent)', () => {
      window.initBlocks(4);

      const blocksEl = document.getElementById("blocks");
      const firstBlock = blocksEl.children[0];
      firstBlock.className = 'block active'; // Modify the DOM

      // Call again with same total
      window.initBlocks(4);

      // DOM should NOT be re-rendered, so the modification should persist
      expect(blocksEl.children[0].className).toBe('block active');
    });
  });
});
