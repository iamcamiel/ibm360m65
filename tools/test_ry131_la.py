from pathlib import Path
import json,subprocess,shutil,re,argparse
R=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--native',type=Path,required=True);a.add_argument('--output',type=Path,required=True);args=a.parse_args()
O=args.output.resolve();O.mkdir(exist_ok=False,parents=True)
old=R/'gen/logout-repair-r123-20261012/native';fixed=args.native.resolve()
s=(old/'probe-word.cpp').read_text().replace('0x58400300','0x58f00300').replace('mode==3?0x07000700:0x54400304','0x41a0f06c').replace('mode==4?0x07000700:0x50400308','0x50a00308')
results={}
for name,base,g in [('r123',old,R/'gen/logout-repair-r123-20261012/source-snapshot/gen/ald'),('corrected-wire',fixed,R/'gen/ald')]:
 d=O/name;d.mkdir();(d/'probe-la.cpp').write_text(s);shutil.copy2(old/'r24-guest-memory.bin',d/'r24-guest-memory.bin');shutil.copy2(R/'src/ros.txt',d/'ros.txt')
 objects=[base/'cpu.obj',*[base/(p.stem+'.obj') for p in sorted(g.glob('360_*.cpp'))]]
 cmd=['@echo off','call "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul','cl /nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(R/'hercules')+'" /I"'+str(g)+'" probe-la.cpp '+' '.join('"'+str(p)+'"' for p in objects)+' /Fe:probe-la.exe','exit /b %errorlevel%'];(d/'build.cmd').write_text('\n'.join(cmd))
 p=subprocess.run(['cmd','/c','build.cmd'],cwd=d,capture_output=True,text=True);(d/'build.log').write_text(p.stdout+p.stderr);p.check_returncode()
 p=subprocess.run([str(d/'probe-la.exe'),'2','40000082','ffffffff'],cwd=d,capture_output=True,text=True);t=p.stdout+p.stderr;(d/'result.log').write_text(t)
 m=re.search(r'stored308=([0-9a-f]+)',t);healthy=p.returncode==0 and 'fault=0' in t and 'ROS=784' in t and 'MBcheck=1 BFcheck=1 ASfull=1 AShalf=1' in t
 results[name]=dict(exit=p.returncode,healthy=healthy,expected_gr10='000000ee',stored_gr10=m[1] if m else None,data_matches=bool(m and int(m[1],16)==0xee),output=t)
 print(name,json.dumps({k:results[name][k] for k in ['healthy','stored_gr10','data_matches']}),flush=True)
assert results['corrected-wire']['healthy'] and results['corrected-wire']['data_matches']
assert not results['r123']['healthy'] or not results['r123']['data_matches']
(O/'result.json').write_text(json.dumps(dict(passed=True,results=results,scope='Native LA 41A0F06C with GR15=40000082; expectedGR10=EE; private immediate storage, not exact historical live replay'),indent=2))
