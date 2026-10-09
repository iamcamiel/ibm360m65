import base64,datetime,hashlib,importlib.util,json,pathlib,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
old=ROOT/'gen/ise-panel-production-20261009'
here=ROOT/'gen/ise-panel-production-half-20261009'
remote_root='/home/ise/ibm360-panel-production-half-20261009'
assert not here.exists(),'Preserve existing trial; do not duplicate'
here.mkdir()
names=json.loads((old/'source-manifest.json').read_text())
for name in names:
    (here/name).write_bytes((old/name).read_bytes())
(here/'production_panel_core.vhd').write_bytes((ROOT/'tools/panel_test/production_panel_core.vhd').read_bytes())
top=(old/'production_panel_top.vhd').read_text()
assert top.count('diagnostic : entity work.PRODUCTION_PANEL_CORE')==1
top=top.replace('diagnostic : entity work.PRODUCTION_PANEL_CORE','diagnostic : entity work.PRODUCTION_PANEL_CORE generic map(SCAN_ENABLE_CYCLES=>1024)')
(here/'production_panel_top.vhd').write_text(top,newline='\n')
test=(old/'test_production_panel.vhd').read_text()
test=test.replace('generic map(PATTERN_CYCLES=>100000)','generic map(PATTERN_CYCLES=>200000,SCAN_ENABLE_CYCLES=>1024)')
test=test.replace('10240 ns','20480 ns').replace('5120 ns','10240 ns').replace('430080 ns','860160 ns')
(here/'test_production_panel.vhd').write_text(test,newline='\n')
(here/'test.tcl').write_text('run 18 ms\nquit\n',newline='\n')
build=(old/'run-build.sh').read_text().replace('/home/ise/ibm360-panel-production-20261009',remote_root)
(here/'run-build.sh').write_text(build,newline='\n')
readme=(old/'README.md').read_text()
readme+='\nHalf-rate experiment: wrapper divider 1024; serial clock 48.828125 kHz,\n10.24 us high/low/latch, 1162.5744 complete scans/s. Core stays 100 MHz.\nCheckerboard inversion stays one second. BLINKEN remains byte-for-byte unchanged.\nPreserves switch scanning and normal power controls, including their observed\nanomalies at the original rate; physical outcome is pending.\n'
(here/'README.md').write_text(readme,newline='\n')
manifest={name:hashlib.sha256((here/name).read_bytes()).hexdigest() for name in names}
(here/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name in ['remote.py','verify_result.py','deploy.py','audit_routed_inputs.py']:
    text=(old/name).read_text().replace('ibm360-panel-production-20261009','ibm360-panel-production-half-20261009').replace('fpga-panel-production-20261009','fpga-panel-production-half-20261009')
    if name=='verify_result.py':
        text=text.replace("assert 'divider=511' in (here/'production_panel_core.vhd').read_text()", "assert 'generic map(SCAN_ENABLE_CYCLES=>1024)' in (here/'production_panel_top.vhd').read_text()")
    text=text.replace('97656.25','48828.125')
    (here/name).write_text(text,newline='\n')
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
launch.update({'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'running','cpu_included':False,'pcie_included':False,'serial_clock_hz':48828.125,'refresh_hz':48828.125/42,'pattern_toggle_seconds':1,'production_driver_unmodified':True,'production_driver_sha256':manifest['blinken.vhd'],'configured':True,'normal_power_controls_retained':True,'scan_enable_cycles':1024,'previous_trial':'gen/ise-panel-production-20261009'})
(here/'trial.json').write_text(json.dumps(launch,indent=2)+'\n')
print(json.dumps(launch,indent=2))
