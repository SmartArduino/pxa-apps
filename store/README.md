# PXA App Store

A reference PXA application that browses the DOIT PXA device catalog
(`https://app.doit.am`), shows each application's signed metadata,
compatibility, permissions and service requirements, and requests Host-managed
download and installation.

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

## Building

```sh
tools/app.sh build store --target simulator --source-root local/pxa-apps
tools/app.sh build store --target esp32s3 --source-root local/pxa-apps
```

## Configuration

| Define | Default | Notes |
| --- | --- | --- |
| `PXA_STORE_ORIGIN` | `https://app.doit.am` | Must stay byte-identical to the signed `net.client` scope in `package.json`; the Host compares the parsed URL origin with that scope. |
The Store queries Device ABI 0.2 at startup to select a catalog profile from
the Host target and supported AOT formats. Catalog profile names are server
keys, not WAMR engine ABI strings. An unsupported Host or catalog profile does
not fall back to another board's artifacts. The current catalog must publish
an `esp32-s31-wamr-2.4.0` profile and matching artifacts before Korvo can
install apps from it.
| `PXA_STORE_CHANNEL` | `stable` | `stable` or `beta`. |

Override them with `PXA_APP_DEFINES`, for example
`PXA_APP_DEFINES=PXA_STORE_ORIGIN=https://store.example.test`. Changing the
origin also requires updating the `net.client` scope in `package.json`, which is
part of the signed manifest.

## Layout

The catalog, installed apps and downloads share a four-destination bottom
navigation bar with icon capsules and text labels. Search, back and refresh
are icon actions with accessible labels. Library actions live in compact, theme-colored cards; the
scroll area ends above the navigation bar so actions never sit underneath it.
The screen follows the Host-reported logical size, the safe area and the system
bar insets:

- `safe_insets` from the UI environment (start configuration and
  `PXA_UI_ENVIRONMENT_CHANGED`) cover the panel's own safe area: rounded
  corners, cutouts and any reserved region the product reports.
- `system-bar-insets` from the Window service snapshot cover the status and
  navigation bars. The App requests `get-snapshot` and also consumes
  `metrics-changed`, then pads by the larger value of both per edge. Nothing is
hardcoded: a Host that reports no bars simply gets no extra padding.
- The tab bar paints through the bottom navigation inset while its actions
  remain above the system controls. Gesture navigation without a visible handle
  reserves no additional bottom bar inset (physical safe area still applies).
- Download, catalog and installation errors retain their inline status and
  additionally use the Window 0.2 toast ABI for a localized reason. Older Hosts
  without this optional ABI simply keep the inline status.
- Layout metrics (header, chips, cards, tab bar, keyboard) switch between
  compact, regular and large tables from the reported width, so the same App
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

Safe area insets and the layout's own margins do not stack: the root only adds
the part of an inset a screen margin does not already cover (`inset_padding`),
so a gesture strip never pushes the content further than the strip itself.

On the catalog, the header, filter row and bottom tab bar stay visible while
scrolling. The list keeps its layout stable instead of hiding the chrome while
retaining its reserved space. A bounded header-action hit region on the list
forwards taps on Hosts whose scroll layer intercepts the header; taps during a
drag are ignored.

Wide displays (`>= 480` logical pixels, `>= 1100` for three columns) render the
catalog as a grid with fractional, equally sized columns through the UI grid
ABI. Narrow panels keep the single-column list.

The catalog requests enough entries to fill the viewport plus one row, bounded
by 12 entries. It prefetches when the current grid is still too short and
scrolling near the last row requests the next page without a button. A bounded
ring of 12 entries evicts the oldest rows as needed. Initial taxonomy changes
rebuild the whole catalog; later pages replace only the list subtree while
preserving the header, filters and tab bar. Download progress patches only its
row's text and progress bar when the displayed percentage changes. Waiting for
the next network chunk and checking each installed app do not redraw the whole
surface; the update check redraws on entry and completion.

The Guest starts with a 4 KiB network response allowance and doubles it on
`limit-exceeded`, growing its WebAssembly linear memory only as needed up to
the Host's 256 KiB safety ceiling. The ESP Host allocates per request and
reports resource exhaustion when PSRAM cannot satisfy the request. The 4 KiB
figure is an initial size, not a fixed catalog limit.

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

The detail parser retains the short-lived `download_ticket_url`. The
`store_client_build_download_url` helper only accepts a same-origin artifact
ticket with the expected route and query shape. The ticket is not a trust
decision: the catalog envelope is not signature-verified by this App, and
the Host must independently authenticate the download and verify the signed
package before installation. Run the focused native test with:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-attributes \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Ilocal/pxa-apps/store -Ideps/pxa-system/sdk/guest-c/include \
  local/pxa-apps/store/tests/store_ticket_test.c \
  local/pxa-apps/store/store_client.c \
  local/pxa-apps/store/store_json.c -o /tmp/pxa-store-ticket-test
/tmp/pxa-store-ticket-test
```

Pressing **Install** sends a bounded ticket, App ID, size and digest to the
ESP Host's `store-installer` download API (ABI 0.5, service 20, opcode 2;
best-effort progress event opcode 0x8001).
The App receives a Host-owned staging filename and passes it to the install
API (opcode 3). The old combined opcode 1 remains available. Any signed Guest
may request installation. The Host accepts only the configured HTTPS origin
(`CONFIG_PXA_STORE_ORIGIN`) and the artifact-ticket path, streams the container
into a Host-owned temporary file (at most 4 MiB), verifies its byte count and
SHA-256, and invokes the existing signed-PXA preview verifier. Only after the
Host shows the verified App, version, publisher and permission count and the
user confirms does the worker invoke the transactional installer. The Host
accepts only a registered filename owned by the calling app. Failed or
interrupted downloads are removed; completed downloads remain available for
retry until deleted in Downloads, including across reboots. The Guest's
catalog response memory and 65536-byte private FS quota do not apply to this
Host-owned download.

The Host origin must match `PXA_STORE_ORIGIN`; changing either requires
rebuilding the product firmware and the App. The ticket is short-lived, so
pressing Install first reloads detail to obtain a fresh one. Desktop
simulator Hosts without this service report an installation failure rather
than writing to Guest storage. See `docs/zh-CN/store-install-abi.md` for the
protocol and remaining enhancements (progress, cancellation, trusted catalog
recheck).
