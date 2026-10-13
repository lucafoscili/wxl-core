// Execute production registry/picking lifetime code with synthetic native objects.
// No WoW process, installation or client API call. GPL-3.0-or-later.
#include "client/CM2Shared/WideIndices.cpp"
#include <cstdio>

namespace fixture
{
    int checks = 0, failures = 0, fullWarnings = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
    }
    struct Model
    {
        alignas(void*) uint8_t bytes[0x200] = {};
        void* Raw() { return bytes; }
        void Skin(M2SkinProfile* skin) { *At<M2SkinProfile*>(bytes, off::kOffModelSkin) = skin; }
    };
    struct Instance
    {
        alignas(void*) uint8_t bytes[0x40] = {};
        void* Raw() { return bytes; }
        void Model(void* model) { *At<void*>(bytes, off::kOffInstModel) = model; }
    };
    struct Skin
    {
        M2SkinProfile profile = {};
        M2SkinSection section = {};
        uint16_t indices[3] = {0, 1, 2}, lookup[3] = {0, 1, 2};
        Skin()
        {
            profile.vertexCount = 0x10001;
            profile.indexCount = 3;
            profile.indices = indices;
            profile.vertexLookup = lookup;
            profile.submeshes = &section;
            profile.submeshCount = 1;
        }
    };

    bool Retired(const WideSkinNote& n)
    {
        return !n.skin && !n.indices && !n.indexCount && !n.vertexCount && !n.model
               && !n.convertedSharedIb && !n.refilledSharedIb && !n.refilledSharedVb
               && !n.rejectedSharedIb && !n.pickingCall && !n.pickingLogged
               && !n.pickingSource.header && !n.pickingSource.vertices
               && !n.pickingSource.vertexCount && !n.pickingSource.sections
               && !n.pickingSource.sectionCount && !n.pickingSource.lookup;
    }
    void Populate(WideSkinNote& n, void* model, PickingCall* call)
    {
        n.model = model;
        n.convertedSharedIb = reinterpret_cast<void*>(0x1010);
        n.refilledSharedIb = reinterpret_cast<void*>(0x2020);
        n.refilledSharedVb = reinterpret_cast<void*>(0x3030);
        n.rejectedSharedIb = reinterpret_cast<void*>(0x4040);
        n.pickingSource = { reinterpret_cast<M2Header*>(0x5050), 0x6060, 0x10001,
                            n.skin->submeshes, 1, n.skin->vertexLookup };
        n.pickingCall = call;
        n.pickingLogged = 0xFFFFFFFFu;
    }

    void* expectedModel = nullptr;
    void* expectedEdx = reinterpret_cast<void*>(0x1234);
    WideSkinNote* expectedRetired[3] = {};
    uint64_t expectedGeneration[3] = {};
    size_t retiredCount = 0;
    unsigned forwards = 0;
    void __fastcall ReleaseNext(void* model, void* edx)
    {
        ++forwards;
        Check(model == expectedModel && edx == expectedEdx, "release preserves native arguments");
        for (size_t i = 0; i < retiredCount; ++i)
        {
            Check(Retired(*expectedRetired[i]), "release next link observes every field retired");
            Check(expectedRetired[i]->pickingGeneration == expectedGeneration[i] + 1,
                  "release next link observes invalidated generation");
        }
        // The native next link can free the skin: all retirement must precede this call.
        *At<M2SkinProfile*>(model, off::kOffModelSkin) = nullptr;
    }
    void Release(Model& model, WideSkinNote* first, WideSkinNote* second = nullptr)
    {
        expectedModel = model.Raw();
        expectedRetired[0] = first;
        expectedRetired[1] = second;
        retiredCount = second ? 2 : 1;
        for (size_t i = 0; i < retiredCount; ++i)
            expectedGeneration[i] = expectedRetired[i]->pickingGeneration;
        const unsigned before = forwards;
        hkSharedDestroy(model.Raw(), expectedEdx);
        Check(forwards == before + 1, "release forwards exactly once");
    }
    void Reset()
    {
        g_wideSkins = {};
        g_wideSkinCount = 0;
        g_origSharedDestroy = &ReleaseNext;
    }

    void CapacityAndReuse()
    {
        Reset();
        Skin skins[kMaxWideSkins + 1];
        WideSkinNote* notes[kMaxWideSkins];
        for (size_t i = 0; i < kMaxWideSkins; ++i)
        {
            notes[i] = NoteWideSkin(&skins[i].profile);
            Check(notes[i] == &g_wideSkins[i], "initial registry slots are stable");
        }
        Check(NoteWideSkin(&skins[64].profile) == nullptr && g_wideSkinCount == 64,
              "64 simultaneous live notes reject overflow");
        Check(fullWarnings == 1, "full registry emits the retained warning");
        Check(NoteWideSkin(&skins[64].profile) == nullptr && fullWarnings == 1,
              "full registry warning remains bounded");

        Model model;
        model.Skin(&skins[17].profile);
        PickingCall active = {};
        // Model-null index-only notes must retire through the live skin comparison.
        Populate(*notes[17], nullptr, &active);
        const uint64_t oldGeneration = notes[17]->pickingGeneration;
        PickingCall stale = {};
        stale.note = notes[17];
        stale.skin = notes[17]->skin;
        stale.generation = oldGeneration;
        stale.instance = reinterpret_cast<void*>(1); // must never be followed after invalidation
        Release(model, notes[17]);
        Check(g_wideSkinCount == 64, "retiring a middle slot keeps the high-water count");
        Check(!ConvertedBinding(reinterpret_cast<void*>(0x1010)), "retired buffer binding cannot be found");
        WideSkinNote* recycled = NoteWideSkin(&skins[64].profile);
        Check(recycled == notes[17] && recycled->pickingGeneration > oldGeneration,
              "middle slot is reused without moving notes or resetting generation");
        // A replacement conversion makes the stale converted check reach the dangerous live
        // instance read if its generation guard is ever removed or moved after that read.
        Populate(*recycled, nullptr, nullptr);
        Check(!CurrentLegacyCall(stale), "stale legacy generation rejects before instance dereference");
        Check(!CurrentPickingCall(stale), "stale converted generation rejects before instance dereference");
        for (size_t i = 0; i < kMaxWideSkins; ++i)
            Check(notes[i] == &g_wideSkins[i]
                  && notes[i]->skin == &skins[i == 17 ? 64 : i].profile,
                  "middle-slot reuse preserves all other notes");

        for (size_t i = kMaxWideSkins; i-- > 0;)
        {
            model.Skin(const_cast<M2SkinProfile*>(notes[i]->skin));
            Release(model, notes[i]);
            Check(g_wideSkinCount == i, "empty trailing slots trim the high-water count");
        }
        Check(g_wideSkinCount == 0, "fully retired registry is empty");
    }

    void SharedAndIndexOnlyRetirement()
    {
        Reset();
        Skin live, old, unrelated;
        Model model, other;
        model.Skin(&live.profile);
        other.Skin(&unrelated.profile);
        WideSkinNote* indexOnly = NoteWideSkin(&live.profile);
        WideSkinNote* shared = NoteWideSkin(&old.profile);
        WideSkinNote* keep = NoteWideSkin(&unrelated.profile);
        PickingCall a = {}, b = {}, c = {};
        Populate(*indexOnly, nullptr, &a);
        Populate(*shared, model.Raw(), &b);
        Populate(*keep, other.Raw(), &c);
        const uint64_t keepGeneration = keep->pickingGeneration;
        Release(model, indexOnly, shared);
        Check(keep->skin == &unrelated.profile && keep->model == other.Raw()
              && keep->convertedSharedIb == reinterpret_cast<void*>(0x1010)
              && keep->pickingCall == &c && keep->pickingGeneration == keepGeneration,
              "release preserves unrelated source, binding and picking state");
        Check(g_wideSkinCount == 3, "release never compacts a surviving borrowed note");
        Check(NoteWideSkin(&live.profile) == indexOnly, "first retired hole is discoverable");
        Check(NoteWideSkin(&old.profile) == shared, "second retired hole is discoverable");
    }

    void LifetimeChurn()
    {
        Reset();
        Skin skins[257];
        Model model;
        uint64_t previous = 0;
        for (auto& skin : skins)
        {
            model.Skin(&skin.profile);
            WideSkinNote* note = NoteWideSkin(&skin.profile);
            Check(note == &g_wideSkins[0] && note->pickingGeneration == previous,
                  "sequential lifetimes reuse trimmed slot with preserved generation");
            Release(model, note);
            previous = note->pickingGeneration;
            Check(g_wideSkinCount == 0, "sequential release recovers capacity");
        }
        Check(previous == 257 && fullWarnings == 1,
              "more than 64 lifetime skins never exhaust registry or reset generation");
    }

    enum class GeometryCase { Ordinary, ReleaseAndReuse, ReplacementOwner };
    GeometryCase geometryCase;
    int depth = 0;
    Model* geometryModel = nullptr;
    Skin* replacementSkin = nullptr;
    WideSkinNote* geometryNote = nullptr;
    PickingCall replacementOwner = {};
    int __fastcall GeometryNext(void* scene, void* edx, void* instance, int mode,
                                float* projection, float distance, float* point,
                                int candidate, float* bestDepth, int currentHit)
    {
        const int here = depth++;
        PickingCall* active = geometryNote->pickingCall;
        Check(active && active->scene == scene && active->instance == instance
              && active->mode == mode && active->projection == projection
              && active->distance == distance && active->point == point
              && active->candidate == candidate && active->bestDepth == bestDepth,
              "geometry next link sees its own scoped call and forwarded arguments");
        Check(active && CurrentLegacyCall(*active), "current legacy frame remains admitted");
        if (here == 0)
        {
            Check(RunPickingGeometry(*geometryNote, true, scene, edx, instance, mode,
                                     projection, distance, point, candidate, bestDepth, currentHit) == 101,
                  "nested geometry preserves native return value");
            Check(geometryNote->pickingCall == (geometryCase == GeometryCase::Ordinary ? active
                  : geometryCase == GeometryCase::ReplacementOwner ? &replacementOwner : nullptr),
                  "nested scope restores only a still-owned stack link");
        }
        else if (here == 1 && geometryCase != GeometryCase::Ordinary)
        {
            PickingCall stale = *active;
            Release(*geometryModel, geometryNote);
            geometryModel->Skin(&replacementSkin->profile);
            Check(NoteWideSkin(&replacementSkin->profile) == geometryNote,
                  "nested release reuses the borrowed note address");
            Check(!CurrentLegacyCall(stale) && !CurrentPickingCall(stale),
                  "nested release rejects both stale picking certificates");
            Check(!PickingFillCall(scene, instance, const_cast<M2SkinProfile*>(stale.skin)),
                  "retired geometry frame is absent from picking lookup");
            Check(!PickingTriangleCall(scene, point, mode, candidate, bestDepth),
                  "retired geometry frame is absent from triangle lookup");
            Check(RunPickingGeometry(*geometryNote, true, scene, edx, instance, mode,
                                     projection, distance, point, candidate, bestDepth, currentHit) == 102,
                  "recycled note supports a fresh nested picking scope");
            Check(!geometryNote->pickingCall, "fresh scope returns to the recycled null link");
            if (geometryCase == GeometryCase::ReplacementOwner)
                geometryNote->pickingCall = &replacementOwner;
        }
        --depth;
        return 100 + here;
    }
    void GeometryLifetime(GeometryCase scenario)
    {
        Reset();
        Skin old, replacement;
        Model model;
        Instance instance;
        model.Skin(&old.profile);
        instance.Model(model.Raw());
        geometryModel = &model;
        replacementSkin = &replacement;
        geometryNote = NoteWideSkin(&old.profile);
        geometryCase = scenario;
        depth = 0;
        g_wideSkins.geometryNext = &GeometryNext;
        float projection[16] = {}, point[3] = {}, bestDepth = 7.0f;
        Check(RunPickingGeometry(*geometryNote, true, &model, expectedEdx, instance.Raw(), 2,
                                 projection, 3.0f, point, 4, &bestDepth, 5) == 100,
              "outer geometry preserves native return value");
        Check(geometryNote->pickingCall == (scenario == GeometryCase::ReplacementOwner
              ? &replacementOwner : nullptr), "outer scope never resurrects retired stack links");
        geometryNote->pickingCall = nullptr;
    }
}

namespace wxl::hook
{
    void RegisterFeature(const char*, bool, bool (*)(), Phase) {}
    bool Install(const char*, void*, void*, void**, int) { return true; }
}
namespace wxl::log
{
    bool Enabled(Level) { return true; }
    void Write(Level level, const char* fmt, ...)
    {
        if (level == Level::Warn && std::strstr(fmt, "wide-skin registry full"))
            ++fixture::fullWarnings;
    }
    void Flush() {}
}

int main()
{
    static_assert(sizeof(void*) == 4, "native registry fixture requires Win32");
    fixture::CapacityAndReuse();
    fixture::SharedAndIndexOnlyRetirement();
    fixture::LifetimeChurn();
    fixture::GeometryLifetime(fixture::GeometryCase::Ordinary);
    fixture::GeometryLifetime(fixture::GeometryCase::ReleaseAndReuse);
    fixture::GeometryLifetime(fixture::GeometryCase::ReplacementOwner);
    std::printf("wide-skin registry: %d checks, %d failures\n", fixture::checks, fixture::failures);
    return fixture::failures ? 1 : 0;
}
