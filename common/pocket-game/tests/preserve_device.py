#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Smoke-test an EXISTING game save without restarting or injecting storage.

Run from the workspace root: --port PORT --game {2048,chess,sokoban}
--output local/artifacts/TOPIC/TIMESTAMP/GAME. Requires a host C/C++ compiler,
Pillow and PXADB. Reads/backups the real KV before launch. Chess has no undo:
its legal human move (and any AI reply) extends the current game. 2048 undo
restores board/score/moves but consumes its previous undo and retains best score.
A bounded result has passed=false, gameplay=false and exit code 2.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import traceback
import zlib

sys.dont_write_bytecode = True
from device import Device as LegacyDevice, IDS, FAULTS, pxadb, require, difference


class Device(LegacyDevice):
    def start(self):
        at = len(self.logs)
        self.request('PACKAGE run '+self.identity)
        allowed = False
        deadline = time.monotonic()+20
        while time.monotonic() < deadline:
            fresh = self.logs[at:]
            if not allowed and any('Runtime permission prompt queued:' in line and ':'+self.identity+' permission=' in line for line in fresh):
                self.request('INPUT TAP 200 183'); self.request('INPUT SYNC'); allowed = True
            ready = any('Pocket: ready app='+self.identity in line for line in fresh)
            completed = any('Launch completion: ' in line and ':'+self.identity+' ' in line and 'status=0' in line for line in fresh)
            active = any(f.kind == 'PKG' and f.payload.split('\t',1)[0].endswith(':'+self.identity) and 'active=1' in f.payload for f in self.request('PACKAGES'))
            if active and (ready or completed):
                self.pump(.4)
                measured = self.configure_viewport(at)
                if not measured:
                    self.ox,self.oy,self.viewport_width,self.viewport_height = 0,5,296,230
                    self.report.setdefault('viewports',[]).append({'origin':[0,5],'size':[296,230],'display':[296,240],'derived':True,'requires_menu_and_committed_payload':True})
                image = self.shot('launch-'+str(len(self.report.get('launches',[]))))
                require(max(high-low for low,high in image.getextrema()) > 16, 'Blank active game capture')
                self.report.setdefault('launches',[]).append({'fresh_ready':ready,'fresh_host_completion':completed,'active':True,'nonblank_capture':True,'measured_viewport':measured,'load_log':any('Pocket: load completed' in line for line in self.logs[at:]),'requires_followup_durable_save':True})
                return
            self.pump(.08)
        raise TimeoutError('Missing selected active package plus fresh ready/Host completion')


APPS = Path(__file__).resolve().parents[3]
WORKSPACE = APPS.parents[1]
# Use the actual shipped rules for legal moves and exact save/history predictions.
# Generated source/binary/logs belong to --output, never to the device.
ORACLE = r'''
#include "game-2048/core.h"
#include "pocket-chess/chess.hpp"
#include "pocket-sokoban/core.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using Bytes=std::vector<uint8_t>;
std::string hex(const uint8_t *p,size_t n) { const char *d="0123456789abcdef"; std::string s; for(size_t i=0;i<n;++i){s+=d[p[i]>>4];s+=d[p[i]&15];}return s; }
std::string pack(const chess::State &s) { uint8_t b[4096];size_t n=chess::serialize(s,b,sizeof b);assert(n);return hex(b,n); }
std::string pack(const sokoban::State &s) { uint8_t b[4096];size_t n=sokoban::serialize(s,b,sizeof b);assert(n);return hex(b,n); }
void ai(chess::State &s) { if(chess::white(s)||s.result!=chess::Result::PLAYING)return; chess::Ai a;int n=0;while(!chess::ai_step(s,a))assert(++n<321);assert(a.best.from<64&&chess::play(s,a.best)); }
int main(int argc,char **argv) {
 assert(argc==3);std::ifstream f(argv[2],std::ios::binary);Bytes b((std::istreambuf_iterator<char>(f)),{});assert(f||f.eof());
 std::string game=argv[1];
 if(game=="2048") {
  assert(b.size()==64&&!memcmp(b.data(),"P204",4));uint32_t score=0;memcpy(&score,b.data()+8,4);
  bool (*moves[])(uint8_t (*)[4],uint32_t*)={moveUp,moveRight,moveDown,moveLeft};
  int chosen=-1;uint32_t best_gain=UINT32_MAX,best_score=0;uint8_t best[4][4]={};
  for(int i=0;i<4;++i){uint8_t board[4][4];memcpy(board,b.data()+20,16);uint32_t value=score;
   if(moves[i](board,&value)&&(chosen<0||uint32_t(value-score)<best_gain)){chosen=i;best_gain=value-score;best_score=value;memcpy(best,board,16);}}
  if(chosen<0)puts("{\"blocked\":\"terminal_2048_board_no_legal_move\"}");
  else printf("{\"action\":%d,\"board\":\"%s\",\"score\":%u}\n",chosen,hex(&best[0][0],16).c_str(),best_score);
 } else if(game=="sokoban") {
  sokoban::State s;assert(sokoban::deserialize(s,b.data(),b.size()));
  if(sokoban::won(s)){puts("{\"blocked\":\"current_level_already_solved\"}");return 0;}
  if(s.history_count==sokoban::HISTORY){puts("{\"blocked\":\"undo_buffer_full_move_would_discard_old_history\"}");return 0;}
  int chosen=-1;std::string after,undo;bool progress=false;
  for(int i=0;i<4;++i){auto t=s;if(!sokoban::move(t,sokoban::Direction(i)))continue;
   bool change=t.completed!=s.completed||t.unlocked!=s.unlocked;
   if(chosen<0||(!change&&progress)){chosen=i;progress=change;after=pack(t);assert(sokoban::undo(t));undo=pack(t);}}
  if(chosen<0)puts("{\"blocked\":\"no_legal_step_or_move_counter_limit\"}");
  else printf("{\"action\":%d,\"after\":\"%s\",\"undo\":\"%s\",\"progress_retained\":%s}\n",chosen,after.c_str(),undo.c_str(),progress?"true":"false");
 } else {
  assert(game=="chess"&&b.size()>12&&!memcmp(b.data(),"PCU1",4));chess::State s;assert(chess::restore(s,b.data()+12,b.size()-12));
  int selected=int(b[6])-1,promotion=int(b[7])-1;
  if(selected>=0)assert(s.result==chess::Result::PLAYING&&(b[4]||chess::white(s))&&s.board[selected]!='.'&&((s.board[selected]>='A'&&s.board[selected]<='Z')==chess::white(s)));
  if(promotion>=0)assert(selected>=0&&chess::promotes(s,unsigned(selected),unsigned(promotion))&&chess::legal(s,unsigned(selected),unsigned(promotion)));
  bool initial_ai=!b[4]&&!chess::white(s)&&s.result==chess::Result::PLAYING;if(initial_ai)ai(s);
  std::string ready=pack(s);int result=int(s.result);
  if(result){printf("{\"blocked\":\"terminal_chess_result_%d\",\"ready\":\"%s\",\"initial_ai\":%s}\n",result,ready.c_str(),initial_ai?"true":"false");return 0;}
  chess::Move m;bool pending=b[7]&&!initial_ai;
  if(pending){assert(b[6]&&b[8]<4);m={uint8_t(b[6]-1),uint8_t(b[7]-1),"qrbn"[b[8]]};}
  else {for(int pass=0;pass<2&&m.from==255;++pass)for(unsigned from=0;from<64&&m.from==255;++from)for(unsigned to=0;to<64;++to){
   if(!pass&&((s.board[from]|32)=='p'||s.board[to]!='.'))continue;
   if(chess::legal(s,from,to)){m={uint8_t(from),uint8_t(to),'q'};break;}}}
  assert(m.from<64&&chess::play(s,m));std::string human=pack(s);bool reply=!b[4]&&!chess::white(s)&&s.result==chess::Result::PLAYING;if(reply)ai(s);
  printf("{\"ready\":\"%s\",\"human\":\"%s\",\"final\":\"%s\",\"move\":[%u,%u],\"promotion\":%d,\"pending\":%s,\"initial_ai\":%s,\"ai_reply\":%s,\"result\":%d}\n",ready.c_str(),human.c_str(),pack(s).c_str(),m.from,m.to,m.promotion=='q'?0:m.promotion=='r'?1:m.promotion=='b'?2:3,pending?"true":"false",initial_ai?"true":"false",reply?"true":"false",int(s.result));
 }
}
'''


def build_oracle(output):
    source, obj, binary = output/'rules.cpp', output/'2048.o', output/'rules'
    source.write_text(ORACLE)
    flags = ['-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all']
    with (output/'native-build.log').open('w') as log:
        subprocess.run(['cc', '-std=c99', *flags, '-c', str(APPS/'game-2048/core.c'), '-o', str(obj)], stdout=log, stderr=log, check=True)
        subprocess.run(['c++', '-std=c++17', *flags, '-Wno-implicit-fallthrough', '-fwrapv', '-I', str(APPS), str(source), str(obj),
                        str(APPS/'pocket-chess/chess.cpp'), str(APPS/'pocket-sokoban/core.cpp'), '-o', str(binary)], stdout=log, stderr=log, check=True)
    return binary


def plan(binary, game, payload, output, label):
    path = output/(label+'.bin'); path.write_bytes(payload)
    value = json.loads(subprocess.check_output([str(binary), game, str(path)], text=True))
    (output/(label+'-plan.json')).write_text(json.dumps(value, indent=2)+'\n')
    return value


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def seal(data):
    struct.pack_into('<I', data, len(data)-4, fnv(data[:-4]))
    return bytes(data)


def valid(game, raw):
    require(len(raw) >= 64, 'Missing/truncated original save; refusing fresh game')
    offset = 12 if game == 'chess' else 0
    core = raw[offset:]
    require(core[:4] == {'2048': b'P204', 'chess': b'PCH1', 'sokoban': b'PSKB'}[game], 'Unexpected existing save format')
    require(struct.unpack_from('<I', core, len(core)-4)[0] == (fnv(core[:-4]) if game != 'sokoban' else zlib.crc32(core[:-4])), 'Original game checksum invalid')
    if game == '2048':
        require(len(raw) == 64 and raw[4] == 1 and raw[5] <= 1 and not any(raw[6:8]) and any(raw[20:36]) and max(raw[20:36]) <= 31, 'Invalid 2048 save')
        require(u32(raw, 12) >= u32(raw, 8), 'Invalid 2048 best score')
        require(not raw[5] or (any(raw[44:60]) and max(raw[44:60]) <= 31), 'Invalid 2048 undo board')
    if game == 'chess':
        require(raw[:4] == b'PCU1' and raw[4] <= 1 and raw[5] < 64 and max(raw[6:8]+raw[9:11]) <= 64 and raw[8] < 4 and not raw[11], 'Invalid chess UI save')


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def core(game, raw):
    return raw[12:] if game == 'chess' else raw


def pause(d, opened):
    at = len(d.logs); d.tap(276, 18, settle=.1)
    d.until(lambda: any('Pocket: menu '+('opened' if opened else 'closed') in line for line in d.logs[at:]), 'pause transition')
    d.pump(.4)


def await_save(d, predicate, label, seconds=25):
    deadline = time.monotonic()+seconds
    while True:
        payload = d.saved(label)
        if predicate(payload):
            return payload
        require(time.monotonic() < deadline, 'Timed out waiting for '+label)
        d.pump(.25)


def square(index):
    return 20+(index % 8)*24, 36+(7-index//8)*24


def exercise(d, binary):
    game, out, report = d.args.game, d.args.output, d.report
    require(not any(f.kind == 'PKG' and 'active=1' in f.payload for f in d.request('PACKAGES')), 'Another app is active')
    # Backup precedes PACKAGE run: init() must never replace a missing old save.
    original = d.saved('original-kv'); valid(game, original)
    initial = plan(binary, game, original, out, 'original')
    report['plan'] = initial
    d.client.subscribe_logs(); d.request('INPUT KEY HOME')
    d.request('INPUT SWIPE 148 192 148 48 600 8'); d.pump(.5); d.logs.clear()
    d.start()
    original_generation = report['storage']['original-kv']['generation']
    ready = bytes.fromhex(initial['ready']) if game == 'chess' else original
    baseline = await_save(d, lambda p: core(game, p) == ready, 'loaded-existing')
    pause(d, True)
    await_save(d, lambda p: core(game,p) == ready and (report['launches'][-1]['load_log'] or report['storage'].get('paused-loaded',{}).get('generation',0) > original_generation), 'paused-loaded')
    require(core(game, d.saved('paused-existing')) == ready, 'Loaded original board changed unexpectedly')
    report['launches'][-1]['verified_by_fresh_load_or_generation_and_original_payload'] = True
    report['checks'].append('real_original_kv_loaded_without_reset')
    if 'blocked' in initial:
        report.update(status='bounded', reason=initial['blocked'], gameplay=False)
        d.shot('bounded-paused'); roundtrip(d, ready)
        return
    pause(d, False); before = d.board('before-move')
    if game == 'chess':
        a, b = initial['move']
        if initial['pending']:
            d.tap(246, 102+18*initial['promotion'])
        else:
            if baseline[6]:
                d.tap(*square(baseline[6]-1))  # clear only the existing selection
            d.tap(*square(a), settle=.1); d.tap(*square(b), settle=.1)
            # A promotion requires a separate real UI choice after the two taps.
            if ready[12+a] in b'Pp' and (b < 8 or b >= 56):
                d.tap(246, 102+18*initial['promotion'])
        expected = bytes.fromhex(initial['final'])
        await_save(d, lambda p: p[4] == baseline[4] and p[12:] == expected, 'legal-move-ai-complete')
        require(difference(before, d.board('after-real-move')) > .001, 'Chess board did not change')
        report.update(undo_supported=False, ai_reply=initial['ai_reply'], initial_ai=initial['initial_ai'])
        report['checks'].append('legal_existing_chess_move_and_exact_native_history'+('_ai_reply' if initial['ai_reply'] else ''))
    else:
        action = initial['action']; dx, dy = ((0,-55),(55,0),(0,55),(-55,0))[action]
        d.swipe(dx, dy)
        if game == '2048':
            moved = await_save(d, lambda p: u32(p,16) == ((u32(original,16)+1) & 0xffffffff), 'real-move')
            valid(game, moved)
            require(u32(moved,12) == max(u32(original,12), initial['score']), '2048 best score changed unexpectedly')
            predicted = bytes.fromhex(initial['board']); board = moved[20:36]
            spawned = [i for i in range(16) if predicted[i] != board[i]]
            require(len(spawned) == 1 and predicted[spawned[0]] == 0 and board[spawned[0]] in (1,2), '2048 legal move/spawn differs from native rules')
            require(moved[5] == 1 and moved[44:60] == original[20:36] and moved[36:44] == original[8:12]+original[16:20] and u32(moved,8) == initial['score'], '2048 undo snapshot/score differs')
            expected_bytes = bytearray(original); expected_bytes[5] = 0
            expected_bytes[12:16] = moved[12:16]; expected_bytes[36:44] = original[8:12]+original[16:20]; expected_bytes[44:60] = original[20:36]
            expected = seal(expected_bytes)
            report['metadata_note'] = 'Undo restores board/score/moves; prior undo is consumed and best score retained.'
        else:
            moved = await_save(d, lambda p: p == bytes.fromhex(initial['after']), 'real-move')
            expected = bytes.fromhex(initial['undo'])
            report['progress_retained'] = initial['progress_retained']
        played = d.board('after-real-move')
        require(difference(before, played) > .001, 'Real gameplay screenshot did not change')
        d.tap(236, 156 if game == '2048' else 106)
        restored = await_save(d, lambda p: p == expected, 'real-undo')
        require(difference(before, d.board('after-undo')) < .005, 'Undo board screenshot differs')
        report['original_bytes_restored'] = restored == original
        report['checks'].append('real_legal_move_and_undo_native_save_semantics')
    pause(d, True)  # Freeze AI/input and force a durable save before exit.
    roundtrip(d, expected)
    report.update(passed=True, gameplay=True, status='passed')


def exit_paused(d):
    # Device.menu_exit() opens a menu first; ours is already open.
    d.pointer('DOWN', 140, 145)
    d.until(lambda: stopped(d), 'menu EXIT', seconds=8); d.request('INPUT CANCEL')


def roundtrip(d, expected):
    final = await_save(d, lambda p: core(d.args.game, p) == expected, 'before-exit')
    (d.args.output/'final.bin').write_bytes(final)
    image = d.shot('final-paused'); exit_paused(d)
    require(d.saved('after-exit') == final, 'Exit changed save')
    previous_generation = d.report['storage']['after-exit']['generation']
    d.start()
    await_save(d, lambda p: p == final, 'reloaded')
    pause(d, True)
    require(difference(image, d.shot('reloaded-paused')) < .005, 'Reload screenshot changed')
    await_save(d, lambda p: p == final and (d.report['launches'][-1]['load_log'] or d.report['storage'].get('reloaded-paused',{}).get('generation',0) > previous_generation), 'reloaded-paused')
    require(d.saved('reloaded-paused') == final, 'Reload changed save semantics')
    d.report['launches'][-1]['verified_by_fresh_load_or_generation_and_exact_payload'] = True
    exit_paused(d)
    d.report['checks'].append('menu_exit_reload_exact_final_save')


def stopped(d):
    return any(f.kind == 'PKG' and ':'+d.identity+'\t' in f.payload and 'active=0' in f.payload for f in d.request('PACKAGES'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--game', choices=IDS, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); args.output = args.output.resolve()
    require((WORKSPACE/'local/artifacts').resolve() in args.output.parents, 'Output must be under workspace local/artifacts')
    args.output.mkdir(parents=True, exist_ok=True)
    binary = build_oracle(args.output)
    report = dict(passed=False, gameplay=False, game=args.game, checks=[], preserve_existing=True)
    with (args.output/'serial.jsonl').open('w', buffering=1) as stream:
        with pxadb.open_client(args.port, 30) as client:
            report['hello'] = client.device_info
            require(pxadb.info_properties(client.device_info).get('target') == 'esp32s3', 'Expected ESP32-S3')
            d = Device(args, client, report, stream)
            try:
                exercise(d, binary)
                require(not any(FAULTS.search(line) for line in d.logs), 'Runtime fault')
            except BaseException:
                report.update(passed=False, status='failed', error=traceback.format_exc())
                raise
            finally:
                try:
                    d.request('INPUT CANCEL')
                    if not stopped(d):
                        d.request('PACKAGE stop '+d.identity)
                finally:
                    (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 2


if __name__ == '__main__':
    sys.exit(main())
