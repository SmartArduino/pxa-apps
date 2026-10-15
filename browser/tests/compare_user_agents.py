#!/usr/bin/env python3
"""Compare response bodies, not rendering speed. No credentials or cookies.

Two requests per identity expose ordinary response variation. A different
hash alone does not prove that a site simplified its page for this browser.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import urllib.error
import urllib.request

AGENTS = {
    'lightweight': 'PXA-Browser/0.1 (Mobile; TextMode)',
    'mobile': 'Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 '
              '(KHTML, like Gecko) Chrome/143.0.0.0 Mobile Safari/537.36 PXA-Browser/0.1',
}


def probe(url):
    results = []
    for mode, agent in AGENTS.items():
        for repeat in range(2):
            row = {'url': url, 'mode': mode, 'repeat': repeat}
            request = urllib.request.Request(url, headers={
                'User-Agent': agent, 'Save-Data': 'on', 'Accept-Encoding': 'identity',
                'Accept': 'text/html, text/plain;q=0.9, application/rss+xml;q=0.8, text/xml;q=0.7'})
            try:
                try:
                    response = urllib.request.urlopen(request, timeout=15)
                except urllib.error.HTTPError as error:
                    response = error
                with response:
                    body = response.read(262145)
                    row.update(status=response.status, final_url=response.url,
                               content_type=response.headers.get('Content-Type'),
                               vary=response.headers.get('Vary'),
                               bytes=min(len(body), 262144), capped=len(body)>262144,
                               sha256=hashlib.sha256(body[:262144]).hexdigest())
            except (OSError, urllib.error.URLError) as error:
                row['error'] = str(error)
            results.append(row)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', action='append', help='Public URL; may be repeated')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    urls = args.url or ['https://example.com/', 'https://pxa.doit.am/',
                        'https://lite.duckduckgo.com/lite/?q=ESP32']
    assert all(url.startswith(('https://', 'http://')) for url in urls)
    with ThreadPoolExecutor(max_workers=3) as pool:
        results = [row for rows in pool.map(probe, urls) for row in rows]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, ensure_ascii=False, indent=2)+'\n')
    for row in results:
        print(row['mode'], row['url'], row.get('status'), row.get('bytes'), row.get('error', ''))


if __name__ == '__main__':
    main()
