#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real signed simulator integration: five 150s days, alternating touch/controller.
Only actual inputs mutate gameplay. Host KV is decoded independently; no save
injection, accelerated clock, serial access or common-source changes occur.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import time
import traceback
import zlib
sys.dont_write_bytecode=True
WORKSPACE=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"common/pocket-game/tests"))
import simulator as transport
require=transport.require
IDENTITY="pxa-harbor-cafe"
INGREDIENTS=((1,0,0),(1,1,0),(0,0,2))
PRICES=(9,14,10)
PACK_COST=(8,4,4)

def payload(raw):
    require(len(raw)==160 and raw[:8]==b"HCFE\x01\0\xa0\0","Invalid HCFE header/length")
    h=2166136261
    for v in raw[:156]:h=((h^v)*16777619)&0xffffffff
    require(h==struct.unpack_from("<I",raw,156)[0],"HCFE checksum mismatch")
    require(raw[98:156]==bytes(58),"HCFE reserved bytes")
    s={"coins":struct.unpack_from("<I",raw,8)[0]}
    for name,at in (("day",12),("paid_day",14),("total_served",16),("total_failed",18),("stars",20),("debt",22),("served",58),("failed",60),("bonus",70),("fee",72)):
        s[name]=struct.unpack_from("<H",raw,at)[0]
    s["stock"]=list(struct.unpack_from("<3H",raw,24));s["level"]=list(raw[30:33])
    for name,at in (("recipe",33),("table",34),("view",35),("shop",36),("sound",37),("rescued",38),("summary",39),("cook_recipe",56),("ready_recipe",57)):s[name]=raw[at]
    for name,at in (("day_ms",40),("spawn_ms",44),("seed",48),("cook_ms",52),("revenue",62),("cogs",66)):s[name]=struct.unpack_from("<I",raw,at)[0]
    s["customers"]=[]
    for at in range(74,98,8):
        require(raw[at+3]==0,"HCFE customer reserved byte")
        stage,recipe,skin=raw[at:at+3];patience=struct.unpack_from("<I",raw,at+4)[0]
        require(stage<=2 and recipe<=2 and skin<=3 and patience<=60000,"Invalid customer")
        s["customers"].append(dict(stage=stage,recipe=recipe,skin=skin,patience=patience))
    require(1<=s["day"]<=999 and s["coins"]<=999999 and s["day_ms"]<=150000,"Invalid economy/time")
    require(all(v<=99 for v in s["stock"]) and all(v<=3 for v in s["level"]) and s["debt"]<=1000,"Invalid inventory/growth")
    return s

def host_kv(raw):
    if len(raw)<26:return None
    magic,version,_,generation,size,crc=struct.unpack_from("<4sHHQII",raw)
    body=raw[24:]
    if magic!=b"PXKV" or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:return None
    at=2;values={}
    for _ in range(struct.unpack_from("<H",body)[0]):
        n=body[at];at+=1;key=body[at:at+n].decode();at+=n
        n=struct.unpack_from("<H",body,at)[0];at+=2;values[key]=body[at:at+n];at+=n
    require(at==len(body),"Trailing KV bytes")
    return generation,values

class Session:
    def __init__(self,b,report):self.b=b;self.report=report;self.mode="touch";self.table=0;self.paused=False
    def check(self,ok,label,**data):
        require(ok,label);self.report["checks"].append(dict(check=label,**data))
    def latest(self):
        candidates=[]
        for path in self.b.state.glob("**/.pxa-kv-*"):
            try:
                kv=host_kv(path.read_bytes())
                if kv and IDENTITY+".save.v1" in kv[1]:candidates.append(kv)
            except (FileNotFoundError,struct.error,IndexError):continue
        if not candidates:return None
        gen,values=max(candidates,key=lambda x:x[0]);raw=values[IDENTITY+".save.v1"]
        return payload(raw),raw,gen
    def wait(self,predicate,label,timeout=12):
        return self.b.wait(lambda:(lambda v:v[0] if v and predicate(v[0]) else None)(self.latest()),label,timeout)
    def snap(self,label):
        value=self.latest();require(value,"No durable cafe KV")
        s,raw,gen=value
        (self.b.args.output/(label+".hcfe")).write_bytes(raw)
        (self.b.args.output/(label+".json")).write_text(json.dumps(s,indent=2)+"\n")
        self.report.setdefault("storage",{})[label]=dict(generation=gen,size=len(raw),sha256=hashlib.sha256(raw).hexdigest())
        self.b.shot(label);return s
    def tap(self,x,y):self.b.tap(x,y,settle=.08)
    def key(self,k):self.b.key(k,settle=.08)
    def primary(self):self.tap(143,179) if self.mode=="touch" else self.key("Z")
    def select(self,table):
        if self.mode=="touch":self.tap(155+48*table,115)
        else:
            for _ in range((table-self.table)%3):self.key("DOWN")
        self.table=table
    def pantry(self,item):
        before=self.wait(lambda s:True,"before supply")
        self.tap(164,200) if self.mode=="touch" else self.key("A")
        self.wait(lambda s:s["view"]==1,"pantry open")
        if self.mode=="touch":self.tap(144,69+item*29)
        else:
            for _ in range(item):self.key("DOWN")
            self.key("Z")
        after=self.wait(lambda s:s["stock"][item]==before["stock"][item]+4,"ingredient delivery")
        self.check(after["coins"]==before["coins"]-PACK_COST[item],"actual_paid_ingredient_purchase",mode=self.mode,item=item,before=before["coins"],after=after["coins"])
        self.tap(78,203) if self.mode=="touch" else self.key("X")
        self.wait(lambda s:s["view"]==0,"back to cafe")
    def grow(self,line):
        before=self.wait(lambda s:True,"before upgrade");lv=before["level"][line];price=20+22*lv+6*lv*lv
        self.tap(242,200) if self.mode=="touch" else self.key("S")
        self.wait(lambda s:s["view"]==2,"workshop open")
        if self.mode=="touch":self.tap(144,68+line*42)
        else:
            for _ in range(line):self.key("DOWN")
            self.key("Z")
        after=self.wait(lambda s:s["level"][line]==lv+1,"equipment installed")
        self.check(after["coins"]==before["coins"]-price,"actual_paid_visible_upgrade",mode=self.mode,line=line,level=lv+1,cost=price)
        self.tap(78,203) if self.mode=="touch" else self.key("X")
        self.wait(lambda s:s["view"]==0,"workshop back")
        self.snap(f"day{after['day']}-{self.mode}-upgrade-{line}-{lv+1}")
    def pause(self):
        if not self.paused:self.tap(276,10);self.paused=True
        time.sleep(.3)
    def resume(self):
        if self.paused:self.tap(140,85);self.paused=False
    def freeze_test(self):
        self.pause();before=self.snap("paused-cooking");time.sleep(2.1);after=self.wait(lambda s:True,"still paused")
        self.check(before==after,"pause_freezes_full_saved_state_and_cooking",cook_ms=before["cook_ms"],day_ms=before["day_ms"])
        self.resume()
    def safety(self):
        before=self.wait(lambda s:s["customers"][0]["stage"]==1,"first guest")
        self.b.pointer("DOWN",143,179);transport.command(self.b.sock,"TOUCH CANCEL_ALL");time.sleep(.2)
        self.check(self.wait(lambda s:True,"after cancel")["customers"][0]["stage"]==1,"pointer_CANCEL_does_not_accept_order")
        self.b.pointer("DOWN",143,179);self.b.pointer("MOVE",143,159);self.b.pointer("UP",143,159);time.sleep(.2)
        self.check(self.wait(lambda s:True,"outside up")["customers"][0]["stage"]==1,"outside_release_does_not_accept_order")
        self.snap("fresh-first-order")
    def sell(self,table,freeze=False):
        self.select(table);s=self.wait(lambda s:True,"selected table")
        if s["customers"][table]["stage"]==1:
            self.primary();s=self.wait(lambda v:v["table"]==table and v["customers"][table]["stage"]==2,"order accepted")
        recipe=s["customers"][table]["recipe"]
        if s["ready_recipe"]==3:
            for item,n in enumerate(INGREDIENTS[recipe]):
                if s["stock"][item]<n:self.pantry(item);s=self.wait(lambda v:True,"restocked")
            before=s;self.primary()
            s=self.wait(lambda v:v["cook_recipe"]==recipe,"craft started")
            expected=[before["stock"][i]-INGREDIENTS[recipe][i] for i in range(3)]
            self.check(s["stock"]==expected and s["cogs"]==before["cogs"]+sum(a*b for a,b in zip(INGREDIENTS[recipe],(2,1,1))),"recipe_consumes_real_stock_and_cost",mode=self.mode,recipe=recipe,stock=s["stock"])
            if freeze:self.freeze_test()
            s=self.wait(lambda v:v["ready_recipe"]==recipe,"real brewing completes",timeout=6)
        before=s;self.primary();after=self.wait(lambda v:v["total_served"]==before["total_served"]+1,"customer served")
        sale=after["revenue"]-before["revenue"];repaid=min(before["debt"],sale)
        self.check(sale>=PRICES[recipe] and after["coins"]==before["coins"]+sale-repaid and after["debt"]==before["debt"]-repaid and after["customers"][table]["stage"]==0,"served_customer_pays_profit",mode=self.mode,recipe=recipe,sale=sale,coins=after["coins"])
        self.report.setdefault("recipes",[0,0,0])[recipe]+=1
        self.report.setdefault("serves_by_mode",{}).setdefault(self.mode,0);self.report["serves_by_mode"][self.mode]+=1
    def exit(self):
        self.pause();state=self.snap("before-exit")
        self.b.pointer("DOWN",140,145);self.b.wait(lambda:self.b.proc.poll() is not None,"clean menu exit",timeout=8)
        self.check(self.b.proc.returncode==0,"normal_exit_flushes_save",exit_code=0)
        return state

def run(args,report):
    state=args.output/"profile";report["state_root"]=str(state)
    # Keep even socket scratch files inside this app's assigned artifact scope.
    # AF_UNIX paths are capped at 108 bytes; a relative socket path avoids that
    # limit while preserving its actual artifact-directory location.
    socket_dir=args.output/"sockets";socket_dir.mkdir(parents=True,exist_ok=True)
    import os
    prior_cwd=Path.cwd();os.chdir(socket_dir)
    try:
        with transport.Simulator(args,state,Path("one.sock"),"game",report) as b:
            b.wait(lambda:"Pocket: audio ready" in b.logs() or "Runtime permission prompt queued:" in b.logs(),"audio ready or prompt",timeout=8)
            if "Runtime permission prompt queued:" in b.logs() and "Pocket: audio ready" not in b.logs():
                b.shot("audio-permission");b.tap(196,175,settle=.5)
            s=Session(b,report);s.wait(lambda v:v["customers"][0]["stage"]==1,"first durable guest");s.safety();s.sell(0,freeze=True)
            for day in range(1,6):
                s.mode="touch" if day%2 else "controller";s.table=s.wait(lambda v:True,"day state")["table"]
                print(f"DAY {day} {s.mode} started",flush=True)
                deadline=time.monotonic()+190
                while True:
                    require(time.monotonic()<deadline,"Cafe day did not close in real time")
                    v=s.wait(lambda v:True,"economy state")
                    if v["summary"]:break
                    if v["day_ms"]>142000:time.sleep(.25);continue
                    upgraded=False
                    for line,lv in enumerate(v["level"]):
                        if lv<2 and v["coins"]>=20+22*lv+6*lv*lv+16:
                            s.grow(line);upgraded=True;break
                    if upgraded:continue
                    ready=v["ready_recipe"]
                    available=[i for i,c in enumerate(v["customers"]) if c["stage"] and c["patience"]>5500 and (ready==3 or (c["stage"]==2 and c["recipe"]==ready))]
                    if available:s.sell(available[0])
                    else:time.sleep(.2)
                v=s.snap(f"day{day}-{s.mode}-settlement")
                s.check(v["day"]==day and v["day_ms"]==150000 and v["served"]>=min(day+2,7) and v["bonus"]==8 and v["fee"]==10,"real_150s_day_goal_and_settlement",day=day,mode=s.mode,served=v["served"],cash=v["coins"],stars=v["stars"],net=v["revenue"]-v["cogs"]-v["fee"])
                report.setdefault("days",[]).append(v)
                print(f"DAY {day} CLOSED served={v['served']} cash={v['coins']} level={v['level']}",flush=True)
                s.primary();v=s.wait(lambda v:v["day"]==day+1 and not v["summary"],"next business day");s.table=0
            s.check(all(v>=2 for v in v["level"]),"all_three_upgrade_lines_reach_level2")
            s.check(all(v>0 for v in report["recipes"]) and report["serves_by_mode"].get("controller",0)>=8,"all_recipes_and_controller_complete_real_business",recipes=report["recipes"],serves=report["serves_by_mode"])
            s.wait(lambda v:any(c["stage"]==1 for c in v["customers"]),"day6 guest")
            v=s.wait(lambda v:True,"day6 table");table=next(i for i,c in enumerate(v["customers"]) if c["stage"]==1);s.select(table);s.primary();s.wait(lambda v:v["customers"][table]["stage"]==2,"interruptible accepted order")
            previous=s.exit()
        time.sleep(2)
        with transport.Simulator(args,state,Path("two.sock"),"reload",report) as b:
            s=Session(b,report);s.pause();after=s.snap("restored-real-progress")
            fields=("coins","stock","level","total_served","total_failed","stars","day","paid_day","debt","revenue","cogs")
            s.check(all(previous[k]==after[k] for k in fields) and previous["day_ms"]<=after["day_ms"]<previous["day_ms"]+1800 and "harbor save-restored" in b.logs(),"durable_business_restores_across_processes_without_offline_time",before_ms=previous["day_ms"],after_ms=after["day_ms"])
            s.tap(140,115);s.paused=False;time.sleep(.1);s.pause();reset=s.snap("restart-keeps-growth")
            s.check(all(reset[k]==after[k] for k in ("coins","stock","level","total_served","stars","day","debt")) and reset["day_ms"]<1000 and reset["served"]==0,"menu_restart_keeps_earned_growth_cash_inventory")
            s.exit()
        report["cleanup"]={"simulator_processes_stopped":True,"inputs_released":True,"serial_access":False}
    finally:os.chdir(prior_cwd)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("simulator","package","publisher-key","output"):p.add_argument("--"+name,type=Path,required=True)
    args=p.parse_args()
    for name in ("simulator","package","publisher_key","output"):setattr(args,name,getattr(args,name).resolve())
    require(args.output.is_relative_to(WORKSPACE/"local/artifacts"),"Output must be an ignored local artifact directory")
    args.output.mkdir(parents=True,exist_ok=True)
    report={"passed":False,"game":"harbor-cafe","checks":[],"processes":[]}
    try:run(args,report);report["passed"]=True
    except BaseException:report["error"]=traceback.format_exc()
    (args.output/"results.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2));return 0 if report["passed"] else 1
if __name__=="__main__":raise SystemExit(main())
