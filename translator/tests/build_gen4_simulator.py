#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Isolated runner using the product's actual Gen4 navigation and Host IME.

Only the test Host uses the built-in decoder, so tests need no external IME
package. The app still uses ordinary native text fields without test hooks.
"""
import argparse, shlex, subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
for name in ('workspace', 'gen4', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
r, out = a.workspace.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
build = r/'build/simulator/doit-gamekit-1'
src = r/'deps/pxa-system/simulator/desktop'
flags = {}
for line in (build/'CMakeFiles/pxsys_product_simulator.dir/flags.make').read_text().splitlines():
    if line.startswith(('C_DEFINES =', 'C_INCLUDES =', 'C_FLAGS =')):
        key, value = line.split('=', 1); flags[key.strip()] = shlex.split(value)
common = flags['C_DEFINES'] + flags['C_INCLUDES']
text = (src/'product_runner.c').read_text()
needle = 'uint32_t delay = lv_timer_handler();'
assert text.count(needle) == 1
text = text.replace(needle, 'translator_navigation_poll(&host, translator_test_edit, host.ime_keyboard && pxa_lvgl_ime_active(host.ime_keyboard) ? pxa_lvgl_ime_root(host.ime_keyboard) : NULL);\n        ' + needle)
text = 'void translator_navigation_poll(void*, void (*)(void*, void*), void*);\nstatic void translator_test_edit(void*, void*);\n' + text
text += '''
static void translator_test_edit(void* context, void* object) {
    product_host_t* host = context;
    if (!host->ime_keyboard) {
        pxa_lvgl_ime_config_t config = host->ime_config;
        config.input = NULL;
        config.preferences = (pxa_ime_preferences_t){PXA_IME_LAYOUT_FULL, PXA_IME_ENGLISH};
        host->ime_keyboard = pxa_lvgl_ime_create(&config);
    }
    if (host->ime_keyboard) pxa_lvgl_ime_focus(host->ime_keyboard, object);
}
'''
(out/'runner.c').write_text(text)
bridge = (r/'local/pxa-apps/common/study/tests/navigation_bridge.cpp').read_text()
bridge = bridge.replace('std::string label(lv_obj_t* object) {', '''std::string label(lv_obj_t* object) {
    if(object && lv_obj_has_class(object, &lv_textarea_class)) return "[文本输入]";
    if(object && lv_obj_has_class(object, &lv_buttonmatrix_class)) {
        auto index=lv_buttonmatrix_get_selected_button(object);
        auto* text=lv_buttonmatrix_get_button_text(object,index);
        return std::string("[键盘]")+(text ? text : "");
    }''')
bridge = bridge.replace('void study_navigation_poll()', 'void translator_navigation_poll(void* context, void (*edit)(void*, void*), void* scope)')
bridge = bridge.replace('navigation.Refresh(root);', 'navigation.Refresh(scope ? static_cast<lv_obj_t*>(scope) : root, scope != nullptr);')
bridge = bridge.replace('if(key==SDLK_RETURN)navigation.Confirm();', '''if(key==SDLK_RETURN) {
            auto* target=navigation.Focused();
            if(target && lv_obj_has_class(target,&lv_textarea_class)) edit(context,target);
            else navigation.Confirm();
        }''')
(out/'navigation.cpp').write_text(bridge)
subprocess.run(['cc', *common, *flags['C_FLAGS'], '-I'+str(src), '-c', str(out/'runner.c'), '-o', str(out/'runner.o')], check=True)
subprocess.run(['c++', *common, '-std=c++20', '-O2', '-I'+str(a.gen4.resolve()/'components/mz79_navigation/include'), '-c', str(out/'navigation.cpp'), '-o', str(out/'navigation.o')], check=True)
command = shlex.split((build/'CMakeFiles/pxsys_product_simulator.dir/link.txt').read_text())
command = [x for x in command if not x.startswith('-Wl,--dependency-file=')]
command[command.index('-o')+1] = str(out/'pxsys_product_simulator')
command = [str(out/'runner.o') if x.endswith('pxsys_product_simulator.dir/product_runner.c.o') else x for x in command]
subprocess.run(command+[str(out/'navigation.o'), '-lstdc++'], cwd=build, check=True)
print(out/'pxsys_product_simulator')
