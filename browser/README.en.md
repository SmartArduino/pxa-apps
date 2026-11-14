# PXA Browser

[简体中文](README.md)

A lightweight reading browser built with the C++26 PXA SDK and streaming HTTP.
The app is named PXA Browser (PXA 浏览器 in Chinese), with a white globe on a blue icon.
Its `pxa-browser` application ID is unchanged, preserving existing bookmarks and settings.
`assets/icon.svg` is the source; packages use the matching `assets/icon.png`.
Enter a URL or search terms, tap links, and drag to scroll. It supports HTTP/HTTPS,
redirects, relative links, back/forward history, bookmarks, system text input,
basic GET/POST forms, and DPI/shape/safe-area adaptation.
Version 0.1.9 adds bounded text CSS from preceding inline style blocks and style
attributes: simple tag/class/ID compounds, cascade and `!important`, inherited
colors/font sizes, display, whitespace and left/center/right alignment. Styles
survive script repaints. Limits are 8 KiB of CSS, 64 rules and 32 nested states;
font sizes follow the reading font within 0.5–2 times its size. Preformatted text
wraps, and light colors darken against the fixed reading background. External
stylesheets, complex selectors, media queries, full box layout and retroactive
restyling of already emitted content remain unsupported.
Version 0.1.10 caches up to 16 repeated style matches with at most 256 bytes per
key; larger attributes bypass the cache. Cached declarations still resolve
relative font sizes and inherited values per element, and later style blocks
invalidate cached matches. Comment-free CSS is parsed without a copied buffer;
plain text and unstyled pages do not allocate a match cache.
Radio groups, disabled controls, explicit empty checkbox values and field
`aria-label` captions are supported. RSS 2.0 and Atom feeds display linked
articles and summaries within an 8 KiB per-entry limit. Navigation includes
Chinese poetry, IT Home and CNBlogs feeds, Chinese Wikisource and NPR's text
edition. These entry pages and scrolling were checked in the real simulator;
linked pages, login and live availability have separate compatibility limits.
The parser skips discarded raw text by transport chunk and batches validated
UTF-8 literals without a second document buffer. Decoded images retain explicit
HTML width limits; small icons request smaller output from the native decoder.
The bottom icons are Back, Forward, Home, Reload/Stop, and More. Hold an enabled
icon to show its name without navigating.

More → Find in page searches the laid-out page text, with exact Chinese and
case-insensitive ASCII matching, highlighting the line fragments containing the
current result. Previous/Next wrap through results; the address
bar shows the current result and total. Tap it or Edit to change the query.
Close or system Back exits find and keeps the current reading position. Queries
are limited to 128 UTF-8 bytes and are not saved. Form fields, passwords and
image alternatives are excluded, and finding never reloads the page.

More → Comic reader provides width-fit reading, image navigation, screen fit,
dragging and 1/2/4/8× zoom by tapping the header. Declared `rel=next/prev` links
continue into adjacent chapters. Detail mode downloads and decodes again with
up to 262144 pixels and retains at most one detailed image. Normal decoding
keeps its 32768-pixel limit; encoded images remain capped at 512 KiB. Loading a chapter title does not verify its comic pages, and memory-saving mode does not reduce the server image's encoded size. Closing
restores the webpage position. Images still depend on the Host budget; WebP and
complex scripted/encrypted readers remain unsupported. Chapter pages can retain
up to 96 bounded image descriptors, with a visible warning when images are omitted.

Image failures retain their cause across page-script reordering. Returning to
an unsupported, over-limit, denied or disabled-GIF image in the comic reader
does not automatically fetch it again. Tap the failed image or change quality
to retry explicitly. Temporary network failures and cancelled downloads can
still resume when entering the reader.

Tap a loaded, unlinked image to preview it; linked pictures still navigate on tap
and open the preview on a long press. Preview controls are Back to page, Zoom out,
Fit, and Zoom in. Drag to pan at 2×–8×; returning preserves the reading position.
Zoom is relative to the fitted view and reuses the existing decoded pixels and
cache, so it does not download again or restore details lost when downsampling.
If a later GIF frame fails, preview shows the failure and offers a tap to retry
without following a linked image's hyperlink. Returning to a GIF preview from
the background reloads its released decoder while preserving zoom and pan;
the webpage itself is not fetched again.
See the [JPEG and preview measurements](docs/jpeg-preview-20261015.zh-CN.md) for
the on-device codec comparison, memory accounting, and validation.

Home contains search, a curated lightweight directory, and your own saved sites. Add a website on Home or
bookmark the current page from More; up to eight sites persist across restarts.
Hold a site to remove it. More → Exit browser finishes saving preferences before
closing normally. The right scrollbar supports dragging and track taps, appears
while scrolling, and hides about 0.9 seconds after interaction stops. Its touch
area is wider than its visible thumb, without allocating a backing bitmap.
More also offers volume-key paging, off by default: Volume + scrolls up and
Volume − scrolls down; holding a key repeats. The preference survives restarts.
Text editing and background operation return volume keys to the system.
On Hosts with HTTP limit diagnostics, confirmed body overflow stops without
an extra GET. Oversized response headers can still fall back to static reading
with scripts disabled. Older Hosts retain the safe fallback; POST is never
automatically replayed. Body and header budgets are unchanged.
Address and toolbar use centered safe horizontal bands. Reading content uses
each side's actual safe margin, without mirroring a one-sided gesture strip,
allowing rounded and circular screens to use more of their middle area.

URL editing, form submission, and script navigation preserve the previous
reading position. Back and Forward restore that position, including zero,
without jumping to the URL fragment again. History within the same complete
document restores directly without another network request; Refresh still
requests the page and preserves the reading position.
The clear icon beside Done empties the URL editor. Canceling editing preserves
the original page and reading position.
See the [layout and volume-key validation report](docs/layout-volume-20261015.zh-CN.md)
for device screenshots, actual display FPS, memory windows, and test commands.

Search uses Bing RSS by default. DuckDuckGo Lite is selectable in More; its image
challenges cannot be completed by this reading browser. Guest-only QuickJS supports classic scripts, a bounded DOM, events, timers and same-origin asynchronous XHR/fetch. Full CSS, modules, Canvas/WebGL, video, CAPTCHA and modern framework compatibility are not provided. Static pages create no VM; script scheduling and DOM paints pause during dragging or text editing. Web Storage is origin-isolated and session-only. See [JS integration and validation](docs/javascript-20261015.zh-CN.md). Basic password inputs and bounded,
session-only cookies support compatible login and search forms. More provides
history selection, cookie clearing and independently persisted image/GIF switches.
Unsupported encodings (other than UTF-8/GB2312/GBK/GB18030), compressed responses, and unsupported media receive explicit messages.

Requires an updated Host and SDK with Net 0.4 / Core 2.0 and the UI dynamic-text,
sized-text, text-input-control, password-input, FS atomic replacement, Assets 2.3 / image-decode / image-frames,
and Window 0.3.
Both the SDK and device firmware must provide the image decoder. From pxa-workspace:

```sh
tools/dev.sh sim browser --source-root local/pxa-apps
tools/app.sh build browser --target esp32s3
bash local/pxa-apps/browser/tests/test.sh
```

`tests/run_regressions.py` runs the text-container, selector, form, event, style, responsive-image, CSS, compatibility, comic, image/preview-failure, foreground-resume and find suites sequentially, starts the base fixture on `127.0.0.1:31878`, and saves each suite's exit status. Repeat `--suite` to select suites; add `--live` for author-site network checks. See `--help` for simulator, TRACE package, publisher public key, IME package and output arguments. Put output under ignored `local/artifacts/`.
`tests/preview_foreground_smoke.py` exercises real desktop navigation gestures. It defaults to `pxsys_desktop_simulator` next to the product simulator; use `--desktop-simulator` when running it separately to override that path.

The updated standalone DevKit can run `pxa run pxa-apps/browser`.
Network access asks for permission. Local HTTP addresses must include `http://`.
Prefix input with `?` to force a search.

The app consumes streamed chunks instead of retaining the entire HTML response.
Readable text is limited to 48 KiB, transfer size to 256 KiB, bookmarks to 8,
history to 24, and each component's Guest linear memory to 8 MiB, growing in 64 KiB pages.
The maximum is not preallocated. More → Script memory offers a persistent automatic
1 MiB ceiling or manual 256 KiB–4 MiB limits, effective on the next page load.
Automatic mode is a conservative fallback, not a measurement of device free RAM.
The optional JS heap lives inside linear memory; raising its limit reserves nothing.
The UI, scrolling, networking and images run in `main.aot`; QuickJS and the DOM
run in an on-demand interpreted `script.wasm` service with bounded, batched IPC.
Static-only runs never load that service. Closing a script page frees its VM,
but does not unload the module or shrink previously grown linear memory.
Interpretation trades script speed for storage; a smaller package is not a
claim of faster JavaScript. UI and service start at 128 KiB and 192 KiB respectively,
each growing in 64 KiB pages up to 8 MiB, subject to actual device memory.
See the [mixed-component design and measurements](docs/aot-wasm-20261016.zh-CN.md)
for storage, memory and interpretation tradeoffs.
The final pai-touch scroll windows measured about 29.0 / 30.3 displayed FPS
for static / scripted pages. Static PSRAM decreases, while active script pages
use about 0.50 MiB more steady PSRAM than the single-AOT baseline and execute
scripts more slowly. See the [stack and event validation](docs/mosaico-script-stack-20261016.zh-CN.md)
for the reproduced mosaico crash and fixes under the same 16 KiB interpreter
stack. Physical mosaico validation is still pending.
See the [measured JS report](docs/javascript-20261015.zh-CN.md)
and [original-page/image validation](docs/original-pages-images-20261016.zh-CN.md).
Canvas commands use an explicit 4000-byte budget, without an extra framebuffer.
The UI/SDK keeps O3; QuickJS uses Oz, with full LTO to reduce device AOT loading
and initial linear memory costs. Device checks
cover both cold-start peaks and steady memory use.
Containers grow on demand and reuse capacity; drawing does not allocate temporary containers.
Scrolling invalidates only the content/status region while retaining a complete
command list for later exposure. Clamped drags do not submit unchanged frames.
Fractional-DPI strokes round up so navigation icons remain visible.
The address bar, document and toolbar each avoid the actual panel outline,
using the wider middle of rounded/circular displays without an extra bitmap.
Ordinary Latin words wrap as a unit; long URLs and code may wrap by character.
Widths remain estimates, without font shaping or CSS layout. More offers
lightweight/mobile compatibility identities for the next load. Both send
`Save-Data: on`; servers may ignore these preferences. Visible PNG and baseline JPEG
images load automatically, preserve aspect ratio, and use two optional cache slots.
PNG keeps transparency; JPEG uses native IDCT downscaling. Direct image URLs work too.
The ESP32-S3 ROM backend does not support grayscale JPEG; the desktop backend does.
Each input is limited to 128 KiB, decoded output to 32768 pixels, source dimensions to
4096 per edge and 1048576 total pixels. Evicted images reload on return; failed images
keep alternative text and can be tapped to retry. GIF is decoded on the Host after
download, with a 64 KiB input and 8192 output-pixel limit. Opaque animations use
RGB565; transparent ones use BGRA8888. Only visible animations advance, menus and
editing pause them, and backgrounding releases their decoder state. Large GIFs
lose detail when downscaled; missed frames are not queued. Progressive JPEG, WebP, SVG,
other data URLs and CSS backgrounds remain unsupported. Script updates to `img.src`
and strict Base64 PNG data URLs work, limited to 4096 bytes each and 8192 retained
bytes per document. Inline PNGs reuse the Host decoder through a temporary private file.
See [image validation and memory measurements](docs/images-20261015.zh-CN.md).

Query URLs are not bookmarked. History is session-only, and POST bodies are not
replayed. HTTPS certificate verification belongs to the Host. This is not a full
privacy or authentication implementation.

See [Chinese documentation](README.md) for all capacity limits and test commands,
and [validation report](docs/validation-20261014.zh-CN.md) for measured results.
The [device performance report](docs/performance-20261014.zh-CN.md) includes actual
LCD completion rates and before/after measurements. Screenshot regression tests
require Python Pillow; optional `PXA_BROWSER_PROFILE=1` records aggregate timings
at load/gesture boundaries and is disabled in production builds.


Legacy Chinese pages now support GB2312/GBK/GB18030 decoding, selected from the HTTP declaration or a bounded 1024-byte HTML meta prescan. The 49,536-byte mapping is a package asset loaded only while decoding a legacy page, then freed; UTF-8 pages do not load it. Legacy-encoded form submission and other encodings such as Big5 remain unsupported.

Explicit `hidden` and inline `display:none` content is excluded. Repeated target URLs share the existing bounded link table; the browser reports when unique links exceed its capacity.

Title/textarea bodies remain text with character references decoded; textarea drops its initial newline and is displayed read-only (editing/submission remains unsupported). Xmp/plaintext preserve literal references, with plaintext lasting until EOF. Iframe/noembed/noframes fallback text creates no scripts, images, links or fields. Frames never auto-load; valid iframe src links can be opened explicitly, while srcdoc remains unsupported. Capture, DOM building and innerHTML use the same text contexts, with DOM source newlines normalized before entity decoding. Repaints project visible raw blocks as escaped preformatted text and protect dynamic style closing tags. The 256-byte title limit and existing document/DOM/script/image budgets remain unchanged.

Markup assigned through `innerHTML` is parsed into a detached staging node before replacing children. Catchable fragment byte/node/attribute/style failures retain old node identity, parent links, handlers and radio state, and restore the failed fragment's style allowance, allowing later valid updates. Parsed attributes initialize fresh nodes directly, avoiding redundant repaint notifications; duplicate HTML attributes retain their first value. The existing 8 KiB/96-token fragment limits and attribute/style budgets remain unchanged. Real VM allocation exhaustion and execution deadlines still stop scripts. This does not implement a complete HTML tree builder or transactions for arbitrary DOM operations.

Bounded `classList` supports add/remove/toggle/replace/contains, raw value reflection, live indices/length and iteration including forEach. It retains object identity and follows className, setAttribute/removeAttribute and CSS selector changes. Tokens split on ASCII whitespace, deduplicate in order, and invalid arguments or attribute quotas leave the prior class value intact. Lists are created on demand, reuse cached tokens, and unchanged operations do not request snapshots. Existing 24-attribute and 4096-character value limits remain. DOMException exposes names, messages, legacy codes and constants; invalid/unsupported selectors raise a SyntaxError DOMException, while resource quotas remain RangeError. This does not provide a complete browser DOM.

`element.dataset` creates a same-object DOMStringMap view on demand. It supports data-* / camel-case conversion, string reads and writes, deletion, enumeration and immediate attribute API reflection, including lazy loaders such as `img.src = img.dataset.src`. It retains the ASCII attribute-name subset (letters, digits, underscore, colon, dot and hyphen), 64-character names, 24 attributes per node and 4096-character values. Unchanged writes stay idle. Accessor and explicitly non-configurable string descriptors are unsupported and rejected before mutation; symbol properties behave as ordinary object properties.

Event handlers and listeners run with the target as this, preserving listener order, nested dispatch and recovery after caught exceptions without relying on an overridable call member.

Nodes expose read-only tagName through the prototype and allocate style state on first use, avoiding duplicate tag names and empty style fields on every node.

Snapshots reuse original tag spellings of at most 64 characters within each batch; rendering a control as a link does not overwrite the cached spelling. Hit counts advance with each snapshot instead of enumerating the entire mapping per interactive node. Short closing tags normalize once before scanning the open stack. Traversal, interaction and fragment budgets remain unchanged.

HTML getters advance bounded snapshots directly. Escaping consumes matches individually instead of retaining callback frames and a complete match list, preserving the existing ampersand, bracket, quote and NUL replacements. Longer tag names retain the original conversion path; no new tag-length limit is imposed.

For nodes whose attribute names are all ASCII and at most 64 bytes, the script DOM parser normalizes names in native code and reuses runtime attribute-name strings. Nodes without attributes pass an empty marker rather than allocating an array. Non-ASCII or longer names retain the existing script path; values, first duplicate values and quota checks remain unchanged. Build and verify the native bridge and compiled runtime together, as their internal argument conventions are versioned together.




Inline script styles support property assignment, cssText, setProperty, getPropertyValue, getPropertyPriority, removeProperty, length and item. Replacing/removing the style attribute clears old declarations while preserving existing style references. Style-only edits schedule a repaint; unchanged assignments stay idle. Declarations allocate lazily, with at most 32 properties and 4096 serialized UTF-16 code units per element, with at most 16 nested bracket groups in values; quota failures preserve the previous value. Parsing preserves quoted/parenthesized semicolons and recognizes !important. Snapshots reuse cached declaration text. This bounded API does not provide full CSSOM, shorthand expansion or CSS variable evaluation; rendering still applies only the documented text CSS subset.

Script querySelector/querySelectorAll, matches and closest support compound tag/universal/ID/class selectors, attribute presence and comparison (=, ~=, |=, ^=, $=, *= with ASCII i/s flags), descendant/child relations, comma lists and :scope. Queries return descendants in tree order; ancestor matching can extend outside the query root, while :scope explicitly identifies it. Collections, including getElementsByTagName and getElementsByClassName, are static arrays with item(), not live NodeList/HTMLCollection objects. A selector is limited to 512 UTF-16 code units, 8 groups, 32 steps and 64 conditions; a query visits at most 512 nodes with 8192 element/selector-step matching attempts. Only the last parsed selector is cached; matching nodes are not cached. Attribute values compare exactly by default; input/button type, form method/enctype and dir default to ASCII folding, with explicit i/s taking priority. CSS escapes, sibling relations and other pseudo-classes/elements are unsupported and throw SyntaxError; quotas throw RangeError. This script API does not extend stylesheet rendering.

After a page is painted, callbacks start DOM snapshots only when content, styles or click targets change. Empty callbacks, unchanged text/styles and duplicate listeners do not start snapshots. Dynamic onclick and click-listener registration/removal update hit regions, including handlers attached from an XHR callback. Removing the last handler restores ordinary content. Assigning null/undefined to onclick disables an inline handler; setting the same onclick attribute again reactivates it. Each event type retains its 16-listener limit; duplicate functions consume no extra slot. This remains a bounded event API, not a complete DOM event model.

Basic forms serialize inputs and the activated submit button in DOM tree order, preserving duplicate-name order, button values, checked state and UTF-8 form encoding. Disabled controls do not activate; ordinary/reset buttons do not submit. GET replaces an existing action query and retains its fragment; POST retains both. Scripts can reflect action/method/enctype, call submit() directly, or use bounded requestSubmit([button]) with a cancellable submit event and event.submitter. Full constraint validation is not implemented. Static forms retain 24 fields and a 2 KiB encoded body; script forms retain the existing DOM and 8 KiB body limits. GET URLs remain capped at 1024 bytes. Default-value reset, external form associations, submitter override attributes, file uploads, full select/textarea and multipart/text/plain POST remain unsupported; reset behavior can be supplied by page scripts.

The directory includes TXT Books, 10000txt, Nunubook, Sina Mobile, Gmanhua's catalog, hao123, 2345 and WeRead. Gmanhua chapter pictures still need unsupported browser APIs. WeRead uses an old Kindle UA to request the official lightweight page. Dedicated login/shelf/reader page replacements have been removed; original-page login and reading remain unverified. Earlier QR/login evidence came from the removed adapter and does not prove original-page compatibility. See the [current validation report](docs/original-pages-images-20261016.zh-CN.md).

QuickJS remains the default. See the [engine evaluation](docs/engine-selection-20261016.zh-CN.md) for measured JerryScript prototype sizes and MicroQuickJS compatibility limitations. The AOT UI plus interpreted Wasm script service is implemented; its measured tradeoffs are documented above.

See the [site and encoding validation report](docs/sites-encoding-20261015.zh-CN.md) for actual PXA screenshots and limitations.

Status messages use spare safe space below the toolbar when available; otherwise they briefly overlay the document. Loading remains visible, normal messages hide after about 4 seconds, and load failures after about 8 seconds. No permanent status row is reserved. Menus and text editing hide the message.

Original HTML, links and page scripts are retained. Automatic WeRead/comic page replacements have been removed. WeRead's unmodified login scripts pass the simulator protocol fixture; real-account login, bookshelf and reading remain unverified. Compilation now polls cancellation in the lexer and between passes. Deadlines are cooperative, not strict bounds on every native operation; Sparse array copies now skip unobservable holes and poll cancellation; real Proxy callbacks have strict recovery tests. Exact-source, app-signed library assets now let the original scripts reach a real QR on pai-touch without extending the compilation deadline. Source hashing advances cooperatively in the existing scratch buffer, and inactive phase tracing is omitted from production Wasm. P50 verified the current original-page QR path on the same pai-touch firmware, but still reported a roughly 1.43-second turn; latency is not accepted. The larger-read candidate remains unmerged because its measured PSRAM peak did not meet the no-growth gate. External source compilation now borrows its owned response and page filename rather than duplicating them; simulator and boundary checks pass with no larger linear high-water. Real-account login and reading remain unverified. See [signed library and preview measurements](docs/signed-script-libraries-20261017.zh-CN.md). See the [original-script report](docs/original-page-scripts-20261017.zh-CN.md). Generic JPEG MCU/scanline adapters now accept up to 4096 pixels per side with bounded output. PNG, GIF and full-frame LVGL backends retain their one-megapixel limit. Browser image input allows up to 512 KiB, while its two 32768-pixel caches and GIF limits remain unchanged. See the [current report](docs/original-pages-images-20261016.zh-CN.md).

Interpreter scheduling, recursive callback limits and allocation measurements: [validation report (Chinese)](docs/script-scheduling-20261017.zh-CN.md).


## 0.1.5: bounded compressed responses

HTML, readable JSON/Markdown, classic scripts and XHR/fetch responses support streaming gzip and standard zlib-wrapped HTTP deflate. Network page bodies remain capped at 512 KiB; decoded document scanning is capped at 2 MiB and retained readable text at 48 KiB. Existing script/DOM/response quotas remain in force. Identity responses allocate no inflate window; compressed input uses a lazy 1 KiB scratch buffer and a 48 KiB hard cap on zlib allocations. Reading yields after four calls or about 16 KiB of decoded data, bounding native AOT continuation depth even on already-buffered identity responses. Checksums, truncation and gzip member counts are validated.

New navigations and Home release the old document/layout buffers. In-page DOM updates continue reusing capacity. Guest linear memory does not shrink after growing; released space can be reused by subsequent pages.

Version 0.1.11 validates responsive image candidates and selects the first usable one in author order, preserving URL commas. Invalid or known unsupported lazy addresses fall through to later alternatives; data-srcset and data-lazy-srcset are supported. Each set is limited to 4 KiB / 32 candidates without a retained candidate list. Picture can choose PNG/JPEG/GIF alternatives to explicitly unsupported formats. Only absent media or simple all/screen apply; sizes calculations, media queries and DPR switching are not implemented. Scripts can reflect srcset, sizes, media and loading attributes. The original img source remains the fallback. Existing image formats, visibility and size limits still apply. JSON and Markdown are rendered as text, never as HTML.

The private inflate-only zlib 1.4 dependency retains its Zlib license, source record and file hashes under `vendor/zlib`; its license is included in package assets. No Host interface changes are required. Brotli, stacked codings, raw DEFLATE, compressed images and full modern CSS/JS framework compatibility are not provided.

Run `bash browser/tests/test_content_encoding.sh` alongside the core/script suites. Runtime logs distinguish wire/decoded bytes and bounded inflater allocations; document buffer diagnostics exclude the VM and Host allocations.

For an integration fixture, run `python3 browser/tests/compression_fixture.py --port 31878` (add `--bind 0.0.0.0` for devices). Open `/compat`, or run `browser/tests/compression_smoke.py --help` for the real PXA simulator regression. Build with `PXA_BROWSER_TRACE=1` and direct `--output` to an ignored `local/artifacts/` directory. The suite covers compressed HTML/JavaScript, a large CSS prefix, decoded quotas, malformed streams and releasing document/layout buffers on Home.
