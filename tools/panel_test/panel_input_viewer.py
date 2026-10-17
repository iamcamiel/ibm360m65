"""Local live view and evidence recorder for the PDI1 raw panel input image."""
import argparse,collections,datetime,json,os,pathlib,sys,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'gen/ise-ros-distribution-20261008/ssh-deps'))
import paramiko
PAGE=r"""<!doctype html><meta charset="utf-8"><title>Panel input mapping</title>
<style>
body{font:16px system-ui;background:#10151e;color:#eaf0ff;margin:28px;max-width:1150px}
h1{margin-bottom:4px}p{color:#aebbd1}button,input{font:inherit;padding:9px 14px;border:1px solid #506079;border-radius:7px;background:#202b3c;color:white}
button{cursor:pointer}#banks{display:grid;gap:10px;margin-top:24px}.bank{display:grid;grid-template-columns:65px repeat(24,1fr);gap:4px;align-items:center}
.bit{padding:8px 0;text-align:center;background:#233045;border-radius:4px;font:13px monospace}.high{background:#257470}.changed{outline:2px solid #ffcb66}
#status{color:#a5e7c8}.bar{display:flex;gap:10px;flex-wrap:wrap;margin:16px 0}pre{white-space:pre-wrap;background:#182232;padding:15px;border-radius:8px;line-height:1.5}.stopped #status{color:#ffb0a0;font-weight:bold}.stopped #banks{opacity:.35}
</style><h1>Panel input mapping</h1><p>Raw levels from all eight input banks. Green = 1, dark = 0; gold border = changed from baseline. Neither colour means pressed.</p>
<div id="status">Connecting…</div><div class="bar"><button onclick="act('baseline')">Set released / reference baseline</button>
<input id="label" placeholder="Control name and position" size="30"><button onclick="act('record')">Record this position</button></div>
<div id="banks"></div><pre id="changes"></pre><pre id="records"></pre>
<script>
async function act(action){let r=await fetch('/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,label:document.querySelector('#label').value})});let s=await r.json();if(s.error)alert(s.error)}
async function update(){try{let s=await(await fetch('/state')).json();let f=s.frame;
document.querySelector('#status').textContent=s.error||(!f?'Waiting for first complete input frame…':(s.stale?'Readout stopped — ':'Live — ')+'Frame '+f.generation+' · '+s.samples+' samples · '+s.age_seconds.toFixed(2)+' s old');
document.body.classList.toggle('stopped',!!s.error||s.stale);
document.querySelectorAll('button').forEach(b=>b.disabled=!!s.error||s.stale||!f);
if(f){document.querySelector('#banks').innerHTML=f.banks.map((v,b)=>'<div class="bank"><b>Bank '+b+'</b>'+Array.from({length:24},(_,n)=>'<div title="Bank '+b+', bit '+n+', level '+((v>>>n)&1)+'" class="bit '+(((v>>>n)&1)?'high ':'')+((s.baseline&&((v^s.baseline.banks[b])>>>n)&1)?'changed':'')+'">'+n+'<br>'+((v>>>n)&1)+'</div>').join('')+'</div>').join('');
document.querySelector('#changes').textContent=(s.baseline?'Changes from baseline:\n'+s.changes.map(c=>'Bank '+c.bank+', bit '+c.bit+': '+c.before+' → '+c.after).join('\n'):'Set a baseline with all momentary buttons released.')+'\n\nLast second: '+s.recent_changes+' input transitions. Build '+f.build_date+'/'+f.build_time;}
document.querySelector('#records').textContent=s.records.map(r=>r.label+': '+r.changes.map(c=>'B'+c.bank+'.'+c.bit+' '+c.before+'→'+c.after).join(', ')+(r.unstable?' [unstable readings]':'')).join('\n');
}catch(e){document.querySelector('#status').textContent='Viewer connection lost: '+e}setTimeout(update,200)}update()
</script>"""
def changes(before,after):
    return [{'bank':b,'bit':n,'before':(before[b]>>n)&1,'after':(after[b]>>n)&1}
            for b in range(8) for n in range(24) if (before[b]^after[b])&(1<<n)]
class Recorder:
    def __init__(self,evidence,remote,poll_interval=0.02):
        self.evidence=evidence;self.remote=remote;self.poll_interval=poll_interval;self.lock=threading.Lock()
        self.frame=None;self.baseline=None;self.samples=0;self.updated=0;self.error=None
        self.recent=collections.deque();self.records=[]
        if (evidence/'baseline.json').exists():self.baseline=json.loads((evidence/'baseline.json').read_text())
        if (evidence/'mapping-candidates.json').exists():self.records=json.loads((evidence/'mapping-candidates.json').read_text())
    def stream(self):
        while True:
            self.stream_once()
            time.sleep(5)
    def stream_once(self):
        c=paramiko.SSHClient();c.load_host_keys(str(pathlib.Path.home()/'.ssh/known_hosts'))
        c.set_missing_host_key_policy(paramiko.RejectPolicy())
        try:
            c.connect('172.22.80.36',username='ibm',password=os.environ['IBM360_PI_PASSWORD'],
                      look_for_keys=False,allow_agent=False,timeout=6,banner_timeout=6,auth_timeout=6)
            c.get_transport().set_keepalive(5)
            # Check separately: embedding the launch in the same shell command
            # makes pgrep match that shell's own command line.
            probe_i,probe_o,probe_e=c.exec_command('pgrep -f \'[p]anel_input_reader.py --watch\'',timeout=5)
            existing=probe_o.read().decode().strip()
            probe_status=probe_o.channel.recv_exit_status()
            if existing or probe_status!=1:raise RuntimeError('A panel input reader already exists or the process check failed')
            i,o,e=c.exec_command('exec sudo -S -p "" python3 '+self.remote+'/panel_input_reader.py --watch '+str(self.poll_interval),timeout=5)
            i.write(os.environ['IBM360_PI_PASSWORD']+'\n');i.flush()
            previous=None
            with (self.evidence/'frames.jsonl').open('a',buffering=1) as log, (self.evidence/'transitions.jsonl').open('a',buffering=1) as edges:
                for line in o:
                    f=json.loads(line)
                    if not f['frame_valid']:continue
                    if f['enable_divider']!=1024:raise ValueError('Unexpected scanner divider')
                    now=time.monotonic()
                    with self.lock:
                        self.error='Snapshot generation did not advance' if previous and previous['generation']==f['generation'] else None
                        delta=changes(previous['banks'],f['banks']) if previous else []
                        if previous is None:self.recent.clear()
                        if delta:edges.write(json.dumps({'checked_at_utc':f['checked_at_utc'],'changes':delta})+'\n')
                        self.frame=f;self.samples+=1;self.updated=now
                        self.recent.append((now,f['banks']))
                        while self.recent and now-self.recent[0][0]>2:self.recent.popleft()
                    log.write(json.dumps(f)+'\n')
                    previous=f
            raise RuntimeError('PCIe reader stopped: '+e.read().decode(errors='replace'))
        except Exception as ex:
            message='Input connection lost: '+(str(ex) or type(ex).__name__)+'; reconnecting'
            with self.lock:self.error=message
            with (self.evidence/'connections.jsonl').open('a') as log:
                log.write(json.dumps({'checked_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'error':message})+'\n')
        finally:c.close()
    def state(self):
        with self.lock:
            now=time.monotonic();recent=list(self.recent)
            delta=changes(self.baseline['banks'],self.frame['banks']) if self.baseline and self.frame else []
            age=now-self.updated if self.updated else 0
            return {'frame':self.frame,'baseline':self.baseline,'samples':self.samples,'age_seconds':age,'poll_interval_seconds':self.poll_interval,
                    'stale':bool(self.updated and age>2),'error':self.error or ('Input stream stopped; showing an old frame' if self.updated and age>2 else None),'changes':delta,
                    'recent_changes':sum(a[1]!=b[1] for a,b in zip(recent,recent[1:]) if now-b[0]<1),
                    'records':self.records}
    def action(self,d):
        with self.lock:
            if not self.frame or time.monotonic()-self.updated>2 or self.error:raise ValueError('No live complete frame')
            recent=[f for t,f in self.recent if time.monotonic()-t<1]
            unstable=any(v!=self.frame['banks'] for v in recent)
            if d['action']=='baseline':
                if unstable:raise ValueError('Inputs changed in the last second; wait for a stable baseline')
                self.baseline=dict(self.frame)
                (self.evidence/'baseline.json').write_text(json.dumps(self.baseline,indent=2)+'\n')
            elif d['action']=='record':
                if not self.baseline:raise ValueError('Set a reference baseline first')
                label=d.get('label','').strip()
                if not label:raise ValueError('Enter a control name and position')
                record={'label':label,'frame':self.frame,'baseline':self.baseline,
                        'changes':changes(self.baseline['banks'],self.frame['banks']),
                        'unstable':unstable,'confirmation':'candidate; repeat and return-to-baseline required'}
                self.records.append(record)
                (self.evidence/'mapping-candidates.json').write_text(json.dumps(self.records,indent=2)+'\n')
            else:raise ValueError('Unknown action')
        return {'ok':True}
def main():
    p=argparse.ArgumentParser();p.add_argument('--evidence',type=pathlib.Path,required=True)
    p.add_argument('--remote',required=True);p.add_argument('--port',type=int,default=8245)
    p.add_argument('--poll-interval',type=float,default=0.02);a=p.parse_args()
    if not 0<a.poll_interval<=1:p.error('poll interval must be greater than zero and at most one second')
    a.evidence.mkdir(parents=True,exist_ok=True);rec=Recorder(a.evidence,a.remote,a.poll_interval)
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def send(self,data,kind='application/json'):
            raw=data.encode();self.send_response(200);self.send_header('Content-Type',kind)
            self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(raw)))
            self.end_headers();self.wfile.write(raw)
        def do_GET(self):
            if self.path=='/state':self.send(json.dumps(rec.state()))
            elif self.path=='/':self.send(PAGE,'text/html; charset=utf-8')
            else:self.send_error(404)
        def do_POST(self):
            if self.path!='/action':self.send_error(404);return
            try:self.send(json.dumps(rec.action(json.loads(self.rfile.read(int(self.headers['Content-Length']))))))
            except Exception as e:self.send(json.dumps({'error':str(e)}))
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler)
    (a.evidence/'viewer-session.json').write_text(json.dumps({'pid':os.getpid(),'port':a.port,
      'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'reader_mode':'read-only BAR0',
      'sample_interval_seconds':a.poll_interval,'mapping_confirmation':'named physical actions; repeat only when readings are odd or inconsistent'},indent=2)+'\n')
    threading.Thread(target=rec.stream,daemon=True).start()
    print('Panel viewer listening on http://127.0.0.1:%d/'%a.port,flush=True);server.serve_forever()
if __name__=='__main__':main()
