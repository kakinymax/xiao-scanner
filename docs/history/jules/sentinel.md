## 2024-05-19 - [XSS via unescaped innerHTML for External API Content]

**Vulnerability:** The application was using `innerHTML` to directly insert the response text from the Gemini API and error messages into the DOM without any sanitization. If the API were tricked into returning malicious code (via prompt injection or other means), or if an error message contained malicious strings, it would be executed in the user's browser, leading to a Cross-Site Scripting (XSS) attack.
**Learning:** Even when interacting with trusted APIs like Gemini, any text originating from outside the application's immediate control must be treated as untrusted, especially if it can be influenced by malicious prompts. Directly inserting unescaped external text into the DOM via `innerHTML` is a critical security risk.
**Prevention:** Always sanitize or escape any dynamic text before rendering it as HTML. We implemented a lightweight `escapeHTML` function to convert potentially dangerous characters (`&`, `<`, `>`, `"`, `'`) into their corresponding safe HTML entities. Ideally, where possible, prefer using `textContent` over `innerHTML` to completely avoid parsing HTML from untrusted sources, though here `innerHTML` was used due to line breaks.

## 2024-05-24 - Remove LocalStorage API Key Dependency

**Vulnerability:** Insecure Storage of API Key in LocalStorage. Storing sensitive API keys in `localStorage` makes them susceptible to XSS (Cross-Site Scripting) attacks, as any script running on the domain can access them.

**Learning:** When a client-side application requires interacting with an API that uses secrets, it's safer to avoid storing the secret on the client entirely if a secure backend proxy cannot be established. For applications integrating with external AI chat platforms (like Gemini), a "semi-automatic" workflow that generates the prompt and copies it to the clipboard, then redirects the user to the platform's UI, is a highly secure alternative that requires zero secret storage.

**Prevention:** Never store sensitive API keys in `localStorage`, `sessionStorage`, or cookies without `HttpOnly`. Rely on secure backend proxies for API communication, or design architectures that delegate the API interaction to a secure external client (like the official Gemini web app) via clipboard or intents.

## 2026-05-20 - [Security Fix] Missing Subresource Integrity (SRI) on CDN Scripts

**Vulnerability:** External scripts loaded from CDNs (`jsQR.min.js`, `Chart.js`) without integrity checks. This allowed potential execution of malicious code if the CDN was compromised, leading to XSS or data theft.
**Learning:** Always use `integrity` and `crossorigin="anonymous"` attributes when loading external resources via CDN to verify their integrity and prevent unauthorized modifications. The `crossorigin` attribute ensures proper error logging and avoids exposing sensitive cross-origin data.
**Prevention:** Pin dependencies to specific versions and calculate SHA-384 hashes using `openssl dgst -sha384 -binary | openssl base64 -A` to use for the `integrity` attribute. Add automated checks if possible to ensure new script tags include SRI.

## 2024-05-20 - [Insecure window.open for external link (prediction)]

**Vulnerability:** `window.open` was used without `noopener` and `noreferrer` to open an external link. This could allow the newly opened page to access the `window.opener` object, potentially enabling it to navigate the original page to a malicious URL or access sensitive information if the external site is compromised or malicious.
**Learning:** Whenever opening external, untrusted links using `window.open` or `<a target="_blank">`, it is crucial to severe the connection between the original page and the new tab/window to prevent Reverse Tabnabbing and information leakage.
**Prevention:** Always append `"noopener,noreferrer"` as the third argument to `window.open`, or use `rel="noopener noreferrer"` for `<a>` tags when linking to external resources.
**Vulnerability:** External scripts loaded from CDNs (`jsQR.min.js`, `Chart.js`) without integrity checks. This allowed potential execution of malicious code if the CDN was compromised, leading to XSS or data theft.
**Learning:** Always use `integrity` and `crossorigin="anonymous"` attributes when loading external resources via CDN to verify their integrity and prevent unauthorized modifications. The `crossorigin` attribute ensures proper error logging and avoids exposing sensitive cross-origin data.
**Prevention:** Pin dependencies to specific versions and calculate SHA-384 hashes using `openssl dgst -sha384 -binary | openssl base64 -A` to use for the `integrity` attribute. Add automated checks if possible to ensure new script tags include SRI.
## 2024-05-20 - [Insecure window.open for external link (prediction)]
 **Vulnerability:** `window.open` was used without `noopener` and `noreferrer` to open an external link. This could allow the newly opened page to access the `window.opener` object, potentially enabling it to navigate the original page to a malicious URL or access sensitive information if the external site is compromised or malicious.
 **Learning:** Whenever opening external, untrusted links using `window.open` or `<a target="_blank">`, it is crucial to severe the connection between the original page and the new tab/window to prevent Reverse Tabnabbing and information leakage.
 **Prevention:** Always append `"noopener,noreferrer"` as the third argument to `window.open`, or use `rel="noopener noreferrer"` for `<a>` tags when linking to external resources.
## 2025-02-15 - Buffer Overflow Prevention with snprintf
 **Vulnerability:** `sprintf` was used to write string data into fixed-size buffers, posing a risk of buffer overflow if the source data exceeds the buffer size.
 **Learning:** In C/C++, relying on `sprintf` for string formatting into statically allocated arrays is a common source of memory corruption vulnerabilities.
 **Prevention:** Always use `snprintf` with `sizeof(buffer)` when formatting strings into fixed-size buffers. This safely truncates the string if it exceeds the boundary, avoiding data overflow.
## 2024-05-21 - [DOM Manipulation Security Improvement]
 **Vulnerability:** Potential XSS via `innerHTML` when dynamically generating UI elements.
 **Learning:** Even when injecting static, seemingly safe HTML strings, using `innerHTML` is bad practice. It establishes a vulnerable pattern that can be easily exploited later if dynamic data is introduced. Replacing it with `document.createElement` and `textContent` is not only more secure but also a recommended best practice for modern JavaScript applications.
 **Prevention:** Always use safe DOM APIs like `document.createElement()`, `element.textContent`, or `element.appendChild()` instead of `innerHTML` for adding or modifying UI components dynamically.
## 2026-05-21 - [Potential Buffer Overflow in C/C++ Firmware via sprintf]
 **Vulnerability:** `sprintf` was used to write string data into fixed-size buffers, posing a risk of buffer overflow if the source data exceeds the buffer size.
 **Learning:** In C/C++, relying on `sprintf` for string formatting into statically allocated arrays is a common source of memory corruption vulnerabilities, especially in embedded systems where memory is constrained. Even if the current format string seems safe, future changes can easily introduce vulnerabilities.
 **Prevention:** Always use `snprintf` with `sizeof(buffer)` when formatting strings into fixed-size buffers. This safely truncates the string if it exceeds the boundary, avoiding buffer overflows.
## 2025-02-28 - [Title] Fix XSS Vulnerability in UI Updates
 **Vulnerability:** Unsafe `.innerHTML` assignments using external responses (like AI content) and error messages.
 **Learning:** Direct `.innerHTML` assignments with untrusted payload allows arbitrary JavaScript execution leading to Cross-Site Scripting (XSS).
 **Prevention:** Use safe DOM manipulation strategies such as `.replaceChildren()`, `document.createElement()`, and `.textContent` for updating contents defensively in UI elements.
