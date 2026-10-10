import base64,datetime,hashlib,importlib.util,json,pathlib,sys,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
old=ROOT/'gen/ise-panel-production-half-20261009'
here=ROOT/'gen/ise-panel-production-forced-20261009'
remote_root='/home/ise/ibm360-panel-production-forced-20261009'
if '--launch-prepared' not in sys.argv:
    assert not here.exists(),'Preserve existing trial; do not duplicate'
    here.mkdir()
    names=json.loads((old/'source-manifest.json').read_text())
    for name in names:(here/name).write_bytes((old/name).read_bytes())
    driver=(old/'blinken.vhd').read_text()
    driver=driver.replace('generic (BLINK_HALF_CYCLES : positive := 50000000);','generic (BLINK_HALF_CYCLES : positive := 50000000; DIAGNOSTIC_FORCE_LEDS : boolean := false);')
    driver=driver.replace("elsif (pwr = '0') then", "elsif (pwr = '0' and not DIAGNOSTIC_FORCE_LEDS) then")
    assert 'DIAGNOSTIC_FORCE_LEDS' in driver
    (here/'blinken.vhd').write_text(driver,newline='\n')
    core=(old/'production_panel_core.vhd').read_text().replace('panel : entity work.BLINKEN','panel : entity work.BLINKEN generic map(DIAGNOSTIC_FORCE_LEDS=>true)')
    (here/'production_panel_core.vhd').write_text(core,newline='\n')
    test=(old/'test_production_panel.vhd').read_text()
    test=test.replace('natural range 0 to 2:=0; -- ON, released, OFF','natural range 0 to 3:=0; -- ON, released, OFF, stuck low')
    test=test.replace("if m=0 then banks(0)(11):='0';elsif m=2 then banks(0)(12):='0';end if;", "if m=0 then banks(0)(11):='0';elsif m=2 then banks(0)(12):='0';elsif m=3 then banks(0):=(others=>'0');end if;")
    test=test.replace('variable pulses : natural:=0;', 'variable pulses : natural:=0;\n    variable frames_since_reset : natural:=0;')
    test=test.replace("if reset='1' then pulses:=0;", "if reset='1' then frames_since_reset:=0;pulses:=0;")
    test=test.replace("if frames>=2 and first_power='0' and not power_changed then",'if frames_since_reset>=2 then')
    test=test.replace('frames<=frames+1;pulses:=0;', 'frames_since_reset:=frames_since_reset+1;frames<=frames+1;pulses:=0;')
    test=test.replace("mode<=0;wait until rising_edge(sck);", "mode<=3;wait until frames=17;wait for 1 ns;\n    expected:=inputs(3);\n    for n in 0 to 23 loop assert sw(191-n)=expected(0)(n) report \"stuck-low inputs scanned\" severity failure;end loop;\n    mode<=0;wait until rising_edge(sck);")
    test=test.replace('reset<=\'0\';wait until frames=17;', 'reset<=\'0\';wait until frames=21;')
    test=test.replace('PRODUCTION_PANEL_PASS exact scan timing, all 192 switch bits, power controls, coherent checkerboards, alternation and reset','FORCED_PANEL_PASS exact scan timing, all 192 switch bits, coherent checkerboards despite Power Off and stuck-low inputs, alternation and reset')
    (here/'test_production_panel.vhd').write_text(test,newline='\n')
    (here/'test.tcl').write_text('run 24 ms\nquit\n',newline='\n')
    build=(old/'run-build.sh').read_text().replace('/home/ise/ibm360-panel-production-half-20261009',remote_root).replace('grep -q PRODUCTION_PANEL_PASS','grep -q FORCED_PANEL_PASS')
    (here/'run-build.sh').write_text(build,newline='\n')
    (here/'README.md').write_text('Forced-LED diagnostic at 48.828125 kHz SCK. All eight switch scanners,\npower latch and input synchronizers remain active and observable.\nOnly the off-state LED mux is bypassed by DIAGNOSTIC_FORCE_LEDS=true.\nConfigured remains 1. Power buttons cannot blank the checkerboard.\nThe board power LED still reflects the real scanned-button power latch.\nPattern inversion remains one second; core remains 100 MHz.\nThe diagnostic BLINKEN copy has two deliberate edits; production HDL is untouched.\nFixture checks all output bits while off, during button transitions and with\nbank-0 serial input stuck low, plus exact clock/latch timing and all switch bits.\nNo CPU, PCIe, flash write or whole-emulator proof. Preserve earlier images.\n',newline='\n')
    manifest={name:hashlib.sha256((here/name).read_bytes()).hexdigest() for name in names}
    (here/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name in ['remote.py','verify_result.py','deploy.py','audit_routed_inputs.py']:
        text=(old/name).read_text().replace('ibm360-panel-production-half-20261009','ibm360-panel-production-forced-20261009').replace('fpga-panel-production-half-20261009','fpga-panel-production-forced-20261009')
        if name=='verify_result.py':
            text=text.replace("assert driver_hash=='6fefb15d654f5a3e53ef09d9f44a2a3dc5a310a20df6b55c6a54049ff225c6a3'", "assert driver_hash==json.loads((here/'source-manifest.json').read_text())['blinken.vhd']\nassert 'generic map(DIAGNOSTIC_FORCE_LEDS=>true)' in (here/'production_panel_core.vhd').read_text()\noriginal=(here/'blinken.vhd').read_text().replace('generic (BLINK_HALF_CYCLES : positive := 50000000; DIAGNOSTIC_FORCE_LEDS : boolean := false);','generic (BLINK_HALF_CYCLES : positive := 50000000);').replace(\"elsif (pwr = '0' and not DIAGNOSTIC_FORCE_LEDS) then\",\"elsif (pwr = '0') then\")\nassert original==(here.parent/'ise-panel-production-half-20261009/blinken.vhd').read_text()")
            text=text.replace("'PRODUCTION_PANEL_PASS'", "'FORCED_PANEL_PASS'")
            text=text.replace("'production_driver_unmodified':True", "'production_driver_unmodified':False,'diagnostic_force_leds':True,'driver_change_scope':'Two edits: optional diagnostic generic and bypass of power-off LED mux only'")
            text=text.replace("'normal_power_controls_retained':True", "'power_latch_retained':True,'scanned_buttons_can_blank_leds':False")
            text=text.replace('Production scan timing, all 192 switch bits, power controls, coherent checkerboards, alternation and reset passed in GHDL and ISim','Half-rate scan timing, all switch bits, checkerboard coherence while off and with stuck-low inputs, alternation and reset passed in GHDL and ISim')
            text=text.replace("'production_driver_sha256':driver_hash", "'diagnostic_driver_sha256':driver_hash,'production_reference_driver_sha256':'6fefb15d654f5a3e53ef09d9f44a2a3dc5a310a20df6b55c6a54049ff225c6a3'")
        (here/name).write_text(text,newline='\n')
    if '--prepare-only' in sys.argv:
        print('Prepared immutable forced-LED diagnostic inputs.');sys.exit(0)
assert not (here/'trial.json').exists(),'Do not launch a duplicate'
manifest=json.loads((here/'source-manifest.json').read_text())
for name,digest in manifest.items():assert hashlib.sha256((here/name).read_bytes()).hexdigest()==digest
spec=importlib.util.spec_from_file_location('trial_remote',here/'remote.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
blobs={name:base64.b64encode((here/name).read_bytes()).decode() for name in list(manifest)+['source-manifest.json']}
code="""import os,json,base64,subprocess,zlib
root=%r
assert not os.path.exists(root),'Preserve existing remote trial'
os.mkdir(root)
blobs=json.loads(zlib.decompress(base64.b64decode(%r)))
for name,data in blobs.items():open(root+'/'+name,'wb').write(base64.b64decode(data))
log=open(root+'/controller.log','wb')
p=subprocess.Popen(['bash','run-build.sh'],cwd=root,stdout=log,stderr=log,preexec_fn=os.setsid)
print(json.dumps({'launcher_pid':p.pid,'remote_root':root}))
"""%(remote_root,base64.b64encode(zlib.compress(json.dumps(blobs).encode())).decode())
launch=json.loads(mod.remote_python(code,True))
launch.update({'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'running','cpu_included':False,'pcie_included':False,'serial_clock_hz':48828.125,'refresh_hz':48828.125/42,'pattern_toggle_seconds':1,'production_driver_unmodified':False,'diagnostic_force_leds':True,'power_latch_retained':True,'scanned_buttons_can_blank_leds':False,'diagnostic_driver_sha256':manifest['blinken.vhd'],'configured':True,'scan_enable_cycles':1024,'previous_trial':'gen/ise-panel-production-half-20261009'})
(here/'trial.json').write_text(json.dumps(launch,indent=2)+'\n')
print(json.dumps(launch,indent=2))
