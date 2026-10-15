# PXA Browser

[简体中文](README.md)

A lightweight reading browser built with the C++26 PXA SDK and streaming HTTP.
The app is named PXA Browser (PXA 浏览器 in Chinese), with a white globe on a blue icon.
Its `pxa-browser` application ID is unchanged, preserving existing bookmarks and settings.
`assets/icon.svg` is the source; packages use the matching `assets/icon.png`.
Enter a URL or search terms, tap links, and drag to scroll. It supports HTTP/HTTPS,
redirects, relative links, back/forward history, bookmarks, system text input,
basic GET/POST forms, and DPI/shape/safe-area adaptation.
The bottom icons are Back, Forward, Home, Reload/Stop, and More. Hold an enabled
icon to show its name without navigating.

Tap a loaded, unlinked image to preview it; linked pictures still navigate on tap
and open the preview on a long press. Preview controls are Back to page, Zoom out,
Fit, and Zoom in. Drag to pan at 2×–8×; returning preserves the reading position.
Zoom is relative to the fitted view and reuses the existing decoded pixels and
cache, so it does not download again or restore details lost when downsampling.
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
Address, content, and toolbar use their own centered safe horizontal bands,
allowing rounded and circular screens to use more of their middle area.
See the [layout and volume-key validation report](docs/layout-volume-20261015.zh-CN.md)
for device screenshots, actual display FPS, memory windows, and test commands.

Search uses Bing RSS by default. DuckDuckGo Lite is selectable in More; its image
challenges cannot be completed by this reading browser. No JavaScript, full CSS,
video, CAPTCHA or complex authentication flows. Basic password inputs and bounded,
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

The updated standalone DevKit can run `pxa run pxa-apps/browser`.
Network access asks for permission. Local HTTP addresses must include `http://`.
Prefix input with `?` to force a search.

The app consumes streamed chunks instead of retaining the entire HTML response.
Readable text is limited to 48 KiB, transfer size to 256 KiB, bookmarks to 8,
history to 24, and Guest linear memory to 512 KiB (initially 128 KiB).
Canvas commands use an explicit 4000-byte budget, without an extra framebuffer.
The app keeps O3 and enables CMake interprocedural optimization; device checks
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
data URLs, CSS backgrounds, and script-loaded images remain unsupported.
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

The directory now includes one TXT Books entry, 10000txt, Nunubook, Sina Mobile, Gmanhua's catalog, hao123 and 2345. Gmanhua chapter pictures require JavaScript. WeRead receives a per-origin old Kindle UA and reaches its simplified login page, but QR login still requires JavaScript and is unsupported. Changing UA alone does not provide authenticated reading.

See the [site and encoding validation report](docs/sites-encoding-20261015.zh-CN.md) for actual PXA screenshots and limitations.

Status messages use spare safe space below the toolbar when available; otherwise they briefly overlay the document. Loading remains visible, normal messages hide after about 4 seconds, and load failures after about 8 seconds. No permanent status row is reserved. Menus and text editing hide the message.
