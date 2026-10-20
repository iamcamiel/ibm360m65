"""Compare every diagnostic reply/halt edge with the frozen pre-pipeline design."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ghdl',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
s=a.baseline.read_text().replace('CPU_DIAGNOSTICS','CPU_DIAGNOSTICS_REFERENCE');(o/'reference.vhd').write_text(s)
def run(name,args):
 z=subprocess.run([str(a.ghdl.resolve()),*args],cwd=o,capture_output=True,text=True);(o/(name+'.log')).write_text(z.stdout+z.stderr);assert z.returncode==0,(name,(z.stdout+z.stderr)[-2000:]);return z.stdout+z.stderr
run('analyze',['-a','--std=93',o/'reference.vhd',r/'src/vhdl/cpu_diagnostics.vhd',r/'tools/test_cpu_diagnostic_history.vhd'])
log=run('simulate',['-r','--std=93','test_cpu_diagnostic_history','--assert-level=error','--stop-time=1ms']);assert 'CPU_DIAGNOSTIC_HISTORY_PASS' in log
# Removing the forwarding mux must be detected on the first immediate read.
text=(r/'src/vhdl/cpu_diagnostics.vhd').read_text();start=text.index("                  if history_write_valid='1' and history_write_address=to_unsigned(slot,8) then");end=text.index('                  history_lane',start);bad=text[:start]+"                  history_read<=history(slot);\n"+text[end:];(o/'without-forwarding.vhd').write_text(bad)
run('negative-analyze',['-a','--std=93',o/'without-forwarding.vhd',r/'tools/test_cpu_diagnostic_history.vhd'])
z=subprocess.run([str(a.ghdl.resolve()),'-r','--std=93','test_cpu_diagnostic_history','--assert-level=error','--stop-time=1ms'],cwd=o,capture_output=True,text=True);(o/'negative.log').write_text(z.stdout+z.stderr);assert z.returncode!=0 and 'history pipeline changed command data' in z.stdout+z.stderr
result={'passed':True,'reference_sha256':hashlib.sha256(a.baseline.read_bytes()).hexdigest(),'negative_without_forwarding_failed':True,'scope':'Exact diagnostic reply and halt edge comparison against R139; directed history entries/timestamps, three wraps, reset/rearm and event/step priority. No routed timing claim.'};(o/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
