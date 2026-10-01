"""Execute the compiled page owner through native row initializer/attachment code.

No client process or installed output is used. DBC/composition leaves remain
synthetic, so ten equipped residents and frame performance need human testing.
"""
import argparse
import json
from pathlib import Path
import struct
from check_story_residents import Fixture
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP


def requests(f, indices, *, activities=False):
    address=f.MEMORY+0x91000
    for slot,index in enumerate(indices):
        guid=struct.unpack('<Q',f.cpu.mem_read(f.ROWS+index*0x198,8))[0]
        f.cpu.mem_write(address+slot*40,struct.pack('<I4xQffIff4x',index,guid,slot*.4,slot*.2,
                                                   (slot%3) if activities else 0,-1,-1))
    return address


def begin(f, indices, **options):
    return f.status('BeginPage',requests(f,indices,**options),len(indices),f.call('PageRevision'))


def model(f,index): return f.get(f.get(f.ROWS+index*0x198+0x188)+0x38)
def placement(f,actor): return bytes(f.cpu.mem_read(actor+0xb4,64))
def walk_clip(f,actor):
    header=f.get(f.get(actor+0x2c)+0x150); sequences=f.get(header+0x20)
    f.put(header+0x1c,3)
    f.cpu.mem_write(sequences+0x80,struct.pack('<HHIfI',4,0,1000,0,0))


def lua_method(f):
    """Lua values are leaves; execute the actual parser and ten-value reply."""
    values, replies = {}, []
    number=f.MEMORY+0x9e000; loader=f.STOP+0x200
    f.cpu.mem_write(loader,b'\xdd\x05'+struct.pack('<I',number)+b'\xc3')
    def to_number():
        f.cpu.mem_write(number,struct.pack('<d',values.get(f.arg(2),0)))
        f.cpu.reg_write(UC_X86_REG_EIP,loader)
    def to_string():
        value=values.get(f.arg(2)); address=f.MEMORY+0xb0000+f.arg(2)*0x100
        if isinstance(value,str): f.cpu.mem_write(address,value.encode()+b'\0')
        f.ret(address if isinstance(value,str) else 0)
    def push_string():
        replies.append(bytes(f.cpu.mem_read(f.arg(2),512)).split(b'\0',1)[0].decode()); f.ret()
    def push_number():
        replies.append(struct.unpack('<d',f.cpu.mem_read(f.cpu.reg_read(UC_X86_REG_ESP)+8,8))[0]); f.ret()
    def push_boolean(): replies.append(bool(f.arg(2))); f.ret()
    f.leaves.update({0x4a81b0:lambda:f.ret(f.FRAME),0x84e030:to_number,0x84e0e0:to_string,
                     0x84e350:push_string,0x84e2a0:push_number,0x84e4d0:push_boolean})
    def call(action,*args):
        values.clear(); values.update({2:action,**{index:value for index,value in enumerate(args,3)}})
        replies.clear(); assert f.call('CallResidents',f.MEMORY+0x9c000)==10
        return list(replies)
    return call


def exercise(client,library):
    passed=[]
    for count in (1,3,10):
        f=Fixture(client,library); f.roster(50)
        order=list(reversed(range(count))) # chosen native actor is the last authored slot
        origin=placement(f,f.selected_model)
        assert begin(f,order)=='loading'
        token=f.call('PairToken'); assert f.call('PageCount')==count
        assert f.call('PageIndex',0)==0
        assert f.get(0xAC436C)==0 and len(f.created)==count-1
        assert f.step(token)=='ready'
        selected_placement=struct.unpack('<16f',placement(f,f.selected_model))
        assert abs(selected_placement[13]-order.index(0)*.4)<1e-5
        for index in range(1,count):
            actor=model(f,index)
            slot=order.index(index)
            placed=struct.unpack('<16f',placement(f,actor))
            assert abs(placed[13]-slot*.4)<1e-5
            assert abs(placed[12]-slot*.2)<1e-5
            f.call('LightPair',actor)
            assert f.lights[-1][0]==f.ROWS+index*0x198
            equipment={display for _,_,_,display in f.equipment if 1000*(index+1)<=display<1000*(index+1)+23}
            assert equipment==set(range(1000*(index+1),1000*(index+1)+23))-{1000*(index+1)+17}, (index,equipment)
        f.call('StopPair'); assert f.call('PageCount')==0 and f.call('PairToken')==0
        assert placement(f,f.selected_model)==origin
        for index in range(1,count):
            actor=model(f,index)
            assert f.get(actor+0x48)==0 and f.get(actor+0x2ac)==0x4e3a20
            assert f.get(actor+0x2b0)==f.FRAME+0x6e8
        assert begin(f,order)=='loading' and len(f.created)==count-1
        next_token=f.call('PairToken'); assert next_token!=token
        assert f.step(token)=='stale-generation' and f.call('PairToken')==next_token
        assert f.step(next_token)=='ready'
    passed.append('one-three-ten-guid-ordered-cold-native-rows-equipment-lighting-focus-and-cached-restart')
    f=Fixture(client,library); f.roster(5)
    assert begin(f,list(range(5)))=='loading'; token=f.call('PairToken'); assert f.step(token)=='ready'
    fixed={index:placement(f,model(f,index)) for index in range(5)}
    for focus in (3,1,4,0,2):
        assert f.call('SelectPair',focus)==77 and f.get(0xAC436C)==focus
        f.call('InitializeNormal') # original selected actor detach/reparent path
        assert begin(f,list(range(5)))=='loading'
        token=f.call('PairToken'); assert f.step(token)=='ready'
        assert all(placement(f,model(f,index))==fixed[index] for index in range(5))
        assert f.call('PageIndex',0)==focus
    f.call('StopPair')
    passed.append('five-resident-authored-slots-stay-fixed-through-native-selection-and-reinitialization')

    for failure,expected in (('duplicate','same-resident'),('guid','invalid-page'),('index','invalid-page'),
                             ('revision','invalid-page'),('outside','selection-outside-page'),('zero','invalid-page'),('eleven','invalid-page')):
        f=Fixture(client,library); f.roster(50)
        at=requests(f,[0,1,2]); count=3; revision=0
        if failure=='duplicate': f.cpu.mem_write(at+40,bytes(f.cpu.mem_read(at,40)))
        elif failure=='guid': f.cpu.mem_write(at+8,struct.pack('<Q',0xdead))
        elif failure=='index': f.put(at,50)
        elif failure=='revision': revision=1
        elif failure=='outside': at=requests(f,[1,2,3])
        elif failure=='zero': count=0
        else: count=11
        assert f.status('BeginPage',at,count,revision)==expected, failure
        assert f.call('PairToken')==0 and f.get(0xAC436C)==0 and not f.created
    passed.append('duplicate-guid-index-revision-selection-and-one-to-ten-bounds-refuse-before-initializing')

    f=Fixture(client,library); f.roster(50)
    f.cpu.mem_write(f.ROWS+7*0x198+0x178,b'\xff')
    assert begin(f,list(range(10)))=='initializer-failed'
    assert f.call('PairToken')==0 and f.get(0xAC436C)==0
    for index in range(1,7):
        actor=model(f,index)
        assert f.get(actor+0x48)==0 and f.get(actor+0x2ac)==0x4e3a20
    assert f.get(f.BACKGROUND+0x58)==f.selected_model
    passed.append('partial-cold-initialization-failure-restores-every-owned-extra-and-keeps-selection')

    for operation in ('timeout','select','refresh','stand','rows','frame'):
        f=Fixture(client,library); f.roster(50)
        assert begin(f,list(range(10)))=='loading'; token=f.call('PairToken')
        if operation=='timeout':
            f.prepared=False
            for _ in range(30): assert f.step(token,.5)=='loading'
            assert f.step(token,.5)=='loading-timeout'
        elif operation=='select': assert f.call('SelectPair',8)==77 and f.get(0xAC436C)==8
        elif operation=='refresh': f.call('RefreshPair')
        elif operation=='stand':
            actor=model(f,8); sequences=f.get(f.get(f.get(actor+0x2c)+0x150)+0x20)
            f.cpu.mem_write(sequences,struct.pack('<H',99))
            assert f.step(token)=='unsupported-clips'
        else:
            f.put(0xB6B240 if operation=='rows' else 0xB6B1FC,0)
            f.cpu.mem_unmap(f.MEMORY,0x100000)
            assert f.step(token)=='interrupted'
        assert f.call('PairToken')==0
        if operation not in ('rows','frame'):
            for index in range(1,10):
                actor=model(f,index)
                assert f.get(actor+0x48)==0 and f.get(actor+0x2ac)==0x4e3a20
    passed.append('loading-timeout-selection-refresh-missing-stand-and-freed-owner-invalidate-all-ten')

    f=Fixture(client,library); f.roster(50)
    assert begin(f,list(range(10)),activities=True)=='loading'; token=f.call('PairToken')
    for index in range(10): walk_clip(f,model(f,index))
    # Optional activities may be absent; Stand remains available.
    missing=model(f,8); header=f.get(f.get(missing+0x2c)+0x150); f.put(header+0x1c,1)
    assert f.step(token)=='ready'
    assert f.status('ActPage',token,8,2)=='unsupported-activity' and f.call('PairToken')==token
    staged={index:placement(f,model(f,index)) for index in range(10)}
    f.sequences.clear()
    for _ in range(17): assert f.step(token,.05)=='ready'
    assert (model(f,1),113) not in f.sequences
    for _ in range(14): assert f.step(token,.05)=='ready'
    assert (model(f,1),113) in f.sequences and (model(f,2),4) not in f.sequences
    for _ in range(14): assert f.step(token,.05)=='ready'
    assert (model(f,2),4) in f.sequences
    assert placement(f,model(f,0))==staged[0]
    assert f.status('ActPage',token,3,2)=='walk'
    for _ in range(110): assert f.step(token,.05)=='ready'
    assert placement(f,model(f,3))==staged[3] # slot 3 has authored Stand only
    f.call('StopPair'); assert f.get(0xAC436C)==0
    passed.append('independent-slot-delays-optional-clip-fallback-and-bounded-walk-returns-to-staging')

    f=Fixture(client,library); f.roster(50); call=lua_method(f)
    args=[0,10]; identities=''
    for index in reversed(range(10)):
        guid=f'{0x1111+index:016x}'; args.extend([index+1,guid,.4,0,0]); identities+=guid+f'@{index+1};'
    reply=call('page',*args)
    assert reply[:2]==[True,'loading'] and reply[3:5]==['0000000000001111',1]
    assert reply[8:]==[10,identities]
    token=int(reply[2]); reply=call('step',token,.016)
    assert reply[:2]==[True,'ready'] and reply[8:]==[10,identities]
    call('stop')
    for at,value in ((0,float('nan')),(1,11),(2,1.5),(3,'bad'),(3,'0'*16),(4,float('inf')),(6,3)):
        malformed=list(args); malformed[at]=value
        try: reply=call('page',*malformed)
        except RuntimeError as error: raise RuntimeError(f'parser input {at}={value!r}: {error}') from error
        assert reply[:2]==[False,'invalid-page'] and reply[8:]==[0,''], (at,value,reply)
    passed.append('compiled-lua-parser-strict-hex-finite-bounds-and-complete-authored-page-identity-reply')
    f=Fixture(client,library); f.roster(5); call=lua_method(f)
    timed=[0,5]
    for index in range(5):
        timed.extend([index+1,f'{0x1111+index:016x}',index*.4,index*.2,index%3,2+index*3,12+index*2])
    reply=call('scene',*timed); assert reply[:2]==[True,'loading']
    token=int(reply[2])
    for index in range(5): walk_clip(f,model(f,index))
    assert call('step',token,.05)[:2]==[True,'ready']
    f.sequences.clear()
    for _ in range(90): assert call('step',token,.05)[:2]==[True,'ready']
    assert (model(f,1),113) not in f.sequences
    for _ in range(12): call('step',token,.05)
    assert (model(f,1),113) in f.sequences and (model(f,2),4) not in f.sequences
    call('stop')
    for at,value in ((7,float('nan')),(7,-1),(8,2),(8,61),(8,float('inf'))):
        malformed=list(timed); malformed[at]=value
        assert call('scene',*malformed)[:2]==[False,'invalid-page']
    passed.append('authored-independent-native-activity-timing-and-strict-scene-parser-bounds')
    return dict(status='offline-page-fixtures-passed',cases=passed,
                limits='Original native initializer/equipment/attachment and compiled owner; synthetic composition leaves. No ten-equipped render or performance acceptance.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client',type=Path,required=True)
    parser.add_argument('--fixture',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args(); result=exercise(args.client,args.fixture)
    encoded=json.dumps(result,indent=2)+'\n'
    if args.output: args.output.write_text(encoded,encoding='utf-8')
    print(encoded)
