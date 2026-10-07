#include "client/StorySelect/ModelMap.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace wxl::story::models;
int main(int argc,char** argv)
{
    // Synthetic fixture emitted by Velora select_screen_models.render().
    // Optional argument exercises fresh generator output with the same contract.
    static const char fixture[]=
        "\127\130\114\123\105\114\061\000\002\000\000\000\002\000\000\000\003\000\000\000\000\000\000\000\117\164\150\145\162\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\126\145\154\157\162\141\040\102\145\164\141\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\103\150\141\162\141\143\164\145\162\134\126\145\154\157\162\141\134\120\145\162\163\157\156\056"
        "\155\062\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\001\004\001\000\001\002\003\000\002\000\000\000\007\000\000\000\000\000\000\000"
        "\105\154\145\156\145\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\126\145\154\157\162\141\040\102\145\164\141\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\103\150\141\162\141\143\164\145\162\134\126\145\154\157\162\141"
        "\134\120\145\162\163\157\156\056\155\062\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
        "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\004\001\000\001\002\003\000";
    std::vector<uint8_t> bytes(fixture,fixture+sizeof(fixture)-1);
    if (argc==2)
    {
        std::ifstream file(argv[1],std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(file),{});
    }
    std::vector<Entry> map;
    assert(Parse(bytes.data(),bytes.size(),map) && map.size()==2);
    const uint8_t appearance[]={1,4,1,0,1,2,3,0};
    const auto selected=Find(map,"Velora Beta",7,"Elene",appearance);
    const auto resident=Find(map,"Velora Beta",3,"Other",appearance);
    assert(selected && resident && selected!=resident);
    assert(!Find(map,"Velora Home",7,"Elene",appearance));
    assert(!Find(map,"Velora Beta",8,"Elene",appearance));
    assert(!Find(map,"Velora Beta",7,"Renamed",appearance));
    uint8_t changed[8]; std::memcpy(changed,appearance,8); ++changed[3];
    assert(!Find(map,"Velora Beta",7,"Elene",changed));
    int calls=0;
    auto factory=[&](const char* path,uint32_t flags)->void* {
        ++calls; assert(flags==42);
        return std::strcmp(path,"stock.m2") ? nullptr : reinterpret_cast<void*>(1);
    };
    assert(Create(selected,"stock.m2",42,factory)==reinterpret_cast<void*>(1) && calls==2);
    calls=0;
    assert(Create(nullptr,"stock.m2",42,factory)==reinterpret_cast<void*>(1) && calls==1);
    calls=0;
    assert(Create(resident,"stock.m2",42,[&](const char* path,uint32_t flags)->void* {
        ++calls; assert(flags==42 && !std::strcmp(path,resident->model)); return reinterpret_cast<void*>(2);
    })==reinterpret_cast<void*>(2) && calls==1);
    assert(ModelPath("Creature\\Tifa\\Tifa.m2"));
    assert(ModelPath("Character\\Velora\\Person.m2"));
    for (const char* path : {"Creature\\..\\bad.m2", "Creature\\.\\bad.m2",
         "Creature\\\\bad.m2", "Creature/Tifa/Tifa.m2", "Creature\\C:\\bad.m2",
         "Creature\\Tifa\\Tifa.M2", "World\\Tifa\\Tifa.m2", "creature\\Tifa.m2"})
        assert(!ModelPath(path));
    const Entry character=*selected;
    auto creatureBytes=bytes;
    std::memset(creatureBytes.data()+12+188,0,260);
    std::strcpy(reinterpret_cast<char*>(creatureBytes.data()+12+188),"Creature\\Tifa\\Tifa.m2");
    assert(Parse(creatureBytes.data(),creatureBytes.size(),map) && CreaturePath(map[0].model));
    InstanceRegistry roots;
    assert(!roots.Add(0) && !roots.Contains(0));
    Entry creature=character; std::strcpy(creature.model,"Creature\\Tifa\\Tifa.m2");
    auto mapped=[&](const char*,uint32_t)->void* { return reinterpret_cast<void*>(23); };
    assert(Create(&creature,"stock.m2",42,mapped,&roots)==reinterpret_cast<void*>(23));
    assert(roots.count==1 && roots.Contains(23) && !roots.Contains(24));
    assert(roots.Add(23) && roots.count==1); // cached selection does not clear or duplicate
    roots.Retire(24); assert(roots.Contains(23)); // attachments / other rows remain native
    roots.Retire(23); assert(!roots.Contains(23) && roots.count==0); // free before reuse
    assert(roots.Add(23)); roots.Retire(23); // world ownership handoff
    assert(!roots.Contains(23));
    calls=0;
    assert(Create(&creature,"stock.m2",42,factory,&roots)==reinterpret_cast<void*>(1) && calls==2);
    assert(roots.count==0); // null mapped result: stock fallback is never registered
    assert(Create(&character,"stock.m2",42,mapped,&roots)==reinterpret_cast<void*>(23));
    assert(roots.count==0); // Character rows remain native
    for (uintptr_t i=1; i<=kMaximum; ++i) assert(roots.Add(i));
    assert(!roots.Add(kMaximum+1));
    calls=0;
    assert(Create(&creature,"stock.m2",42,factory,&roots)==reinterpret_cast<void*>(1) && calls==1);
    for (uintptr_t i=1; i<=kMaximum; ++i) roots.Retire(i);
    assert(roots.count==0);
    auto bad=bytes; bad.push_back(0); assert(!Parse(bad.data(),bad.size(),map) && map.empty());
    assert(!Parse(bytes.data(),bytes.size()-1,map));
    bad=bytes; bad[0]=0; assert(!Parse(bad.data(),bad.size(),map));
    bad=bytes; bad[8]=51; assert(!Parse(bad.data(),bad.size(),map));
    bad=bytes; std::memcpy(bad.data()+12+kRecordSize,bad.data()+12,kRecordSize);
    assert(!Parse(bad.data(),bad.size(),map));
    bad=bytes; std::memset(bad.data()+12+12,'x',48); assert(!Parse(bad.data(),bad.size(),map));
    bad=bytes; std::memcpy(bad.data()+12+188,"Character\\..\\bad.m2",20);
    assert(!Parse(bad.data(),bad.size(),map));
    bad=bytes; bad[12+448+2]=2; assert(!Parse(bad.data(),bad.size(),map));
    assert(!Parse(nullptr,0,map));
    assert(Parse(bytes.data(),12,map)==false); // nonzero count, no rows
    bad.assign(bytes.begin(),bytes.begin()+12); std::memset(bad.data()+8,0,4);
    assert(Parse(bad.data(),bad.size(),map) && map.empty());
    std::puts("select-screen model map: passed");
}
