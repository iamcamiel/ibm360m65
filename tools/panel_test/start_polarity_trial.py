import base64,datetime,hashlib,importlib.util,json,pathlib,sys,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
old=ROOT/'gen/ise-panel-production-forced-20261009'
here=ROOT/'gen/ise-panel-polarity-20261009'
remote_root='/home/ise/ibm360-panel-polarity-20261009'
if '--launch-prepared' not in sys.argv:
    assert not here.exists(),'Preserve existing trial; do not duplicate'
    here.mkdir()
    names=json.loads((old/'source-manifest.json').read_text())
    for name in names:(here/name).write_bytes((old/name).read_bytes())
    (here/'production_panel_core.vhd').write_bytes((ROOT/'tools/panel_test/polarity_panel_core.vhd').read_bytes())
    (here/'test_production_panel.vhd').write_bytes((ROOT/'tools/panel_test/test_polarity_panel.vhd').read_bytes())
    (here/'test.tcl').write_text('run 110 ms\nquit\n',newline='\n')
    build=(old/'run-build.sh').read_text().replace('/home/ise/ibm360-panel-production-forced-20261009',remote_root).replace('FORCED_PANEL_PASS','POLARITY_PANEL_PASS')
    (here/'run-build.sh').write_text(build,newline='\n')
    (here/'README.md').write_text('Button polarity diagnostic at 48.828125 kHz serial clock, core 100 MHz.\nSame private diagnostic BLINKEN as forced-checkerboard trial; LED power gating\nremains bypassed, while all eight switch scanners and the power latch remain.\n\nOutput banks 0,1,2: bits 0..23 mirror bank-0 scanned bits directly.\nOutput banks 3,4,5: bits 0..23 show their inverse.\nBits 24..35 repeat button-bank bits 8..19, including adjacent bits.\nGreen Power On pair (bank3 bits36/37) = NOT bank0 bit11.\nRed Power Off pair (bank3 bits38/39) = NOT bank0 bit12.\nBank0 bits36..39 heartbeat changes once per second; other auxiliary LEDs off.\nButtons cannot blank the LED display. Complete scanned frames are held for output.\nThese indices follow the existing panel ALD wiring; physical input alignment and\npolarity are exactly what this test investigates. No CPU/PCIe or flash write.\nFixture walks every bank-0 bit low, checks all 192 inputs and all 240 outputs,\nreleased and stuck-low inputs, exact serial timing, complementary indicators\nand reset. Production HDL and every earlier trial remain unchanged.\n',newline='\n')
    manifest={name:hashlib.sha256((here/name).read_bytes()).hexdigest() for name in names}
    (here/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name in ['remote.py','verify_result.py','deploy.py','audit_routed_inputs.py']:
        text=(old/name).read_text().replace('ibm360-panel-production-forced-20261009','ibm360-panel-polarity-20261009').replace('fpga-panel-production-forced-20261009','fpga-panel-polarity-20261009')
        text=text.replace('FORCED_PANEL_PASS','POLARITY_PANEL_PASS')
        text=text.replace("'pattern_toggle_seconds':1", "'heartbeat_toggle_seconds':1,'output_mode':'button-bank raw and inverse'")
        text=text.replace('Half-rate scan timing, all switch bits, checkerboard coherence while off and with stuck-low inputs, alternation and reset passed in GHDL and ISim','All 24 walking button bits, raw/inverse output mapping, Power On/Off lamps, all 192 input bits, stuck-low input, timing and reset passed in GHDL and ISim')
        text=text.replace('Checkerboard checks begin after two complete frames following reset.','Polarity checks begin after three complete scans per changed stimulus or reset.')
        text=text.replace('gen/ise-panel-production-half-20261009/physical-observation.json','gen/ise-panel-production-forced-20261009/physical-observation.json')
        (here/name).write_text(text,newline='\n')
    if '--prepare-only' in sys.argv:
        print('Prepared immutable polarity-test inputs.');sys.exit(0)
assert not (here/'trial.json').exists(),'Do not launch a duplicate'
manifest=json.loads((here/'source-manifest.json').read_text())
for name,digest in manifest.items():assert hashlib.sha256((here/name).read_bytes()).hexdigest()==digest
assert json.loads((here/'ghdl-result.json').read_text())['passed']
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
launch.update({'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'running','cpu_included':False,'pcie_included':False,'serial_clock_hz':48828.125,'refresh_hz':48828.125/42,'heartbeat_toggle_seconds':1,'output_mode':'button-bank raw and inverse','production_driver_unmodified':False,'diagnostic_force_leds':True,'power_latch_retained':True,'scanned_buttons_can_blank_leds':False,'diagnostic_driver_sha256':manifest['blinken.vhd'],'configured':True,'scan_enable_cycles':1024,'previous_trial':'gen/ise-panel-production-forced-20261009'})
(here/'trial.json').write_text(json.dumps(launch,indent=2)+'\n')
print(json.dumps(launch,indent=2))
