"""Audit the single routed input-mapper XDL, timing report, pins and frozen inputs."""
import csv,datetime,hashlib,importlib.util,io,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[2]
H=ROOT/'gen/ise-panel-input-pcie-20261009'
s=importlib.util.spec_from_file_location('remote',H/'remote.py');remote=importlib.util.module_from_spec(s);s.loader.exec_module(remote)
code="""import os,json,hashlib
root='/home/ise/ibm360-panel-input-pcie-20261009'
diff={}
for manifest in ['source-manifest.json','input-manifest.json']:
 d=json.load(open(root+'/'+manifest));diff[manifest]=[]
 for name,want in d.items():
  if hashlib.sha256(open(root+'/'+name,'rb').read()).hexdigest()!=want:diff[manifest].append(name)
p=root+'/panel_input.bit'
print(json.dumps({'manifest_differences':diff,'bitstream_sha256':hashlib.sha256(open(p,'rb').read()).hexdigest(),'bitstream_bytes':os.stat(p).st_size,'bitstream_mtime_epoch':os.stat(p).st_mtime,'build_start_mtime_epoch':os.stat(root+'/build.start').st_mtime,'build_exit':open(root+'/build.exit').read().strip(),'timing_exit':open(root+'/timing.exit').read().strip(),'xdl_exit':open(root+'/routed-xdl.exit').read().strip()}))
"""
e=json.loads(remote.remote_python(code,True))
assert all(not v for v in e['manifest_differences'].values())
assert e['build_exit']==e['timing_exit']==e['xdl_exit']=='0'
assert e['bitstream_bytes']>0 and e['bitstream_mtime_epoch']>e['build_start_mtime_epoch']
assert hashlib.sha256((H/'panel_input.bit').read_bytes()).hexdigest()==e['bitstream_sha256']
assert 'Bitstream generation is complete.' in (H/'panel_input.bgn').read_text()
for n in ['synthesis','translate','map','par','timing','bitgen','controller','test-compile','rate-compile']:
 assert not re.search(r'^\s*ERROR[: ]',(H/(n+'.log')).read_text(errors='replace'),re.M),n
assert 'INPUT_PCIE_PASS' in (H/'test.log').read_text()
assert 'INPUT_PCIE_RATE_PASS' in (H/'rate.log').read_text()
assert json.loads((H/'ghdl-result.json').read_text())['passed']
x=(H/'panel_input.xdl').read_text();p=(H/'panel_input.pcf').read_text()
t=(H/'panel_input.twr').read_text();tsi=(H/'panel_input.tsi').read_text()
assert 'Timing errors: 0  Score: 0  (Setup/Max: 0, Hold: 0)' in t
core=re.search(r'Timing constraint: NET "clk" PERIOD = 10 ns HIGH 50%;.*?(\d+) paths analyzed, (\d+) endpoints analyzed, 0 failing endpoints.*?Minimum period is\s+([0-9.]+)ns',t,re.S)
assert core,'No verified core period'
cdc=re.search(r'Timing constraint: TS_CDC_PANEL_DATA.*?8 ns DATAPATHONLY.*?(\d+) paths analyzed, (\d+) endpoints analyzed, 0 failing endpoints.*?Maximum delay is\s+([0-9.]+)ns',t,re.S)
assert cdc,'No verified 8 ns panel payload constraint'
inst={re.match(r'inst "([^"]+)"',b)[1]:b for b in re.findall(r'(?ms)^inst .*?;',x)}
nets={re.match(r'net "([^"]+)"',b)[1]:b for b in re.findall(r'(?ms)^net .*?;',x)}
ff={}
for name,b in inst.items():
 for bel,logical in re.findall(r'([ABCD]FF):([^\s:]+):#FF',b):
  ff[logical]={'instance':name,'bel':bel,'async_reg':('_BEL_PROP::'+bel+':ASYNC_REG:') in b,'asynchronous_sr':'SYNC_ATTR::ASYNC' in b,'cfg':b}
groups={k:set(re.findall(r'BEL\s+"([^"]+)"',v)) for k,v in re.findall(r'TIMEGRP\s+(\w+)\s*=\s*(.*?);',p,re.S)}
base='pcie/app/PIO_interface/PIO_EP_ins/EP_MEM/EP_MEM_inst/'
box=base+'panel_to_pcie/'
coverage={}
for role,suffix in [('HOLD','source_hold'),('CAPTURE','destination_data')]:
 physical={n for n in ff if n.startswith(box+suffix+'_')}
 assert physical and physical==groups['CDC_PANEL_'+role],role
 assert {box+suffix+'_'+str((3+b)*32+n) for b in range(8) for n in range(24)}<=physical,'Missing raw input payload bits'
 coverage[role]={'surviving_registers':sorted(physical),'count':len(physical),'all_192_raw_input_bits_retained':True,'exact_pcf_group_match':True}
expected_scanned={'panel/blinken/scanned_switches<%d>_%d'%(b,n) for b in range(8) for n in range(24)}
assert expected_scanned<=ff.keys(),'Missing input scanner registers'
first={'panel/blinken/switch_meta_'+str(n) for n in range(8)}
second={'panel/blinken/switch_sync_'+str(n) for n in range(8)}
assert first|second<=ff.keys() and all(ff[n]['async_reg'] for n in first|second)
assert groups['PANEL_INPUT_FIRST']==first
assert groups['CDC_REQUEST_FIRST']=={box+'request_meta'}
assert groups['CDC_ACK_FIRST']=={box+'ack_meta'}
excluded=set().union(*(v for k,v in groups.items() if k in ['PANEL_INPUT_FIRST','PCIE_STATUS_FIRST','CDC_REQUEST_FIRST','CDC_ACK_FIRST']))
inputs={};outputs={}
for net,b in nets.items():
 for i,pin in re.findall(r'inpin "([^"]+)" (\w+)',b):inputs[i,pin]=net
 for i,pin in re.findall(r'outpin "([^"]+)" (\w+)',b):outputs[net]=(i,pin)
physical={(v['instance'],v['bel'][0]+'Q'):n for n,v in ff.items()}
def info(n):
 f=ff[n];i=f['instance'];letter=f['bel'][0];b=f['cfg']
 props=dict(re.findall(r'(\w+)::([^\s"]+)',b));mux=props[letter+'FFMUX']
 d={k:v for k,v in f.items() if k!='cfg'};d.update(clock_net=inputs.get((i,'CLK')),sr_net=inputs.get((i,'SR')))
 if mux in ['O6','O5']:
  m=re.search(letter+mux[1]+'LUT:[^\\s]+:#LUT:'+mux+r'=([^\s"]+)',b);assert m,(n,mux)
  ns={v:inputs.get((i,letter+v[1:])) for v in sorted(set(re.findall(r'A[1-6]',m[1])))}
  d.update(data_lut=m[1],data_input_nets=ns,data_ff_drivers=sorted(set(physical.get(outputs.get(net)) for net in ns.values())- {None}))
 elif mux==letter+'X':d.update(data_net=inputs.get((i,mux)),data_ff_drivers=[physical.get(outputs.get(inputs.get((i,mux))))])
 else:raise AssertionError((n,mux))
 return d
pairs={}
for meta,sync in [(box+'request_meta',box+'request_sync'),(box+'ack_meta',box+'ack_sync')]+[('panel/blinken/switch_meta_'+str(n),'panel/blinken/switch_sync_'+str(n)) for n in range(8)]:
 a,b=info(meta),info(sync)
 assert meta in b['data_ff_drivers'] and a['clock_net']==b['clock_net'],(meta,sync)
 assert a['async_reg'] and b['async_reg'] and sync not in excluded
 pairs[sync]={'first':a,'second':b}
reset={n:info(n) for n in ff if any(stem in n for stem in ['ready_sync','pio_reset_sync','source_reset','destination_reset','user_reset'])}
reset_pairs=[]
for n,v in reset.items():
 if n.endswith('_1'):
  assert any(d in reset and d.endswith('_0') for d in v['data_ff_drivers']),n
  assert v['async_reg'] and n not in excluded
  reset_pairs.append(n)
assert len(reset_pairs)>=3,'Missing local reset-release pairs'
raw_sinks=re.findall(r'inpin "([^"]+)" (\w+)',nets.get('pcie/cdc_reset_i',''))
assert raw_sinks and all(pin=='SR' for i,pin in raw_sinks),'Raw reset used in synchronous data'
overlap_text=tsi.split('Clock Domain Overlap Report',1)[1]
overlaps=re.findall(r'([^{}]+)\{([^{}]+)\}',overlap_text)
assert len(overlaps)==3, 'Unexpected clock-domain overlap groups'
clock_overlaps=[]
for index,(header,members) in enumerate(overlaps):
 lines=[line.strip() for line in header.splitlines() if line.strip() and not line.startswith('=')]
 assert len(lines)==2,(index,lines)
 if index==0:
  assert lines==['TS_MGTCLK = PERIOD TIMEGRP "MGTCLK" 100 MHz HIGH 50%;','NET "pcie/sys_clk_c" PERIOD = 10 ns HIGH 50%;']
  period=10.0
 else:
  clock='clkout'+str(index-1)
  factor='2.5' if index==1 else '0.625'
  period=4.0 if index==1 else 16.0
  assert 'TS_MGTCLK * '+factor+' HIGH' in lines[0] and clock in lines[0]
  assert 'PERIOD analysis for net "pcie/ep/pcie_ep0/pcie_blk/clocking_i/'+clock+'"' in lines[1]
  assert 'to '+str(int(period))+' nS' in lines[1]
 sinks=[line.strip() for line in members.splitlines() if line.strip()]
 assert sinks and all(line.startswith('pcie/') for line in sinks)
 clock_overlaps.append({'constraints':lines,'equivalent_period_ns':period,'shared_synchronous_elements':sinks,'scope':'PCIe only'})
expected={'clk_fpga_p':'L19','clk_fpga_n':'K19','disp_clk_o':'F34','disp_latch_n_o':'H34','sys_clk_p':'AF4','sys_clk_n':'AF3','sys_reset_n':'AF24'}
expected.update(dict(('disp_shift_o<%d>'%n,pin) for n,pin in enumerate(['G33','G32','M32','P34','N34','AA34'])))
expected.update(dict(('disp_shift_i<%d>'%n,pin) for n,pin in enumerate(['AD32','Y34','Y32','W32','AH34','AE32','AG32','AH32'])))
rows=list(csv.DictReader(io.StringIO('\n'.join(l for l in (H/'panel_input_pad.csv').read_text().splitlines() if l and not l.startswith('#')))))
pins={r['Signal Name']:r for r in rows if r['Signal Name']}
for name,pin in expected.items():
 assert pins[name]['Pin Number']==pin,(name,pins.get(name))
 if name.startswith('disp_'):
  assert pins[name]['IO Standard'].rstrip('*')=='LVCMOS25'
  if not name.startswith('disp_shift_i'):
   assert pins[name]['Drive (mA)']=='12' and pins[name]['Slew Rate']=='SLOW'
assert 'GTP_DUAL_X0Y2' in p and 'GTP_DUAL_X0Y2' in x
for pin in ['AD2','AE2','AE1','AF1']:assert pin in (H/'panel_input.pad').read_text(),pin
stamp=(H/'src/vhdl/pcie/fpga_build.vhd').read_text()
constants=dict(re.findall(r'constant (M65_\w+).*?x"([0-9A-Fa-f]+)"',stamp))
assert constants['M65_FPGA_VERSION']=='00000000' and constants['M65_INTERFACE_VERSION']=='00010004'
assert 'ALD' not in (H/'sources.prj').read_text() and 'gen/ald' not in (H/'sources.prj').read_text()
audit={'verified':True,'input_scanner_registers':192,'mailbox_coverage':coverage,'physical_pairs':pairs,'reset_release':reset,'reset_second_stages':reset_pairs,'exception_groups':{k:sorted(v) for k,v in groups.items() if k in ['PANEL_INPUT_FIRST','PCIE_STATUS_FIRST','CDC_REQUEST_FIRST','CDC_ACK_FIRST']},'raw_reset_only_async_sr':True,'pins':expected,'pcie_gtp':'GTP_DUAL_X0Y2','unconstrained_reports':re.findall(r'Timing constraint: Unconstrained.*?\n\s*(\d+) paths analyzed, (\d+) endpoints analyzed, (\d+) failing endpoints',t,re.S),'source_xdl_sha256':hashlib.sha256((H/'panel_input.xdl').read_bytes()).hexdigest()}
audit['equivalent_pcie_clock_overlaps']=clock_overlaps
audit['unapplied_constraint_observation']='PAR reports five timing constraints not applied without naming them; retained as a limitation. Core and panel payload constraints are explicitly reported as analyzed.'
(H/'routed-input-cdc-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
e.update(verified_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),verified_build_and_timing=True,pins_verified=True,routed_input_and_cdc_verified=True,cpu_included=False,pcie_included=True,diagnostic_signature='PDI1',fpga_revision='0.0',interface_revision='1.4',build_date=constants['M65_BUILD_DATE'],build_time=constants['M65_BUILD_TIME'],serial_clock_hz=48828.125,raw_input_bits=192,polarity_assumed=False,core_minimum_period_ns=float(core.group(3)),core_paths=int(core.group(1)),core_endpoints=int(core.group(2)),panel_cdc_paths=int(cdc.group(1)),panel_cdc_endpoints=int(cdc.group(2)),panel_cdc_maximum_delay_ns=float(cdc.group(3)),routed_audit='routed-input-cdc-audit.json',limitations=['External panel cable, voltage levels and reset/LED/input I/O timing are unspecified; routed timing is not electrical wiring proof.','The existing scanner is intentionally preserved; mapping finds its observed bank/bit/polarity and may reveal framing faults.','Legacy button flags in PNL1 still use the original active-low interpretation; the diagnostic reader ignores them and reports raw banks only.','Physical control mapping requires named repeated presses/lever positions and return to baseline.'])
(H/'result.json').write_text(json.dumps(e,indent=2)+'\n')
e['limitations'].extend(['Three PCIe clock-domain overlaps have equivalent 10 ns, 4 ns and 16 ns definitions; both constraint definitions and their shared elements are saved in the routed audit.','PAR reports five unnamed timing constraints not applied. Individual unreported synchronizer-stage slacks and external I/O timing remain unspecified.'])
(H/'result.json').write_text(json.dumps(e,indent=2)+'\n')
q=H/'trial.json';d=json.loads(q.read_text());d.update(status='verified-ready-for-programming',result='result.json',bitstream_sha256=e['bitstream_sha256']);q.write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps(e,indent=2))
