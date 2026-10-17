import base64,datetime,hashlib,importlib.util,json,pathlib,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
old=ROOT/'gen/ise-panel-checker-100khz-20261009'
here=ROOT/'gen/ise-panel-production-20261009'
remote_root='/home/ise/ibm360-panel-production-20261009'
assert not here.exists(),'Preserve existing trial; do not duplicate'
here.mkdir()
copies={'blinken.vhd':ROOT/'src/vhdl/blinken.vhd','core_clock100.vhd':ROOT/'src/vhdl/core_clock100.vhd',
 'production_panel_core.vhd':ROOT/'tools/panel_test/production_panel_core.vhd',
 'production_panel_top.vhd':ROOT/'tools/panel_test/production_panel_top.vhd',
 'test_production_panel.vhd':ROOT/'tools/panel_test/test_production_panel.vhd',
 'test_display_clock.vhd':ROOT/'tools/test_display_clock.vhd',
 'panel_test.ucf':ROOT/'tools/panel_test/production_panel.ucf',
 'README.md':ROOT/'tools/panel_test/production_panel_README.md'}
for name,p in copies.items():(here/name).write_bytes(p.read_bytes())
(here/'sources.prj').write_text('vhdl work core_clock100.vhd\nvhdl work blinken.vhd\nvhdl work production_panel_core.vhd\nvhdl work production_panel_top.vhd\n',newline='\n')
(here/'test.prj').write_text('vhdl work blinken.vhd\nvhdl work production_panel_core.vhd\nvhdl work test_production_panel.vhd\nvhdl work test_display_clock.vhd\n',newline='\n')
(here/'test.tcl').write_text('run 9 ms\nquit\n',newline='\n')
(here/'display-test.tcl').write_text('run 3 ms\nquit\n',newline='\n')
(here/'synth.xst').write_text((old/'synth.xst').read_text().replace('PANEL_TEST_TOP','PRODUCTION_PANEL_TOP'),newline='\n')
build=(old/'run-build.sh').read_text().replace('/home/ise/ibm360-panel-checker-100khz-20261009',remote_root).replace('test_panel_serial','test_production_panel').replace('PANEL_TEST_PASS','PRODUCTION_PANEL_PASS')
build=build.replace('fuse -prj rate-test.prj -o rate-test test_panel_rate > rate-test-compile.log 2>&1\n./rate-test -tclbatch rate-test.tcl > rate-test.log 2>&1\ngrep -q PANEL_RATE_PASS rate-test.log', 'fuse -prj test.prj -o display-test test_display_clock > display-test-compile.log 2>&1\n./display-test -tclbatch display-test.tcl > display-test.log 2>&1\ngrep -q DISPLAY_CLOCK_TEST_PASS display-test.log')
(here/'run-build.sh').write_text(build,newline='\n')
manifest={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in here.iterdir() if p.is_file()}
(here/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(here/'remote.py').write_text((old/'remote.py').read_text().replace('/home/ise/ibm360-panel-checker-100khz-20261009',remote_root),newline='\n')
deploy=(old/'deploy.py').read_text().replace('fpga-panel-checker-100khz-20261009','fpga-panel-production-20261009')
deploy=deploy.replace("'serial_clock_hz':100000,'panel_refresh_hz':100000.0/42", "'serial_clock_hz':97656.25,'panel_refresh_hz':97656.25/42")
(here/'deploy.py').write_text(deploy,newline='\n')
verify=(old/'verify_result.py').read_text().replace('/home/ise/ibm360-panel-checker-100khz-20261009',remote_root)
verify=verify.replace("assert 'generic map(HALF_CYCLES=>500)' in (here/'panel_test_top.vhd').read_text()", "assert 'divider=511' in (here/'production_panel_core.vhd').read_text()")
verify=verify.replace("(here/'panel_test_serial.vhd')", "(here/'production_panel_core.vhd')")
verify=verify.replace('PANEL_TEST_PASS','PRODUCTION_PANEL_PASS').replace("assert 'PANEL_RATE_PASS' in (here/'rate-test.log').read_text()", "assert 'DISPLAY_CLOCK_TEST_PASS' in (here/'display-test.log').read_text()")
verify=verify.replace("rows=list(csv.DictReader", "expected.update(dict(('disp_shift_i<%d>'%n,pin) for n,pin in enumerate(['AD32','Y34','Y32','W32','AH34','AE32','AG32','AH32'])))\nrows=list(csv.DictReader")
verify=verify.replace("if name.startswith('disp_'):","if name.startswith('disp_') and not name.startswith('disp_shift_i'):")
verify=verify.replace("'serial_clock_hz':100000,'refresh_hz':100000/42", "'serial_clock_hz':97656.25,'refresh_hz':97656.25/42")
verify=verify.replace('40 coherent frames, 40 clocks/frame, timing, alternation and mid-frame reset passed in GHDL and ISim','Production scan timing, all 192 switch bits, power controls, coherent checkerboards, alternation and reset passed in GHDL and ISim')
verify=verify.replace('Serial clock and latch pulse width change together; this experiment does not isolate their effects.','Configured is fixed to 1; the pre-configuration blink and PCIe snapshot mailbox are not exercised. Physical placement/load differs from the full CPU image.')
(here/'verify_result.py').write_text(verify,newline='\n')
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
