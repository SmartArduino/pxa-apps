#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build an isolated Host using file-driven public sensors for navigation tests."""
import argparse
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--workspace', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--build', type=Path)
parser.add_argument('--gen4', type=Path)
args = parser.parse_args()
root, output = args.workspace.resolve(), args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
source = root/'deps/pxa-system/simulator/desktop'
build = args.build.resolve() if args.build else root/'build/simulator/pai-touch'
text = (source/'product_runner.c').read_text()
start = text.index('static pxa_status_t desktop_sensor_subscribe(')
end = text.index('static void drain_net_completions(', start)
text = text[:start] + r'''
static pxa_status_t desktop_sensor_subscribe(void* context, uint16_t id,
        uint32_t period, void** token) {
    (void)period;
    const char* mode = getenv("NAV_TEST_MODE");
    if (!context || !token || id < 1 || id > 5) return PXA_STATUS_NOT_FOUND;
    if (mode && ((!strcmp(mode, "deny-gyro") && id == 3) ||
                 (!strcmp(mode, "deny-primary") && id == 1))) return PXA_STATUS_DENIED;
    *token = context;
    fprintf(stderr,"NAV_TEST_SUB %u\n",id);fflush(stderr);
    return PXA_STATUS_OK;
}
static pxa_status_t desktop_sensor_read(void* context, void* token,
        uint16_t id, int32_t values[PXA_SENSOR_MAX_DIMENSIONS]) {
    (void)context; (void)token;
    const char* pause=getenv("NAV_TEST_PAUSE_ABSOLUTE");
    if (pause && id!=3 && access(pause,F_OK)==0) return PXA_STATUS_UNAVAILABLE;
    int data[10];
    const char* path=getenv("NAV_TEST_SENSORS");
    FILE* file=path ? fopen(path,"r") : NULL;
    if (!file) return PXA_STATUS_UNAVAILABLE;
    int count=fscanf(file,"%d %d %d %d %d %d %d %d %d %d",
        data,data+1,data+2,data+3,data+4,data+5,data+6,data+7,data+8,data+9);
    fclose(file);
    if (count!=10) return PXA_STATUS_UNAVAILABLE;
    if (id==4) values[0]=data[9];
    else if (id==1 || id==5) memcpy(values,data,3*sizeof(int32_t));
    else if (id==2 || id==3) memcpy(values,data+(id-1)*3,3*sizeof(int32_t));
    else return PXA_STATUS_NOT_FOUND;
    return PXA_STATUS_OK;
}
static void desktop_sensor_unsubscribe(void* context, void* token, uint16_t id) {
    (void)context;(void)token;
    fprintf(stderr,"NAV_TEST_UNSUB %u\n",id);fflush(stderr);
}
''' + text[end:]
needle = 'sensor_config.struct_size = sizeof(sensor_config);'
assert text.count(needle) == 1
text = text.replace(needle, r'''
        static const uint8_t magnetic_name[]="magnetic-field.0", gravity_name[]="motion.acceleration",
            angular_name[]="motion.angular-velocity", heading_name[]="orientation.heading",
            second_name[]="magnetic-field.1";
        static pxa_sensor_descriptor_t nav_descriptors[]={
            {1,{magnetic_name,sizeof(magnetic_name)-1},PXA_SENSOR_UNIT_NANO_TESLA,3,100,3000},
            {2,{gravity_name,sizeof(gravity_name)-1},PXA_SENSOR_UNIT_MICRO_METERS_PER_SECOND_SQUARED,3,100,3000},
            {3,{angular_name,sizeof(angular_name)-1},PXA_SENSOR_UNIT_MICRO_RADIANS_PER_SECOND,3,20,3000},
            {4,{heading_name,sizeof(heading_name)-1},PXA_SENSOR_UNIT_MICRO_DEGREES,1,100,3000},
            {5,{second_name,sizeof(second_name)-1},PXA_SENSOR_UNIT_NANO_TESLA,3,100,3000}};
        const char* mode=getenv("NAV_TEST_MODE");
        unsigned count=3;
        if (mode && !strcmp(mode,"heading")) {nav_descriptors[0]=nav_descriptors[3];count=1;}
        else if (mode && !strcmp(mode,"all")) count=5;
        else if (mode && !strcmp(mode,"dual")) {nav_descriptors[3]=nav_descriptors[4];count=4;}
        else if (mode && !strcmp(mode,"gyro-only")) {nav_descriptors[0]=nav_descriptors[2];count=1;}
        else if (mode && !strcmp(mode,"no-gravity")) count=1;
        else if (mode && !strcmp(mode,"no-gyro")) count=2;
        else if (mode && !strcmp(mode,"none")) count=0;
        else if (mode && !strcmp(mode,"bad-unit")) {
            nav_descriptors[0]=nav_descriptors[3];nav_descriptors[0].unit=PXA_SENSOR_UNIT_MICRO_RADIANS_PER_SECOND;count=1;
        }
        sensor_config.descriptors=nav_descriptors;
        sensor_config.descriptor_count=count;
        ''' + needle)
text = text.replace('sensor_config.max_subscriptions_per_component = 2;',
                    'sensor_config.max_subscriptions_per_component = 4;')
needle = 'uint32_t delay = lv_timer_handler();'
assert text.count(needle) == 1
calls = 'navigation_test_revoke(&host);\n        '
if args.gen4:
    calls += 'study_navigation_poll();\n        '
    text = 'void study_navigation_poll(void);\n' + text
text = 'static void navigation_test_revoke(void*);\n' + text.replace(needle, calls + needle)
text += r'''
static void navigation_test_revoke(void* context) {
    product_host_t* host=context;
    const char* path=getenv("NAV_TEST_REVOKE");
    FILE* file=path ? fopen(path,"r") : NULL;
    if (!file) return;
    char scope[80]={0};
    if (!fgets(scope,sizeof(scope),file)) {fclose(file);return;}
    fclose(file);unlink(path);
    size_t n=strcspn(scope,"\r\n");
    static const uint8_t name[]="sensor.read";
    pxa_component_t affected[8];size_t count=0;
    pxa_status_t result=pxa_permission_revoke(host->permissions,
        (pxa_bytes_t){name,sizeof(name)-1},(pxa_bytes_t){(const uint8_t*)scope,n},affected,8,&count);
    fprintf(stderr,"NAV_TEST_REVOKED %s status=%d count=%zu\n",scope,result,count);fflush(stderr);
}
'''
(output/'runner.c').write_text(text)
flags = {}
for line in (build/'CMakeFiles/pxsys_product_simulator.dir/flags.make').read_text().splitlines():
    if line.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')):
        key,value=line.split('=',1)
        flags[key.strip()]=shlex.split(value)
common = flags['C_DEFINES']+flags['C_INCLUDES']
subprocess.run(['cc',*common,*flags['C_FLAGS'],'-I'+str(source),'-c',str(output/'runner.c'),'-o',str(output/'runner.o')],check=True)
command=shlex.split((build/'CMakeFiles/pxsys_product_simulator.dir/link.txt').read_text())
command=[part for part in command if not part.startswith('-Wl,--dependency-file=')]
command[command.index('-o')+1]=str(output/'pxsys_product_simulator')
command=[str(output/'runner.o') if part.endswith('pxsys_product_simulator.dir/product_runner.c.o') else part for part in command]
if args.gen4:
    bridge=root/'local/pxa-apps/common/study/tests/navigation_bridge.cpp'
    subprocess.run(['c++',*common,'-std=c++20','-O2','-I'+str(args.gen4.resolve()/'components/mz79_navigation/include'),
        '-c',str(bridge),'-o',str(output/'navigation.o')],check=True)
    command.extend([str(output/'navigation.o'),'-lstdc++','-Wl,--wrap=read'])
subprocess.run(command,cwd=build,check=True)
print(output/'pxsys_product_simulator')
