# PXA App Store

A reference PXA application that browses the DOIT PXA device catalog
(`https://app.doit.am`), shows each application's signed metadata,
compatibility, permissions and service requirements, and requests Host-managed
download and installation. Icons load independently of catalog pages; application
details show a bounded screenshot gallery. Loaded cards stay usable while the
next page or an image is downloading.

The catalog marks installed apps and available updates. **Installed** lists
device apps and provides **Check updates**, which queries each installed app's
detail endpoint rather than relying on the currently loaded, filtered catalog
window. Individual lookup failures are reported as a partial check. **Downloads**
retains verified packages in Host storage so
they can be installed later or deleted. Failed downloads have removable entries
for the current Store session. After confirmation and successful installation,
the Host displays a localized result dialog with **Open** and **Confirm**;
the Store also shows the installed app on a success card. Host download records survive a reboot;
unfinished files are removed during startup cleanup. The manager keeps at most
eight completed downloads and the installed view lists up to twelve apps.

Removable installed apps show **Uninstall** next to **Open** and any available
update. This sends only the App ID to the Host; the Host resolves the unique
installed identity, refuses built-in or active apps, and asks for confirmation
in a bilingual system dialog before transactional removal. The installed list
refreshes only after the Host reports success and shows a localized completion
dialog. The Store does not offer to
uninstall itself.

## Account and likes

Open **Account → Sign in**, visit `https://app.doit.am/device` on a phone,
then enter the short code and explicitly confirm the matching device/account.
Codes expire after ten minutes. The device never handles the website password.
The revocable credential lasts 30 days and is stored by the app-scoped Storage
service. Password changes and account suspension invalidate device credentials;
**Sign out** removes the local credential and revokes it online. If storage is
unavailable, a successful sign-in lasts only for the current app session.

An application detail includes **Like / Liked · undo**. Counts and personal
likes share the website's data, keyed by the full PXA application identity.
Failed requests retain the last confirmed state and provide a retry action.
Account controls are semantic system buttons, available to touch and gamepads,
and follow the system language and theme. Authorization polls every five
seconds while waiting, pauses in the background and stops after expiry;
ordinary browsing has no account polling.

## Building

```sh
tools/app.sh build store --target simulator --source-root local/pxa-apps
tools/app.sh build store --target esp32s3 --source-root local/pxa-apps
```

## Configuration

| Define | Default | Notes |
| --- | --- | --- |
| `PXA_STORE_ORIGIN` | `https://app.doit.am` | Must stay byte-identical to the signed `net.client` scope in `package.json`; the Host compares the parsed URL origin with that scope. |
| `PXA_STORE_CHANNEL` | `stable` | `stable` or `beta`. |

Hardware device identity is optional. Denying `device.identity` continues
without a rollout device ID; browsing requires only the configured origin's
network permission.

The Store queries Device ABI 0.2 at startup to select a catalog profile from
the Host target and supported AOT formats. Catalog profile names are server
keys, not WAMR engine ABI strings. An unsupported Host or catalog profile does
not fall back to another board's artifacts. The current catalog must publish
an `esp32-s31-pxa-0.3.0-r2` profile and matching artifacts before Korvo can
install apps from it.

String overrides belong in `store_client.h` (or a properly quoted compiler
definition), and the matching `net.client` scope in `package.json` must be
updated before signing. `PXA_APP_DEFINES` accepts numeric/identifier values,
not URL string literals. Use an isolated source copy for local HTTP fixtures;
keep the distributed package scoped to its production HTTPS origin.

## Layout

The catalog, installed apps and downloads share a five-destination bottom
navigation bar with plain icons and text labels. Search and back are icon
actions with accessible labels; categories open from **Filter**. Refresh, retry
and manual paging share the list footer. Library actions live in compact, theme-colored cards; the
scroll area ends above the navigation bar so actions never sit underneath it.
The screen follows the Host-reported panel size, density, safe area and system
bar insets. Environment dimensions, corner radii and Window insets are panel
pixels; style lengths are density-scaled dp (also named `PXA_UI_LENGTH_PX` in
the legacy codec). `store_layout.h` converts panel pixels to dp before layout,
rounding outward to protect safe areas. Both inset sources use top, right,
bottom, left internally. These conversions keep the bottom navigation at the
screen edge at 130 DPI as well as at high densities:

- `safe_insets` from the UI environment (start configuration and
  `PXA_UI_ENVIRONMENT_CHANGED`) cover the panel's own safe area: rounded
  corners, cutouts and any reserved region the product reports.
- `system-bar-insets` from the Window service snapshot cover the status and
  navigation bars. The App requests `get-snapshot` and also consumes
  `metrics-changed`, then pads by the larger value of both per edge. Nothing is
hardcoded: a Host that reports no bars simply gets no extra padding.
- The Store leaves system navigation visible and reserves the reported bottom
  inset once at the root; the tab bar ends above the system controls. Gesture navigation without a visible handle
  reserves no additional bottom bar inset (physical safe area still applies).
- Download, catalog and installation errors retain their inline status and
  additionally use the Window 0.2 toast ABI for a localized reason. Older Hosts
  without this optional ABI simply keep the inline status.
- Layout metrics (header, chips, cards, tab bar, keyboard) switch between
  compact, regular and large tables from the width in dp, so the same App
  stays usable on a 296x240 panel and on a phone-sized display.
- UI text uses six Host typography roles (caption 12, label 14, body 16,
  title 20, headline 24, display 28 pixels with the default theme). System
  colors are semantic tokens, not hardcoded light/dark values; the Host repaints
  retained theme-token nodes when the system palette changes.

The search screen uses a real text input (`PXA_UI_CONTROL_TEXT_INPUT`) and
subscribes to `PXA_UI_EVENT_TEXT`. It draws no keyboard of its own: the system
input method binds to the focused input, shows the nine key pad with pinyin
candidates, English multi tap and symbol pages on narrow panels, and the full
keyboard with pinyin on wide panels. The system reports each edit as a text
event, so the query follows the system keyboard, and the input's submitted
event (its confirm key) applies the search.
The search entry uses a centered, width-limited system-colored field with an
inline hint and a clear icon that appears only after text is entered. Clearing
the field patches the input in place, preserving the system keyboard.

Safe area insets and the layout's own margins do not stack: the root only adds
the part of an inset a screen margin does not already cover (`inset_padding`),
so a gesture strip never pushes the content further than the strip itself.
The search header instead uses its own horizontal safe band: rounded corners
reserve extra space beyond the search field on the right, while the back arrow
gets a narrower slot on the left. Circular panels keep their larger reported
shape insets, and rectangular displays retain the normal content margin.

On the catalog, the header, optional category row, scroll viewport and bottom
navigation are normal siblings in a column. No list content is drawn behind
an overlaid header or navigation bar. Categories are collapsed initially and
close after selection. Compact cards show the icon, name, version and installed
state; descriptions remain available on the detail page. Card borders, nested
name-row boxes, navigation capsules and the extra navigation wrapper are gone.
The detail page folds compatibility, permissions and other technical metadata
into an expandable section, without removing the information.
Its screenshot gallery sits below the App icon and release information, above
the description and changelog.
Its footer has one full-width action: **Install**, **Update**, or **Open**, as
appropriate. Install includes downloading; there is no separate Download button.

Wide displays (`>= 480` logical pixels, `>= 1100` for three columns) render the
catalog as a grid with fractional, equally sized columns through the UI grid
ABI. Narrow panels keep the single-column list.

The catalog requests enough entries to fill the viewport plus one row, bounded
by 12 entries. It prefetches when the current grid is still too short and
scrolling near the last row requests the next page without a button. A bounded
ring of 12 entries evicts the oldest rows as needed. Initial taxonomy changes
rebuild the catalog. Later pages append cards to the existing scroll object,
keeping active gestures intact. Eviction replaces only the bounded list
subtree and compensates the offset by the number of displaced rows. Download progress patches only its
row's text and progress bar when the displayed percentage changes. Waiting for
the next network chunk and checking each installed app do not redraw the whole
surface; the update check redraws on entry and completion.

The catalog response budget starts at a power of two estimated from the
viewport page size, with a 4 KiB minimum, and doubles on `limit-exceeded` up to
256 KiB. Storage uses the SDK allocator's `realloc`; the application never
manually grows Wasm memory alongside that allocator. The desktop Host now
distinguishes an exceeded response budget from allocation failure, matching
the ESP Host, so a large page can retry with a larger budget.

Image requests use the optional C++ HTTP streaming/image-decoding services.
Only viewport icons (plus a small look-ahead), or the selected detail icon and
screenshot, stay resident. The cache has eight slots and one request in flight;
leaving a page cancels unwanted work. Failed images retain a fallback instead
of blocking navigation, and explicit refresh/preview retry can retry them.
Only same-origin `/media/apps/` URLs are accepted. Each request uses a 2 KiB
stream ring, at most 128 KiB encoded data, and Host decoding limits of 64×64 /
4096 pixels for icons or 320×320 / 32768 pixels for previews. Encoded bodies and
pixel buffers do not pass through Guest memory. PNG/JPEG support comes from
the Host image service. A single-image gallery has no redundant paging bar.

Tap a screenshot to open its viewer. Previous/next controls and horizontal
swipes at fit size select another screenshot; double-tap or the zoom button
enlarges it. Zoomed images pan through the native scroll object. Returning to
the detail page restores its scroll position. The viewer reuses the same
bounded decoded image, so zooming does not fetch an original-resolution image
or allocate another framebuffer.

Image completion replaces only its icon/preview subtree. Scrolling without new
data emits no UI commits and does not rebuild the list. There is no full-screen
bitmap cache or additional compositing layer in this app.

The UI environment's circle or corner-radius geometry contributes to the
effective safe insets, so interactive content stays inside the inscribed
rectangle even if a product reports only small physical safe insets.

## Backend contract

The App reads the signed device API and skips envelope signature verification:

```text
GET /api/v2/catalog?profile=...&channel=stable&view=compact&page_size=2..12
    [&kind=game|app][&category=SLUG][&cursor=N][&device_id=MAC][&q=QUERY]
GET /api/v2/apps/{app_id}?profile=...&channel=stable&view=compact[&device_id=MAC]
```

`kind`/`category` filter the catalog server-side; the first compact page also
carries the signed `taxonomy` (kinds and categories with Chinese and English
labels) that the App renders as its bottom tabs and category chips.

`view=compact` keeps paging responsive on memory-constrained boards. A full
catalog item is about 4.3 KiB: it embeds the Host
capability declaration, the publisher SPKI, all three digests and the full
compatibility detail. The compact catalog entry drops the repeated Host
declaration, the SPKI, the container/manifest digests, the download ticket URL
and the changelog, truncates free text, and caps the permission list. The
compact application detail keeps the full
compatibility, permission, service and artifact detail for one entry.

Measured against the production catalog (`pxa-voxel-craft`):

| Response | Bytes |
| --- | ---: |
| `GET /api/v2/catalog` (full, `page_size=1`) | 4578 |
| `GET /api/v2/catalog?view=compact&page_size=2` | 2152 |
| `GET /api/v2/apps/{id}?view=compact` | 2927 |

The device identity (`device.identity` / `mac.wifi.station.hardware`) is only
used for `device_id` rollout bucketing; when it is unavailable the App still
loads the catalog.

## Installation

Catalog cards and the detail header show the complete fresh download: the
selected program container plus its bound resource container. Resource apps
also show the program/resource breakdown and explain that already installed
resources may be reused. All sizes use binary units; disk allocation and the
actual transfer are decided by the Host's install plan.

The parser keeps `size` as the program transport length for the installer and
reads the additive `resource_size` display field. A missing or null resource
size produces an unknown total, including with older servers; it never presents
code-only bytes as the complete download of a resource-dependent app. Total
addition is checked for overflow and uses no extra payload buffer.

The detail parser retains the short-lived `download_ticket_url`. The
`store_client_build_download_url` helper only accepts a same-origin artifact
ticket with the expected route and query shape. The ticket is not a trust
decision: the catalog envelope is not signature-verified by this App, and
the Host must independently authenticate the download and verify the signed
package before installation. Run the focused native test with:

```sh
clang++ -std=c++26 -Wall -Wextra -Werror -Wno-attributes \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Ilocal/pxa-apps/store -Ideps/pxa-system/sdk/guest-c/include -Ideps/pxa-system/sdk/guest-cpp/include \
  local/pxa-apps/store/tests/store_ticket_test.cpp \
  local/pxa-apps/store/store_client.cpp \
  local/pxa-apps/store/store_json.cpp -o /tmp/pxa-store-ticket-test
/tmp/pxa-store-ticket-test
```

The catalog parser truncates long text only at complete UTF-8 code points;
invalid Unicode is rejected even after the display buffer is full. This avoids
an invalid UI transaction leaving the catalog on its loading screen. Run the
Chinese, emoji, escaped Unicode and malformed-input regression with:

```sh
clang++ -std=c++26 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ilocal/pxa-apps/store local/pxa-apps/store/tests/store_utf8_test.cpp \
  local/pxa-apps/store/store_json.cpp -o /tmp/pxa-store-utf8-test
/tmp/pxa-store-utf8-test
```

Check pixel/dp conversion at 80–480 DPI and asymmetric Window inset ordering:

```sh
clang++ -std=c++26 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ilocal/pxa-apps/store local/pxa-apps/store/tests/store_layout_test.cpp \
  -o /tmp/pxa-store-layout-test
/tmp/pxa-store-layout-test
```

The application tab uses U+F015 (HOME), available in both LVGL symbol fonts
and the product's compact Font Awesome font. U+F00B (TH_LIST) is absent in the
product font and must not be assumed available merely because it works in the
desktop simulator. See [device verification](docs/dpi-layout-20261014/README.zh-CN.md).

Pressing **Install** sends a bounded ticket, identity, size and digest to the
Host's `store-installer` API (Service 20). The Host authenticates the signed
manifest prefix before downloading the container, then asks for native
confirmation. The dialog offers eligible storage volumes, with fixed managed
directories; the Guest never supplies installation paths. Updates retain their
current application volume. Explicit relocation requires a separate migration operation.

Store Installer 0.8 authenticates both application and referenced resource
metadata before confirmation. Its native dialog has separate application/data
and resource volume choices, normally selecting one fitting volume for both.
Admission reserves both destinations atomically and accounts for the actual
transfer/preparation peak. Resources are fully verified before code commits;
existing immutable generations can be reused. After transfer reaches 100%, the
Store displays verification and installation while the Host finishes.

The Store queries Host capabilities before choosing its catalog baseline:
resource installation plus volume placement selects `*-pxa-0.5.0-r2`; older Hosts
retain `*-pxa-0.4.0-r3`. Capability bits do not imply resource offline caching.
Resource-bundle DOWNLOAD currently returns UNSUPPORTED; cached code can be
installed when its exact resources have already been prepared.

The Host reserves the selected volume for the encoded transfer, decoded files,
local control records and initial private storage. It streams the container
there, checks its exact size and SHA-256, then runs the signed transactional
installer. Approval binds the exact manifest and previous installation record;
a concurrent change fails instead of being silently overwritten. Capacity is
limited by the selected filesystem and Host profile, rather than a fixed 4 MiB
Flash staging area. The calling App's private FS quota does not apply.

Opcode 2 retains an offline transport copy and an authenticated ownership
descriptor on that same volume. Downloads are rediscovered from mounted volumes
without a primary-Flash index. Opcode 3 accepts only the calling App's cache
token and asks for a fresh installation confirmation; it does not redownload
the container. Declining or failing that installation preserves the offline
copy for retry; successful installation consumes it. Interrupted transfers
without completion descriptors are reclaimed at startup. Stopping the
requesting App cancels unfinished work; an atomic commit already in progress
finishes safely.

The Host accepts only its configured HTTPS origin and artifact-ticket paths.
The `install-metadata` endpoint serves the bounded signed prefix without
counting a download. With an older backend, the Host requests a range from the
container endpoint, reads only the prefix and closes the response. Full
container and decoded-file verification remain mandatory before installation.

Since 0.8.7 the App preserves synchronous installer errors instead of reporting
every rejected request as a network outage. The ESP Host must include the open
installer policy: older firmware only accepts built-in `pxa-store` packages and
rejects a normally installed or updated Store with `DENIED`. Updating the App
alone cannot change that Host policy. Every install/update/offline install still
requires a fresh native confirmation; the API has no silent-install option.
See [the Mosaico diagnosis and validation](docs/install-access-20261016/README.zh-CN.md).

The Host origin must match `PXA_STORE_ORIGIN`; changing either requires
rebuilding the product firmware and the App. The ticket is short-lived, so
pressing Install first reloads detail to obtain a fresh one. Desktop
simulator Hosts without this service report an installation failure rather
than writing to Guest storage. See `docs/zh-CN/store-install-abi.md` for the
protocol and remaining enhancements (progress, cancellation, trusted catalog
recheck).

## Host baseline selection

Since 0.8.11 the Core 2 client selects the immutable `*-pxa-0.3.0-r2` Host
capability snapshot (Window 0.3, UI 0.9, Net 0.4 and Assets 2.3). This names
a capability revision, not a new toolchain or AOT ABI. The original
`*-pxa-0.3.0` baseline remains available for older firmware and Apps.
The store retains legacy `*-wamr-2.4.0` profiles and packages independently.
Profiles describe Host capabilities; package eligibility compares Core, services
and every component artifact, not the App SDK release number. Products using
a newer verified Host profile may override `STORE_PROFILE_SUFFIX` at build time.
The URL/profile buffers and runtime allocations are unchanged.

## Simulator verification

Run `SANITIZE=1 bash local/pxa-apps/tests/native/run.sh` from the product root.
Store parser/media URL, UTF-8, density layout, ticket and image cancellation
regressions are part of that suite. For deterministic signed-AOT scroll and
memory comparison, see
[the 2026-10-15 report](docs/store-refactor-20261015/README.zh-CN.md).
The report includes the probe build/run commands and distinguishes desktop
results from unmeasured hardware performance.

The [Mosaico DPI and paging regression report](docs/mosaico-dpi-20261016/README.zh-CN.md)
covers 305 DPI device font sizing and the deterministic `paged` probe mode.
The [screenshot viewer report](docs/screenshot-viewer-20261016/README.zh-CN.md)
covers the single install action, zoom/pan, paging, scroll restoration, and six
resolution/density/shape profiles with the production AOT and UI backend.


## Search submission and package size (2026-10-16)

The search field is explicitly single-line, with a byte limit matching the query
buffer. The system IME **Done** key emits the normal input submit action; the
inline magnifier is also a button. A submit while a catalog request is running
retains the latest query and replays it after either success or failure. Catalog
and search-image completions patch or preserve an open editor, keeping focus,
text and the keyboard intact. No additional request queue or dynamic container
is introduced.

Catalog and detail sizes use the complete selected `.pxa` download byte count,
including container headers, manifest, signatures, code and resources. Compact
sizes use rounded KiB/MiB; the technical detail shows the exact byte count. A
simulator package, device package and multi-architecture package can differ in
size, so compare the same target and version. `store_size.hpp` formats bounded
buffers without allocation.

The production Host/AOT probe now also checks native input submission, the
magnifier, deferred Chinese queries, editor identity across a late response and
recovery after HTTP failure, in six resolution/DPI/screen-shape profiles. Run:

```sh
python local/pxa-apps/store/tests/run_viewer_probe.py \
  --build build/simulator/sensecap-watcher \
  --package local/app-output/simulator/pxa-store \
  --publisher deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
  --ime-package local/app-output/simulator/pxa-pinyin-ime \
  --output local/store-ui-check
```

The provider argument supplies real text-input capability to the isolated
product Host. The older multiline search fails the new assertion before a
submit is attempted. The full signed-provider keyboard flow is also exercised
with the production product simulator.

作者显示绑定为可选能力：Host 同步拒绝或异步完成为 `UNAVAILABLE` / `UNSUPPORTED` 时，商城继续按完整签名身份安装；`DENIED`、格式或签名错误仍会中止。显示绑定不替代安装器的包签名、摘要与应用身份校验。

Catalog sorting uses the same server ordering as the web store: `updated` (default),
`downloads` (all release downloads), and `name`. Open the Sort control to choose an
order or category. The numeric continuation is an application ID used as a keyset
anchor in that order; a sort change resets pagination. Older clients omitting
`sort` retain the device API's original ID order.

安装时，详情页底部和下载页显示 Host 上报的已下载量、实际传输总量和百分比，区分程序与资源下载；传输完成后显示校验与安装状态。系统确认前不显示估算百分比。资源复用时，进度按 Host 的实际传输总量计算。进度事件仅局部更新文字和进度条，不重建页面或重新拉取资源。
