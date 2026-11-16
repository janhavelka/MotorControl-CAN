import argparse,json,time
from pathlib import Path
import serial
from serial.tools import list_ports
ap=argparse.ArgumentParser();ap.add_argument('--period',type=int,required=True);ap.add_argument('--count',type=int,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
assert a.period in (10,100) and a.count==20
ports=[p for p in list_ports.comports() if p.device=='COM15' and p.vid==0x303A and p.pid==0x1002 and p.serial_number=='94A990398B40'];assert len(ports)==1
out=a.output;out.mkdir(parents=True,exist_ok=False)
(out/'procedure.py').write_bytes(Path(__file__).read_bytes())
records=[];buffer=b'';history=[]
s=serial.Serial(port=None,baudrate=115200,timeout=.05,write_timeout=1);s.dtr=False;s.rts=False;s.port='COM15';s.open()
raw=(out/'rx.bin').open('wb')
def pump(until,match=None):
 global buffer
 while time.monotonic()<until:
  chunk=s.read(4096)
  if chunk: raw.write(chunk);raw.flush();buffer+=chunk
  while b'\n' in buffer:
   line,buffer=buffer.split(b'\n',1);line=line.rstrip(b'\r').decode('ascii',errors='replace');history.append(line)
   if line.startswith('{'):
    r=json.loads(line)
    if match and match(r):return r
   elif match and match(line):return line
 return None

def command(text,ident):
 wire=(text+'\n').encode('ascii');records.append({'send':text,'monotonic':time.monotonic()});s.write(wire)
 r=pump(time.monotonic()+5,lambda r:isinstance(r,dict) and r.get('id')==ident and r.get('type')=='reply')
 assert r and r.get('ok'),repr(r)
 if r.get('result')=='accepted':
  term=pump(time.monotonic()+8,lambda r:isinstance(r,dict) and r.get('id')==ident and r.get('type')=='terminal')
  assert term, 'missing terminal'
  records.append({'terminal':term})
  return term
 return r
try:
 records.append({'send':'P8'});s.write(b'P8\n');assert pump(time.monotonic()+3,lambda r:isinstance(r,str) and r.startswith('PDOW admitted'))
 command('@5200 host 0',5200)
 command('@5201 debug raw',5201)
 command('@5202 sync arm',5202)
 pump(time.monotonic()+.5)
 t=command('@5203 sync period %d %d'%(a.period,a.count),5203)
 command('@5204 result %d'%t['operation_id'],5204)
 command('@5205 release %d'%t['operation_id'],5205)
 command('@5206 sync disarm',5206)
 command('@5207 debug off',5207)
 pump(time.monotonic()+9)
 command('@5208 stats',5208)
 print(json.dumps(t))
finally:
 s.close();raw.close();(out/'rx.txt').write_bytes((out/'rx.bin').read_bytes());(out/'request.json').write_text(json.dumps({'period_ms':a.period,'count':a.count,'steps':records,'fixture':'COM15 303A:1002 94A990398B40; same P51 image','no_automatic_retry':True},indent=2)+'\n')
