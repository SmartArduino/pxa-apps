#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real signed-app browsing, keyboard, login and screenshot smoke tests.
Installer mutations need the real device backend; standalone Host returns unsupported.
All generated output stays in the caller supplied artifact directory.
"""
import argparse,json,os,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','simulator','output'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--width',type=int,default=240);ap.add_argument('--height',type=int,default=284)
    ap.add_argument('--full',action='store_true')
    ap.add_argument('--install-error',action='store_true')
    ap.add_argument('--install-error-only',action='store_true')
    a=ap.parse_args()
    if a.install_error_only:a.install_error=True
    for key in ('PXA_SIMULATOR_PERF','STUDY_TRACE_LAYOUT'):os.environ.pop(key,None)
    sys.path.insert(0,str(a.workspace/'local/pxa-apps/common/study/tests'))
    from test_three_key_simulator import ThreeKey
    a.output.mkdir(parents=True,exist_ok=True)
    with ThreeKey(a.workspace,a.package_root,a.output,'store-lite',a.width,a.height,'browse',executable=a.simulator) as s:
        def wait(predicate,seconds=30):
            deadline=time.monotonic()+seconds
            while time.monotonic()<deadline:
                if predicate():return
                assert s.process.poll() is None,'simulator exited'
                time.sleep(.1)
            s.screenshot('timeout')
            raise AssertionError(('timeout',s.focus(),s.log_path.read_text()[-1800:]))
        def count(text):return s.log_path.read_text().count(text)
        def choose(label):
            for _ in range(45):
                if s.focus().startswith(label):s.key('ENTER');return
                s.key('DOWN')
            raise AssertionError(('unreachable',label,s.focus()))
        def load(label):
            before=count('Lite catalog loaded');choose(label)
            wait(lambda:count('Lite catalog loaded')>before)
            wait(lambda:s.focus() not in ('返回','取消本次操作'));time.sleep(.5)
        wait(lambda:s.focus()=='应用');s.screenshot('home')
        load('应用');s.screenshot('catalog')
        before=count('Lite page 3');s.key('ENTER');wait(lambda:count('Lite page 3')>before+0 and '取消本次操作'!=s.focus())
        wait(lambda:s.focus() in ('安装','更新','打开'));time.sleep(.6);s.screenshot('detail')
        if a.install_error:
            assert s.focus()=='安装'
            s.key('ENTER')
            wait(lambda:'Lite install failed:' in s.log_path.read_text())
            wait(lambda:s.focus()=='安装')
            from PIL import Image
            def check_error_visible(name):
                path=s.screenshot(name)
                with Image.open(path) as capture:
                    image=capture.convert('RGB')
                    red=sum(r>100 and g<120 and b<120 and r>g+35
                            for y in range(40,min(205,image.height))
                            for x in range(8,image.width-8)
                            for r,g,b in [image.getpixel((x,y))])
                    assert red>40,('error not visible near Install',red)
            check_error_visible('install-error')
            time.sleep(3)
            check_error_visible('install-error-retained')
            s.key('ENTER')
            wait(lambda:count('Lite install failed:')>=2)
            wait(lambda:s.focus()=='安装')
            if a.install_error_only:
                print(json.dumps({'result':'passed','install_failure_visible':True,
                                  'retained_after_seconds':3,'retry':True}),flush=True)
                return
        choose('权限与兼容信息');s.screenshot('permissions')
        if a.full:
            before=count('Lite image ready');choose('查看截图');s.screenshot('picture-pending')
            wait(lambda:count('Lite image ready')>before)
            s.screenshot('picture');choose('返回')
        choose('返回');choose('返回');load('热门榜');load('下一页');s.screenshot('next-page');load('上一页')
        choose('分类筛选');s.screenshot('categories');load('全部分类')
        choose('返回');choose('搜索');choose('按键输入');s.screenshot('keyboard')
        # Type a game name with only Up/Down/Confirm, with no system IME.
        for group,letter in [('ghi','g'),('abc','b')]:
            choose(group);choose(letter)
        load('完成并搜索');s.screenshot('search-results')
        assert 'GB/GBC' in s.focus(),s.focus()
        choose('返回');choose('已安装与更新')
        wait(lambda:s.focus()=='检查所有更新');choose('检查所有更新');wait(lambda:s.focus()=='检查所有更新')
        s.screenshot('installed-empty');choose('返回');choose('下载管理');wait(lambda:'Lite page 6' in s.log_path.read_text());s.screenshot('downloads-empty');choose('返回')
        choose('我的账号');s.screenshot('account')
        if a.full:
            choose('网页登录');wait(lambda:'Lite account phase=3' in s.log_path.read_text(),45);s.screenshot('account-code')
            choose('取消登录')
        choose('返回');s.expect('应用')
        text=s.log_path.read_text()
        for error in ('unreachable instruction','out of bounds memory','coroutine frame allocation','WASM module instantiate failed'):assert error not in text,error
    print(json.dumps({'viewport':[a.width,a.height],'three_key':True,'live_catalog':True,'keyboard_search':True,'screenshots_and_login':a.full,'result':'passed'},ensure_ascii=False),flush=True)
if __name__=='__main__':main()
