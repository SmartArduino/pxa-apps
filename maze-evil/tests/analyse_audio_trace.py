#!/usr/bin/env python3
"""Summarize AudioTrace software timestamps; never estimates acoustic onset."""
import argparse
import json
from pathlib import Path
import re
import statistics


def events(lines):
    result = []
    for line in lines:
        match = re.search(r'AudioTrace.*?\b(input|tone_rejected|effect_rejected|effect_render|effect|tone|render|uart) us=(\d+)', line)
        if match:
            result.append(dict(kind=match[1], **{
                key: int(value) for key, value in re.findall(r'(\w+)=(-?\d+)', line[match.start():])}))
    # LOGSUB can replay the small device history to recover a burst. Do not
    # count replayed timestamps as additional commands.
    unique = {tuple(sorted(event.items())): event for event in result}
    return sorted(unique.values(), key=lambda event: event['us'])


def distribution(values):
    return dict(count=len(values), mean=statistics.mean(values),
                min=min(values), max=max(values)) if values else dict(count=0)


def analyse(lines, start_us=0):
    trace = [event for event in events(lines) if event["us"] >= start_us]
    paired = set()
    tones = []
    for event in trace:
        if event['kind'] not in ('tone', 'effect'):
            continue
        tone = dict(event)
        if event['kind'] == 'tone':
            tone['fifo_ahead_ms'] = event['queued']/16-event['duration']-event['delay']
        render_kind = 'render' if event['kind'] == 'tone' else 'effect_render'
        match_keys = ('packet', 'duration', 'delay') if event['kind'] == 'tone' else ('packet', 'layers', 'track')
        render = next(((index, item) for index, item in enumerate(trace)
            if index not in paired and item['kind'] == render_kind
            and item['us'] >= event['us'] and all(item[k] == event[k]
                for k in match_keys)), None)
        if render:
            index, item = render
            paired.add(index)
            tone['render_wait_ms'] = (item['us']-event['us'])/1000
        if ((event['kind'] == 'tone' and event['duration'] == 220 and event['delay'] == 0) or
                (event['kind'] == 'effect' and event['layers'] == 3 and event['track'] == 0)):
            release = next((item for item in reversed(trace)
                if item['kind'] == 'input' and item['phase'] == 2
                and item['x'] == 220 and item['y'] == 120
                and item['us'] <= event['us']), None)
            if release:
                tone['release_sample_to_submit_ms'] = (event['us']-release['sampled'])/1000
            previous = next((item for item in reversed(trace) if item['kind'] == 'uart'
                and item['nonzero'] and item['us'] < event['us']), None)
            # Only associate a UART onset after a quiet interval. When effects
            # overlap, the next nonzero packet can belong to an older sound.
            if not previous or event['us']-previous['us'] > 100000:
                uart = next((item for item in trace if item['kind'] == 'uart'
                    and item['nonzero'] and item['us'] >= event['us']), None)
                # With edge-only UART tracing, absence of a log does not mean
                # silence. A later edge can belong to this effect's third
                # layer. Do not pair an edge hundreds of milliseconds later.
                if uart and uart['us']-event['us'] <= 50000:
                    tone['quiet_submit_to_uart_ms'] = (uart['us']-event['us'])/1000
                    if release:
                        tone['quiet_release_sample_to_uart_ms'] = (uart['us']-release['sampled'])/1000
        tones.append(tone)
    keys = ('fifo_ahead_ms', 'render_wait_ms', 'release_sample_to_submit_ms',
            'quiet_submit_to_uart_ms', 'quiet_release_sample_to_uart_ms')
    return dict(accepted=len(tones), rejected=sum(e['kind'] in ('tone_rejected', 'effect_rejected') for e in trace),
        metrics={key: distribution([t[key] for t in tones if key in t]) for key in keys},
        tones=tones)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    result = dict(scope=__doc__, passed=report['passed'],
                  phases=[dict(name=p['name'], **analyse(report['logs'][p['log_begin']:p['log_end']], p.get('device_us', 0)))
                          for p in report['phases']])
    for phase in result['phases']:
        if phase['name'] not in ('isolated', 'long-tap'):
            # These scenarios do not guarantee an idle output path. An edge
            # from an earlier layer cannot identify the newly submitted shot.
            for tone in phase['tones']:
                for key in ('quiet_submit_to_uart_ms', 'quiet_release_sample_to_uart_ms'):
                    tone.pop(key, None)
            for key in ('quiet_submit_to_uart_ms', 'quiet_release_sample_to_uart_ms'):
                phase['metrics'][key] = dict(count=0)
    text = json.dumps(result, indent=2)+'\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
