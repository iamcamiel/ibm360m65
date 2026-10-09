import base64,datetime,hashlib,importlib.util,json,pathlib,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
here=ROOT/'gen/ise-panel-production-20261009'
assert not (here/'trial.json').exists(), 'Do not launch a duplicate'
manifest=json.loads((here/'source-manifest.json').read_text())
for name,digest in manifest.items():assert hashlib.sha256((here/name).read_bytes()).hexdigest()==digest
remote_root='/home/ise/ibm360-panel-production-20261009'
spec=importlib.util.spec_from_file_location('trial_remote',here/'remote.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
blobs={name:base64.b64encode((here/name).read_bytes()).decode() for name in list(manifest)+['source-manifest.json']}
code="""import os,json,base64,subprocess,zlib
root=%r
assert not os.path.exists(root),'Preserve existing remote trial'
os.mkdir(root)
blobs=json.loads(zlib.decompress(base64.b64decode(%r)))
for name,data in blobs.items():
 open(root+'/'+name,'wb').write(base64.b64decode(data))
log=open(root+'/controller.log','wb')
p=subprocess.Popen(['bash','run-build.sh'],cwd=root,stdout=log,stderr=log,preexec_fn=os.setsid)
print(json.dumps({'launcher_pid':p.pid,'remote_root':root}))
"""%(remote_root,base64.b64encode(zlib.compress(json.dumps(blobs).encode())).decode())
launch=json.loads(mod.remote_python(code,True))
launch.update({'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'running','cpu_included':False,'pcie_included':False,'serial_clock_hz':97656.25,'refresh_hz':97656.25/42,'pattern_toggle_seconds':1,'production_driver_unmodified':True,'production_driver_sha256':manifest['blinken.vhd'],'configured':True,'power_on_required':True,'previous_trial':'gen/ise-panel-checker-100khz-20261009'})
(here/'trial.json').write_text(json.dumps(launch,indent=2)+'\n')
print(json.dumps(launch,indent=2))
