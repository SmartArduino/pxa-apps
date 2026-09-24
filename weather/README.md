# Weather App

The App requests only origin-scoped `net.client` permissions. It does not read
the device MAC address or embed an API key. On the first request it resolves the
device's **public IP** via ipwho.is, falling back to ipapi.co on HTTP, transport,
permission, or parsing failure. IP geolocation is approximate (and may point
to a VPN/proxy exit); it is not GPS. The App labels it as an estimate. Users
can search for a city, choose among results differentiated by region and
country, and save their selection. "Use IP location" clears the preference.
Open-Meteo Geocoding resolves city names to coordinates, and every forecast
provider receives coordinates rather than provider-specific city IDs.

At those coordinates the App requests Open-Meteo current conditions, an
8-hour outlook, a 7-day forecast (high/low, weather icon and maximum rain
chance), today's maximum UV index and sunrise/sunset. It also fetches seven
days of archived daily weather ending five days before the current local date
from Open-Meteo's archive endpoint. Archive data is modeled/reanalysis data,
not a direct reading from a local weather station. When the forecast service
is unavailable, wttr.in provides current conditions only, and unavailable
sections are labeled rather than showing another city's stale forecast.
Open-Meteo Air Quality adds optional European AQI and PM2.5. Failure of air
or archive requests does not invalidate current weather. The location header
opens the city picker; the chosen city persists, and IP mode re-resolves its
location on each manual or ten-minute automatic update.

Provider-specific origins, URLs and parsers live in `weather_providers.c`;
the UI and asynchronous permission/stream state machine live in `main.c`.
When adding a provider, update the enum, origin, URL, parser, fallback sequence,
and exact-origin permission in `package.json`. The Host does not follow HTTP
redirects. Every provider must return a bounded response (currently 4 KiB).

All interface symbols and weather conditions use packaged PNG assets. Regenerate
them with `python3 generate_icons.py` (requires `rsvg-convert`).

The App uses a normal window with visible system status/navigation bars, not
fullscreen. Layout sizes follow the display width and shape; safe-area, bar and
rounded-corner insets are applied on startup and when window metrics change.
Colors use system theme tokens so light and dark themes remain legible.

Run the native parser tests with:

```sh
cc -std=c11 -Wall -Wextra -Werror weather_providers.c tests/weather_providers_test.c -o /tmp/weather-providers-test
/tmp/weather-providers-test
```

Build the App from the project root with:

```sh
tools/app.sh build weather --target simulator --source-root local/pxa-apps
```

To test city entry, launch the package in the full UI simulator. The isolated
`product` simulator does not host the system input method; the full UI
simulator and device show the keyboard when the search field is tapped.
