from pathlib import Path
import sys,os,time,json,subprocess
root=Path.cwd();sys.path.insert(0,str(root/'tools/pxadb'));import pxadb
os.environ['PXADB_BAUD']='2000000'
port='/dev/serial/by-id/usb-1a86_USB_Dual_Serial_56D5054872-if02'
name=sys.argv[1];out=root/'local/game-render-opt3-20261015'/name
assert name in ['control-cold-voxel','endpoint-final-voxel','width-voxel']
epoch=time.monotonic()
with pxadb.PxaDbClient(port,30) as c:
 info=pxadb.info_properties(c.hello());assert info['serial']=='8c:bf:ea:09:af:6c'
 frames=c.request('REBOOT');assert any(f.kind=='OK' and 'rebooting' in f.payload for f in frames)
reset=time.monotonic()
while time.monotonic()-reset<23:time.sleep(.1)
(root/'local/game-render-opt3-20261015'/(name+'-reset.json')).write_text(json.dumps({'serial':info['serial'],'reset':'PXADB REBOOT','settle_after_reply_seconds':time.monotonic()-reset,'requested_settle_seconds':23},indent=2)+'\n')
args=[sys.executable,'tools/measure-device-gameplay.py','--port',port,'--expected-serial',info['serial'],'--app','pxa-voxel-craft','--package','local/game-render-opt-20261014/voxel-package/pxa-voxel-craft.pxa','--output',str(out),'--repeat','2','--warmup','6','--seconds','4','--size','412','412','--prepare-tap','206','258','--start','206','165','--scenes','idle']
subprocess.run(args,check=True)
