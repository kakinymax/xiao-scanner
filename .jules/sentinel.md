## 2024-05-19 - [XSS via unescaped innerHTML for External API Content]

**Vulnerability:** The application was using `innerHTML` to directly insert the response text from the Gemini API and error messages into the DOM without any sanitization. If the API were tricked into returning malicious code (via prompt injection or other means), or if an error message contained malicious strings, it would be executed in the user's browser, leading to a Cross-Site Scripting (XSS) attack.
**Learning:** Even when interacting with trusted APIs like Gemini, any text originating from outside the application's immediate control must be treated as untrusted, especially if it can be influenced by malicious prompts. Directly inserting unescaped external text into the DOM via `innerHTML` is a critical security risk.
**Prevention:** Always sanitize or escape any dynamic text before rendering it as HTML. We implemented a lightweight `escapeHTML` function to convert potentially dangerous characters (`&`, `<`, `>`, `"`, `'`) into their corresponding safe HTML entities. Ideally, where possible, prefer using `textContent` over `innerHTML` to completely avoid parsing HTML from untrusted sources, though here `innerHTML` was used due to line breaks.

## 2024-05-24 - Remove LocalStorage API Key Dependency

**Vulnerability:** Insecure Storage of API Key in LocalStorage. Storing sensitive API keys in `localStorage` makes them susceptible to XSS (Cross-Site Scripting) attacks, as any script running on the domain can access them.

**Learning:** When a client-side application requires interacting with an API that uses secrets, it's safer to avoid storing the secret on the client entirely if a secure backend proxy cannot be established. For applications integrating with external AI chat platforms (like Gemini), a "semi-automatic" workflow that generates the prompt and copies it to the clipboard, then redirects the user to the platform's UI, is a highly secure alternative that requires zero secret storage.

**Prevention:** Never store sensitive API keys in `localStorage`, `sessionStorage`, or cookies without `HttpOnly`. Rely on secure backend proxies for API communication, or design architectures that delegate the API interaction to a secure external client (like the official Gemini web app) via clipboard or intents.

## 2024-05-20 - [Insecure window.open for external link (prediction)]

**Vulnerability:** `window.open` was used without `noopener` and `noreferrer` to open an external link. This could allow the newly opened page to access the `window.opener` object, potentially enabling it to navigate the original page to a malicious URL or access sensitive information if the external site is compromised or malicious.
**Learning:** Whenever opening external, untrusted links using `window.open` or `<a target="_blank">`, it is crucial to severe the connection between the original page and the new tab/window to prevent Reverse Tabnabbing and information leakage.
**Prevention:** Always append `"noopener,noreferrer"` as the third argument to `window.open`, or use `rel="noopener noreferrer"` for `<a>` tags when linking to external resources.

## 2024-06-25 - [Missing Subresource Integrity (SRI) on External Scripts]

**Vulnerability:** The application was loading external JavaScript dependencies (`jsQR`, `chart.js`) from a CDN without using Subresource Integrity (SRI). Furthermore, `chart.js` did not have a version pinned. This creates a risk where if the CDN is compromised, or a malicious version is pushed as "latest", the application would execute the attacker's code, leading to XSS or data exfiltration.
**Learning:** Loading third-party scripts without verification blindly trusts the provider. Even reputable CDNs can be compromised or hijacked. Pinning versions ensures stability, and SRI ensures that the file content exactly matches what the developer expected when the integration was tested.
**Prevention:** Always pin dependencies to specific versions and add `integrity` attributes with cryptographic hashes to `<script>` and `<link>` tags loading external resources. Ensure `crossorigin="anonymous"` is also set to prevent credential leaks.
