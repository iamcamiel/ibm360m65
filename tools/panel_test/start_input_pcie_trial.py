"""Prepare, then launch one immutable CPU-free PCIe panel-input diagnostic."""
import base64,datetime,hashlib,importlib.util,json,pathlib,sys,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=ROOT/'gen/ise-panel-input-pcie-20261009'
REMOTE='/home/ise/ibm360-panel-input-pcie-20261009'
BASE='/home/ise/ibm360-panel-r14-retry1-20261009'
def save(name,data):
    p=HERE/name;p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes(data if isinstance(data,bytes) else data.encode())
if '--launch-prepared' not in sys.argv:
    assert not HERE.exists(),'Preserve the existing trial'
    HERE.mkdir()
    for name in ['cdc_mailbox.vhd','core_clock100.vhd']:
        save('src/vhdl/'+name,(ROOT/'src/vhdl'/name).read_bytes())
    for p in (ROOT/'src/vhdl/pcie').glob('*.vhd'):save('src/vhdl/pcie/'+p.name,p.read_bytes())
    text=(ROOT/'src/vhdl/pcie/EP_MEM.vhd').read_text()
    save('src/vhdl/pcie/EP_MEM.vhd',text.split('architecture rtl of EP_MEM is')[0]+(ROOT/'tools/panel_test/input_pcie_mem_arch.vhd').read_text())
    text=(ROOT/'src/vhdl/ibm360.vhd').read_text()
    save('src/vhdl/ibm360.vhd',text.split('architecture Behavioral of IBM360 is')[0]+(ROOT/'tools/panel_test/input_pcie_top_arch.vhd').read_text())
    save('src/vhdl/input_pcie_core.vhd',(ROOT/'tools/panel_test/input_pcie_core.vhd').read_bytes())
    save('src/vhdl/blinken.vhd',(ROOT/'gen/ise-panel-polarity-20261009/blinken.vhd').read_bytes())
    stamp=datetime.datetime.now(datetime.timezone.utc)
    save('src/vhdl/pcie/fpga_build.vhd','''library ieee;
use ieee.std_logic_1164.all;
package fpga_build is
 constant M65_BUILD_MAGIC : std_logic_vector(31 downto 0):=x"4D363542";
 constant M65_BUILD_TIME : std_logic_vector(31 downto 0):=x"%s";
 constant M65_BUILD_DATE : std_logic_vector(31 downto 0):=x"%s";
 constant M65_FPGA_VERSION : std_logic_vector(31 downto 0):=x"00000000";
 constant M65_INTERFACE_VERSION : std_logic_vector(31 downto 0):=x"00010004";
end;
'''%(stamp.strftime('00%H%M%S'),stamp.strftime('%Y%m%d')))
    # Only the observation mailbox survives. Do not constrain removed CPU paths.
    ucf=(ROOT/'src/ucf/ibm360.ucf').read_text()
    ucf='\n'.join(line for line in ucf.splitlines() if not any(s in line for s in
      ['cpu_to_pcie/','pcie_to_cpu/','TS_CDC_CMD_DATA','TS_CDC_RESP_DATA']))+'\n'
    ucf=ucf.replace('INST "blinken/switch_meta*"','INST "panel/blinken/switch_meta*"')
    for line in (ROOT/'tools/panel_test/production_panel.ucf').read_text().splitlines():
        if 'IOSTANDARD' in line and 'disp_' in line:ucf+=line+'\n'
    save('panel-input.ucf',ucf)
    save('test_input_pcie.vhd',(ROOT/'tools/panel_test/test_input_pcie.vhd').read_bytes())
    save('test_input_pcie_rate.vhd',(ROOT/'tools/panel_test/test_input_pcie_rate.vhd').read_bytes())
    prj=''.join('vhdl work "%s"\n'%p for p in ['src/vhdl/pcie/fpga_build.vhd','src/vhdl/cdc_mailbox.vhd','src/vhdl/pcie/EP_MEM.vhd','src/vhdl/blinken.vhd','src/vhdl/input_pcie_core.vhd','test_input_pcie.vhd'])
    save('test.prj',prj+'vhdl work "test_input_pcie_rate.vhd"\n');save('test.tcl','run 15 ms\nquit\n')
    save('rate.tcl','run 5 ms\nquit\n')
    save('synth.xst','run\n-ifn sources.prj\n-ifmt mixed\n-ofn panel_input\n-ofmt NGC\n-p xc5vlx110t-ff1136-1\n-top IBM360\n-opt_mode Speed\n-opt_level 2\n-keep_hierarchy Yes\n')
    build='''#!/bin/bash
set -eo pipefail
cd REMOTE_ROOT
finish() { status=$?; printf '%s\\n' "$status" > build.exit; date -u +%Y-%m-%dT%H:%M:%SZ > build.end; }
trap finish EXIT
source /opt/Xilinx/14.7/ISE_DS/settings64.sh
date -u +%Y-%m-%dT%H:%M:%SZ > build.start
printf 'input-and-pcie-simulation\\n' > stage.txt
fuse -prj test.prj -o input-test test_input_pcie > test-compile.log 2>&1
./input-test -tclbatch test.tcl > test.log 2>&1
grep -q INPUT_PCIE_PASS test.log
fuse -prj test.prj -o rate-test test_input_pcie_rate > rate-compile.log 2>&1
./rate-test -tclbatch rate.tcl > rate.log 2>&1
grep -q INPUT_PCIE_RATE_PASS rate.log
printf 'synthesis\\n' > stage.txt
xst -ifn synth.xst > synthesis.log 2>&1
printf 'translate\\n' > stage.txt
ngdbuild -p xc5vlx110t-ff1136-1 -uc panel-input.ucf panel_input.ngc panel_input.ngd > translate.log 2>&1
printf 'map\\n' > stage.txt
map -w -p xc5vlx110t-ff1136-1 -o panel_input_map.ncd panel_input.ngd panel_input.pcf > map.log 2>&1
printf 'place-and-route\\n' > stage.txt
par -w panel_input_map.ncd panel_input.ncd panel_input.pcf > par.log 2>&1
printf 'timing-audit\\n' > stage.txt
test ! -e timing.start
date -u +%Y-%m-%dT%H:%M:%SZ > timing.start
trce -intstyle silent -v 1 -n 40000 -u 20 -tsi panel_input.tsi -o panel_input.twr panel_input.ncd panel_input.pcf > timing.log 2>&1
printf '0\\n' > timing.exit
date -u +%Y-%m-%dT%H:%M:%SZ > timing.complete
printf 'bitstream-generation\\n' > stage.txt
bitgen -w -g UserID:0x50444931 panel_input.ncd panel_input.bit panel_input.pcf > bitgen.log 2>&1
test -s panel_input.bit
sha256sum panel_input.bit > bitstream.sha256
printf 'routed-metadata\\n' > stage.txt
test ! -e routed-xdl.start
date -u +%Y-%m-%dT%H:%M:%SZ > routed-xdl.start
xdl -ncd2xdl panel_input.ncd panel_input.xdl > routed-xdl.log 2>&1
printf '0\\n' > routed-xdl.exit
date -u +%Y-%m-%dT%H:%M:%SZ > routed-xdl.complete
printf 'build-complete-verification-pending\\n' > stage.txt
'''.replace('REMOTE_ROOT',REMOTE)
    save('run-build.sh',build)
    save('remote.py',(ROOT/'gen/ise-panel-polarity-20261009/remote.py').read_text().replace('/home/ise/ibm360-panel-polarity-20261009',REMOTE))
    save('README.md','CPU-free PCIe input mapper. PDI1 diagnostic identity BAR0 0x3FC/0x7E8. FPGA revision zero prevents CPU compatibility. PNL1 at 0x400 captures a coherent held frame; SW0..SW7 at 0x410..0x42C preserve 24 raw bits each. Every BAR write ignored. No polarity assumption in host viewer. Fixed configured=1, forced LEDs and frame-coherent checkerboard, 48.828125 kHz SCK, 100 MHz core. Snapshot divider corrected to 1024. Same diagnostic BLINKEN scanner as polarity image. All CPU HDL omitted. Source and required PCIe IP inputs frozen before synthesis. Volatile programming only; all earlier images preserved.\n')
    manifest={str(p.relative_to(HERE)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in HERE.rglob('*') if p.is_file()}
    save('source-manifest.json',json.dumps(manifest,indent=2)+'\n')
    print('Prepared local diagnostic inputs.');sys.exit(0)
assert not (HERE/'trial.json').exists(),'Do not launch a duplicate'
assert json.loads((HERE/'ghdl-result.json').read_text())['passed']
manifest=json.loads((HERE/'source-manifest.json').read_text())
for name,digest in manifest.items():assert hashlib.sha256((HERE/name).read_bytes()).hexdigest()==digest
spec=importlib.util.spec_from_file_location('trial_remote',HERE/'remote.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
blobs={name:base64.b64encode((HERE/name).read_bytes()).decode() for name in list(manifest)+['source-manifest.json','ghdl-result.json']}
packed=base64.b64encode(zlib.compress(json.dumps(blobs).encode())).decode()
# Keep every Windows helper invocation below its command-line size limit.
mod.remote_python("import os;root=%r;assert not os.path.exists(root),'Preserve existing remote trial';os.mkdir(root)"%REMOTE,True)
for offset in range(0,len(packed),8000):
    mod.remote_python("open(%r,'ab').write(%r)"%(REMOTE+'/upload.b64',packed[offset:offset+8000]),True)
code='''import os,json,base64,subprocess,zlib,re,hashlib,shutil
root=%r
base=%r
assert os.path.exists(root) and not os.path.exists(root+'/controller.log'),'Do not duplicate build'
blobs=json.loads(zlib.decompress(base64.b64decode(open(root+'/upload.b64','rb').read())))
for name,data in blobs.items():
 p=root+'/'+name
 if not os.path.isdir(os.path.dirname(p)):os.makedirs(os.path.dirname(p))
 open(p,'wb').write(base64.b64decode(data))
lines=[]
for line in open(base+'/xise/IBM360.prj'):
 path=re.search('"(.*)"',line).group(1)
 if path.startswith('ipcore_dir/'):
  dest=root+'/'+path
  if not os.path.isdir(os.path.dirname(dest)):os.makedirs(os.path.dirname(dest))
  shutil.copyfile(base+'/xise/'+path,dest)
  lines.append(line)
 elif path.startswith('../src/vhdl/pcie/') or path in ['../src/vhdl/cdc_mailbox.vhd','../src/vhdl/core_clock100.vhd','../src/vhdl/blinken.vhd','../src/vhdl/ibm360.vhd']:
  if path.endswith('/ibm360.vhd'):lines.append('vhdl work "src/vhdl/input_pcie_core.vhd"\\n')
  lines.append(line.replace('../src/','src/'))
open(root+'/sources.prj','w').writelines(lines)
inputs={}
for directory,dirs,files in os.walk(root):
 for name in files:
  p=directory+'/'+name
  inputs[os.path.relpath(p,root)]=hashlib.sha256(open(p,'rb').read()).hexdigest()
open(root+'/input-manifest.json','w').write(json.dumps(inputs,indent=2)+'\\n')
log=open(root+'/controller.log','wb')
p=subprocess.Popen(['bash','run-build.sh'],cwd=root,stdout=log,stderr=log,preexec_fn=os.setsid)
print(json.dumps({'launcher_pid':p.pid,'remote_root':root,'input_count':len(inputs)}))
'''%(REMOTE,BASE)
launch=json.loads(mod.remote_python(code,True))
launch.update(created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),status='running',cpu_included=False,pcie_included=True,serial_clock_hz=48828.125,raw_input_bits=192,polarity_assumed=False,diagnostic_signature='PDI1',fpga_revision='0.0',interface_revision='1.4')
save('trial.json',json.dumps(launch,indent=2)+'\n');print(json.dumps(launch,indent=2))
