"""Execute compiled row contexts through the unmodified native equipment policy.

Native initializer, redirected instructions and attachment code are real. Engine
model/DBC/composition leaves are synthetic and logged: no equipped render pass.
No process, account, client writes or installed library is involved.
"""
import argparse
import json
from pathlib import Path
import struct

import pefile
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS, UC_X86_REG_GDTR,
    UC_X86_REG_FS, UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
    UC_X86_REG_FPSW, UC_X86_REG_FPTAG)


class Fixture:
    MEMORY, STACK, STOP = 0x20000000, 0x21000000, 0x22000000
    FRAME, ROWS, BACKGROUND = MEMORY+0x1000, MEMORY+0x3000, MEMORY+0x8000
    MODEL_DEF, PATH, NAME, SCENE = MEMORY+0x40000, MEMORY+0x41000, MEMORY+0x42000, MEMORY+0x43000

    def __init__(self, client, library):
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.methods, self.leaves, self.events = {}, {}, []
        for path in (client, library):
            pe = pefile.PE(str(path), fast_load=True)
            assert pe.FILE_HEADER.Machine == 0x14c
            base = pe.OPTIONAL_HEADER.ImageBase
            self.cpu.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
            self.cpu.mem_write(base, pe.get_memory_mapped_image())
            if path == library:
                pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT'],
                                                       pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_TLS']])
                for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                    if symbol.name: self.methods[symbol.name.decode().lstrip('_')]=base+symbol.address
                tls=pe.DIRECTORY_ENTRY_TLS.struct
                self.tls_image=bytes(self.cpu.mem_read(tls.StartAddressOfRawData,
                                                      tls.EndAddressOfRawData-tls.StartAddressOfRawData))
                self.put(tls.AddressOfIndex,0)
        self.cpu.mem_map(self.MEMORY,0x100000)
        self.cpu.mem_map(self.STACK,0x10000); self.cpu.mem_map(self.STOP,0x1000)
        self.cpu.mem_map(0x23000000,0x1000)  # GDT
        self.cpu.mem_map(0x24000000,0x4000)  # two thread TEB/vector pairs
        self.cpu.mem_map(0x25000000,0x4000)  # independent module TLS blocks
        assert len(self.tls_image)<0x2000
        for index,access in ((1,0x9b),(2,0x93)):
            descriptor=0xffff|(access<<40)|(0xf<<48)|(0xc<<52)
            self.cpu.mem_write(0x23000000+index*8,struct.pack('<Q',descriptor))
        for thread in (0,1):
            teb=0x24000000+thread*0x2000
            vector=teb+0x1000; data=0x25000000+thread*0x2000
            self.cpu.mem_write(data,self.tls_image)
            self.put(teb+0x2c,vector); self.put(vector,data)
            descriptor=(0xffff | ((teb&0xffffff)<<16) | (0x93<<40) |
                        (0xf<<48) | (0xc<<52) | ((teb>>24)<<56))
            self.cpu.mem_write(0x23000000+(3+thread)*8,struct.pack('<Q',descriptor))
        self.cpu.reg_write(UC_X86_REG_GDTR,(0,0x23000000,0x1000,0))
        self.cpu.reg_write(UC_X86_REG_CS,8)
        for register in (UC_X86_REG_DS,UC_X86_REG_ES,UC_X86_REG_SS): self.cpu.reg_write(register,16)
        self.thread(0)
        self.cpu.hook_add(UC_HOOK_CODE,self.dispatch)
        self.redirects=[]
        for index in range(100):
            out=self.MEMORY+0x90000
            if not self.call('GetRedirect',index,out): break
            site,size=struct.unpack('<II',self.cpu.mem_read(out,8))
            self.redirects.append((site,size))
            self.cpu.mem_write(site,bytes(self.cpu.mem_read(out+8,size)))
        assert len(self.redirects)==37

    def thread(self,index): self.cpu.reg_write(UC_X86_REG_FS,(3+index)*8)
    def put(self,address,value): self.cpu.mem_write(address,struct.pack('<I',value))
    def get(self,address): return struct.unpack('<I',self.cpu.mem_read(address,4))[0]
    def ret(self,value=0,stack=0):
        sp=self.cpu.reg_read(UC_X86_REG_ESP)
        self.cpu.reg_write(UC_X86_REG_EAX,value)
        self.cpu.reg_write(UC_X86_REG_EIP,self.get(sp))
        self.cpu.reg_write(UC_X86_REG_ESP,sp+4+stack)
    def arg(self,index): return self.get(self.cpu.reg_read(UC_X86_REG_ESP)+index*4)
    def this(self): return self.cpu.reg_read(UC_X86_REG_ECX)
    def dispatch(self,_,address,size,data):
        if address in self.leaves: self.leaves[address]()
    def call(self,method,*args):
        sp=self.STACK+0xf000
        self.put(sp,self.STOP)
        for index,value in enumerate(args,1): self.put(sp+index*4,value&0xffffffff)
        self.cpu.reg_write(UC_X86_REG_ESP,sp)
        try: self.cpu.emu_start(self.methods[method],self.STOP,count=100000)
        except UcError as error:
            raise RuntimeError(f'{method}: fault at {self.cpu.reg_read(UC_X86_REG_EIP):08x}, '
                               f'ESP={self.cpu.reg_read(UC_X86_REG_ESP):08x}') from error
        assert self.cpu.reg_read(UC_X86_REG_EIP)==self.STOP
        assert self.cpu.reg_read(UC_X86_REG_ESP)==sp+4
        return self.cpu.reg_read(UC_X86_REG_EAX)

    def model(self,address):
        self.cpu.mem_write(address,b'\0'*0x400)
        self.put(address,1); self.put(address+0x10,0x203)
        matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        self.cpu.mem_write(address+0xb4,struct.pack('<16f',*matrix))
        return address

    def roster(self):
        self.put(0xB6B1FC,self.FRAME); self.put(0xB6B23C,2); self.put(0xB6B240,self.ROWS)
        self.put(0xAC436C,0); self.put(0xB6B200,1)
        self.put(0xB6B204,0x3f123456); self.put(0xD38C2C,0x3f654321)
        self.cpu.mem_write(self.PATH,b'Character\\Fixture\\NativeRace.m2\0')
        self.cpu.mem_write(self.NAME,b'Fixture\0')
        self.put(self.MODEL_DEF+8,self.PATH)
        self.put(self.FRAME+0x2a0,self.BACKGROUND)
        self.model(self.BACKGROUND); self.put(self.BACKGROUND+0x10,0)
        self.components={}; self.created=[]; self.equipment=[]; self.requests=[]
        for index in (0,1):
            row=self.ROWS+index*0x198
            self.cpu.mem_write(row,struct.pack('<Q',0x1111+index))
            self.cpu.mem_write(row+0x178,bytes([1+index,1,0,4+index,5+index,6+index,7+index,8+index]))
            for slot in range(23): self.put(row+0x50+slot*4,1000*(index+1)+slot)
        selected=self.MEMORY+0x6000; model=self.model(self.MEMORY+0x18000)
        self.put(selected+0x38,model); self.put(self.ROWS+0x188,selected)
        self.components[selected]=0
        # The native selector already owns and displays resident 0.
        self.put(model+0x48,self.BACKGROUND); self.put(model+0x50,0)
        self.put(model+0x5c,self.BACKGROUND+0x58); self.put(model,2)
        self.put(self.BACKGROUND+0x58,model)
        self.selected_model=model
        self.leaves.update({
            0x4DFDA0:self.request_default, 0x6DC810:self.race_model,
            0x4F0980:self.allocate, 0x95F650:lambda:self.ret(self.SCENE),
            0x81F8F0:self.create_model, 0x4F24D0:self.init_component,
            0x832AB0:lambda:self.ret(1,28), 0x8B7DA0:self.item_default,
            0x4CFD90:self.item_data, 0x65C290:lambda:self.ret(0,4),
            0x4F2880:self.equip, 0x4EACD0:self.weapon,
            0x4F2640:self.special, 0x5EEB70:lambda:self.ret(),
            0x8C02E0:lambda:self.side_effect('curve'),
            0x4E2E70:lambda:self.side_effect('facing'),
            0x4E6AE0:lambda:self.side_effect('fade'),
            0x4E2CE0:lambda:self.ret(),
        })
    def request_default(self):
        self.cpu.mem_write(self.this(),b'\0'*0x178); self.ret()
    def race_model(self):
        self.events.append(('race',self.arg(1),self.arg(2)))
        self.ret(0 if self.arg(1)==255 else self.MODEL_DEF)
    def allocate(self):
        component=self.MEMORY+0x10000+len(self.created)*0x1000
        self.created.append(component); self.components[component]=1
        self.ret(component)
    def create_model(self):
        model=self.model(self.MEMORY+0x20000+(len(self.created)-1)*0x1000)
        self.ret(model,8)
    def init_component(self):
        component,request=self.this(),self.arg(1)
        body=bytes(self.cpu.mem_read(request,0x178))
        self.requests.append((component,struct.unpack_from('<II',body)))
        self.cpu.mem_write(component+0x18,body); self.ret(1,8)
    def item_default(self):
        self.cpu.mem_write(self.this(),b'\0'*0x60); self.ret()
    def item_data(self):
        display,out=self.arg(1),self.arg(2)
        self.put(out,display); self.put(out+4,self.NAME)
        self.ret(self.MODEL_DEF,8)
    def equip(self):
        self.equipment.append(('equip',self.this(),self.arg(1),self.arg(2)))
        self.ret(1,12)
    def weapon(self):
        self.equipment.append(('weapon',self.arg(1),self.arg(3),self.get(self.arg(2))))
        self.ret()
    def special(self):
        self.equipment.append(('special',self.this(),self.arg(1),self.get(self.arg(2))))
        self.ret(1,12)
    def side_effect(self,name):
        self.events.append((name,self.arg(1))); self.ret()


def exercise(client,library):
    passed=[]
    registers=(UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,
               UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
    destinations=(UC_X86_REG_ESI,)*4+(UC_X86_REG_EAX,)*2+(UC_X86_REG_EDX,)*2+(
        UC_X86_REG_EBX,UC_X86_REG_EDX,UC_X86_REG_EDX,UC_X86_REG_EDX,
        UC_X86_REG_EBX,UC_X86_REG_EBX,UC_X86_REG_EBX,UC_X86_REG_EBX,
        UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ECX,UC_X86_REG_EDX,
        UC_X86_REG_EDX,UC_X86_REG_EDX,UC_X86_REG_EDX,UC_X86_REG_ECX,
        UC_X86_REG_EDX,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EAX,UC_X86_REG_EAX)
    f=Fixture(client,library); f.put(0xAC436C,7)
    for context in (-1,1):
        f.call('SetContext',context,0)
        for (site,size),destination in zip(f.redirects[:29],destinations):
            saved={register:0xabcd0100+index for index,register in enumerate(registers)}
            for register,value in saved.items(): f.cpu.reg_write(register,value)
            f.cpu.reg_write(UC_X86_REG_ESP,f.STACK+0xe000)
            f.cpu.reg_write(UC_X86_REG_EFLAGS,0x246)
            f.cpu.mem_write(f.STOP+0x100,b'\xdb\xe3\xd9\xe8') # FNINIT; FLD1
            f.cpu.emu_start(f.STOP+0x100,f.STOP+0x104)
            fp=(f.cpu.reg_read(UC_X86_REG_FPSW),f.cpu.reg_read(UC_X86_REG_FPTAG))
            f.cpu.emu_start(site,site+size,count=1000)
            for register,value in saved.items():
                assert f.cpu.reg_read(register)==((context if context>=0 else 7) if register==destination else value)
            assert f.cpu.reg_read(UC_X86_REG_ESP)==f.STACK+0xe000
            assert f.cpu.reg_read(UC_X86_REG_EFLAGS)==0x246
            assert fp==(f.cpu.reg_read(UC_X86_REG_FPSW),f.cpu.reg_read(UC_X86_REG_FPTAG))
            assert f.get(0xAC436C)==7
    passed.append('all-29-compiled-row-reads-preserve-registers-flags-x87-and-native-selection')
    f.thread(1); f.call('SetContext',3,0)
    site,size=f.redirects[28]
    f.cpu.emu_start(site,site+size,count=1000); assert f.cpu.reg_read(UC_X86_REG_EAX)==3
    f.thread(0); f.cpu.emu_start(site,site+size,count=1000); assert f.cpu.reg_read(UC_X86_REG_EAX)==1
    passed.append('thread-local-and-nested-native-entry-context-isolation')

    f=Fixture(client,library); f.roster()
    selected=bytes(f.cpu.mem_read(f.selected_model,0x400))
    assert f.get(f.ROWS+0x198+0x188)==0  # resident 1 is cold, never selected
    f.call('InitializeRow',1)
    component=f.get(f.ROWS+0x198+0x188); root=f.get(component+0x38)
    assert component and root and root!=f.selected_model
    assert f.requests==[(component,(2,0))]
    assert f.get(0xAC436C)==0 and f.get(0xB6B204)==0x3f123456 and f.get(0xD38C2C)==0x3f654321
    assert not [event for event in f.events if event[0] in ('curve','facing','fade')]
    assert all(2000<=display<2023 for _,_,_,display in f.equipment)
    assert len(f.equipment)==22, f.equipment
    assert {display for _,_,_,display in f.equipment}==set(range(2000,2023))-{2017}
    assert f.get(f.BACKGROUND+0x58)==root and f.get(root+0x60)==f.selected_model
    assert f.get(f.selected_model+0x48)==f.BACKGROUND and f.get(f.selected_model)==2
    # Adding a sibling necessarily changes the selected child's list back-link, and nothing else.
    after=bytearray(f.cpu.mem_read(f.selected_model,0x400))
    after[0x5c:0x60]=selected[0x5c:0x60]
    assert bytes(after)==selected
    f.call('InitializeRow',1)  # cached native path, no clone
    assert len(f.created)==1
    f.call('SetContext',1,1); f.call('InitializeNormal')
    assert f.get(0xAC436C)==0
    assert [event[0] for event in f.events if event[0] in ('curve','facing','fade')]==['curve','facing','fade']
    passed.append('cold-equipped-row-native-policy-and-shared-side-effect-isolation')

    f=Fixture(client,library); f.roster()
    f.cpu.mem_write(f.ROWS+0x198+0x178,b'\xff')
    f.call('InitializeRow',1)
    assert f.get(f.ROWS+0x198+0x188)==0 and f.get(0xAC436C)==0
    f.put(0xAC436C,7)
    site,size=f.redirects[28]; f.cpu.emu_start(site,site+size,count=1000)
    assert f.cpu.reg_read(UC_X86_REG_EAX)==7
    passed.append('initializer-early-failure-unwinds-row-context-without-changing-selection')
    return dict(status='offline-scoped-native-initializer-passed',cases=passed,
                limits='Synthetic engine model/DBC/composition leaves; no rendered/equipped/native-client acceptance.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client',type=Path,required=True)
    parser.add_argument('--fixture',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args(); result=exercise(args.client,args.fixture)
    encoded=json.dumps(result,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(encoded,encoding='utf-8')
    print(encoded)
