#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify signed packages with the real Gen4 navigator and Host input method."""
import argparse, json, re, time
from array import array
from pathlib import Path
from simulator import Simulator, seed_settings, fixture

class NativeSimulator(Simulator):
    def key(self, value):
        assert value in ('UP', 'DOWN', 'ENTER'), 'Gen4 has only three keys'
        self.command('KEY_DOWN '+value); time.sleep(.035)
        if self.proc.poll() is not None:return
        try:self.command('KEY_UP '+value)
        except (FileNotFoundError, ConnectionRefusedError):
            assert self.proc.wait(timeout=2)==0
        time.sleep(.11)

    def focus(self):
        values = re.findall(r'^STUDY_FOCUS (.*)$', self.logs(), re.M)
        return values[-1] if values else ''

    def choose(self, label, activate=True):
        for page in range(6):
            start=self.focus();seen_next=False
            for step in range(64):
                current=self.focus()
                if current==label:
                    if activate:self.key('ENTER')
                    return
                seen_next |= current=='下一页'
                if step and current==start and not current.startswith('['):break
                self.key('DOWN')
            if not seen_next:break
            self.choose('下一页')
        raise AssertionError(('unreachable control',label,self.focus(),self.logs()[-1200:]))

    def text(self, value):
        # Text injection is used only for long/error fixtures; the separate
        # keyboard path enters actual characters using three physical keys.
        self.choose('输入原文'); self.choose('清空原文')
        self.choose('[文本输入]', activate=False)
        for chunk in re.findall('.{1,20}',value,re.S):self.command('TEXT '+chunk)
        time.sleep(.15)
        self.choose('完成并翻译')

    def keyboard_hi(self):
        self.choose('输入原文'); self.choose('[文本输入]')
        self.wait(lambda:self.focus().startswith('[键盘]'), 'system keyboard')
        self.shot('system-ime')
        self.key('DOWN'); self.key('ENTER') # Letter matrix, editing mode.
        for char in ('h', 'i'):
            self.choose('[键盘]'+char)
        self.shot('system-ime-typed')
        self.choose('[键盘]完成')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--publisher-key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
    p.add_argument('--mode',choices=('layouts','fixture','live','touch'),default='fixture')
    a=p.parse_args()
    for name in ('simulator','package','output','publisher_key'):setattr(a,name,getattr(a,name).resolve())
    a.output.mkdir(parents=True,exist_ok=True)
    if a.mode=='touch':
        state=a.output/'state'
        with Simulator(a,'bootstrap',480,320,state=state):pass
        with fixture(a.output) as (url,calls):
            seed_settings(state,url)
            with Simulator(a,'touch',480,320,state=state) as s:
                s.tap(90,130);s.tap(100,75);s.command('TEXT Hello world.');s.tap(180,290)
                s.wait(lambda:'translation completed' in s.logs(),'touch translation');s.shot('translated')
                assert json.loads(calls[0][1])['q']=='Hello world.'
        print('PASS native controls and translation using touch.',flush=True)
        return
    if a.mode=='layouts':
        cases=[('tiny',160,128,()),('small',240,240,()),('gen4',240,284,()),
               ('gen4-dpi',240,284,('--density-dpi','125')),('device',296,240,()),
               ('wide',800,480,()),('portrait',390,844,()),('square',480,480,()),
               ('round',412,412,('--round',)),
               ('dpi',800,480,('--density-dpi','240','--safe-insets','12,8,16,8','--corner-radius','28'))]
        for label,w,h,extra in cases:
            with NativeSimulator(a,'layout-'+label,w,h,extra=extra) as s:
                s.shot('home');s.choose('输入原文');s.shot('editor');s.choose('返回')
                s.choose('原文语言');s.shot('languages');s.choose('返回')
                s.choose('设置');s.shot('settings');s.choose('语音服务与密钥');s.shot('voice-settings')
                if label in ('tiny','small','device'):
                    # On short screens the URL/key/model occupy separate pages.
                    s.choose('下一页');s.shot('voice-key');s.choose('下一页');s.shot('voice-model')
                s.choose('返回');s.choose('文本翻译接口');s.choose('LibreTranslate');s.shot('text-settings');s.choose('返回')
                assert 'native control failed' not in s.logs()
            print('PASS native layout',label,w,h,flush=True)
        return
    if a.mode=='live':
        with NativeSimulator(a,'live',480,320,audio='disk') as s:
            s.text('你好，世界。');s.wait(lambda:'translation completed' in s.logs(),'online translation',45);s.shot('translated')
            s.choose('朗读译文');s.wait(lambda:'speech completed' in s.logs(),'online TTS',45)
            assert sum(x!=0 for x in array('h',s.pcm.read_bytes()))>1000
        print('PASS actual online translation and TTS through native controls.',flush=True)
        return
    state=a.output/'state'
    with NativeSimulator(a,'bootstrap',240,284,state=state):pass
    with fixture(a.output) as (url,calls):
        seed_settings(state,url)
        with NativeSimulator(a,'gen4',240,284,state=state) as s:
            s.keyboard_hi();s.wait(lambda:'translation completed' in s.logs(),'three-key text entry and translation')
            assert json.loads(calls[0][1])['q']=='qhi',[(p,len(b)) for p,b in calls]
            s.shot('translated');s.choose('原文语言');s.choose('日语');s.choose('交换语言')
            s.choose('设置');s.choose('语音服务与密钥');s.choose('OpenAI');s.choose('Groq');s.choose('返回');s.choose('返回')
            # Restore the fixture's saved custom endpoint, and verify save is reachable.
            s.choose('设置');s.choose('语音服务与密钥');s.choose('保存设置');s.choose('返回')
            s.choose('语音翻译');time.sleep(1);s.shot('recording');s.choose('完成并翻译')
            s.wait(lambda:s.logs().count('translation completed')==2,'record -> Ogg -> ASR -> translation')
            s.choose('翻译记录');s.shot('history');s.choose('翻译记录 1');s.choose('查看完整译文');s.shot('output');s.choose('返回')
            for value in ('Long '+('hello world. '*25), 'Failure probe.', 'Cancel probe.'):
                before=s.logs().count('translation completed');s.text(value)
                if value.startswith('Long'):
                    s.wait(lambda:s.logs().count('translation completed')>before,'long result')
                    s.choose('查看完整译文');s.shot('long-first');s.choose('下一页');s.shot('long-next');s.choose('上一页');s.choose('返回')
                elif value=='Failure probe.':
                    s.wait(lambda:'operation failed' in s.logs(),'rate limit');s.shot('rate-limit')
                else:
                    s.wait(lambda:len(calls)>=6,'pending request');s.choose('翻译');time.sleep(2.2)
                    assert s.logs().count('translation completed')==before
            s.choose('语音翻译');time.sleep(.4);s.choose('返回');before=len(calls);time.sleep(.3);assert len(calls)==before
            s.choose('设置');s.choose('本次记录开关');s.choose('清空本次记录');s.choose('退出应用')
            assert s.proc.wait(timeout=5)==0, 'three-key exit failed'
        with NativeSimulator(a,'restart',240,284,state=state) as s:
            s.choose('翻译记录');s.shot('empty-after-restart')
            assert '翻译记录 1' not in s.logs()
    print('PASS Gen4 Up/Down/Confirm: real Host IME characters, language/swap, settings, recording/ASR, history, long-result paging, rate limits, cancel, exit and restart.',flush=True)

if __name__=='__main__':main()
