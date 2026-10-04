const { TextEncoder, TextDecoder } = require('util');
global.TextEncoder = TextEncoder;
global.TextDecoder = TextDecoder;
const fs = require('fs');
const path = require('path');
const { JSDOM, VirtualConsole } = require('jsdom');

describe('AI response rendering', () => {
  let dom;
  let element;
  beforeEach(() => {
    dom = new JSDOM(fs.readFileSync(path.join(__dirname, '../archives/web-scanner-2026-10-04/index.html.txt'), 'utf8'), {
      runScripts: 'dangerously',
      virtualConsole: new VirtualConsole().sendTo(console, { omitJSDOMErrors: true }),
    });
    element = dom.window.document.createElement('div');
  });
  afterEach(() => dom.window.close());

  test('displays untrusted response text without creating executable HTML', () => {
    const text = '<img src=x onerror="alert(1)">\n<script>alert(2)</script>';
    dom.window.setSafeHTML(element, text);
    expect(element.querySelector('img, script')).toBeNull();
    expect(element.querySelectorAll('br')).toHaveLength(1);
    expect(element.textContent).toBe(text.replace('\n', ''));
  });

  test('replaces a previous response and clears absent responses', () => {
    element.innerHTML = '<b>old response</b>';
    dom.window.setSafeHTML(element, 'new\nresponse');
    expect(element.innerHTML).toBe('new<br>response');
    dom.window.setSafeHTML(element, undefined);
    expect(element.innerHTML).toBe('');
  });
});
