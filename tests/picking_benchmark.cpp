// Offline Win32 fixture: captured client fillers/predicate + generated production fill hook.
#include "client/CM2Shared/VertexWindow.hpp"
#include "engine/assets/shared/models/m2/M2Format.hpp"
#include "offsets/game/M2.hpp"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
namespace window = wxl::client::m2::window;
using Native = int(__fastcall*)(void*,void*,uint16_t*,uint16_t*,int,float*,int,int,float*,int);
using Fill = void(__fastcall*)(void*,void*,void*,void*,void*,int,float*,float);
namespace off = wxl::offsets::game::m2;
using wxl::structure::m2::M2Header;
using wxl::structure::m2::M2SkinSection;
char* nativeFillers;
namespace wxl::game {
template<class T> T Native(uintptr_t address) {
    return reinterpret_cast<T>(nativeFillers + (address==off::kVec3Transform ? 0x1000 : address-0x81d2c0));
}}
template<class T> T* At(void* base,size_t offset) {return reinterpret_cast<T*>((char*)base+offset);}
// Boundary doubles only. Every production fill/preparation body below is generated verbatim.
struct WideSkinNote {};
struct M2SkinProfile { uint16_t* indices;uint32_t indexCount,vertexCount;
    M2SkinSection* submeshes;uint32_t submeshCount; };
#include "types.generated.hpp"
#include "refill.generated.hpp"
bool certificateCurrent=true;
bool CurrentPickingCall(const PickingCall&) {return certificateCurrent;}
bool CurrentLegacyCall(const PickingCall&) {return certificateCurrent;}
#include "prepare.generated.hpp"
PickingCall* activeCall=nullptr;
PickingCall* PickingFillCall(void* scene,void* instance,void* skin) {
    return activeCall && activeCall->scene==scene && activeCall->instance==instance
        && activeCall->skin==skin ? activeCall:nullptr;
}
struct FillChain { Fill fillNext[3]; } g_wideSkins;
#include "fill-hook.generated.hpp"
unsigned nativeCalls=0;
Fill forwardedNative=nullptr;void* forwardedSkin=nullptr;
bool executeForward=true;
void* observedSection;int observedMode;float* observedProjection;float observedDistance;
void __fastcall CountNative(void* scene,void* edx,void* instance,void*,void* section,
                           int mode,float* projection,float distance) {
    assert(!activeCall || !activeCall->pending);++nativeCalls;
    observedSection=section;observedMode=mode;observedProjection=projection;observedDistance=distance;
    if(executeForward)forwardedNative(scene,edx,instance,forwardedSkin,section,mode,projection,distance);
}
    // A conservative broad phase over the positions the native filler just produced. Native
    // barycentric/depth arbitration still tests every possible hit. Keep nonfinite inputs and
    // a rounding margin (32 float ulps at this coordinate scale) on the native path.
    inline bool PickingTriangleMayCover(const float* positions, uint16_t a, uint16_t b,
                                         uint16_t c, const float* point)
    {
        for (uint32_t axis = 0; axis < 2; ++axis)
        {
            const float x = positions[size_t(a) * 3 + axis];
            const float y = positions[size_t(b) * 3 + axis];
            const float z = positions[size_t(c) * 3 + axis];
            const float p = point[axis];
            const float margin = (std::fabs(p) + std::fabs(x) + std::fabs(y)
                                  + std::fabs(z) + 1.0f) * 0.000003814697265625f;
            if ((x < p - margin && y < p - margin && z < p - margin)
                || (x > p + margin && y > p + margin && z > p + margin))
            {
                // Infinite/NaN sums cannot pass the comparisons above; still explicitly keep
                // nonfinite coordinates in the other axis on the native path.
                return !std::isfinite(positions[size_t(a) * 3 + (1 - axis)])
                      || !std::isfinite(positions[size_t(b) * 3 + (1 - axis)])
                      || !std::isfinite(positions[size_t(c) * 3 + (1 - axis)]);
            }
        }
        return true;
    }


std::vector<char> read(const std::string& path) {
    std::ifstream in(path, std::ios::binary); assert(in);
    return {std::istreambuf_iterator<char>(in), {}};
}
struct Section { uint32_t id, first, influences; std::vector<float> points; std::vector<uint16_t> indices; };
struct Scene { char pad[0x124] = {}; float* points;uint32_t capacity=65536; };
struct Result { int hit; float depth; size_t submitted; };
Result run(Native native, std::vector<Section>& sections, float* point, bool filtered, bool hidden) {
    Result r{0,10000.0f,0}; Scene scene;
    uint16_t local[window::kPickingIndexChunk];
    for (auto& s: sections) {
        if (hidden && s.id != 0) continue;
        scene.points = s.points.data();
        for (size_t done=0; done<s.indices.size();) {
            unsigned take=window::PickingChunk(unsigned(s.indices.size()-done)), count=0;
            for (unsigned k=0;k<take;k+=3) {
                auto* t=s.indices.data()+done+k;
                if (!filtered || PickingTriangleMayCover(scene.points,t[0],t[1],t[2],point)) {
                    for (unsigned j=0;j<3;++j) local[count++]=t[j];
                }
            }
            if(count) r.hit=native(&scene,nullptr,local,local+count,0,point,0,1,&r.depth,r.hit);
            r.submitted+=count/3; done+=take;
        }
    }
    return r;
}
int main(int argc,char** argv) {
    assert(argc>=3 && sizeof(void*)==4);
    auto bytes=read(std::string(argv[1])+"/native.bin"); assert(bytes.size()==0x16c);
    float epsilon; std::memcpy(&epsilon,bytes.data()+0x168,4);
    auto* executable=(char*)VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(executable); std::memcpy(executable,bytes.data(),0x168);
    uint32_t old=0x9ea558, replacement=(uint32_t)&epsilon; unsigned relocated=0;
    for(unsigned i=0;i<0x165;++i) if(std::memcmp(executable+i,&old,4)==0) {
        std::memcpy(executable+i,&replacement,4); ++relocated;
    }
    assert(relocated==1); Native native=(Native)executable;
    auto fills=read(std::string(argv[1])+"/fillers.bin"),transform=read(std::string(argv[1])+"/transform.bin");
    auto* fillerCode=(char*)VirtualAlloc(nullptr,0x2000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(fillerCode && fills.size()==0x830 && transform.size()==0x5d);
    std::memcpy(fillerCode,fills.data(),fills.size());std::memcpy(fillerCode+0x1000,transform.data(),transform.size());
    auto scaleBytes=read(std::string(argv[1])+"/blend-scale.bin");float scale;
    assert(scaleBytes.size()==4);std::memcpy(&scale,scaleBytes.data(),4);
    uint32_t scaleAddress=(uint32_t)&scale,originalScale=0xa45564;unsigned scaleRelocations=0;
    for(unsigned i=0;i<fills.size()-3;++i)if(std::memcmp(fillerCode+i,&originalScale,4)==0) {
        std::memcpy(fillerCode+i,&scaleAddress,4);++scaleRelocations;
    }
    assert(scaleRelocations==2);
    for(uint32_t address: {0x81d75c,0x81d8f7,0x81da2c}) {
        unsigned offset=address-0x81d2c0;assert(fillerCode[offset]=='\xe8');
        int32_t target=0x1000-offset-5;std::memcpy(fillerCode+offset+1,&target,4);
    }
    Fill fill=(Fill)(fillerCode+0x81d680-0x81d2c0);
    nativeFillers=fillerCode;
    float special[9]={0,0,1, 1,0,1, 0,1,1}, edge[2]={0.5f,0.5f};
    assert(PickingTriangleMayCover(special,0,1,2,edge));
    float outside[2]={2,2}; assert(!PickingTriangleMayCover(special,0,1,2,outside));
    special[0]=std::numeric_limits<float>::quiet_NaN();
    assert(PickingTriangleMayCover(special,0,1,2,outside));
    for(int file=2;file<argc;++file) {
        auto blob=read(std::string(argv[1])+"/"+argv[file]+".bin"); size_t at=0;
        auto u32=[&](){uint32_t v;std::memcpy(&v,blob.data()+at,4);at+=4;return v;};
        unsigned n=u32(); std::vector<Section> sections;
        float minx=1e30f,miny=1e30f,maxx=-1e30f,maxy=-1e30f;
        for(unsigned i=0;i<n;++i) {
            Section s; s.id=u32();s.first=u32();unsigned nv=u32(),ni=u32();s.influences=u32();
            s.points.resize(nv*3);s.indices.resize(ni);
            std::memcpy(s.points.data(),blob.data()+at,nv*12);at+=nv*12;
            std::memcpy(s.indices.data(),blob.data()+at,ni*2);at+=ni*2;
            // View a character in front: projected coordinates (model X,Z); positive ray depth.
            for(unsigned k=0;k<nv;++k) {auto* p=s.points.data()+k*3;
                float y=p[1];p[1]=p[2];p[2]=y+10;
                minx=std::min(minx,p[0]);maxx=std::max(maxx,p[0]);
                miny=std::min(miny,p[1]);maxy=std::max(maxy,p[1]);}
            sections.push_back(std::move(s));
        }
        assert(at==blob.size()); size_t submitted=0,total=0,checks=0;
        auto vertices=read(std::string(argv[1])+"/"+argv[file]+".vertices");
        auto lookup=read(std::string(argv[1])+"/"+argv[file]+".lookup");
        char model[0x200]={},header[0x200]={},skin[0x100]={},instance[0x400]={};
        *(void**)(instance+0x2c)=model;*(void**)(model+0x150)=header;
        *(void**)(header+0x40)=vertices.data();*(void**)(skin+8)=lookup.data();
        alignas(16) float palette[256*16]={};
        auto setPalette=[&](bool animated) {
            std::memset(palette,0,sizeof(palette));
            for(unsigned b=0;b<256;++b) {
                float* m=palette+b*16;
                if(!animated) {for(unsigned j=0;j<4;++j)m[j*5]=1;continue;}
                // Vary bone translations and compose noncommuting X/Z rotations.
                float x=0.13f+float(b%7)*0.017f,z=-0.21f+float(b%11)*0.023f;
                float cx=std::cos(x),sx=std::sin(x),cz=std::cos(z),sz=std::sin(z);
                m[0]=cz;m[1]=sz*cx;m[2]=sz*sx;
                m[4]=-sz;m[5]=cz*cx;m[6]=cz*sx;
                m[9]=-sx;m[10]=cx;m[15]=1;
                m[12]=float(b%5)*0.019f;m[13]=-float(b%13)*0.011f;m[14]=0.07f;
            }
        };
        setPalette(false);
        *(void**)(instance+0x98)=palette;
        std::vector<float> scratch(65536*3);Scene fillScene;fillScene.points=scratch.data();
        ((M2Header*)header)->bones.count=256;
        float projection[3]={0,1,0};double fillTime=0,denseTime=0;unsigned upper=0;
        auto fillStart=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<100;++repeat)for(auto& s:sections) {
            uint16_t section[24]={};section[2]=(uint16_t)s.first;
            section[3]=(uint16_t)(s.points.size()/3);section[8]=(uint16_t)s.influences;
            auto chosen=s.influences==1?(Fill)(fillerCode+0x81d9c0-0x81d2c0):fill;
            chosen(&fillScene,nullptr,instance,skin,section,0,projection,0);
            if(repeat==0 && window::NeedsPickingPositions(s.first,section[3])) upper+=section[3];
        }
        fillTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fillStart).count()/100;
        fillStart=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<100;++repeat)for(auto& s:sections) {
            PickingCall call={};call.section.vertexStart=(uint16_t)s.first;
            call.section.vertexCount=(uint16_t)(s.points.size()/3);call.first=s.first;
            call.scene=&fillScene;call.instance=instance;call.filler=s.influences==1?2:0;
            call.source={ (M2Header*)header,(uint32_t)vertices.data() };
            call.projection=projection;
            if(window::NeedsPickingPositions(s.first,call.section.vertexCount))assert(RefillPickingPositions(call));
        }
        denseTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fillStart).count()/100;
        double composed[2]={};unsigned fillChecks=0;
        std::vector<float> expected(scratch.size());
        std::vector<M2SkinSection> placements(sections.size());uint32_t triangleCount=0;
        for(unsigned i=0;i<sections.size();++i) {
            auto& sec=placements[i];auto& s=sections[i];sec.vertexStart=(uint16_t)s.first;
            sec.vertexCount=(uint16_t)(s.points.size()/3);sec.indexStart=(uint16_t)triangleCount;
            sec.level=(uint16_t)(triangleCount>>16);sec.indexCount=(uint16_t)s.indices.size();
            sec.boneInfluences=(uint16_t)s.influences;triangleCount+=sec.indexCount;
        }
        M2SkinProfile profile={nullptr,triangleCount,(uint32_t)(lookup.size()/2),placements.data(),(uint32_t)placements.size()};
        for(auto& next:g_wideSkins.fillNext)next=CountNative;
        for(unsigned pose=0;pose<2;++pose) {
        setPalette(pose!=0);
        for(unsigned mode=0;mode<2;++mode)for(unsigned filler=0;filler<2;++filler)for(auto& s:sections) {
            PickingCall call={};call.section.vertexStart=(uint16_t)s.first;
            call.section.vertexCount=(uint16_t)(s.points.size()/3);call.first=s.first;
            call.scene=&fillScene;call.instance=instance;call.filler=s.influences==1?2:filler;
            call.source={(M2Header*)header,(uint32_t)vertices.data()};call.projection=projection;call.mode=mode;
            uint16_t sec[24]={};sec[2]=call.section.vertexStart;sec[3]=call.section.vertexCount;sec[8]=(uint16_t)s.influences;
            auto chosen=call.filler==2?(Fill)(fillerCode+0x700):
                call.filler==1?(Fill)(fillerCode+0x570):fill;
            chosen(&fillScene,nullptr,instance,skin,sec,mode,projection,0);
            bool high=window::NeedsPickingPositions(call.first,call.section.vertexCount);
            if(high)assert(RefillPickingPositions(call));
            std::memcpy(expected.data(),scratch.data(),call.section.vertexCount*12);
            if(high)assert(RefillPickingPositions(call));
            else chosen(&fillScene,nullptr,instance,skin,sec,mode,projection,0);
            assert(std::memcmp(expected.data(),scratch.data(),call.section.vertexCount*12)==0);++fillChecks;
            // Execute the actual production hook/preparation/refill, redirecting only native
            // boundaries and the certificate predicate. Low and upper are both real sections.
            unsigned slot=(unsigned)(&s-sections.data());call.skin=&profile;
            call.legacy=profile.vertexCount<=65536;
            call.source.vertexCount=(uint32_t)(vertices.size()/48);
            call.source.sections=placements.data();call.source.sectionCount=(uint32_t)placements.size();
            activeCall=&call;forwardedNative=chosen;forwardedSkin=skin;
            unsigned previous=nativeCalls;
            if(call.filler==2)hkFillPickingVertices<2>(&fillScene,nullptr,instance,&profile,&placements[slot],mode,projection,0);
            else if(call.filler==1)hkFillPickingVertices<1>(&fillScene,nullptr,instance,&profile,&placements[slot],mode,projection,0);
            else hkFillPickingVertices<0>(&fillScene,nullptr,instance,&profile,&placements[slot],mode,projection,0);
            assert(nativeCalls-previous==(high?0u:1u));assert(call.pending && call.identified);
            if(!call.legacy)assert(call.ready);
            assert(std::memcmp(expected.data(),scratch.data(),call.section.vertexCount*12)==0);
            // Compare the unchanged native predicate after the old and new fill paths,
            // including a prior nearer hit. Both fill modes feed the actual captured kernel.
            for(unsigned prior=0;prior<2;++prior) {
                float point[2]={expected[0],expected[1]}, oldDepth=prior?0.25f:10000.0f;
                float newDepth=oldDepth;Scene oldScene;oldScene.points=expected.data();
                int oldHit=native(&oldScene,nullptr,s.indices.data(),s.indices.data()+s.indices.size(),
                                  0,point,0,1,&oldDepth,prior);
                int newHit=native(&fillScene,nullptr,s.indices.data(),s.indices.data()+s.indices.size(),
                                  0,point,0,1,&newDepth,prior);
                assert(oldHit==newHit && std::memcmp(&oldDepth,&newDepth,sizeof(float))==0);
            }
            if(high && call.filler==0) {
                executeForward=false;
                auto fallback=[&](void* section,int incomingMode,float* incomingProjection,float incomingDistance) {
                    unsigned before=nativeCalls;
                    hkFillPickingVertices<0>(&fillScene,nullptr,instance,&profile,section,incomingMode,incomingProjection,incomingDistance);
                    assert(nativeCalls==before+1 && observedSection==section && observedMode==incomingMode
                        && observedProjection==incomingProjection && observedDistance==incomingDistance);
                };
                call.legacy=true;fallback(&placements[slot],mode,projection,0);call.legacy=false;
                certificateCurrent=false;fallback(&placements[slot],mode,projection,0);certificateCurrent=true;
                M2SkinSection foreign=placements[slot];fallback(&foreign,mode,projection,0);
                fallback(&placements[slot],1-mode,projection,0);
                float foreignProjection[3]={0,1,0};fallback(&placements[slot],mode,foreignProjection,0);
                fallback(&placements[slot],mode,nullptr,0);
                fallback(&placements[slot],mode,projection,1);
                uint16_t savedCount=placements[slot].vertexCount;placements[slot].vertexCount=0;
                fallback(&placements[slot],mode,projection,0);placements[slot].vertexCount=savedCount;
                uint16_t savedIndices=placements[slot].indexCount;placements[slot].indexCount=1;
                fallback(&placements[slot],mode,projection,0);placements[slot].indexCount=savedIndices;
                fillScene.capacity=0;fallback(&placements[slot],mode,projection,0);fillScene.capacity=65536;
                *(void**)(instance+0x98)=nullptr;fallback(&placements[slot],mode,projection,0);
                *(void**)(instance+0x98)=(char*)palette+4;fallback(&placements[slot],mode,projection,0);
                *(void**)(instance+0x98)=palette;((M2Header*)header)->bones.count=0;
                fallback(&placements[slot],mode,projection,0);((M2Header*)header)->bones.count=256;
                executeForward=true;
            }
            activeCall=nullptr;
        }
        }
        setPalette(false); // Timings below deliberately keep the original identity-palette case.
        for(unsigned optimized=0;optimized<2;++optimized) {
            fillStart=std::chrono::steady_clock::now();
            for(unsigned repeat=0;repeat<100;++repeat)for(auto& s:sections) {
                PickingCall call={};call.section.vertexStart=(uint16_t)s.first;
                call.section.vertexCount=(uint16_t)(s.points.size()/3);call.first=s.first;
                call.scene=&fillScene;call.instance=instance;call.filler=s.influences==1?2:0;
                call.source={(M2Header*)header,(uint32_t)vertices.data()};call.projection=projection;
                uint16_t sec[24]={};sec[2]=call.section.vertexStart;sec[3]=call.section.vertexCount;
                sec[8]=(uint16_t)s.influences;
                auto chosen=call.filler==2?(Fill)(fillerCode+0x700):fill;
                bool high=window::NeedsPickingPositions(call.first,call.section.vertexCount);
                if(!optimized || !high)chosen(&fillScene,nullptr,instance,skin,sec,0,projection,0);
                if(high)assert(RefillPickingPositions(call));
            }
            composed[optimized]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fillStart).count()/100;
        }
        for(bool hidden: {false,true}) for(unsigned ix=0;ix<21;++ix) for(unsigned iy=0;iy<31;++iy) {
            float p[2]={minx+(maxx-minx)*(float(ix)-0.5f)/20,miny+(maxy-miny)*(float(iy)-0.5f)/30};
            auto before=run(native,sections,p,false,hidden),after=run(native,sections,p,true,hidden);
            assert(before.hit==after.hit && std::memcmp(&before.depth,&after.depth,4)==0);
            submitted+=after.submitted;total+=before.submitted;++checks;
        }
        double times[2]={};volatile int consume=0;
        for(unsigned filtered=0;filtered<2;++filtered) {
            auto start=std::chrono::steady_clock::now();
            for(unsigned repeat=0;repeat<8;++repeat) for(unsigned iy=0;iy<31;++iy) {
                float p[2]={(minx+maxx)*0.5f,miny+(maxy-miny)*(float(iy)+0.5f)/31};
                consume+=run(native,sections,p,filtered!=0,false).hit;
            }
            times[filtered]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/248;
        }
        std::printf("%s checks=%zu retained=%.3f%% triangle=%.4fms rejectedFilter=%.4fms nativeFill=%.4fms denseRefill=%.4fms duplicatedUpperVertices=%u fillChecks=%u composedFillBefore=%.4fms composedFillAfter=%.4fms\n",
            argv[file],checks,100.0*submitted/total,times[0],times[1],fillTime,denseTime,upper,fillChecks,composed[0],composed[1]);
    }
    VirtualFree(executable,0,MEM_RELEASE);
}
