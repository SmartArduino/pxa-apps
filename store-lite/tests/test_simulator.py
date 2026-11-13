#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real signed-app browsing, keyboard, login and screenshot smoke tests.
Installer mutations need the real device backend; standalone Host returns unsupported.
All generated output stays in the caller supplied artifact directory.
"""
import argparse,json,os,re,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','simulator','output'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--width',type=int,default=240);ap.add_argument('--height',type=int,default=284)
    ap.add_argument('--full',action='store_true')
    ap.add_argument('--ui-pages-only',action='store_true',help='Check lazy page lifetime and keyboard without requiring a particular catalog entry')
    ap.add_argument('--format-choice',action='store_true')
    ap.add_argument('--install-error',action='store_true')
    ap.add_argument('--install-error-only',action='store_true')
    a=ap.parse_args()
    if a.install_error_only:a.install_error=True
    for key in ('PXA_SIMULATOR_PERF','STUDY_TRACE_LAYOUT'):os.environ.pop(key,None)
    if a.ui_pages_only:os.environ['STUDY_TRACE_TREE']='1'
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
        def focus(label):
            for _ in range(45):
                if s.focus().startswith(label):return s.focus()
                s.key('DOWN')
            raise AssertionError(('unreachable',label,s.focus()))
        def choose(label):
            focus(label);s.key('ENTER')
        def load(label):
            before=count('Lite catalog loaded');choose(label)
            wait(lambda:count('Lite catalog loaded')>before)
            wait(lambda:s.focus() not in ('返回','取消'));time.sleep(.5)
        wait(lambda:s.focus()=='精选');s.screenshot('home')
        if a.ui_pages_only:
            def tree_labels():
                text=s.log_path.read_text()
                begin=text.rfind('STUDY_TREE_BEGIN')
                end=text.find('STUDY_TREE_END',begin)
                assert begin>=0 and end>begin,'Use the three-key fixture with native tree tracing'
                block=text[begin:end]
                nodes=int(re.search(r'STUDY_TREE_NODES (\d+)',block)[1])
                assert nodes<120,('inactive pages remain mounted',nodes)
                return [bytes.fromhex(x).decode('utf-8') for x in re.findall(r'STUDY_TREE_LABEL ([0-9a-f]+)',block)]
            home=tree_labels()
            for hidden in ('检查更新','清空','abc','取消'):
                assert hidden not in home,('inactive page eagerly mounted',hidden)
            choose('搜索')
            search=tree_labels()
            assert '按键输入' not in search
            assert all(word in search for word in ('学习','工具','英语','音乐'))
            s.screenshot('search-empty')
            choose('abc');choose('a')
            wait(lambda:tree_labels().count('a')==2);s.screenshot('keyboard-letter')
            choose('退一格');wait(lambda:tree_labels().count('a')==1)
            choose('返回');wait(lambda:s.focus()=='精选')
            choose('账号');wait(lambda:s.focus()=='网页登录')
            tree_labels();s.screenshot('account');choose('返回')
            choose('已安装');wait(lambda:s.focus()=='检查更新')
            tree_labels();s.screenshot('installed-empty');choose('返回')
            choose('下载');wait(lambda:'暂无下载' in '\n'.join(tree_labels()))
            s.screenshot('downloads-empty');choose('返回')
            load('应用');choose('分类');tree_labels();s.screenshot('categories')
            choose('返回');choose('返回');wait(lambda:s.focus()=='精选')
            assert 'Lite page scheduling failed' not in s.log_path.read_text()
            for error in ('auxiliary stack','out of bounds memory','unreachable instruction'):
                assert error not in s.log_path.read_text(),error
            print(json.dumps({'result':'passed','lazy_pages':True,'bounded_native_tree':True,
                              'three_key_keyboard':True,'return_navigation':True,
                              'viewport':[a.width,a.height]},ensure_ascii=False),flush=True)
            return
        load('精选');s.screenshot('catalog')
        before=count('Lite page 3');s.key('ENTER');wait(lambda:count('Lite page 3')>before+0 and '取消'!=s.focus())
        wait(lambda:s.focus() in ('安装','更新','打开') or (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')));time.sleep(.6);s.screenshot('detail')
        if a.format_choice:
            assert s.focus() in ('安装','更新','打开'),s.focus()
            focus('仅下载');s.key('DOWN')
            aot=s.focus()
            assert aot.startswith('AOT 版') and re.search(r'\d+\.\d+ [KMG]iB',aot),aot
            before=count('Lite page 3');s.key('ENTER')
            wait(lambda:count('Lite page 3')>before and s.focus()=='安装')
            focus('仅下载');s.key('DOWN')
            wasm=s.focus()
            assert wasm.startswith('WASM 版') and re.search(r'\d+\.\d+ [KMG]iB',wasm),wasm
            assert aot.split('·')[-1].strip()!=wasm.split('·')[-1].strip(),(aot,wasm)
            s.screenshot('format-wasm')
            before=count('Lite page 3');s.key('ENTER')
            wait(lambda:count('Lite page 3')>before and s.focus()=='安装')
            focus('仅下载');s.key('DOWN')
            assert s.focus()==aot,(s.focus(),aot)
            s.screenshot('format-aot')
            print(json.dumps({'result':'passed','format_switch':['aot','wasm','aot'],
                              'below_download_only':True,'sizes':[aot,wasm],
                              'install_label':'安装'},ensure_ascii=False),flush=True)
            return
        if (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')):s.key('DOWN')
        if a.install_error:
            assert s.focus()=='安装'
            s.key('ENTER')
            wait(lambda:'Lite install failed:' in s.log_path.read_text())
            wait(lambda:s.focus()=='安装' or (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')))
            if (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')):s.key('DOWN')
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
            wait(lambda:s.focus()=='安装' or (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')))
            if (s.focus().startswith('AOT 版') or s.focus().startswith('WASM 版')):s.key('DOWN')
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
        choose('分类');s.screenshot('categories');load('全部分类')
        choose('返回');choose('搜索');s.screenshot('keyboard')
        # Type a game name with only Up/Down/Confirm, with no system IME.
        for group,letter in [('ghi','g'),('abc','b')]:
            choose(group);choose(letter)
        load('搜索');s.screenshot('search-results')
        assert 'GB/GBC' in s.focus(),s.focus()
        choose('返回');choose('已安装')
        wait(lambda:s.focus()=='检查更新');choose('检查更新');wait(lambda:s.focus()=='检查更新')
        s.screenshot('installed-empty');choose('返回');choose('下载');wait(lambda:'Lite page 6' in s.log_path.read_text());s.screenshot('downloads-empty');choose('返回')
        choose('账号');s.screenshot('account')
        if a.full:
            choose('网页登录');wait(lambda:'Lite account phase=3' in s.log_path.read_text(),45);s.screenshot('account-code')
            choose('取消登录')
        choose('返回');s.expect('精选')
        text=s.log_path.read_text()
        for error in ('unreachable instruction','out of bounds memory','coroutine frame allocation','WASM module instantiate failed'):assert error not in text,error
    print(json.dumps({'viewport':[a.width,a.height],'three_key':True,'live_catalog':True,'keyboard_search':True,'screenshots_and_login':a.full,'result':'passed'},ensure_ascii=False),flush=True)
if __name__=='__main__':main()
