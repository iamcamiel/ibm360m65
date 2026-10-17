import base64,datetime,hashlib,importlib.util,json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
old=ROOT/'gen/ise-panel-checker-slow-20261009'
here=ROOT/'gen/ise-panel-checker-50khz-20261009'
remote_root='/home/ise/ibm360-panel-checker-50khz-20261009'
assert not here.exists(), 'Preserve existing trial; do not duplicate'
here.mkdir()
for name in ['panel_test_serial.vhd','panel_test_top.vhd','panel_test.ucf','test_panel_serial.vhd','test_panel_rate.vhd','README.md']:
 (here/name).write_text((ROOT/'tools/panel_test'/name).read_text(),newline='\n')
for name in ['core_clock100.vhd','sources.prj','test.prj','test.tcl','synth.xst']:
 (here/name).write_text((old/name).read_text(),newline='\n')
(here/'rate-test.prj').write_text('vhdl work panel_test_serial.vhd\nvhdl work test_panel_rate.vhd\n',newline='\n')
(here/'rate-test.tcl').write_text('run 2 ms\nquit\n',newline='\n')
build=(old/'run-build.sh').read_text().replace(str('/home/ise/ibm360-panel-checker-slow-20261009'),remote_root)
build=build.replace("printf 'synthesis\\n' > stage.txt", "fuse -prj rate-test.prj -o rate-test test_panel_rate > rate-test-compile.log 2>&1\n./rate-test -tclbatch rate-test.tcl > rate-test.log 2>&1\ngrep -q PANEL_RATE_PASS rate-test.log\nprintf 'synthesis\\n' > stage.txt")
(here/'run-build.sh').write_text(build,newline='\n')
inputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in here.iterdir() if p.is_file()}
(here/'source-manifest.json').write_text(json.dumps(inputs,indent=2)+'\n',newline='\n')
(here/'remote.py').write_text((old/'remote.py').read_text().replace('/home/ise/ibm360-panel-checker-slow-20261009',remote_root),newline='\n')
verify=(old/'verify_result.py').read_text().replace('/home/ise/ibm360-panel-checker-slow-20261009',remote_root)
verify=verify.replace("assert '1816 paths analyzed, 152 endpoints analyzed, 0 failing endpoints' in twr", "core=re.search(r'Timing constraint: NET \\\"clk100\\\" PERIOD = 10 ns HIGH 50%;.*?(\\d+) paths analyzed, (\\d+) endpoints analyzed, 0 failing endpoints.*?Minimum period is\\s+([0-9.]+)ns',twr,re.S)\nassert core\nassert 'PANEL_RATE_PASS' in (here/'rate-test.log').read_text()")
verify=verify.replace("'serial_clock_hz':1000,'refresh_hz':1000/42", "'serial_clock_hz':50000,'refresh_hz':50000/42")
verify=verify.replace("'core_minimum_period_ns':3.948,'core_setup_slack_ns':6.052,'core_hold_slack_ns':0.476,'constrained_paths':1944,'unconstrained_paths':116", "'core_minimum_period_ns':float(core.group(3)),'core_paths':int(core.group(1)),'core_endpoints':int(core.group(2))")
verify=verify.replace('explicit 10 ns core constraint covers 1816 paths and 152 endpoints','explicit 10 ns core constraint has verified zero failing endpoints')
(here/'verify_result.py').write_text(verify,newline='\n')
deploy=(old/'deploy.py').read_text().replace('fpga-panel-checker-slow-20261009','fpga-panel-checker-50khz-20261009')
deploy=deploy.replace('test -L /sys/bus/platform/devices/fd500000.pcie/driver\n', '')
deploy=deploy.replace('then echo fd500000.pcie > "$driver/bind"; fi;', 'then if echo fd500000.pcie > "$driver/bind"; then echo PCIE_HOST_REBOUND; else echo PCIE_HOST_UNBOUND_NO_ENDPOINT_EXPECTED; fi; fi;')
deploy=deploy.replace('echo fd500000.pcie > "$driver/unbind"', 'if test -L /sys/bus/platform/devices/fd500000.pcie/driver; then echo fd500000.pcie > "$driver/unbind"; fi')
deploy=deploy.replace('rebind\ntrap - EXIT', 'echo PROGRAMMER_EXIT_0\nrebind\ntrap - EXIT')
deploy=deploy.replace("'serial_clock_hz':1000,'panel_refresh_hz':1000.0/42", "'serial_clock_hz':50000,'panel_refresh_hz':50000.0/42")
(here/'deploy.py').write_text(deploy,newline='\n')
spec=importlib.util.spec_from_file_location('trial_remote',here/'remote.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
blobs={name:base64.b64encode((here/name).read_bytes()).decode() for name in list(inputs)+['source-manifest.json']}
code="""import os,json,base64,subprocess
root=%r
assert not os.path.exists(root),'Preserve existing remote trial'
os.mkdir(root)
blobs=json.loads(%r)
for name,data in blobs.items():
 open(root+'/'+name,'wb').write(base64.b64decode(data))
log=open(root+'/controller.log','wb')
p=subprocess.Popen(['bash','run-build.sh'],cwd=root,stdout=log,stderr=log,preexec_fn=os.setsid)
print(json.dumps({'launcher_pid':p.pid,'remote_root':root}))
"""%(remote_root,json.dumps(blobs))
launch=json.loads(mod.remote_python(code,True))
launch.update({'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'running','cpu_included':False,'pcie_included':False,'serial_clock_hz':50000,'refresh_hz':50000/42,'pattern_toggle_seconds':1,'previous_trial':'gen/ise-panel-checker-10khz-20261009'})
(here/'trial.json').write_text(json.dumps(launch,indent=2)+'\n')
print(json.dumps(launch,indent=2))
