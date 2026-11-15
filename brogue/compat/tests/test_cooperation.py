#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check the real frontend scheduler against a controlled monotonic clock."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
source=Path(__file__).resolve().parents[2]/'main.c';text=source.read_text()
def body(signature):
 start=text.index(signature);first=text.index('{',start);depth=0
 for i in range(first,len(text)):
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if not depth:return text[start:i+1]+'\n'
 raise AssertionError('Unclosed function')
assert 'static void report_game_state(void) {\n    if (cooperative_pause) return;' in text
code=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line=%d\n",__LINE__);exit(2);}}while(0)
static uint64_t slice_started_us,fake_us;static uint32_t slice_poll_count,deadline;static int waiting_input,cooperative_pause,yields,br_calls,reports,call_should_cooperate;
static struct{uint32_t time_ms;}fb;
static int test_clock_gettime(clockid_t clock,struct timespec*out){CHECK(clock==CLOCK_MONOTONIC);out->tv_sec=fake_us/1000000;out->tv_nsec=(fake_us%1000000)*1000;return 0;}
#define clock_gettime test_clock_gettime
static void yield_to_pxa(void){++yields;}
static void br_call_game(void){++br_calls;if(call_should_cooperate)cooperative_pause=1;}
static void report_game_state(void){if(!cooperative_pause)++reports;}
"""+body('static uint64_t brogue_monotonic_us(void)')+body('__attribute__((noinline)) void pxa_brogue_cooperate(void)')+body('static void run_game_slice(void)')+r"""
int main(void){fb.time_ms=1234;deadline=4321;waiting_input=1;fake_us=7999;
for(int n=0;n<64;++n)pxa_brogue_cooperate();CHECK(yields==0 && waiting_input==1 && deadline==4321 && !cooperative_pause && fb.time_ms==1234);
fake_us=8000;for(int n=0;n<63;++n)pxa_brogue_cooperate();CHECK(!yields);pxa_brogue_cooperate();CHECK(yields==1 && !waiting_input && deadline==1234 && cooperative_pause && fb.time_ms==1234);
fake_us=123456789;run_game_slice();CHECK(slice_started_us==fake_us && !slice_poll_count && !cooperative_pause && br_calls==1 && reports==1 && fb.time_ms==1234);
call_should_cooperate=1;run_game_slice();CHECK(br_calls==2 && reports==1 && cooperative_pause);
slice_started_us=fake_us-8000;slice_poll_count=UINT32_MAX;pxa_brogue_cooperate();CHECK(yields==2 && slice_poll_count==0 && fb.time_ms==1234);
printf("{\"checks\":\"8ms threshold, 64poll sampling, nexttick deadline, unchanged gameclock, reset at each resume, partial-world report suppression, unsigned poll-counter wrap\",\"passed\":true}\n");return 0;}
"""
c=a.output/'scheduler.c';c.write_text(code);exe=a.output/'scheduler';command=[a.cc,'-std=c11','-D_POSIX_C_SOURCE=200809L','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)]
r=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(a.output/'compile.log').write_text(r.stdout);assert r.returncode==0,r.stdout
r=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(a.output/'run.log').write_text(r.stdout);assert r.returncode==0,r.stdout
receipt={'passed':True,'frontend_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'compile_command':command,'actual_frontend_functions_extracted':True,'result':json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout.strip())
