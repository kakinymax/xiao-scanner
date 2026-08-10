const { TextEncoder, TextDecoder } = require("util");
global.TextEncoder = TextEncoder;
global.TextDecoder = TextDecoder;

const fs = require("fs");
const path = require("path");
const { JSDOM } = require("jsdom");

describe("setSafeHTML", () => {
  let window;
  let document;

  beforeEach(() => {
    const html = fs.readFileSync(
      path.resolve(__dirname, "../index.html"),
      "utf8",
    );
    const dom = new JSDOM(html, {
      runScripts: "dangerously",
      virtualConsole: new (require("jsdom").VirtualConsole)().sendTo(console, {
        omitJSDOMErrors: true,
      }),
    });
    window = dom.window;
    document = window.document;
  });

  it("is exposed to window", () => {
    expect(typeof window.setSafeHTML).toBe("function");
  });

  describe("behavior", () => {
    it("sets simple text safely", () => {
      const el = document.createElement("div");
      window.setSafeHTML(el, "Hello World");
      expect(el.innerHTML).toBe("Hello World");
    });

    it("handles line breaks by converting them to <br>", () => {
      const el = document.createElement("div");
      window.setSafeHTML(el, "Hello\nWorld");
      expect(el.innerHTML).toBe("Hello<br>World");
    });

    it("escapes HTML tags to prevent XSS", () => {
      const el = document.createElement("div");
      window.setSafeHTML(el, '<script>alert("XSS")</script>');
      // Since createTextNode is used, the tags should be encoded as text
      expect(el.innerHTML).toBe('&lt;script&gt;alert("XSS")&lt;/script&gt;');
    });

    it("handles null and undefined gracefully", () => {
      const el = document.createElement("div");
      window.setSafeHTML(el, null);
      expect(el.innerHTML).toBe("");

      window.setSafeHTML(el, undefined);
      expect(el.innerHTML).toBe("");
    });

    it("handles numbers and converts them to string safely", () => {
      const el = document.createElement("div");
      window.setSafeHTML(el, 12345);
      expect(el.innerHTML).toBe("12345");
    });
  });
});
