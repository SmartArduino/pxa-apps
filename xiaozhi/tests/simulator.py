#!/usr/bin/env python3
"""Xiaozhi fixture for the real PXA simulator (no public AI service required)."""
import argparse,base64,ctypes,hashlib,json,math,os,pathlib,socket,socketserver,struct,subprocess,threading,time,zlib,tempfile,shutil
p=argparse.ArgumentParser();p.add_argument('--simulator',required=True);p.add_argument('--package',required=True);p.add_argument('--publisher-key',required=True);p.add_argument('--artifacts',required=True);p.add_argument('--interactive',action='store_true');p.add_argument('--ime-package');p.add_argument('--offline',action='store_true');p.add_argument('--discovery',action='store_true');p.add_argument('--activation',action='store_true');p.add_argument('--emotions',action='store_true');p.add_argument('--drop',action='store_true');p.add_argument('--scenario',choices=['normal','http-error','no-websocket','activation-flow','malformed','handshake-stall','reconnect','settings-cancel','audio-tail','network-flap','large-headers','profile-separation','save-clicks','muted','capture-warmup'],default='normal');p.add_argument('--scene',choices=['auto','morning','day','evening','night'],default='auto');args=p.parse_args()
if args.scenario=='large-headers': args.discovery=True
root=pathlib.Path(args.artifacts).resolve();root.mkdir(parents=True,exist_ok=True);(root/'finish').unlink(missing_ok=True)
state=root/'state';state.mkdir(exist_ok=True)
short=pathlib.Path(tempfile.mkdtemp(prefix='pxa-xiaozhi-'));(short/'artifacts').symlink_to(root,target_is_directory=True);control=short/'artifacts/control.sock'
seen=[];errors=[];discoveries=0
def opus_audio():
    lib=ctypes.CDLL('libopus.so.0');lib.opus_encoder_create.restype=ctypes.c_void_p
    lib.opus_encoder_create.argtypes=[ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.POINTER(ctypes.c_int)]
    lib.opus_encode.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_int16),ctypes.c_int,ctypes.POINTER(ctypes.c_ubyte),ctypes.c_int32]
    lib.opus_encoder_destroy.argtypes=[ctypes.c_void_p]
    error=ctypes.c_int();encoder=lib.opus_encoder_create(16000,1,2048,ctypes.byref(error));assert encoder
    try:
        for frame in range(160 if args.emotions else 90):
            pcm=(ctypes.c_int16*320)(*[int(4000*math.sin(2*math.pi*440*(frame*320+i)/16000)) for i in range(320)])
            out=(ctypes.c_ubyte*1275)();n=lib.opus_encode(encoder,pcm,320,out,1275);assert n>0;yield bytes(out[:n])
    finally:lib.opus_encoder_destroy(encoder)
def exact(conn,n):
    data=b''
    while len(data)<n:
        chunk=conn.recv(n-len(data))
        if not chunk:raise EOFError
        data+=chunk
    return data
def frame(conn,opcode,payload,fin=True):
    conn.sendall(bytes([(0x80 if fin else 0)|opcode])+((bytes([len(payload)])) if len(payload)<126 else b'\x7e'+struct.pack('!H',len(payload)))+payload)
def send_json(conn,value):frame(conn,1,json.dumps(value,ensure_ascii=False).encode())
class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        global discoveries
        try:
            conn=self.request;conn.settimeout(120);head=b''
            while b'\r\n\r\n' not in head:
                chunk=conn.recv(1024)
                if not chunk: raise EOFError
                head+=chunk
                if len(head)>16384: raise ValueError('oversized HTTP header')
            head,posted=head.split(b'\r\n\r\n',1)
            fields={k.strip().lower():v.strip() for k,v in (line.split(b':',1) for line in head.split(b'\r\n')[1:] if b':' in line)}
            if head.startswith(b'POST '):
                required=int(fields.get(b'content-length',b'0'))
                if required>8192:raise ValueError('oversized configuration request')
                if len(posted)<required:posted+=exact(conn,required-len(posted))
                metadata=json.loads(posted[:required])
                if args.scenario=='large-headers':
                    assert metadata['mac_address']=='02:00:00:00:00:01'
                    assert metadata['uuid']=='00112233-4455-4677-8899-aabbccddeeff'
                    assert metadata['board']['type']=='generic'
                seen.append('discovery');discoveries+=1
                payload = ({'activation': {'code': '123456', 'message': '请在小智官网绑定设备'}}
                    if args.activation or (args.scenario=='activation-flow' and discoveries==1) else {'websocket': {'url': f'ws://127.0.0.1:{server.server_address[1]}/xiaozhi', 'token': 'fixture-token', 'version': 1}})
                if args.scenario in ('http-error','no-websocket'): payload={}
                body = b'bad json' if args.scenario=='malformed' else json.dumps(payload, ensure_ascii=False).encode()
                status=b'503 Service Unavailable' if args.scenario=='http-error' else b'200 OK'
                conn.sendall(b'HTTP/1.1 '+status+b'\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ' + str(len(body)).encode() + b'\r\n' + (b'X-Fixture: '+b'x'*700+b'\r\n' if args.scenario=='large-headers' else b'') + b'\r\n' + body)
                return
            if args.scenario=='handshake-stall':
                time.sleep(24);return
            assert fields[b'protocol-version']==b'1' and fields[b'authorization']==b'Bearer fixture-token'
            accept=base64.b64encode(hashlib.sha1(fields[b'sec-websocket-key']+b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
            response=b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+b'\r\n\r\n'
            for i in range(0,len(response),7):conn.sendall(response[i:i+7])
            while True:
                first,second=exact(conn,2);assert second&128
                n=second&127
                if n==126:n=struct.unpack('!H',exact(conn,2))[0]
                if n==127:n=struct.unpack('!Q',exact(conn,8))[0]
                mask=exact(conn,4);data=exact(conn,n);data=bytes(v^mask[i%4] for i,v in enumerate(data));opcode=first&15
                if opcode==2:seen.append('microphone');continue
                if opcode in (8,10):continue
                value=json.loads(data);seen.append(value)
                if value['type']=='hello':
                    assert value['audio_params']['format']=='opus' and value['audio_params']['frame_duration']==20
                    send_json(conn,{'type':'hello','transport':'websocket','session_id':'fixture-1','audio_params':{'format':'opus','sample_rate':24000,'channels':1,'frame_duration':20}})
                    if args.drop or (args.scenario in ('reconnect','profile-separation') and sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)==1):
                        time.sleep(2);seen.append('connection.dropped');return
                elif value['type']=='listen' and value['state'] in ('detect','stop'):
                    send_json(conn,{'type':'stt','text':value.get('text','你好，小智')});send_json(conn,{'type':'llm','emotion':'happy'});send_json(conn,{'type':'tts','state':'start'})
                    text=json.dumps({'type':'tts','state':'sentence_start','text':'你好，我是运行在 PXA 上的小智。'},ensure_ascii=False).encode()
                    # Split inside a UTF-8 code point and interleave a ping.
                    at=text.index('你'.encode())+1;frame(conn,1,text[:at],False);frame(conn,9,b'fixture');frame(conn,0,text[at:])
                    moods = ['neutral', 'happy', 'sad', 'thinking', 'surprised', 'angry', 'sleepy', 'neutral']
                    for index, packet in enumerate(opus_audio()):
                        if args.emotions and index % 20 == 0:
                            mood = moods[index // 20]
                            send_json(conn, {'type':'llm', 'emotion':mood});seen.append('emotion:'+mood)
                        frame(conn,2,packet);time.sleep(0 if args.scenario=='audio-tail' else .02)
                    send_json(conn,{'type':'tts','state':'stop'});seen.append('tts.done')
        except (EOFError,OSError):pass
        except Exception as exc:errors.append(repr(exc))
class Server(socketserver.ThreadingTCPServer):daemon_threads=True;allow_reuse_address=True
server=Server(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
publisher=hashlib.sha256(pathlib.Path(args.publisher_key).read_bytes()).hexdigest()
private=state/'app-data'/f'{publisher}-pxa-xiaozhi';private.mkdir(parents=True,exist_ok=True)
profile={'url':f'ws://127.0.0.1:{server.server_address[1]}/xiaozhi','token':'fixture-token','device':'02:00:00:00:00:01','client':'00112233-4455-4677-8899-aabbccddeeff','endpoint':f'http://127.0.0.1:{server.server_address[1]}/ota','official':args.discovery or args.activation or args.scenario in ['http-error','no-websocket','activation-flow','malformed','large-headers'],'auto_listen':True,'voice_enabled':True,'scene_mode':['auto','morning','day','evening','night'].index(args.scene)}
profile.update(config_version=2,keep_awake=True)
if args.scenario=='profile-separation':profile.update(official=True,url='ws://custom.invalid/preserved',token='custom-private-token')
if args.scenario=='muted':profile['voice_enabled']=False
config=json.dumps(profile).encode()
body=struct.pack('<H',1)+bytes([10])+b'connection'+struct.pack('<H',len(config))+config
(private/'.pxa-kv-b').unlink(missing_ok=True)
(root/'control.sock').unlink(missing_ok=True)
(private/'.pxa-kv-a').write_bytes(b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))+body)
log=(root/'simulator.log').open('w')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NETWORK_CONNECTED='0' if args.offline else '1',PXA_SIM_NAV_MODE='gesture-no-bar')
if args.scenario=='network-flap':
    (root/'network-state').write_text('0');env['PXA_SIM_NETWORK_STATE_FILE']=str(root/'network-state')
if args.scenario=='capture-warmup':env['PXA_SIM_CAPTURE_START_DELAY_MS']='1200'
if args.scenario=='muted':env['XIAOZHI_EXPECT_MUTED']='1'
if args.ime_package: env['PXA_IME_PACKAGE']=str(pathlib.Path(args.ime_package).resolve())
proc=subprocess.Popen([args.simulator,'--package',args.package,'--publisher-key',args.publisher_key,'--state-root',str(state),'--locale','zh-CN','--width','320','--height','240','--pxadb-control-socket',str(control)],env=env,stdout=log,stderr=subprocess.STDOUT)
def command(value):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(5);s.connect(str(control));s.sendall(value.encode()+b'\n');data=s.recv(4096)
        if data.startswith(b'PNG '):
            header,body=data.split(b'\n',1);size=int(header.split()[1])
            while len(body)<size:body+=s.recv(size-len(body))
            return body
        assert data.startswith(b'OK'),data
        return data
def shot(name):(root/name).write_bytes(command('SCREENSHOT'))
try:
    deadline=time.monotonic()+30
    while not control.exists():
        assert proc.poll() is None,(root/'simulator.log').read_text()[-3000:]
        if time.monotonic()>deadline:raise TimeoutError('simulator startup')
        time.sleep(.05)
    time.sleep(1);shot('startup.png')
    print(f'Ready: control={control} screenshots={root}',flush=True)
    if args.interactive:
        while not (root/'finish').exists() and proc.poll() is None:time.sleep(.1)
    else:
        def wait_for(predicate):
            deadline=time.monotonic()+30
            while not predicate():
                assert proc.poll() is None
                if time.monotonic()>deadline:raise TimeoutError(str([v for v in seen if v!='microphone']))
                time.sleep(.02)
        def saved_profile():
            files=list(private.glob('.pxa-kv-*'))
            raw=max((f.read_bytes() for f in files),key=lambda b:struct.unpack_from('<Q',b,8)[0])
            data=raw[24:];key=data[2];length=struct.unpack_from('<H',data,3+key)[0]
            return struct.unpack_from('<Q',raw,8)[0],json.loads(data[5+key:5+key+length])
        if args.scenario=='profile-separation':
            wait_for(lambda:'connection.dropped' in seen)
            sequence,before=saved_profile()
            assert before['url']=='ws://custom.invalid/preserved' and before['token']=='custom-private-token'
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>=2)
            wait_for(lambda:seen.count('microphone')>5)
            after_sequence,after=saved_profile()
            assert after_sequence==sequence, 'reconnecting rewrote an unchanged configuration'
            assert after['url']==before['url'] and after['token']==before['token']
            shot('profile-restored.png')
            print('PASS: official session preserves custom credentials; reconnect performs no configuration write',flush=True)
        elif args.scenario=='save-clicks':
            wait_for(lambda:'microphone' in seen)
            command('POINTER DOWN 160 120');time.sleep(.85);command('POINTER UP 160 120')
            command('TAP 256 68');time.sleep(.1);command('TAP 270 182');time.sleep(.1)
            sequence,_=saved_profile()
            command('TAP 160 212');time.sleep(.08);command('TAP 160 212')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>=2)
            wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='start' for v in seen)>=2)
            time.sleep(.5)
            after_sequence,after=saved_profile()
            assert after_sequence==sequence+1 and after['scene_mode']==4, (sequence,after_sequence,after)
            assert not any(isinstance(v,dict) and v.get('state')=='stop' for v in seen), 'second Save tap stopped the new microphone session'
            shot('saved-once.png')
            print('PASS: double Save writes once and does not trigger a main-screen action',flush=True)
        elif args.scenario=='capture-warmup':
            wait_for(lambda:'microphone' in seen)
            assert 'PXA_CAPTURE_WARMUP' in (root/'simulator.log').read_text()
            command('TAP 160 180');wait_for(lambda:'tts.done' in seen)
            wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='start' for v in seen)>=2)
            shot('capture-recovered.png')
            print('PASS: delayed microphone retries, recovers and keeps automatic listening',flush=True)
        elif args.scenario=='muted':
            wait_for(lambda:'microphone' in seen);command('TAP 160 180')
            wait_for(lambda:'tts.done' in seen)
            wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='start' for v in seen)>=2)
            time.sleep(.2);shot('muted-answer.png')
            assert 'XIAOZHI_MUTED_HANDOFF' in (root/'simulator.log').read_text(), 'muted scenario requires the Host probe'
            print('PASS: muted reply retains readable text and opens no playback session',flush=True)
        elif args.scenario in ('http-error','no-websocket','malformed'):
            wait_for(lambda:'discovery' in seen);time.sleep(1)
            shot('configuration-error.png')
            assert not any(isinstance(v,dict) and v.get('type')=='hello' for v in seen), 'configuration failure connected using stale cached credentials'
            print('PASS: configuration failure never connects using cached credentials',flush=True)
        elif args.scenario=='activation-flow':
            wait_for(lambda:'discovery' in seen);time.sleep(.5);shot('activation-before.png')
            assert not any(isinstance(v,dict) and v.get('type')=='hello' for v in seen)
            command('TAP 160 180')
            wait_for(lambda:'microphone' in seen);shot('activation-after.png')
            assert discoveries==2, discoveries
            print('PASS: activation card -> retry configuration -> verified conversation',flush=True)
        elif args.scenario=='reconnect':
            wait_for(lambda:'connection.dropped' in seen);time.sleep(.4);shot('disconnected.png')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>=2)
            wait_for(lambda:'microphone' in seen);shot('reconnected.png')
            print('PASS: server disconnect -> automatic reconnection -> microphone resumes',flush=True)
        elif args.scenario=='handshake-stall':
            command('POINTER DOWN 160 120');time.sleep(.9);command('POINTER UP 160 120')
            shot('cancel-pending-connect.png')
            print('PASS: long press stays responsive during a pending handshake',flush=True)
        elif args.scenario=='network-flap':
            time.sleep(.4);shot('offline-initial.png');assert not seen
            (root/'network-state').write_text('1')
            wait_for(lambda:'microphone' in seen);shot('online-initial.png')
            (root/'network-state').write_text('0');time.sleep(2.5)
            stopped=seen.count('microphone');time.sleep(.4)
            assert seen.count('microphone')==stopped, 'microphone still runs after network loss'
            shot('offline-after-chat.png')
            (root/'network-state').write_text('1')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>=2)
            wait_for(lambda:seen.count('microphone')>stopped);shot('online-restored.png')
            print('PASS: offline -> online -> capture stops on link loss -> automatic recovery',flush=True)
        elif args.scenario=='audio-tail':
            wait_for(lambda:'microphone' in seen);command('TAP 160 180')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='start' for v in seen)>=2)
            time.sleep(.1);shot('audio-tail-complete.png')
            assert 'XIAOZHI_AUDIO_HANDOFF' in (root/'simulator.log').read_text(), 'audio-tail requires the Host audio probe'
            print('PASS: complete TTS playback before automatic microphone resume',flush=True)
        elif args.scenario=='settings-cancel':
            wait_for(lambda:'microphone' in seen)
            command('POINTER DOWN 160 120');time.sleep(.85);command('POINTER UP 160 120');time.sleep(.1)
            command('TAP 256 68');time.sleep(.1);command('TAP 270 182');time.sleep(.1)
            command('KEY_DOWN X');time.sleep(.1);command('KEY_UP X')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>=2)
            private_snapshots=list(private.glob('.pxa-kv-*'))
            newest=max(private_snapshots,key=lambda f:struct.unpack_from('<Q',f.read_bytes(),8)[0]).read_bytes()[24:]
            key_length=newest[2];n=struct.unpack_from('<H',newest,3+key_length)[0]
            saved=json.loads(newest[5+key_length:5+key_length+n])
            assert saved['scene_mode']==['auto','morning','day','evening','night'].index(args.scene), 'Back saved unconfirmed settings'
            print('PASS: Back discards unconfirmed configuration',flush=True)
        elif args.offline:
            time.sleep(.5);shot('offline.png')
            assert not seen, seen
            print('PASS: offline screen without remote connection attempts',flush=True)
        elif args.drop:
            wait_for(lambda:'connection.dropped' in seen);time.sleep(.6);shot('reconnecting.png')
            print('PASS: retain the whole character after a verified session disconnects',flush=True)
        elif args.activation:
            wait_for(lambda:'discovery' in seen);time.sleep(.5);shot('activation.png')
            assert not any(isinstance(v,dict) and v.get('type')=='hello' for v in seen)
            print('PASS: official configuration discovery and activation screen',flush=True)
        else:
            wait_for(lambda:'microphone' in seen)
            assert any(isinstance(v,dict) and v.get('state')=='start' and v.get('mode')=='auto' for v in seen)
            if args.discovery: assert 'discovery' in seen
            shot('listening.png')
            command('TAP 160 180')
            wait_for(lambda:any(isinstance(v,dict) and v.get('state')=='stop' for v in seen))
            time.sleep(.3);shot('conversation.png')
            if args.emotions:
                for mood in ['happy', 'sad', 'thinking', 'surprised', 'angry', 'sleepy']:
                    wait_for(lambda: 'emotion:'+mood in seen)
                    time.sleep(.18);shot('emotion-'+mood+'.png')
                wait_for(lambda:'tts.done' in seen)
                wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='start' for v in seen)>=2)
                command('TAP 160 180')
                wait_for(lambda:sum(isinstance(v,dict) and v.get('state')=='stop' for v in seen)>=2)
                time.sleep(.2)
            # Tap while the response is playing: abort and resume listening.
            command('TAP 160 180')
            wait_for(lambda:any(isinstance(v,dict) and v.get('type')=='abort' for v in seen))
            wait_for(lambda:'tts.done' in seen)
            command('KEY_DOWN Z');time.sleep(.08);command('KEY_UP Z')
            time.sleep(.2)
            command('POINTER DOWN 160 120');time.sleep(.85)
            shot('settings.png');command('POINTER UP 160 120')
            time.sleep(.15)
            command('TAP 160 68');time.sleep(.12);shot('settings-dialogue.png')
            command('TAP 256 68');time.sleep(.12);shot('settings-scene.png')
            command('TAP 270 182');time.sleep(.15);shot('settings-scene-night.png')
            command('TAP 64 68');time.sleep(.1)
            command('TAP 275 28');time.sleep(.1);shot('settings-details.png')
            command('TAP 30 28');time.sleep(.1)
            if not args.discovery:
                command('TAP 180 150');time.sleep(.2);shot('settings-editor-url.png')
                command('TAP 275 30');time.sleep(.1)
                command('TAP 180 178');time.sleep(.2);shot('settings-editor-token.png')
                command('TAP 30 28');time.sleep(.1)
            before=sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)
            command('KEY_DOWN X');time.sleep(.1);command('KEY_UP X')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>before)
            command('KEY_DOWN Z');time.sleep(.85);shot('settings-button.png');command('KEY_UP Z')
            command('TAP 256 68');time.sleep(.1);command('TAP 270 182');time.sleep(.1)
            before_save=sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)
            command('TAP 160 212')
            wait_for(lambda:sum(isinstance(v,dict) and v.get('type')=='hello' for v in seen)>before_save)
            command('POINTER DOWN 160 120');time.sleep(.85);command('POINTER UP 160 120');time.sleep(.1)
            command('TAP 160 68');time.sleep(.1);command('TAP 275 28');time.sleep(.2)
            command('TEXT 你好，小智');time.sleep(.1);shot('settings-editor-text.png')
            command('TAP 275 30')
            wait_for(lambda:any(isinstance(v,dict) and v.get('state')=='detect' and v.get('text')=='你好，小智' for v in seen))
            print('PASS: auto connection/listening, STT/emotion/TTS, Unicode fragmentation/ping, Opus, touch abort, controller and long-press settings',flush=True)
    shot('final.png')
    (root/'protocol.json').write_text(json.dumps({'messages':seen,'errors':errors},ensure_ascii=False,indent=2))
    assert not errors,errors
finally:
    proc.terminate()
    try:proc.wait(timeout=10)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
    log.close();server.shutdown();server.server_close();shutil.rmtree(short)
