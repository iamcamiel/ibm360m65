"""Verify and load the CPU-free input mapper, then validate its PCIe identity."""
import datetime,hashlib,json,os,pathlib,shlex,sys,time
HERE=pathlib.Path(__file__).resolve().parents[2]/'gen/ise-panel-input-pcie-20261009'
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'gen/ise-ros-distribution-20261008/ssh-deps'))
import paramiko
result=json.loads((HERE/'result.json').read_text())
assert result['verified_build_and_timing'] and result['pins_verified'] and result['routed_input_and_cdc_verified']
BIT=HERE/'panel_input.bit';digest=hashlib.sha256(BIT.read_bytes()).hexdigest()
assert digest==result['bitstream_sha256']
remote='/home/ibm/ibm360/fpga-panel-input-pcie-20261009'
c=paramiko.SSHClient();c.load_host_keys(str(pathlib.Path.home()/'.ssh/known_hosts'));c.set_missing_host_key_policy(paramiko.RejectPolicy())
c.connect('172.22.80.36',username='ibm',password=os.environ['IBM360_PI_PASSWORD'],look_for_keys=False,allow_agent=False,timeout=10)
def run(label,cmd,sudo=False):
    i,o,e=c.exec_command(cmd,timeout=180)
    if sudo:i.write(os.environ['IBM360_PI_PASSWORD']+'\n');i.flush()
    out=o.read().decode(errors='replace');err=e.read().decode(errors='replace');status=o.channel.recv_exit_status()
    (HERE/(label+'.log')).write_text(out+err,encoding='utf-8')
    print(json.dumps({'step':label,'exit':status,'stdout':out,'stderr':err}),flush=True)
    assert status==0,label
    return out
try:
    run('deployment-preflight','test -z "$(pgrep -x hercules)" && test -z "$(pgrep -x xc3sprog)" && test -z "$(pgrep -f \'[p]anel_input_reader.py --watch\')" && mkdir -p '+remote)
    s=c.open_sftp()
    try:
        try:
            with s.open(remote+'/panel_input.bit','rb') as saved:assert hashlib.sha256(saved.read()).hexdigest()==digest,'Preserve different existing image'
        except FileNotFoundError:s.put(str(BIT),remote+'/panel_input.bit')
        s.put(str(ROOT/'tools/panel_test/panel_input_reader.py'),remote+'/panel_input_reader.py')
        script='''#!/bin/bash
set -euo pipefail
root=REMOTE_ROOT
test -z "$(pgrep -x hercules)"
test -z "$(pgrep -x xc3sprog)"
echo "BIT_HASH  $root/panel_input.bit" | sha256sum -c -
driver=/sys/bus/platform/drivers/brcm-pcie
rebind() { if ! test -L /sys/bus/platform/devices/fd500000.pcie/driver; then echo fd500000.pcie > "$driver/bind"; fi; }
trap rebind EXIT
if test -L /sys/bus/platform/devices/fd500000.pcie/driver; then echo fd500000.pcie > "$driver/unbind"; fi
xc3sprog -c xpc -p 4 "$root/panel_input.bit"
echo PROGRAMMER_EXIT_0
rebind
trap - EXIT
sleep 2
lspci -nn
test "$(cat /sys/bus/pci/devices/0000:01:00.0/vendor)" = 0x0360
test "$(cat /sys/bus/pci/devices/0000:01:00.0/device)" = 0x2065
echo 1 > /sys/bus/pci/devices/0000:01:00.0/enable
setpci -s 01:00.0 COMMAND=0402
python3 "$root/panel_input_reader.py"
echo INPUT_PCIE_TEST_PROGRAMMED
'''.replace('REMOTE_ROOT',remote).replace('BIT_HASH',digest)
        with s.open(remote+'/deploy.sh','w') as f:f.write(script)
    finally:s.close()
    out=run('deployment-program','sudo -S -p "" bash '+shlex.quote(remote+'/deploy.sh'),True)
    frame=json.loads(next(line for line in out.splitlines() if line.startswith('{"checked_at_utc"')))
    assert frame['frame_valid'] and frame['enable_divider']==1024
    assert frame['build_date']==result['build_date'] and frame['build_time']==result['build_time']
    (HERE/'deployed-identity.json').write_text(json.dumps(frame,indent=2)+'\n')
    d={'programmed_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'bitstream_sha256':digest,
       'programmer_exit':0,'programmed_volatile_ram':True,'configuration_flash_written':False,
       'cpu_included':False,'pcie_included':True,'hercules_started':False,'pcie_readback_verified':True,
       'diagnostic_signature':'PDI1','fpga_revision':'0.0','interface_revision':'1.4',
       'serial_clock_hz':48828.125,'raw_input_bits':192,'polarity_assumed':False,
       'physical_control_mapping_pending':True,'remote_root':remote}
    (HERE/'deployment-result.json').write_text(json.dumps(d,indent=2)+'\n')
    p=HERE/'trial.json';t=json.loads(p.read_text());t.update(status='programmed-pcie-readback-verified-mapping-pending',deployment_result='deployment-result.json');p.write_text(json.dumps(t,indent=2)+'\n')
finally:c.close()
