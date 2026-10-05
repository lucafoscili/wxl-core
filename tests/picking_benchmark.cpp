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
#include <cstdlib>
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
#define WLOG_WARN(...) ((void)0)
#define WLOG_INFO(...) ((void)0)
namespace wxl::log { void Flush() {} }
namespace full {
#include "dispatch.generated.hpp"
static_assert(sizeof(M2SkinProfile)==0x30 && offsetof(M2SkinProfile,submeshes)==0x20);
}
// Only the native allocator boundary is unavailable offline; warm runs must not reach it.
void __cdecl UnexpectedAllocation() { std::abort(); }
int __fastcall NoTriangles(void*,void*,uint16_t*,uint16_t*,int,float*,int,int,float*,int hit) {return hit;}
Native observedKernel=nullptr;
Fill observedFillEntries[3]={};unsigned observedFills=0,observedTriangles=0;
full::WideSkinNote* invalidateNote=nullptr;
full::PickingCall* reprepareCall=nullptr;
int prefixHit=0;float prefixDepth=0;
template<unsigned F> void __fastcall ObserveFill(void* scene,void* edx,void* instance,void* skin,
                                               void* section,int mode,float* projection,float distance) {
    ++observedFills;observedFillEntries[F](scene,edx,instance,skin,section,mode,projection,distance);
}
int __fastcall ObserveTriangles(void* scene,void* edx,uint16_t* begin,uint16_t* end,int base,
                                float* point,int mode,int candidate,float* depth,int hit) {
    ++observedTriangles;int result=observedKernel(scene,edx,begin,end,base,point,mode,candidate,depth,hit);
    if(observedTriangles==1) {
        prefixHit=result;prefixDepth=*depth;
        if(invalidateNote)++invalidateNote->pickingGeneration;
        if(reprepareCall)++reprepareCall->prepareEpoch;
    }
    return result;
}
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
    const bool dispatchOnly=std::string(argv[2])=="--dispatch";
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
    auto geometryBytes=read(std::string(argv[1])+"/geometry.bin");
    assert(geometryBytes.size()==0x25b && geometryBytes[0x258]=='\xc2');
    auto* geometry=(char*)VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(geometry);std::memcpy(geometry,geometryBytes.data(),geometryBytes.size());
    auto redirect=[&](uint32_t address,uintptr_t target) {
        unsigned offset=address-0x81daf0;assert(geometry[offset]=='\xe8');
        int32_t relative=(int32_t)(target-(uintptr_t)(geometry+offset+5));
        std::memcpy(geometry+offset+1,&relative,4);
    };
    redirect(0x81dc23,(uintptr_t)UnexpectedAllocation);redirect(0x81dc88,(uintptr_t)UnexpectedAllocation);
    redirect(0x81dc9f,(uintptr_t)UnexpectedAllocation);
    redirect(0x81dccf,(uintptr_t)full::hkFillPickingVertices<2>);
    redirect(0x81dcdf,(uintptr_t)full::hkFillPickingVertices<0>);
    redirect(0x81dce6,(uintptr_t)full::hkFillPickingVertices<1>);
    redirect(0x81dd19,(uintptr_t)full::hkSceneTriangleHitTest);
    uint32_t cpuFlags=4,oldCpuAddress=0xd3fcec,cpuAddress=(uint32_t)&cpuFlags,cpuRelocations=0;
    for(unsigned i=0;i<geometryBytes.size()-3;++i)if(std::memcmp(geometry+i,&oldCpuAddress,4)==0) {
        std::memcpy(geometry+i,&cpuAddress,4);++cpuRelocations;
    }
    assert(cpuRelocations==1);
    full::g_wideSkins.geometryNext=(off::M2_HitTestGeometryFn)geometry;
    full::g_wideSkins.fillNext[0]=fill;
    full::g_wideSkins.fillNext[1]=(Fill)(fillerCode+0x570);
    full::g_wideSkins.fillNext[2]=(Fill)(fillerCode+0x700);
    float special[9]={0,0,1, 1,0,1, 0,1,1}, edge[2]={0.5f,0.5f};
    assert(PickingTriangleMayCover(special,0,1,2,edge));
    float outside[2]={2,2}; assert(!PickingTriangleMayCover(special,0,1,2,outside));
    special[0]=std::numeric_limits<float>::quiet_NaN();
    assert(PickingTriangleMayCover(special,0,1,2,outside));
    for(int file=dispatchOnly?3:2;file<argc;++file) {
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
        ((M2Header*)header)->vertices.count=(uint32_t)(vertices.size()/48);
        if(dispatchOnly) {
            auto sectionBytes=read(std::string(argv[1])+"/"+argv[file]+".sections");
            auto batchBytes=read(std::string(argv[1])+"/"+argv[file]+".batches");
            auto indexBytes=read(std::string(argv[1])+"/"+argv[file]+".indices");
            full::M2SkinProfile nativeSkin={};nativeSkin.vertexCount=(uint32_t)(lookup.size()/2);
            nativeSkin.vertexLookup=(uint16_t*)lookup.data();nativeSkin.indices=(uint16_t*)indexBytes.data();
            nativeSkin.indexCount=(uint32_t)(indexBytes.size()/2);
            nativeSkin.submeshes=(M2SkinSection*)sectionBytes.data();nativeSkin.submeshCount=(uint32_t)(sectionBytes.size()/48);
            nativeSkin.batches=(wxl::structure::m2::M2Batch*)batchBytes.data();nativeSkin.batchCount=(uint32_t)(batchBytes.size()/24);
            *(void**)(model+0x170)=&nativeSkin;
            *(void**)(model+off::kOffSharedIndexBuf)=model; // stable live IB identity, never followed
            std::vector<uint32_t> visible(nativeSkin.submeshCount,1),materials(65536,0);
            std::vector<uint16_t> weights(65536,0);float alpha[3]={1,1,1};
            *(void**)(instance+0x9c)=visible.data();*(float*)(instance+0x19c)=1;
            *(void**)(instance+0xa8)=alpha;*(void**)(header+0x94)=weights.data();
            *(void**)(header+0x74)=materials.data();
            // View-space camera rotation: model Z maps to screen Y, depth = 10 - model Y.
            for(unsigned b=0;b<256;++b) {
                float* m=palette+b*16;m[5]=m[10]=0;m[6]=-1;m[9]=1;m[14]=10;
            }
            float projection[3]={0,0,1},point[2]={(minx+maxx)*0.5f,(miny+maxy)*0.5f};
            double fullTime[2]={},withoutTriangles=0,topologyTime=0;volatile int sink=0;
            for(unsigned registry=0;registry<2;++registry) {
                full::g_wideSkins={};full::g_wideSkinCount=registry?64:1;
                auto& note=full::g_wideSkins[full::g_wideSkinCount-1];
                note.skin=&nativeSkin;note.indices=nativeSkin.indices;note.indexCount=nativeSkin.indexCount;
                note.vertexCount=nativeSkin.vertexCount;
                if(nativeSkin.vertexCount>65536) {
                    note.model=model;note.convertedSharedIb=model;note.pickingGeneration=1;
                    note.pickingSource={(M2Header*)header,(uint32_t)vertices.data(),(uint32_t)(vertices.size()/48),
                                        nativeSkin.submeshes,nativeSkin.submeshCount,nativeSkin.vertexLookup};
                    assert(full::PickingNote(instance)==&note && full::CurrentPickingSource(note,instance));
                }
                full::g_wideSkins.geometryNext=(off::M2_HitTestGeometryFn)geometry;
                full::g_wideSkins.fillNext[0]=fill;full::g_wideSkins.fillNext[1]=(Fill)(fillerCode+0x570);
                full::g_wideSkins.fillNext[2]=(Fill)(fillerCode+0x700);
                full::g_origTriangleHitTest=native;
                auto invoke=[&]() {float depth=10000;return full::hkHitTestGeometry(&fillScene,nullptr,instance,0,
                                                       projection,0,point,1,&depth,0);};
                if(!registry) {
                    observedKernel=native;observedTriangles=observedFills=0;
                    observedFillEntries[0]=fill;observedFillEntries[1]=(Fill)(fillerCode+0x570);
                    observedFillEntries[2]=(Fill)(fillerCode+0x700);
                    full::g_wideSkins.fillNext[0]=ObserveFill<0>;full::g_wideSkins.fillNext[1]=ObserveFill<1>;
                    full::g_wideSkins.fillNext[2]=ObserveFill<2>;full::g_origTriangleHitTest=ObserveTriangles;
                    auto noHit=[&]() {
                        observedTriangles=observedFills=0;float depth=0.25f;
                        int hit=full::hkHitTestGeometry(&fillScene,nullptr,instance,0,projection,0,point,7,&depth,3);
                        assert(hit==3 && depth==0.25f && !observedTriangles && !observedFills && !note.pickingCall);
                    };
                    // Actual native filters: hidden geometry, disabled global alpha, no-pick,
                    // secondary material layer and material filtering all avoid both kernels.
                    std::fill(visible.begin(),visible.end(),0);noHit();std::fill(visible.begin(),visible.end(),1);
                    *(float*)(instance+0x19c)=0;noHit();*(float*)(instance+0x19c)=1;
                    auto batchOriginal=batchBytes;
                    for(unsigned b=0;b<nativeSkin.batchCount;++b)nativeSkin.batches[b].flags|=8;
                    noHit();std::memcpy(batchBytes.data(),batchOriginal.data(),batchBytes.size());
                    for(unsigned b=0;b<nativeSkin.batchCount;++b)nativeSkin.batches[b].materialLayer=1;
                    noHit();std::memcpy(batchBytes.data(),batchOriginal.data(),batchBytes.size());
                    *(uint32_t*)(instance+0x2d4)=2;noHit();*(uint32_t*)(instance+0x2d4)=0;
                    observedTriangles=observedFills=0;invoke();assert(observedTriangles && !note.pickingCall);
                    if(nativeSkin.vertexCount>65536) {
                        // A rebuild during the first chunk keeps exactly the genuine prefix.
                        observedTriangles=0;invalidateNote=&note;float depth=10000;
                        int hit=full::hkHitTestGeometry(&fillScene,nullptr,instance,0,projection,0,point,1,&depth,0);
                        assert(observedTriangles==1 && hit==prefixHit && std::memcmp(&depth,&prefixDepth,4)==0);
                        assert(!note.pickingCall);invalidateNote=nullptr;
                        // A re-preparation epoch stops this section before a later chunk.
                        full::PickingCall call={&note,&fillScene,instance,&nativeSkin,note.pickingSource,note.pickingGeneration,
                                                 0,projection,0,point,1,&depth};
                        for(unsigned s=0;s<nativeSkin.submeshCount;++s)if(nativeSkin.submeshes[s].indexCount>3072) {
                            call.section=nativeSkin.submeshes[s];call.sectionIndex=s;
                            call.triangleStart=full::TriangleStart(call.section,nativeSkin);break;
                        }
                        assert(call.section.indexCount>3072);depth=10000;observedTriangles=0;reprepareCall=&call;
                        hit=full::TestPickingSection(call,nullptr,point,0,1,&depth,0);
                        assert(observedTriangles==1 && hit==prefixHit && std::memcmp(&depth,&prefixDepth,4)==0);
                        reprepareCall=nullptr;
                    }
                    full::g_wideSkins.fillNext[0]=fill;full::g_wideSkins.fillNext[1]=(Fill)(fillerCode+0x570);
                    full::g_wideSkins.fillNext[2]=(Fill)(fillerCode+0x700);full::g_origTriangleHitTest=native;
                }
                invoke();auto started=std::chrono::steady_clock::now();
                for(unsigned repeat=0;repeat<200;++repeat)sink+=invoke();
                fullTime[registry]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/200;
                if(!registry) {
                    full::g_origTriangleHitTest=NoTriangles;started=std::chrono::steady_clock::now();
                    for(unsigned repeat=0;repeat<200;++repeat)sink+=invoke();
                    withoutTriangles=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/200;
                    if(nativeSkin.vertexCount>65536) {
                        full::PickingCall call={&note,&fillScene,instance,&nativeSkin,note.pickingSource,note.pickingGeneration,
                                                 0,projection,0,point,1,nullptr};
                        float depth=10000;call.bestDepth=&depth;
                        started=std::chrono::steady_clock::now();
                        for(unsigned repeat=0;repeat<200;++repeat)for(unsigned s=0;s<nativeSkin.submeshCount;++s) {
                            call.section=nativeSkin.submeshes[s];call.sectionIndex=s;
                            call.triangleStart=full::TriangleStart(call.section,nativeSkin);
                            sink+=full::TestPickingSection(call,nullptr,point,0,1,&depth,0);
                        }
                        topologyTime=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/200;
                    }
                }
            }
            std::printf("%s fullRegistry1=%.4fms fullRegistry64=%.4fms noTriangleKernel=%.4fms topologyValidationRebaseCurrency=%.4fms batches=%u sections=%u\n",
                        argv[file],fullTime[0],fullTime[1],withoutTriangles,topologyTime,nativeSkin.batchCount,nativeSkin.submeshCount);
            continue;
        }
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
