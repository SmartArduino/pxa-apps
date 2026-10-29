#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Link an isolated simulator with a file-driven public IMU provider for tests."""
import argparse,shlex,subprocess,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build',type=Path);a=p.parse_args()
r=a.workspace.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
build=a.build.resolve() if a.build else r/'build/simulator/doit-gamekit-1'
src=r/'deps/pxa-system/simulator/desktop';text=(src/'product_runner.c').read_text()
start=text.index('static pxa_status_t desktop_sensor_subscribe(');end=text.index('static void desktop_sensor_unsubscribe(',start)
text=text[:start]+'''static pxa_status_t desktop_sensor_subscribe(void* context,uint16_t id,uint32_t period,void** token) {
    (void)period;if(!context||!token||id<1||id>2)return PXA_STATUS_NOT_FOUND;*token=context;return PXA_STATUS_OK;
}
static pxa_status_t desktop_sensor_read(void* context,void* token,uint16_t id,int32_t values[PXA_SENSOR_MAX_DIMENSIONS]) {
    (void)context;(void)token;int data[6]={0,0,9806650,0,0,0};
    const char* path=getenv("GRAVITY_TEST_MOTION");FILE* file=path?fopen(path,"r"):NULL;
    if(!file)return PXA_STATUS_UNAVAILABLE;
    int n=fscanf(file,"%d %d %d %d %d %d",data,data+1,data+2,data+3,data+4,data+5);fclose(file);
    if(n!=6)return PXA_STATUS_UNAVAILABLE;
    if(id<1||id>2)return PXA_STATUS_NOT_FOUND;
    memcpy(values,data+(id-1)*3,3*sizeof(int32_t));return PXA_STATUS_OK;
}
''' + text[end:]
needle='sensor_config.struct_size = sizeof(sensor_config);';assert text.count(needle)==1
text=text.replace(needle,'''static const uint8_t acc_name[]="motion.acceleration",gyro_name[]="motion.angular-velocity";
        static const pxa_sensor_descriptor_t motion_descriptors[]={
            {1,{acc_name,sizeof(acc_name)-1},PXA_SENSOR_UNIT_MICRO_METERS_PER_SECOND_SQUARED,3,50,3000},
            {2,{gyro_name,sizeof(gyro_name)-1},PXA_SENSOR_UNIT_MICRO_RADIANS_PER_SECOND,3,50,3000}};
        sensor_config.descriptors=motion_descriptors;sensor_config.descriptor_count=2;
        '''+needle)
(out/'runner.c').write_text(text)
flags={}
for line in (build/'CMakeFiles/pxsys_product_simulator.dir/flags.make').read_text().splitlines():
 if line.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')):
  k,v=line.split('=',1);flags[k.strip()]=shlex.split(v)
subprocess.run(['cc',*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],'-I'+str(src),'-c',str(out/'runner.c'),'-o',str(out/'runner.o')],check=True)
cmd=shlex.split((build/'CMakeFiles/pxsys_product_simulator.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
cmd[cmd.index('-o')+1]=str(out/'pxsys_product_simulator');cmd=[str(out/'runner.o') if x.endswith('pxsys_product_simulator.dir/product_runner.c.o') else x for x in cmd]
subprocess.run(cmd,cwd=build,check=True);print(out/'pxsys_product_simulator')
