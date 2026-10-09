"""Enable and identify the already loaded diagnostic; never invoke a programmer."""
import datetime,hashlib,json,os,pathlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
H=ROOT/'gen/ise-panel-input-pcie-20261009'
sys.path.insert(0,str(ROOT/'gen/ise-ros-distribution-20261008/ssh-deps'))
import paramiko
expected=json.loads((H/'result.json').read_text())
remote='/home/ibm/ibm360/fpga-panel-input-pcie-20261009'
c=paramiko.SSHClient();c.load_host_keys(str(pathlib.Path.home()/'.ssh/known_hosts'))
c.set_missing_host_key_policy(paramiko.RejectPolicy())
c.connect('172.22.80.36',username='ibm',password=os.environ['IBM360_PI_PASSWORD'],look_for_keys=False,allow_agent=False,timeout=10)
try:
    i,o,e=c.exec_command('test -z "$(pgrep -x hercules)" && test -z "$(pgrep -x xc3sprog)" && test -z "$(pgrep -f \'[p]anel_input_reader.py --watch\')"',timeout=10)
    assert o.channel.recv_exit_status()==0,'Competing hardware process'
    s=c.open_sftp()
    try:
        s.put(str(ROOT/'tools/panel_test/panel_input_reader.py'),remote+'/panel_input_reader.py')
        s.put(str(H/'panel_input.bit'),remote+'/panel_input.verified.bit')
        script='''#!/bin/bash
set -euo pipefail
test -z "$(pgrep -x hercules)"
test -z "$(pgrep -x xc3sprog)"
echo "DIGEST  REMOTE/panel_input.verified.bit" | sha256sum -c -
test "$(cat /sys/bus/pci/devices/0000:01:00.0/vendor)" = 0x0360
test "$(cat /sys/bus/pci/devices/0000:01:00.0/device)" = 0x2065
if test "$(cat /sys/bus/pci/devices/0000:01:00.0/enable)" = 0; then
 echo 1 > /sys/bus/pci/devices/0000:01:00.0/enable
fi
setpci -s 01:00.0 COMMAND=0402
python3 REMOTE/panel_input_reader.py
'''.replace('REMOTE',remote).replace('DIGEST',expected['bitstream_sha256'])
        with s.open(remote+'/recover-readback.sh','w') as f:f.write(script)
    finally:s.close()
    i,o,e=c.exec_command('sudo -S -p "" bash '+remote+'/recover-readback.sh',timeout=20)
    i.write(os.environ['IBM360_PI_PASSWORD']+'\n');i.flush()
    out=o.read().decode(errors='replace');err=e.read().decode(errors='replace');status=o.channel.recv_exit_status()
    (H/'deployment-recovered-readback.log').write_text(out+err,encoding='utf8')
    print(out+err,flush=True);assert status==0,'Recovery readback failed'
    frame=json.loads(next(line for line in out.splitlines() if line.startswith('{"checked_at_utc"')))
    assert frame['frame_valid'] and frame['enable_divider']==1024
    assert frame['build_date']==expected['build_date'] and frame['build_time']==expected['build_time']
    (H/'deployed-identity.json').write_text(json.dumps(frame,indent=2)+'\n')
    d={'verified_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
       'bitstream_sha256':expected['bitstream_sha256'],'programmer_exit':None,
       'programmer_exit_limitation':'Initial deployment connection was interrupted by a Pi restart; programmer exit was not captured. No duplicate programming was performed. Loaded PDI1 identity, unique build stamp and valid input frames are verified.',
       'programmed_volatile_ram':True,'configuration_flash_written':False,
       'cpu_included':False,'pcie_included':True,'hercules_started':False,
       'pcie_readback_verified':True,'diagnostic_signature':'PDI1','fpga_revision':'0.0',
       'interface_revision':'1.4','serial_clock_hz':48828.125,'raw_input_bits':192,
       'polarity_assumed':False,'physical_control_mapping_pending':True,'remote_root':remote,
       'remote_archive_bitstream_hash_verified':True,'readback':'deployed-identity.json',
       'deployment_interruption':'deployment-interrupted.json'}
    (H/'deployment-result.json').write_text(json.dumps(d,indent=2)+'\n')
    p=H/'trial.json';t=json.loads(p.read_text());t.update(status='programmed-pcie-readback-verified-mapping-pending',deployment_result='deployment-result.json');p.write_text(json.dumps(t,indent=2)+'\n')
finally:c.close()
