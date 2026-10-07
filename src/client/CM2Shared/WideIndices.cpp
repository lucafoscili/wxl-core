// Reads a submesh's triangle start the way the skin format spells it once it passes 65535 indices.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

// A submesh's first triangle index is (level << 16) | indexStart. indexStart alone stops addressing
// at 65535, so that pairing is the only way a skin larger than that can say where a submesh begins.
// Both of the client's index-buffer fills read indexStart as a plain 16-bit field, so every submesh
// past the ceiling sources its triangles from the wrong place -- the geometry is in the file, the
// fill just looks 65536 indices too early.
//
// level also had an older life as a LOD / sub-batch marker, so the fold is conditional: it is taken
// only when the widened start plus the submesh's own index count still lands inside the triangle
// array the skin itself declares. A marker names an offset that array cannot contain, so it is
// rejected and the client's own 16-bit reading stands. Two consequences worth stating: a skin that
// is not writing an extended start keeps the engine fill unless the explicit dense shared-vertex
// conversion below applies, and no value of level can extend the folded source beyond its array.
//
// Picking follows the same split (see the picking section below): a skin with a current dense
// shared-window conversion gets repaired positions and bounded local triangle chunks; any other noted
// skin keeps the triangle-start remap (66a64d5) and the crossing-section skip (e9c68f6).

#include "common/Log.hpp"
#include "engine/assets/shared/models/m2/M2Format.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "game/Binding.hpp"
#include "game/Gx.hpp"
#include "game/M2.hpp"
#include "offsets/engine/Gx.hpp"
#include "offsets/game/M2.hpp"
#include "client/CM2Shared/VertexWindow.hpp"

#include <cstring>

namespace
{
    namespace off   = wxl::offsets::game::m2;
    namespace gxoff = wxl::offsets::engine::gx;
    namespace window = wxl::client::m2::window;

    using wxl::game::m2::M2SkinProfile;
    using wxl::structure::m2::M2Header;
    using wxl::structure::m2::M2SkinSection;

    off::M2_SetModelIndicesFn   g_origSetModelIndices  = nullptr;
    off::M2_SharedSetIndicesFn  g_origSharedSetIndices = nullptr;
    gxoff::GxDeviceDrawFn       g_origDeviceDraw       = nullptr;
    off::M2_SharedSetVerticesFn g_origSharedSetVertices = nullptr;
    using DrawBatchFn = void(__fastcall*)(void* ctx, void* edx);
    DrawBatchFn                 g_origDrawBatch        = nullptr;

    /// The submesh and skin of the batch currently between a draw-batch entry and its device draw.
    /// Null outside one, which is every draw that is not an M2's.
    const M2SkinSection* g_drawSection = nullptr;
    const M2SkinProfile* g_drawSkin    = nullptr;
    void* g_drawModel = nullptr;
    void* g_drawInstance = nullptr;

    // Temporary Beta address experiment: process-lifetime caps, no retained payloads.
    constexpr uint32_t kSharedRefillLogLimit = 16;
    constexpr uint32_t kInstanceRefillLogLimit = 16;
    constexpr uint32_t kVertexRefillLogLimit = 8;
    constexpr uint32_t kDrawLogLimit = 96;
    uint32_t g_sharedRefillLogs = 0, g_instanceRefillLogs = 0;
    uint32_t g_vertexRefillLogs = 0, g_drawLogs = 0;

    template <class T>
    T* At(void* base, size_t offset)
    {
        return reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
    }

    /** @brief The section's 16-bit placement fields, for the shared window arithmetic. */
    window::SectionPlacement Placement(const M2SkinSection& s)
    {
        return { s.level, s.vertexStart, s.vertexCount, s.indexStart, s.indexCount };
    }

    /**
     * @brief The submesh's first index into the skin's triangle array.
     * @param section  the submesh being placed.
     * @param skin     the skin profile it belongs to.
     * @return the widened start when level is a high half, else the client's own 16-bit value.
     */
    uint32_t TriangleStart(const M2SkinSection& section, const M2SkinProfile& skin)
    {
        return window::SectionTriangleStart(Placement(section), skin.indexCount);
    }

    /** @brief True when at least one submesh of this skin needs the fold; nothing else is touched. */
    bool UsesWideStarts(const M2SkinProfile* skin)
    {
        if (!skin || !skin->submeshes || !skin->indices) return false;
        for (uint32_t i = 0; i < skin->submeshCount; ++i)
        {
            const M2SkinSection& s = skin->submeshes[i];
            if (TriangleStart(s, *skin) != s.indexStart) return true;
        }
        return false;
    }

    /**
     * @brief A submesh's first vertex, recovered for a model that names more than 16 bits can hold.
     *
     * There is no second field to spill the high half into: level is already carrying the triangle
     * start's. So it is recomputed instead, from the one property such a model is built with --
     * submeshes laid out contiguously, in order, each starting where the last ended. That makes the
     * first vertex a running total, and the total is checked against what the section still holds:
     * over the range a 16-bit field can express, the two must agree exactly. Where they do not, the
     * model is not laid out this way and nothing here applies to it.
     *
     * @param skin   the skin whose submeshes are being walked.
     * @param index  which submesh the start is wanted for.
     * @param start  receives it when the layout holds.
     * @return false when the layout does not hold, and the client's own reading should stand.
     */
    bool WideVertexStart(const M2SkinProfile& skin, uint32_t index, uint32_t& start)
    {
        if (!skin.submeshes || index >= skin.submeshCount) return false;
        uint32_t running = 0;
        for (uint32_t i = 0; i <= index; ++i)
        {
            const M2SkinSection& s = skin.submeshes[i];
            // While the running total still fits, the section's own field is the authority and any
            // disagreement means the submeshes are not laid out end to end after all.
            if (running <= 0xFFFFu && uint16_t(running) != s.vertexStart) return false;
            if (i == index) break;
            running += s.vertexCount;
            if (running > skin.vertexCount) return false;
        }
        start = running;
        return true;
    }

    /** @brief True when this skin names more vertices than one of its triangles could address. */
    bool NeedsWideVertices(const M2SkinProfile* skin)
    {
        return skin && skin->submeshes && skin->vertexCount > 0xFFFFu;
    }

    /**
     * @brief Which submesh of the skin the batch's own copy came from.
     *
     * The draw is handed a copy, not the skin's record, and the copy's identifying field is not
     * carried through -- so the two are matched on what does survive: the widened triangle start,
     * which is unique per submesh because no two of them begin at the same triangle.
     * @return false when no submesh matches, which is every model this does not apply to.
     */
    bool SubmeshIndexOf(const M2SkinSection& copy, const M2SkinProfile& skin, uint32_t& index)
    {
        const uint32_t want = (static_cast<uint32_t>(copy.level) << 16) | copy.indexStart;
        for (uint32_t i = 0; i < skin.submeshCount; ++i)
        {
            const M2SkinSection& s = skin.submeshes[i];
            if (TriangleStart(s, skin) == want && s.indexCount == copy.indexCount)
            {
                index = i;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief The client's own choice between copying the skin's indices as they stand and rebasing
     *        each submesh onto its own vertex window. The explicit single-copy shared conversion
     *        changes only its uploaded indices and matching draw descriptor, never these flags.
     */
    bool UsesGlobalIndices(void* model)
    {
        void* record = *At<void*>(model, off::kOffSharedFileRecord);
        auto* header = *At<M2Header*>(model, off::kOffModelHeader);
        if (!record || !header) return false;
        const uint32_t flags = *At<uint32_t>(record, off::kOffM2FileFlags);
        if (flags & off::kM2FileFlagGlobalIndices) return true;
        return header->bones.count == 1 && (flags & off::kM2FileFlagSingleBoneGlobal) != 0;
    }

    // The certificate borrows only immutable source identities, never a retained pose. It is
    // published with a completed shared conversion and checked through the current live instance.
    struct PickingSource
    {
        const M2Header* header = nullptr;
        uint32_t vertices = 0, vertexCount = 0;
        const M2SkinSection* sections = nullptr;
        uint32_t sectionCount = 0;
        const uint16_t* lookup = nullptr;
    };
    struct PickingCall;

    // Shared with picking: a converted binding is recorded only after a completed refill.
    struct WideSkinNote
    {
        const M2SkinProfile* skin;
        const uint16_t* indices;
        uint32_t indexCount;
        uint32_t vertexCount;
        void* model;
        void* convertedSharedIb;
        PickingSource pickingSource = {};
        PickingCall* pickingCall = nullptr; // borrowed stack frame, scoped to kHitTestGeometry
        uint64_t pickingGeneration = 0;     // invalidates in-flight calls across a rebuild
        uint32_t pickingLogged = 0;         // low / crossing / above, legacy-wide, reject-reason bits
        void* refilledSharedIb = nullptr;   // native built/valid does not certify our repair
        void* refilledSharedVb = nullptr;
        void* rejectedSharedIb = nullptr;   // window admission refused by the skin's own data
    };
    constexpr size_t kMaxWideSkins = 64;
    // Keep the added hook-chain slots and log budget in the existing registry too. No second
    // global picking context, pose cache, heap owner or thread-local registry is introduced.
    struct WideSkinRegistry
    {
        WideSkinNote notes[kMaxWideSkins] = {};
        off::M2_HitTestGeometryFn geometryNext = nullptr;
        off::M2_FillHitTestVerticesFn fillNext[3] = {};
        uint32_t pickingLogs = 0;
        WideSkinNote& operator[](size_t index) { return notes[index]; }
    };
    WideSkinRegistry g_wideSkins = {};
    size_t g_wideSkinCount = 0;

    WideSkinNote* NoteWideSkin(const M2SkinProfile* skin)
    {
        if (!skin || !skin->indices) return nullptr;
        for (size_t i = 0; i < g_wideSkinCount; ++i)
        {
            WideSkinNote& n = g_wideSkins[i];
            if (n.skin != skin) continue;
            // Instance-index notes must not erase a still-valid shared conversion.
            if (n.indices != skin->indices || n.indexCount != skin->indexCount
                || n.vertexCount != skin->vertexCount)
            {
                // A synchronous nested refill must not leave the geometry scope's stack link
                // dangling. Its old certificate no longer matches, so its pending pick is rejected.
                PickingCall* active = n.pickingCall;
                const uint64_t generation = n.pickingGeneration + 1;
                n = { skin, skin->indices, skin->indexCount, skin->vertexCount, nullptr, nullptr };
                n.pickingCall = active;
                n.pickingGeneration = generation;
            }
            return &n;
        }
        if (g_wideSkinCount == kMaxWideSkins) return nullptr;
        WideSkinNote& n = g_wideSkins[g_wideSkinCount++];
        n = { skin, skin->indices, skin->indexCount, skin->vertexCount, nullptr, nullptr };
        return &n;
    }

    void ClearSharedConversion(void* model, bool clearVertices = false)
    {
        for (size_t i = 0; i < g_wideSkinCount; ++i)
            if (g_wideSkins[i].model == model)
            {
                g_wideSkins[i].convertedSharedIb = nullptr;
                g_wideSkins[i].refilledSharedIb = nullptr;
                g_wideSkins[i].rejectedSharedIb = nullptr;
                if (clearVertices) g_wideSkins[i].refilledSharedVb = nullptr;
                ++g_wideSkins[i].pickingGeneration;
            }
    }

    // A vertex rebuild retires only the vertex repair: converted indices stay true to their own
    // buffer, and a converted draw also requires the current vertex repair (hkDeviceDraw).
    void ClearVertexRepair(void* model)
    {
        for (size_t i = 0; i < g_wideSkinCount; ++i)
            if (g_wideSkins[i].model == model) g_wideSkins[i].refilledSharedVb = nullptr;
    }

    // One note's index binding, leaving every other note of its model alone. The vertex repair stays:
    // a new buffer at the same address is a rebuild, which its setter retires.
    void RetireBinding(WideSkinNote& n)
    {
        n.convertedSharedIb = nullptr;
        n.refilledSharedIb = nullptr;
        n.rejectedSharedIb = nullptr;
        ++n.pickingGeneration;
    }

    WideSkinNote* ConvertedBinding(void* buffer)
    {
        if (buffer)
            for (size_t i = 0; i < g_wideSkinCount; ++i)
                if (g_wideSkins[i].convertedSharedIb == buffer) return &g_wideSkins[i];
        return nullptr;
    }

    // This is a fill-time check, not a per-draw scan of all triangle payloads. dataRejected reports a
    // refusal by the skin's own indices or vertex lookup, which no later bind of the buffer changes.
    bool CanWindowSharedIndices(void* model, const M2SkinProfile* skin, bool* dataRejected = nullptr)
    {
        if (!model || !skin || skin->vertexCount <= 0x10000u || !skin->submeshes
            || !skin->indices || !skin->vertexLookup || !skin->bones || !UsesGlobalIndices(model)
            || *At<uint32_t>(model, off::kOffSharedInstanceCopies) != 1
            || !*reinterpret_cast<const uint32_t*>(off::kEnableShaders)) return false;
        void* device = wxl::game::gx::RawGraphicsDevice();
        auto* header = *At<M2Header*>(model, off::kOffModelHeader);
        auto* copies = *At<M2SkinSection*>(model, off::kOffModelSubmeshBuf);
        if (!device || *At<uint32_t>(device, gxoff::kGxDeviceBaseVertexMode)
            || !header || !header->vertices.offset || header->vertices.count < skin->vertexCount
            || !copies) return false;
        uint32_t first = 0, written = 0;
        for (uint32_t i = 0; i < skin->submeshCount; ++i)
        {
            const M2SkinSection& s = skin->submeshes[i];
            const M2SkinSection& copy = copies[i];
            const uint32_t start = TriangleStart(s, *skin);
            // Keep the existing source/copy triangle matching valid on this single-copy path.
            if (!window::Fits(first, s.vertexStart, s.vertexCount, skin->vertexCount)
                || start != written || start > skin->indexCount || s.indexCount > skin->indexCount - start
                || copy.indexCount != s.indexCount || copy.vertexCount != s.vertexCount
                || copy.vertexStart != s.vertexStart || copy.indexStart != s.indexStart || copy.level != s.level)
                return false;
            first += s.vertexCount;
            written += s.indexCount;
        }
        if (!(first == skin->vertexCount && written == skin->indexCount)) return false;
        first = 0;
        for (uint32_t i = 0; i < skin->submeshCount; ++i)
        {
            const M2SkinSection& s = skin->submeshes[i];
            const uint32_t start = TriangleStart(s, *skin);
            bool fits = true;
            for (uint32_t k = 0; fits && k < s.indexCount; ++k)
                if (window::LocalIndex(skin->indices[start + k], s.vertexStart) >= s.vertexCount) fits = false;
            for (uint32_t k = 0; fits && k < s.vertexCount; ++k)
                if (skin->vertexLookup[first + k] != uint16_t(first + k)) fits = false;
            if (!fits)
            {
                if (dataRejected) *dataRejected = true;
                return false;
            }
            first += s.vertexCount;
        }
        return true;
    }

    uint32_t DiagnosticFileFlags(void* model)
    {
        void* record = model ? *At<void*>(model, off::kOffSharedFileRecord) : nullptr;
        return record ? *At<uint32_t>(record, off::kOffM2FileFlags) : 0;
    }

    void LogIndexRefill(bool shared, void* model, void* instance, void* buffer,
                        const M2SkinProfile& skin, uint32_t section, uint32_t groupOrCopy,
                        int groupVertexBase, uint16_t refillVertexStart, bool globalIndices,
                        int16_t bias, uint32_t start, uint32_t count, size_t written,
                        const uint16_t* emitted, bool windowed = false)
    {
        uint32_t& used = shared ? g_sharedRefillLogs : g_instanceRefillLogs;
        const uint32_t limit = shared ? kSharedRefillLogLimit : kInstanceRefillLogLimit;
        if (!NeedsWideVertices(&skin) || (section > 1 && skin.submeshCount - section > 3) || used >= limit) return;
        ++used;
        const bool triangle = count >= 3 && start <= skin.indexCount
                              && count <= skin.indexCount - start;
        const uint16_t* raw = triangle ? skin.indices + start : nullptr;
        WLOG_INFO("m2wide-beta: refill=%s record=%u model=%p instance=%p skin=%p ib=%p "
                  "section=%u global=%u flags=0x%08X groupOrCopy=%u groupVertexBase=%d "
                  "sourceVertexStart=%u refillVertexStart=%u bias=%d sourceIndexStart=%u "
                  "written=%zu count=%u triangle=%u raw=(%u,%u,%u) emitted=(%u,%u,%u) windowed=%u",
                  shared ? "shared" : "instance", used, model, instance, &skin, buffer,
                  section, unsigned(globalIndices), DiagnosticFileFlags(model), groupOrCopy,
                  groupVertexBase, unsigned(skin.submeshes[section].vertexStart),
                  unsigned(refillVertexStart), int(bias), start, written, count, unsigned(triangle),
                  raw ? unsigned(raw[0]) : 0u, raw ? unsigned(raw[1]) : 0u,
                  raw ? unsigned(raw[2]) : 0u, triangle ? unsigned(emitted[0]) : 0u,
                  triangle ? unsigned(emitted[1]) : 0u, triangle ? unsigned(emitted[2]) : 0u,
                  unsigned(windowed));
        wxl::log::Flush();
    }

    /** @brief True while the buffer's contents still stand, which is when a fill is skipped. */
    bool BufferHolds(void* buffer)
    {
        return buffer && *At<uint8_t>(buffer, off::kOffGxBufBuilt) && *At<uint8_t>(buffer, off::kOffGxBufValid);
    }

    /** @brief Locks a GPU buffer for writing, exactly as the engine fill does. */
    void* LockBuffer(void* device, void* buffer)
    {
        return wxl::game::gx::Vtbl<off::Gx_BufLockFn>(device, static_cast<unsigned>(off::kGxVtblBufLock / sizeof(void*)))(device, buffer);
    }

    /** @brief Finishes a refill without changing which kind of buffer the device has bound. */
    void UnlockBuffer(void* device, void* buffer)
    {
        wxl::game::gx::Vtbl<off::Gx_BufUnlockFn>(device, static_cast<unsigned>(off::kGxVtblBufUnlock / sizeof(void*)))(device, buffer, 0);
        *At<uint8_t>(buffer, off::kOffGxBufBuilt) = 1;
    }

    /** @brief Commits an index refill and restores the engine's index binding. */
    void CommitIndexBuffer(void* device, void* buffer)
    {
        UnlockBuffer(device, buffer);
        wxl::game::Native<off::Gx_PrimIndexPtrFn>(off::kPrimIndexPtr)(device, buffer);
    }

    /**
     * @brief Writes one submesh's block, reproducing the engine's two index spellings.
     * @param dst      cursor into the locked index buffer.
     * @param source   the skin's triangle array.
     * @param start    the submesh's first index in that array.
     * @param count    how many indices the block holds.
     * @param bias     added to every index; zero on the global-index path.
     */
    void EmitBlock(uint16_t* dst, const uint16_t* source, uint32_t start, uint32_t count, int16_t bias)
    {
        if (!bias) { std::memcpy(dst, source + start, count * sizeof(uint16_t)); return; }
        for (uint32_t i = 0; i < count; ++i)
            dst[i] = static_cast<uint16_t>(static_cast<int16_t>(source[start + i]) + bias);
    }

    /**
     * @brief Refills the per-instance index buffer: the compacted draw list, in the same order and at
     *        the same offsets the engine just used, with every block sourced from its widened start.
     */
    void RefillInstanceIndices(void* instance, const M2SkinProfile& skin, bool globalIndices)
    {
        void* device = wxl::game::gx::RawGraphicsDevice();
        void* geo = *At<void*>(instance, off::kOffInstGeometryCtx);
        if (!device || !geo) return;
        void* buffer = *At<void*>(geo, off::kOffGeoCtxIndexBuf);
        auto* groups = *At<uint8_t*>(geo, off::kOffGeoCtxGroups);
        auto* ranges = *At<uint32_t*>(geo, off::kOffGeoCtxRanges);
        auto* visible = *At<int32_t*>(instance, off::kOffInstSectionVisible);
        const uint32_t groupCount = *At<uint32_t>(geo, off::kOffGeoCtxGroupCount);
        if (!buffer || !groups || !ranges || !visible || !skin.batches) return;

        auto* dst = static_cast<uint16_t*>(LockBuffer(device, buffer));
        if (!dst) return;

        const uint16_t* const base = dst;
        bool reported = false;

        for (uint32_t g = 0; g < groupCount; ++g)
        {
            const uint32_t* range = ranges + *reinterpret_cast<uint32_t*>(groups + g * off::kGeoCtxGroupStride) * 2;
            // The vertex base restarts with each group: a group is one contiguous vertex window.
            int16_t vertexBase = 0;
            for (uint32_t b = range[0]; b <= range[1]; ++b)
            {
                const uint16_t sectionIndex = skin.batches[b].skinSectionIndex;
                if (!visible[sectionIndex]) continue;
                const M2SkinSection& s = skin.submeshes[sectionIndex];
                if (!reported && size_t(dst - base) > 0xFFFF)
                {
                    WLOG_DEBUG("m2native-indices: compacted block for submesh %u begins at index %zu, "
                               "which a 16-bit start cannot name; the draw path is expected to "
                               "restore the high half from level",
                               unsigned(sectionIndex), size_t(dst - base));
                    reported = true;
                }
                EmitBlock(dst, skin.indices, TriangleStart(s, skin), s.indexCount,
                          globalIndices ? int16_t(0) : static_cast<int16_t>(vertexBase - static_cast<int16_t>(s.vertexStart)));
                LogIndexRefill(false, *At<void*>(instance, off::kOffInstModel), instance,
                               buffer, skin, sectionIndex, g, int(vertexBase), s.vertexStart,
                               globalIndices, globalIndices ? int16_t(0) : static_cast<int16_t>(vertexBase - static_cast<int16_t>(s.vertexStart)),
                               TriangleStart(s, skin), s.indexCount, size_t(dst - base), dst);
                vertexBase = static_cast<int16_t>(vertexBase + static_cast<int16_t>(s.vertexCount));
                dst += s.indexCount;
            }
        }
        CommitIndexBuffer(device, buffer);
    }

    /**
     * @brief Refills the shared index buffer: every submesh in skin order, each repeated once per
     *        instance copy, at the offsets the engine wrote back into the submesh copies.
     */
    void RefillSharedIndices(void* model, const M2SkinProfile& skin, bool globalIndices,
                             WideSkinNote* windowNote = nullptr, WideSkinNote* refillNote = nullptr)
    {
        void* device = wxl::game::gx::RawGraphicsDevice();
        void* buffer = *At<void*>(model, off::kOffSharedIndexBuf);
        auto* copies = *At<M2SkinSection*>(model, off::kOffModelSubmeshBuf);
        const uint32_t instanceCopies = *At<uint32_t>(model, off::kOffSharedInstanceCopies);
        if (!device || !buffer || !copies) return;

        auto* dst = static_cast<uint16_t*>(LockBuffer(device, buffer));
        if (!dst) return;

        const uint16_t* const base = dst;
        bool reported = false;

        for (uint32_t i = 0; i < skin.submeshCount; ++i)
        {
            // Counts come from the copy, which finalize may have adjusted; the start comes from the
            // skin, which is the only place the untruncated pairing survives.
            const M2SkinSection& copy = copies[i];
            const size_t written = size_t(dst - base);
            if (!reported && written > 0xFFFF)
            {
                WLOG_DEBUG("m2native-indices: submesh %u of %u begins at index %zu, which the "
                           "engine's own 16-bit record cannot name -- it holds %u, the low half. "
                           "The draw path is expected to restore the high half from level; nothing "
                           "here can, and the bytes at that offset are correct.",
                           unsigned(i), unsigned(skin.submeshCount), written, unsigned(copy.indexStart));
                reported = true;
            }
            const uint32_t start = TriangleStart(skin.submeshes[i], skin);
            int16_t bias = globalIndices ? int16_t(0) : static_cast<int16_t>(-static_cast<int16_t>(copy.vertexStart));
            for (uint32_t c = 0; c < instanceCopies; ++c)
            {
                if (windowNote)
                    for (uint32_t k = 0; k < copy.indexCount; ++k)
                        dst[k] = window::LocalIndex(skin.indices[start + k], skin.submeshes[i].vertexStart);
                else
                    EmitBlock(dst, skin.indices, start, copy.indexCount, bias);
                LogIndexRefill(true, model, nullptr, buffer, skin, i, c, 0, copy.vertexStart,
                               globalIndices, windowNote ? static_cast<int16_t>(-static_cast<int16_t>(skin.submeshes[i].vertexStart)) : bias,
                               start, copy.indexCount, size_t(dst - base), dst, windowNote != nullptr);
                dst += copy.indexCount;
                bias = static_cast<int16_t>(bias + static_cast<int16_t>(globalIndices ? skin.vertexCount : copy.vertexCount));
            }
        }
        CommitIndexBuffer(device, buffer);
        if (refillNote)
        {
            refillNote->model = model;
            refillNote->refilledSharedIb = buffer;
        }
        if (windowNote)
        {
            // A freed model's note can still name this address; only one buffer lives there.
            for (size_t i = 0; i < g_wideSkinCount; ++i)
                if (&g_wideSkins[i] != windowNote && g_wideSkins[i].convertedSharedIb == buffer)
                    RetireBinding(g_wideSkins[i]);
            windowNote->model = model;
            windowNote->convertedSharedIb = buffer;
            const auto* header = *At<M2Header*>(model, off::kOffModelHeader);
            windowNote->pickingSource = { header, header->vertices.offset, header->vertices.count,
                                          skin.submeshes, skin.submeshCount, skin.vertexLookup };
            ++windowNote->pickingGeneration;
            if (g_sharedRefillLogs < kSharedRefillLogLimit)
            {
                ++g_sharedRefillLogs;
                WLOG_INFO("m2wide-beta: converted=1 record=%u model=%p skin=%p ib=%p vertices=%u",
                          g_sharedRefillLogs, model, &skin, buffer, skin.vertexCount);
                wxl::log::Flush();
            }
        }
    }

    /**
     * @brief Refills the vertex buffer for a model the skin's own lookup can no longer address.
     *
     * The engine resolves each slot through skin->vertexLookup, whose entries are 16 bits, so past
     * 65535 it fetches whatever the wrapped entry names -- the front of the model. There is nothing
     * to widen: the array cannot hold the value. What there is instead is the property such a model
     * is built with, that its vertices are its skin's in the same order, so a slot's source is its
     * own number and no lookup is needed to say so.
     *
     * Every slot is rewritten rather than only those past the line, because the same walk has to
     * visit each submesh anyway to know the bone count its co-instance shift is scaled by, and a
     * partial pass would need the submesh a slot belongs to worked out a second, different way.
     */
    void RefillWideVertices(void* model, const M2SkinProfile& skin, WideSkinNote* refillNote = nullptr)
    {
        void* device = wxl::game::gx::RawGraphicsDevice();
        void* buffer = *At<void*>(model, off::kOffSharedVertexBuf);
        auto* header = *At<M2Header*>(model, off::kOffModelHeader);
        if (!device || !buffer || !header || !skin.bones
            || !*reinterpret_cast<const uint32_t*>(off::kEnableShaders)
            || *At<uint32_t>(buffer, gxoff::kGxBufStreamStride) != off::kModelVertexStride) return;

        // Resolved in place by the load, so what the header holds is already a pointer.
        const auto* source = reinterpret_cast<const uint8_t*>(header->vertices.offset);
        if (!source || header->vertices.count < skin.vertexCount) return;

        const uint32_t copies = *At<uint32_t>(model, off::kOffSharedInstanceCopies);
        const bool logRefill = NeedsWideVertices(&skin) && g_vertexRefillLogs < kVertexRefillLogLimit;
        const uint32_t strideBefore = logRefill ? *At<uint32_t>(buffer, gxoff::kGxBufStreamStride) : 0;
        const uint32_t offsetBefore = logRefill ? *At<uint32_t>(buffer, gxoff::kGxBufStreamOffset) : 0;
        auto* dst = static_cast<uint8_t*>(LockBuffer(device, buffer));
        if (!dst) return;

        for (uint32_t c = 0; c < (copies ? copies : 1u); ++c)
        {
            for (uint32_t i = 0; i < skin.submeshCount; ++i)
            {
                const M2SkinSection& s = skin.submeshes[i];
                uint32_t start = 0;
                if (!WideVertexStart(skin, i, start)) continue;
                if (uint64_t(start) + s.vertexCount > skin.vertexCount) continue;

                // One dword add over the four slot bytes at once, which is what the engine does:
                // every slot of this submesh shifts by the same co-instance stride.
                const uint32_t shift = uint32_t(s.boneCount) * c * 0x01010101u;
                for (uint32_t k = 0; k < s.vertexCount; ++k)
                {
                    const uint32_t v = start + k;
                    uint8_t* slot = dst + (size_t(c) * skin.vertexCount + v) * off::kModelVertexStride;
                    std::memcpy(slot, source + size_t(v) * off::kModelVertexStride,
                                off::kModelVertexStride);
                    uint32_t bones;
                    std::memcpy(&bones, skin.bones + size_t(v) * 4, sizeof bones);
                    bones += shift;
                    std::memcpy(slot + off::kOffVertexBoneSlots, &bones, sizeof bones);
                }
            }
        }
        // The original SharedSetVertices already selected this vertex stream. Do not
        // pass its buffer to PrimIndexPtr: that would replace the device's index source.
        UnlockBuffer(device, buffer);
        if (refillNote)
        {
            refillNote->model = model;
            refillNote->refilledSharedVb = buffer;
        }
        if (logRefill)
        {
            ++g_vertexRefillLogs;
            WLOG_INFO("m2wide-beta: vertex-refill record=%u model=%p skin=%p vb=%p vertices=%u "
                      "copies=%u strideBefore=%u strideAfter=%u offsetBefore=%u offsetAfter=%u",
                      g_vertexRefillLogs, model, &skin, buffer, skin.vertexCount, copies,
                      strideBefore, *At<uint32_t>(buffer, gxoff::kGxBufStreamStride),
                      offsetBefore, *At<uint32_t>(buffer, gxoff::kGxBufStreamOffset));
            wxl::log::Flush();
        }
    }

    uint32_t __fastcall hkSetModelIndices(void* instance, void* edx)
    {
        void* model = instance ? *At<void*>(instance, off::kOffInstModel) : nullptr;
        auto* skin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        void* geo = instance ? *At<void*>(instance, off::kOffInstGeometryCtx) : nullptr;
        // Read the dirty flags before the original clears them: only a real rebuild needs refilling.
        const bool rebuilding = geo && !BufferHolds(*At<void*>(geo, off::kOffGeoCtxIndexBuf));

        const uint32_t result = g_origSetModelIndices(instance, edx);
        if (!result || !rebuilding || !UsesWideStarts(skin)) return result;
        NoteWideSkin(skin);
        RefillInstanceIndices(instance, *skin, UsesGlobalIndices(model));
        return result;
    }

    uint32_t __fastcall hkSharedSetIndices(void* model, void* edx)
    {
        void* previousBuffer = model ? *At<void*>(model, off::kOffSharedIndexBuf) : nullptr;
        const auto* previousSkin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        const bool rebuilding = model && !BufferHolds(previousBuffer);
        if (rebuilding) ClearSharedConversion(model);

        // The original owns creating the pool/buffer pair and sizing it, so it always runs first.
        const uint32_t result = g_origSharedSetIndices(model, edx);
        auto* skin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        void* buffer = model ? *At<void*>(model, off::kOffSharedIndexBuf) : nullptr;
        if (model && (buffer != previousBuffer || skin != previousSkin))
            ClearSharedConversion(model, skin != previousSkin);
        if (!result || (!UsesWideStarts(skin) && !NeedsWideVertices(skin))) return result;
        WideSkinNote* note = NoteWideSkin(skin);
        // A native-valid buffer can still contain an uncorrected fill. Retry until our commit,
        // then leave ordinary binds alone; dirty flags also catch rebuilds at the same address.
        if (!buffer) return result;
        if (!note)
        {
            // A full registry has nowhere to mark completion: keep the one-shot repair on rebuild.
            if (rebuilding && UsesWideStarts(skin)) RefillSharedIndices(model, *skin, UsesGlobalIndices(model));
            return result;
        }
        const bool refilled = !rebuilding && note->model == model && note->refilledSharedIb == buffer;
        const bool pendingWindows = skin->vertexCount > 0x10000u && UsesGlobalIndices(model)
                                    && note->convertedSharedIb != buffer;
        if (refilled && !pendingWindows) return result;
        bool dataRejected = false;
        const bool windows = note->rejectedSharedIb != buffer
                             && CanWindowSharedIndices(model, skin, &dataRejected);
        if (dataRejected)
        {
            note->model = model;
            note->rejectedSharedIb = buffer;
        }
        if (!UsesWideStarts(skin) && !windows) return result;
        // A completed triangle fold need not run again while window admission is deferred.
        // Still retry admission so a later eligible bind can upgrade it to a conversion.
        if (refilled && !windows) return result;
        if (!rebuilding) ClearSharedConversion(model);
        RefillSharedIndices(model, *skin, UsesGlobalIndices(model), windows ? note : nullptr, note);
        return result;
    }




    /**
     * @brief Notes which submesh the batch about to run belongs to, for the device draw to widen.
     *
     * The section is read off the batch record rather than from the draw context's own copy: the
     * context is reused across every batch in a pass and this entry refreshes that copy as its very
     * first act, so a hook on its entry still sees the batch that already ran.
     *
     * Saved and restored around the original rather than simply assigned, because nothing promises
     * a batch draw never nests, and a stale section outliving its batch would widen someone else's.
     */
    void __fastcall hkDrawBatch(void* ctx, void* edx)
    {
        const M2SkinSection* prevSection = g_drawSection;
        const M2SkinProfile* prevSkin    = g_drawSkin;
        void* prevModel = g_drawModel;
        void* prevInstance = g_drawInstance;

        auto* context = static_cast<gxoff::DrawBatchContext*>(ctx);
        void* element = context ? context->element : nullptr;
        g_drawSection = element ? *At<M2SkinSection*>(element, gxoff::kM2ElementSectionField) : nullptr;
        // The context names the instance, not the shared model, and the skin hangs off the latter.
        void* shared = context && context->model
                           ? *At<void*>(context->model, off::kOffInstModel) : nullptr;
        g_drawSkin = shared ? *At<M2SkinProfile*>(shared, off::kOffModelSkin) : nullptr;
        g_drawModel = shared;
        g_drawInstance = context ? context->model : nullptr;

        g_origDrawBatch(ctx, edx);

        g_drawSection = prevSection;
        g_drawSkin    = prevSkin;
        g_drawModel = prevModel;
        g_drawInstance = prevInstance;
    }

    /**
     * @brief Restores the high half of a submesh's triangle start on its way to the device.
     *
     * The M2 draw builds its descriptor by reading the submesh's start through a 16-bit field, so a
     * start past 65535 arrives here holding its low half alone. This is the first place on the path
     * where the value has room: the descriptor's own field is 32 bits, and the device passes it
     * through to the draw call untouched.
     *
     * Deliberately here and not at the vtable slot above it. That slot is owned elsewhere by an
     * explicit arrangement, and widening a value the batch carries needs nothing the slot offers --
     * only somewhere the full value fits, which this is.
     *
     * The same conditional fold the fills use: level had an older life as a marker, so the widened
     * start is taken only when it and the submesh's own count still land inside the triangle array
     * the skin declares. The low half is checked against the section too, so a descriptor that is
     * not this section's -- anything that reached the device by another route -- is left alone.
     */
    void __fastcall hkDeviceDraw(void* device, void* edx, uint32_t* batch, int indexed)
    {
        WideSkinNote* converted = indexed
            ? ConvertedBinding(*At<void*>(device, gxoff::kGxDeviceIndexBuffer)) : nullptr;
        const auto stale = [device](const WideSkinNote& n)
        {
            if (!g_drawModel || !g_drawSkin)
            {
                // Outside every batch context (projected decals bind the model's own buffers too)
                // only a stream in no model's vertex layout proves the address was reused, as the
                // stride-36 draw did; anything else keeps the unsupported-draw skip below.
                void* stream = *At<void*>(device, gxoff::kGxDeviceVertexStream);
                return stream && *At<uint32_t>(stream, gxoff::kGxBufStreamStride) != off::kModelVertexStride;
            }
            return n.model != g_drawModel || n.skin != g_drawSkin
                   || *At<M2SkinProfile*>(g_drawModel, off::kOffModelSkin) != g_drawSkin
                   || n.convertedSharedIb != *At<void*>(g_drawModel, off::kOffSharedIndexBuf)
                   || !BufferHolds(*At<void*>(g_drawModel, off::kOffSharedIndexBuf))
                   || n.indices != g_drawSkin->indices || n.indexCount != g_drawSkin->indexCount
                   || n.vertexCount != g_drawSkin->vertexCount;
        };
        // An address can outlive its buffer/model, and two notes can name it: retire each stale match
        // by itself, never reading its stored model, and look again; unrelated draws stay native.
        while (converted && stale(*converted))
        {
            RetireBinding(*converted);
            converted = ConvertedBinding(*At<void*>(device, gxoff::kGxDeviceIndexBuffer));
        }
        const bool logDraw = indexed && batch && g_drawSection && NeedsWideVertices(g_drawSkin)
                             && g_drawLogs < kDrawLogLimit;
        void* diagnosticStream = logDraw ? *At<void*>(device, gxoff::kGxDeviceVertexStream) : nullptr;
        const uint32_t incomingOffset = diagnosticStream ? *At<uint32_t>(diagnosticStream, gxoff::kGxBufStreamOffset) : 0;
        const uint32_t incomingStartIndex = logDraw ? *At<uint32_t>(batch, gxoff::kGxBatchStartIndex) : 0;
        if (indexed && batch && g_drawSection && g_drawSkin && g_drawSection->level)
        {
            const M2SkinSection& s = *g_drawSection;
            uint32_t& startIndex = *At<uint32_t>(batch, gxoff::kGxBatchStartIndex);
            if ((startIndex & 0xFFFFu) == s.indexStart)
            {
                const uint32_t wide = (static_cast<uint32_t>(s.level) << 16) | s.indexStart;
                // Phrased as a subtraction so a garbage level cannot wrap the bound it is checked against.
                if (wide <= g_drawSkin->indexCount && s.indexCount <= g_drawSkin->indexCount - wide)
                    startIndex = wide;
            }
        }
        // The submesh's own window, for a model whose triangles are written relative to it. The
        // base is not a field of the descriptor: the device derives it by dividing the bound vertex
        // stream's stored offset by its stride. So the offset is what gets set -- the device then
        // computes the base itself, with its own arithmetic, and nothing here reimplements a draw.
        //
        // Unconverted global indices already name their model vertices. Adding a section base again fetches
        // another section (or past the buffer); preserve the native stream offset on that path.
        // This does not make uint16 global indices address vertices beyond 65535.
        //
        // Restored around the call rather than left: the stream is shared by everything that draws
        // after this, and a base meant for one submesh would silently displace all of them.
        uint32_t* streamOffset = nullptr;
        uint32_t  savedOffset  = 0;
        uint16_t savedMin = 0, savedMax = 0;
        bool windowedDraw = false;
        if (converted)
        {
            uint32_t section = 0, wideStart = 0, imposed = 0;
            void* stream = *At<void*>(device, gxoff::kGxDeviceVertexStream);
            const uint32_t stride = stream ? *At<uint32_t>(stream, gxoff::kGxBufStreamStride) : 0;
            bool supported = batch && g_drawModel && g_drawSkin && g_drawSection
                && converted->model == g_drawModel && converted->skin == g_drawSkin
                && converted->indices == g_drawSkin->indices && converted->indexCount == g_drawSkin->indexCount
                && converted->vertexCount == g_drawSkin->vertexCount
                && converted->convertedSharedIb == *At<void*>(g_drawModel, off::kOffSharedIndexBuf)
                && *At<uint32_t>(g_drawModel, off::kOffSharedInstanceCopies) == 1
                && UsesGlobalIndices(g_drawModel) && *reinterpret_cast<const uint32_t*>(off::kEnableShaders)
                && stream && stream == *At<void*>(g_drawModel, off::kOffSharedVertexBuf)
                && stream == converted->refilledSharedVb
                && stride == off::kModelVertexStride && !*At<uint32_t>(device, gxoff::kGxDeviceBaseVertexMode)
                && SubmeshIndexOf(*g_drawSection, *g_drawSkin, section)
                && WideVertexStart(*g_drawSkin, section, wideStart);
            if (supported)
            {
                const M2SkinSection& s = g_drawSkin->submeshes[section];
                supported = window::Fits(wideStart, s.vertexStart, s.vertexCount, g_drawSkin->vertexCount)
                    && g_drawSection->vertexCount == s.vertexCount
                    && *At<uint32_t>(batch, gxoff::kGxBatchStartIndex) == TriangleStart(s, *g_drawSkin)
                    && *At<uint32_t>(batch, gxoff::kGxBatchIndexCount) == s.indexCount
                    && window::StreamOffset(*At<uint32_t>(stream, gxoff::kGxBufStreamOffset), wideStart, stride, imposed);
            }
            if (!supported)
            {
                // Once converted, falling through with a global descriptor is no longer safe.
                static bool warnedConverted = false;
                if (!warnedConverted)
                {
                    warnedConverted = true;
                    WLOG_WARN("m2wide-beta: skipping unsupported draw of converted ib=%p model=%p skin=%p vb=%p stride=%u",
                              converted->convertedSharedIb, g_drawModel, g_drawSkin, stream, stride);
                }
                return;
            }
            streamOffset = At<uint32_t>(stream, gxoff::kGxBufStreamOffset);
            savedOffset = *streamOffset;
            savedMin = *At<uint16_t>(batch, gxoff::kGxBatchMinIndex);
            savedMax = *At<uint16_t>(batch, gxoff::kGxBatchMaxIndex);
            *streamOffset = imposed;
            *At<uint16_t>(batch, gxoff::kGxBatchMinIndex) = 0;
            *At<uint16_t>(batch, gxoff::kGxBatchMaxIndex) = g_drawSkin->submeshes[section].vertexCount - 1;
            windowedDraw = true;
        }
        else if (indexed && g_drawSection && NeedsWideVertices(g_drawSkin)
            && g_drawModel && !UsesGlobalIndices(g_drawModel))
        {
            uint32_t submesh = 0, wideStart = 0;
            if (SubmeshIndexOf(*g_drawSection, *g_drawSkin, submesh)
                && WideVertexStart(*g_drawSkin, submesh, wideStart))
            {
                void* stream = *At<void*>(device, gxoff::kGxDeviceVertexStream);
                const uint32_t mode = *At<uint32_t>(device, gxoff::kGxDeviceBaseVertexMode);
                const uint32_t stride = stream ? *At<uint32_t>(stream, gxoff::kGxBufStreamStride) : 0;
                if (stream && stride && !mode)
                {
                    streamOffset = At<uint32_t>(stream, gxoff::kGxBufStreamOffset);
                    savedOffset  = *streamOffset;
                    *streamOffset = wideStart * stride;
                }
                else
                {
                    // Said once rather than left to look like it worked: on this branch the device
                    // passes a base of zero whatever the offset says, so every submesh of a wide
                    // model draws from the front of it.
                    static bool warned = false;
                    if (!warned)
                    {
                        warned = true;
                        WLOG_WARN("m2native-indices: this device derives no base vertex (mode %u, "
                                  "stream %p, stride %u), so a model relying on a per-submesh "
                                  "window cannot be drawn correctly here",
                                  mode, stream, stride);
                    }
                }
            }
        }

        if (logDraw)
        {
            ++g_drawLogs;
            uint32_t section = 0, wideStart = 0;
            const bool matched = SubmeshIndexOf(*g_drawSection, *g_drawSkin, section);
            const bool wideValid = matched && WideVertexStart(*g_drawSkin, section, wideStart);
            void* geo = g_drawInstance ? *At<void*>(g_drawInstance, off::kOffInstGeometryCtx) : nullptr;
            WLOG_INFO("m2wide-beta: draw record=%u model=%p instance=%p skin=%p boundIb=%p "
                      "sharedIb=%p instanceIb=%p vb=%p global=%u flags=0x%08X matched=%u section=%u "
                      "sourceVertexStart=%u copyVertexStart=%u wideValid=%u wideStart=%u stride=%u "
                      "mode=%u incomingOffset=%u imposedOffset=%u override=%u incomingStartIndex=%u "
                      "startIndex=%u count=%u min=%u max=%u converted=%u windowed=%u",
                      g_drawLogs, g_drawModel, g_drawInstance, g_drawSkin,
                      *At<void*>(device, gxoff::kGxDeviceIndexBuffer),
                      g_drawModel ? *At<void*>(g_drawModel, off::kOffSharedIndexBuf) : nullptr,
                      geo ? *At<void*>(geo, off::kOffGeoCtxIndexBuf) : nullptr, diagnosticStream,
                      unsigned(g_drawModel && UsesGlobalIndices(g_drawModel)), DiagnosticFileFlags(g_drawModel),
                      unsigned(matched), section,
                      matched ? unsigned(g_drawSkin->submeshes[section].vertexStart) : 0u,
                      unsigned(g_drawSection->vertexStart), unsigned(wideValid), wideStart,
                      diagnosticStream ? *At<uint32_t>(diagnosticStream, gxoff::kGxBufStreamStride) : 0u,
                      *At<uint32_t>(device, gxoff::kGxDeviceBaseVertexMode), incomingOffset,
                      diagnosticStream ? *At<uint32_t>(diagnosticStream, gxoff::kGxBufStreamOffset) : 0u,
                      unsigned(streamOffset != nullptr), incomingStartIndex,
                      *At<uint32_t>(batch, gxoff::kGxBatchStartIndex),
                      *At<uint32_t>(batch, gxoff::kGxBatchIndexCount),
                      unsigned(*At<uint16_t>(batch, gxoff::kGxBatchMinIndex)),
                      unsigned(*At<uint16_t>(batch, gxoff::kGxBatchMaxIndex)),
                      unsigned(converted != nullptr), unsigned(windowedDraw));
            wxl::log::Flush();
        }

        g_origDeviceDraw(device, edx, batch, indexed);

        if (windowedDraw)
        {
            *At<uint16_t>(batch, gxoff::kGxBatchMinIndex) = savedMin;
            *At<uint16_t>(batch, gxoff::kGxBatchMaxIndex) = savedMax;
        }
        if (streamOffset) *streamOffset = savedOffset;
    }

    // ---- picking: keep the native batch filters, scratch allocator and hit arbitration. A scoped
    // geometry call is admitted only by a live, validated shared-window note. The filler entry gives
    // the exact section and blend path; guessing a section from a truncated triangle range cannot
    // distinguish aliases. The fillers still run through their chains with the original arguments.
    // Only their wrapped CPU positions are replaced, from the dense source and current bone palette.
    // The triangle chain receives bounded local-index chunks and vertex base zero, in file order.
    //
    // A noted skin that no current certificate admits -- every skin of 65,536 vertices or fewer that
    // writes wide triangle starts, and any larger skin whose admission fails -- gets a legacy frame
    // instead, matched through the live instance's skin; the note's addresses are only comparison
    // keys. Its filler call records the exact section and its triangle call keeps the two older
    // repairs for that section's exact stock arguments: the triangle-start remap from 66a64d5 (the
    // stock test otherwise walks 65536 * level indices too early and reads before its scratch
    // window, 0x0081D569 on a 44k-triangle model, 2026-09-07) and the crossing-section skip from
    // e9c68f6 (the signed index - vertexStart goes negative, ERROR #132 at 0x0081D55C, 2026-09-23).
    // A legacy frame never repairs positions or chunks indices, so a wholly-above section of an
    // uncertified skin still picks against positions read through the 16-bit lookup. Unnoted skins,
    // and collision calls outside every geometry scope, reach the next triangle link unchanged.
    off::M2_SceneTriangleHitTestFn g_origTriangleHitTest = nullptr;
    constexpr uint32_t kPickingLogLimit = 24;
    constexpr uint32_t kLegacyWideLogBit = 8; // after the low / crossing / wholly-above bits

    // Why an admitted call was not tested with repaired data. Each reason is one sampled log bit
    // per note (0x8 << reason), so a repeated rejection stays visible after the one-time warning.
    enum class PickingReject : uint32_t
    {
        Call = 1,  // stale certificate, or a different mode, projection or distance at the fill
        Section,   // section pointer outside the certified array
        Shape,     // filler pairing, dense placement or vertex bounds
        Triangles, // triangle range
        Positions, // scratch capacity, bone palette or bone references
        Arguments, // no pending fill, a changed certificate, or not the filled section's arguments
        Window,    // a stored index outside its section window
        Changed,   // the certificate or the prepared section changed between chunks
    };

    struct PickingCall
    {
        WideSkinNote* note;
        void* scene;
        void* instance;
        const M2SkinProfile* skin;
        PickingSource source;
        uint64_t generation;
        int mode;
        float* projection;
        float distance;
        float* point;
        int candidate;
        float* bestDepth;
        M2SkinSection section = {};
        uint32_t sectionIndex = 0, first = 0, triangleStart = 0;
        uint32_t prepareEpoch = 0; // advanced by every admitted preparation; chunking stops on change
        unsigned filler = 0;
        PickingReject reject = PickingReject::Arguments;
        bool pending = false, ready = false;
        bool identified = false; // section is this frame's own array entry, snapshotted above
        bool legacy = false;     // noted but not admitted: remap and skip only, never repair
    };

    // A registry address is only a comparison key. In particular, never dereference n.skin to
    // decide whether a freed note is still valid; follow the instance the native caller owns.
    bool CurrentPickingSource(const WideSkinNote& n, void* instance)
    {
        if (!instance || !n.convertedSharedIb) return false;
        void* model = *At<void*>(instance, off::kOffInstModel);
        if (!model || n.model != model
            || *At<void*>(model, off::kOffSharedIndexBuf) != n.convertedSharedIb) return false;
        const auto* skin = *At<M2SkinProfile*>(model, off::kOffModelSkin);
        const auto* header = *At<M2Header*>(model, off::kOffModelHeader);
        return skin && header && skin == n.skin && header == n.pickingSource.header
               && skin->indices == n.indices && skin->indexCount == n.indexCount
               && skin->vertexCount == n.vertexCount && skin->vertexCount > 0x10000u
               && skin->submeshes == n.pickingSource.sections
               && skin->submeshCount == n.pickingSource.sectionCount
               && skin->vertexLookup == n.pickingSource.lookup
               && header->vertices.offset == n.pickingSource.vertices
               && header->vertices.count == n.pickingSource.vertexCount;
    }

    WideSkinNote* PickingNote(void* instance)
    {
        void* model = instance ? *At<void*>(instance, off::kOffInstModel) : nullptr;
        if (!model) return nullptr;
        for (size_t i = 0; i < g_wideSkinCount; ++i)
        {
            WideSkinNote& n = g_wideSkins[i];
            if (n.model == model && CurrentPickingSource(n, instance)) return &n;
        }
        return nullptr;
    }

    // The skin the native caller walks right now, read through the instance it owns.
    const M2SkinProfile* LivePickingSkin(void* instance)
    {
        void* model = instance ? *At<void*>(instance, off::kOffInstModel) : nullptr;
        return model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
    }

    // Address and size keys only: n.skin is compared with the live skin, never followed.
    bool LegacyNoteMatches(const WideSkinNote& n, const M2SkinProfile* liveSkin)
    {
        return liveSkin && n.skin == liveSkin && n.indices == liveSkin->indices
               && n.indexCount == liveSkin->indexCount && n.vertexCount == liveSkin->vertexCount;
    }

    // The note for a call no current certificate admits, found through the live skin alone. It
    // needs no conversion, model or source record: index-only notes are the common case.
    WideSkinNote* LegacyPickingNote(void* instance, const M2SkinProfile*& liveSkin)
    {
        liveSkin = LivePickingSkin(instance);
        if (!liveSkin || !liveSkin->indices || !liveSkin->submeshes) return nullptr;
        for (size_t i = 0; i < g_wideSkinCount; ++i)
            if (LegacyNoteMatches(g_wideSkins[i], liveSkin)) return &g_wideSkins[i];
        return nullptr;
    }

    // A legacy frame stays usable while its instance still walks the same noted skin.
    bool CurrentLegacyCall(const PickingCall& call)
    {
        const M2SkinProfile* live = LivePickingSkin(call.instance);
        return live == call.skin && LegacyNoteMatches(*call.note, live);
    }

    bool CurrentPickingCall(const PickingCall& call)
    {
        // Check the snapshot too: a nested rebuild may publish a different valid certificate in
        // the same note. It must not silently change the source of this in-flight geometry call.
        const PickingSource& live = call.note->pickingSource;
        return call.generation == call.note->pickingGeneration
               && CurrentPickingSource(*call.note, call.instance)
               && call.skin == call.note->skin && call.source.header == live.header
               && call.source.vertices == live.vertices && call.source.vertexCount == live.vertexCount
               && call.source.sections == live.sections && call.source.sectionCount == live.sectionCount
               && call.source.lookup == live.lookup;
    }

    void WarnPicking(const PickingCall& call, PickingReject reason)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            WLOG_WARN("m2native-indices: picking rejects an unsupported validated-wide call "
                      "(scene=%p instance=%p skin=%p reason=%u); no hit or depth is invented; "
                      "check source identity, section/filler pairing, bounds and bone palette; "
                      "later rejects are only sampled as m2wide-beta picking-reject records",
                      call.scene, call.instance, call.skin, static_cast<unsigned>(reason));
        }
    }

    // One sampled record per note and reason under the shared picking cap. remapped=1: a low
    // section was forwarded through the triangle-start remap; remapped=0: nothing further was tested.
    void LogPickingReject(PickingCall& call, PickingReject reason, bool remapped)
    {
        const uint32_t bit = 0x8u << static_cast<uint32_t>(reason);
        if ((call.note->pickingLogged & bit) || g_wideSkins.pickingLogs >= kPickingLogLimit) return;
        call.note->pickingLogged |= bit;
        ++g_wideSkins.pickingLogs;
        WLOG_INFO("m2wide-beta: picking-reject record=%u instance=%p skin=%p reason=%u identified=%u "
                  "section=%u vertexStartLow=%u vertices=%u indices=%u remapped=%u",
                  g_wideSkins.pickingLogs, call.instance, call.skin, static_cast<unsigned>(reason),
                  unsigned(call.identified), call.sectionIndex, unsigned(call.section.vertexStart),
                  unsigned(call.section.vertexCount), unsigned(call.section.indexCount),
                  unsigned(remapped));
        wxl::log::Flush();
    }

    void RejectPicking(PickingCall& call, PickingReject reason, bool remapped)
    {
        WarnPicking(call, reason);
        LogPickingReject(call, reason, remapped);
    }

    // Said once per note, so an admission failure of a skin above 65,536 vertices shows in the log
    // rather than only as a skipped section or wrong-place hover.
    void LogLegacyWide(WideSkinNote& note, void* instance, const M2SkinProfile& skin)
    {
        if ((note.pickingLogged & kLegacyWideLogBit) || g_wideSkins.pickingLogs >= kPickingLogLimit)
            return;
        note.pickingLogged |= kLegacyWideLogBit;
        ++g_wideSkins.pickingLogs;
        WLOG_INFO("m2wide-beta: picking-legacy record=%u instance=%p skin=%p vertices=%u "
                  "certificate=%u; no current shared-window certificate admits this wide skin, "
                  "so its triangle starts are remapped and crossing sections skipped",
                  g_wideSkins.pickingLogs, instance, &skin, skin.vertexCount,
                  unsigned(note.convertedSharedIb != nullptr));
        wxl::log::Flush();
    }

    // Keep the stack context off the unnoted path. The owner restores its predecessor on every
    // ordinary C++ return, including nested calls; it is not a lifetime pin or a thread lock.
    __declspec(noinline) int RunPickingGeometry(WideSkinNote& note, bool legacy, void* scene,
                                               void* edx, void* instance, int mode,
                                               float* projection, float distance, float* point,
                                               int candidate, float* bestDepth, int currentHit)
    {
        PickingCall call = { &note, scene, instance, note.skin, note.pickingSource, note.pickingGeneration,
                             mode, projection, distance, point, candidate, bestDepth };
        call.legacy = legacy;
        struct Scope
        {
            WideSkinNote& note;
            PickingCall* previous;
            ~Scope() { note.pickingCall = previous; }
        } scope { note, note.pickingCall };
        note.pickingCall = &call;
        return g_wideSkins.geometryNext(scene, edx, instance, mode, projection, distance,
                                        point, candidate, bestDepth, currentHit);
    }

    int __fastcall hkHitTestGeometry(void* scene, void* edx, void* instance, int mode,
                                     float* projection, float distance, float* point,
                                     int candidate, float* bestDepth, int currentHit)
    {
        WideSkinNote* note = PickingNote(instance);
        const bool legacy = !note;
        const M2SkinProfile* liveSkin = nullptr;
        if (legacy) note = LegacyPickingNote(instance, liveSkin);
        if (!note)
            return g_wideSkins.geometryNext(scene, edx, instance, mode, projection, distance,
                                            point, candidate, bestDepth, currentHit);
        if (legacy && liveSkin->vertexCount > 0x10000u) LogLegacyWide(*note, instance, *liveSkin);
        return RunPickingGeometry(*note, legacy, scene, edx, instance, mode, projection, distance,
                                   point, candidate, bestDepth, currentHit);
    }

    PickingCall* PickingFillCall(void* scene, void* instance, void* skin)
    {
        for (size_t i = 0; i < g_wideSkinCount; ++i)
        {
            PickingCall* call = g_wideSkins[i].pickingCall;
            if (call && call->scene == scene && call->instance == instance && call->skin == skin)
                return call;
        }
        return nullptr;
    }

    bool RefillPickingPositions(PickingCall& call)
    {
        const M2SkinSection& section = call.section;
        auto* dst = *At<float*>(call.scene, off::kOffSceneHitTestPositions);
        if (!dst || *At<uint32_t>(call.scene, off::kOffSceneHitTestCapacity) < section.vertexCount)
            return false;
        if (!window::NeedsPickingPositions(call.first, section.vertexCount)) return true;
        void* palette = *At<void*>(call.instance, off::kOffInstBonePalette);
        if (!palette || (call.filler == 0 && (reinterpret_cast<uintptr_t>(palette) & 15u))) return false;
        const auto* source = reinterpret_cast<const uint8_t*>(call.source.vertices);
        const uint32_t boneCount = call.source.header->bones.count;
        // Both blended fillers start with identity and a cached (weights, bones) pair of (0, 0).
        // Reuse the native blend, including its first-zero termination and its scalar/SSE choice.
        alignas(16) float blended[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        uint32_t lastWeights = 0, lastBones = 0;
        for (uint32_t k = 0; k < section.vertexCount; ++k)
        {
            const uint8_t* vertex = source + size_t(call.first + k) * off::kModelVertexStride;
            uint32_t weights, bones;
            std::memcpy(&weights, vertex + off::kOffVertexWeights, sizeof weights);
            std::memcpy(&bones, vertex + off::kOffVertexBoneSlots, sizeof bones);
            const float* matrix = blended;
            if (call.filler == 2)
            {
                if (!window::PickingBonesFit(weights, bones, boneCount, true)) return false;
                matrix = At<float>(palette, size_t(bones & 0xFFu) * off::kBonePaletteStride);
            }
            else if (weights != lastWeights || bones != lastBones)
            {
                if (!window::PickingBonesFit(weights, bones, boneCount, false)) return false;
                const uintptr_t blend = call.filler == 0 ? off::kBlendHitTestMatrixSse
                                                         : off::kBlendHitTestMatrixScalar;
                wxl::game::Native<off::M2_BlendHitTestMatrixFn>(blend)(palette, weights, bones, blended);
                lastWeights = weights;
                lastBones = bones;
            }
            float position[3];
            wxl::game::Native<off::C3_TransformFn>(off::kVec3Transform)(
                position, reinterpret_cast<const float*>(vertex + off::kOffVertexPosition), matrix);
            const auto projected = window::ProjectPickingPosition(
                position, reinterpret_cast<const float*>(vertex + off::kOffVertexNormal), matrix,
                call.mode, call.projection, call.distance);
            std::memcpy(dst + size_t(k) * 3, &projected, sizeof projected);
        }
        return true;
    }

    void PreparePickingSection(PickingCall& call, void* section, unsigned filler,
                               int mode, float* projection, float distance)
    {
        call.pending = true;
        call.ready = false;
        call.identified = false;
        ++call.prepareEpoch;
        call.reject = PickingReject::Call;
        if (!CurrentPickingCall(call) || mode != call.mode || projection != call.projection
            || std::memcmp(&distance, &call.distance, sizeof distance) != 0 || !projection)
            return;
        uint32_t index = 0;
        call.reject = PickingReject::Section;
        if (!window::ArraySlot(reinterpret_cast<uintptr_t>(section),
                                reinterpret_cast<uintptr_t>(call.source.sections),
                                call.source.sectionCount, sizeof(M2SkinSection), index)) return;
        call.sectionIndex = index;
        call.section = call.source.sections[index];
        call.filler = filler;
        call.identified = true;
        call.reject = PickingReject::Shape;
        const M2SkinSection& sec = call.section;
        if ((filler == 2) != (sec.boneInfluences == 1)
            || !WideVertexStart(*call.skin, index, call.first)
            || !window::Fits(call.first, sec.vertexStart, sec.vertexCount, call.skin->vertexCount)
            || !window::Fits(call.first, sec.vertexStart, sec.vertexCount, call.source.vertexCount)) return;
        call.triangleStart = TriangleStart(sec, *call.skin);
        call.reject = PickingReject::Triangles;
        if (!window::TriangleRange(call.triangleStart, sec.indexCount, call.skin->indexCount)) return;
        call.reject = PickingReject::Positions;
        call.ready = RefillPickingPositions(call);
    }

    // Record the exact section from the frame's own skin, followed only once CurrentLegacyCall
    // shows it is the skin the instance walks. The recorded section and the index array the
    // triangle call plans against are then one skin by construction, whatever skin argument the
    // filler was given. Nothing is filled, projected or chunked here.
    void PrepareLegacySection(PickingCall& call, void* section)
    {
        call.pending = true;
        call.identified = false;
        uint32_t index = 0;
        if (!CurrentLegacyCall(call)
            || !window::ArraySlot(reinterpret_cast<uintptr_t>(section),
                                  reinterpret_cast<uintptr_t>(call.skin->submeshes),
                                  call.skin->submeshCount, sizeof(M2SkinSection), index)) return;
        call.sectionIndex = index;
        call.section = call.skin->submeshes[index];
        call.identified = true;
    }

    template <unsigned Filler>
    void __fastcall hkFillPickingVertices(void* scene, void* edx, void* instance, void* skin,
                                          void* section, int mode, float* projection, float distance)
    {
        static_assert(Filler < 3);
        PickingCall* call = PickingFillCall(scene, instance, skin);
        if (call) call->pending = false;
        // Geometry grows its native scratch BEFORE calling a filler (0x81DC01-0x81DCAE).
        // An exact certified upper section can use the existing dense refill as its sole fill:
        // the native fillers only write these positions and would skin a wrong 16-bit lookup
        // immediately before we overwrite it. No pose/position cache or forged model is needed.
        // This deliberately replaces the filler chain for that successful case. Low, legacy,
        // unsupported and rejected calls keep forwarding. A custom downstream filler must be
        // reconciled here before it can add side effects to the successful replacement path.
        uint32_t index = 0, first = 0;
        if (call && !call->legacy && CurrentPickingCall(*call)
            && window::ArraySlot(reinterpret_cast<uintptr_t>(section),
                                 reinterpret_cast<uintptr_t>(call->source.sections),
                                 call->source.sectionCount, sizeof(M2SkinSection), index)
            && WideVertexStart(*call->skin, index, first)
            && window::NeedsPickingPositions(first, call->source.sections[index].vertexCount))
        {
            PreparePickingSection(*call, section, Filler, mode, projection, distance);
            if (call->ready) return;
            call->pending = false; // a rejected/partial preparation cannot match a nested call
        }
        g_wideSkins.fillNext[Filler](scene, edx, instance, skin, section, mode, projection, distance);
        if (!call) return;
        if (call->legacy)
            PrepareLegacySection(*call, section);
        else
            PreparePickingSection(*call, section, Filler, mode, projection, distance);
    }

    PickingCall* PickingTriangleCall(void* scene, float* point, int mode, int candidate, float* bestDepth)
    {
        for (size_t i = 0; i < g_wideSkinCount; ++i)
        {
            PickingCall* call = g_wideSkins[i].pickingCall;
            if (call && call->scene == scene && call->point == point && call->mode == mode
                && call->candidate == candidate && call->bestDepth == bestDepth) return call;
        }
        return nullptr;
    }

    void LogPicking(PickingCall& call, uint32_t chunks)
    {
        const uint32_t bit = window::CrossesWrap(call.section.vertexStart, call.section.vertexCount) ? 2u
                             : call.first >= 0x10000u ? 4u : 1u;
        if ((call.note->pickingLogged & bit) || g_wideSkins.pickingLogs >= kPickingLogLimit) return;
        call.note->pickingLogged |= bit;
        ++g_wideSkins.pickingLogs;
        const uint16_t* raw = call.skin->indices + call.triangleStart;
        WLOG_INFO("m2wide-beta: picking record=%u instance=%p skin=%p section=%u "
                  "wideStart=%u vertexStartLow=%u vertices=%u triangleStart=%u indices=%u "
                  "positionRepair=%u filler=%u chunks=%u raw=(%u,%u,%u) local=(%u,%u,%u)",
                  g_wideSkins.pickingLogs, call.instance, call.skin, call.sectionIndex,
                  call.first, unsigned(call.section.vertexStart), unsigned(call.section.vertexCount),
                  call.triangleStart, unsigned(call.section.indexCount),
                  unsigned(window::NeedsPickingPositions(call.first, call.section.vertexCount)),
                  call.filler, chunks, unsigned(raw[0]), unsigned(raw[1]), unsigned(raw[2]),
                  unsigned(window::LocalIndex(raw[0], call.section.vertexStart)),
                  unsigned(window::LocalIndex(raw[1], call.section.vertexStart)),
                  unsigned(window::LocalIndex(raw[2], call.section.vertexStart)));
        wxl::log::Flush();
    }

    // A separate, non-inlined frame is important: an unnoted triangle call must not reserve or
    // probe the 6 KiB local array. Validate the entire section before any chunk can update a hit.
    __declspec(noinline) int TestPickingSection(PickingCall& call, void* edx, float* point, int mode,
                                               int candidate, float* bestDepth, int currentHit)
    {
        // Snapshots, not references into the frame: a re-preparation of this frame during a chunk
        // rewrites its section, first and triangle start, and must not retarget later chunks.
        const M2SkinSection sec = call.section;
        const uint32_t triangleStart = call.triangleStart;
        const M2SkinProfile* const skin = call.skin;
        const uint32_t epoch = call.prepareEpoch;
        const uint16_t* source = skin->indices + triangleStart;
        for (uint32_t k = 0; k < sec.indexCount; ++k)
            if (window::LocalIndex(source[k], sec.vertexStart) >= sec.vertexCount)
            {
                RejectPicking(call, PickingReject::Window, false);
                return currentHit;
            }
        uint16_t local[window::kPickingIndexChunk];
        uint32_t chunks = 0;
        for (uint32_t done = 0; done < sec.indexCount; )
        {
            // A downstream hook may have triggered an observed rebuild or re-prepared this frame
            // during the last chunk. Keep any genuine result already returned (the native prefix),
            // but do not read the retired payload or submit a retargeted chunk.
            if (!CurrentPickingCall(call) || call.prepareEpoch != epoch)
            {
                RejectPicking(call, PickingReject::Changed, false);
                return currentHit;
            }
            const uint32_t count = window::PickingChunk(sec.indexCount - done);
            for (uint32_t k = 0; k < count; ++k)
                local[k] = window::LocalIndex(source[done + k], sec.vertexStart);
            // Every chunk passes on the last return and the same depth pointer. Neither zero nor
            // candidate substitutes for currentHit; the native triangle predicate still decides.
            currentHit = g_origTriangleHitTest(call.scene, edx, local, local + count, 0,
                                               point, mode, candidate, bestDepth, currentHit);
            done += count;
            ++chunks;
        }
        if (chunks && CurrentPickingCall(call) && call.prepareEpoch == epoch) LogPicking(call, chunks);
        return currentHit;
    }

    // An admitted call that cannot use repaired data. A low section needs no position repair, so
    // it is forwarded through the triangle-start remap with its own incoming vertex base, as the
    // legacy path does. A crossing or wholly-above section (or one whose placement is unknown) has
    // no safe stock form, so the native no-hit result is kept: currentHit, depth untouched.
    int RejectPickingSection(PickingCall& call, bool pending, bool matched, void* scratch, void* edx,
                             int vertexBase, float* point, int mode, int candidate,
                             float* bestDepth, int currentHit)
    {
        const PickingReject reason = !pending ? PickingReject::Arguments
                                     : (!call.identified || matched) ? call.reject
                                     : PickingReject::Arguments;
        const M2SkinSection& sec = call.section;
        uint32_t first = 0;
        // matched includes a current certificate, so the frame's skin is the live one here.
        if (matched && WideVertexStart(*call.skin, call.sectionIndex, first)
            && !window::NeedsPickingPositions(first, sec.vertexCount)
            && !window::CrossesWrap(sec.vertexStart, sec.vertexCount))
        {
            RejectPicking(call, reason, true);
            uint16_t* begin = call.skin->indices + TriangleStart(sec, *call.skin);
            return g_origTriangleHitTest(scratch, edx, begin, begin + sec.indexCount, vertexBase,
                                         point, mode, candidate, bestDepth, currentHit);
        }
        RejectPicking(call, reason, false);
        return currentHit;
    }

    void WarnLegacySkip(const PickingCall& call)
    {
        // Said once. The wide start is walked only for this message, so the per-frame cost of
        // the guard stays the comparisons in LegacyPickingTriangle.
        static bool warnedCrossing = false;
        if (warnedCrossing) return;
        warnedCrossing = true;
        uint32_t wideStart = 0;
        const bool wideValid = WideVertexStart(*call.skin, call.sectionIndex, wideStart);
        WLOG_WARN("m2native-indices: picking skips section %u of model=%p skin=%p "
                  "(vertexStart low=%u wide=%u wideValid=%u, vertexCount=%u) until the "
                  "hit test handles wide skins: its 16-bit indices wrap below its 16-bit "
                  "start; later skips are silent",
                  call.sectionIndex, *At<void*>(call.instance, off::kOffInstModel), call.skin,
                  unsigned(call.section.vertexStart), wideStart, unsigned(wideValid),
                  unsigned(call.section.vertexCount));
    }

    // The legacy frame's triangle call: only the exact stock arguments of the section its filler
    // recorded are changed (window::PlanLegacyTriangle). A crossing section returns currentHit with
    // depth untouched, the stock no-hit result (e9c68f6); any other is remapped to its widened
    // triangle start with the incoming vertex base (66a64d5). Everything else reaches the next
    // link as it came.
    int LegacyPickingTriangle(PickingCall& call, bool pending, void* scratch, void* edx,
                              uint16_t* indexBegin, uint16_t* indexEnd, int vertexBase,
                              float* point, int mode, int candidate, float* bestDepth,
                              int currentHit)
    {
        const M2SkinSection& sec = call.section;
        const M2SkinProfile* skin = call.skin;
        window::LegacyTriangle plan;
        // The frame's skin is followed only once it is shown to be the one the instance walks.
        if (pending && call.identified && CurrentLegacyCall(call))
            plan = window::PlanLegacyTriangle(reinterpret_cast<uintptr_t>(skin->indices), skin->indexCount,
                                              Placement(sec), reinterpret_cast<uintptr_t>(indexBegin),
                                              reinterpret_cast<uintptr_t>(indexEnd), vertexBase);
        if (plan.action == window::LegacyAction::Skip)
        {
            WarnLegacySkip(call);
            return currentHit;
        }
        if (plan.action == window::LegacyAction::Remap)
        {
            indexBegin = skin->indices + plan.triangleStart;
            indexEnd = indexBegin + sec.indexCount;
        }
        return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase,
                                     point, mode, candidate, bestDepth, currentHit);
    }

    int __fastcall hkSceneTriangleHitTest(void* scratch, void* edx, uint16_t* indexBegin, uint16_t* indexEnd,
                                          int vertexBase, float* point, int mode, int candidate,
                                          float* bestDepth, int currentHit)
    {
        PickingCall* call = PickingTriangleCall(scratch, point, mode, candidate, bestDepth);
        // Unnoted skins, and collision's call outside every geometry scope, go on as they came.
        if (!call)
            return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase,
                                         point, mode, candidate, bestDepth, currentHit);
        const bool pending = call->pending;
        call->pending = false; // a fill belongs to one triangle call, not a later batch or collision
        if (call->legacy)
            return LegacyPickingTriangle(*call, pending, scratch, edx, indexBegin, indexEnd,
                                         vertexBase, point, mode, candidate, bestDepth, currentHit);
        const M2SkinSection& sec = call->section;
        // The certificate is checked before the skin is followed to compare the stock arguments.
        const bool matched = pending && call->identified && CurrentPickingCall(*call)
                             && window::StockTriangleCall(reinterpret_cast<uintptr_t>(call->skin->indices),
                                                          Placement(sec), reinterpret_cast<uintptr_t>(indexBegin),
                                                          reinterpret_cast<uintptr_t>(indexEnd), vertexBase);
        // Empty sections already have the native no-hit meaning; preserve their chain call too,
        // without a warning and without spending the one-time warning.
        if (matched && (!sec.indexCount || !sec.vertexCount))
            return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase,
                                         point, mode, candidate, bestDepth, currentHit);
        if (matched && call->ready)
            return TestPickingSection(*call, edx, point, mode, candidate, bestDepth, currentHit);
        return RejectPickingSection(*call, pending, matched, scratch, edx, vertexBase, point, mode,
                                    candidate, bestDepth, currentHit);
    }

    uint32_t __fastcall hkSharedSetVertices(void* model, void* edx, int texCoordSet)
    {
        void* previousBuffer = model ? *At<void*>(model, off::kOffSharedVertexBuf) : nullptr;
        const auto* previousSkin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        const bool rebuilding = model && !BufferHolds(previousBuffer);
        if (rebuilding) ClearVertexRepair(model);

        // The original owns creating and sizing the pool and buffer, so it always runs first.
        const uint32_t result = g_origSharedSetVertices(model, edx, texCoordSet);
        auto* skin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        void* buffer = model ? *At<void*>(model, off::kOffSharedVertexBuf) : nullptr;
        if (model && skin != previousSkin) ClearSharedConversion(model, true);
        else if (model && buffer != previousBuffer) ClearVertexRepair(model);
        if (!result || !NeedsWideVertices(skin)) return result;
        WideSkinNote* note = NoteWideSkin(skin);
        if (!buffer) return result;
        if (!note)
        {
            if (rebuilding) RefillWideVertices(model, *skin);
            return result;
        }
        if (!rebuilding && note->model == model && note->refilledSharedVb == buffer) return result;
        RefillWideVertices(model, *skin, note);
        return result;
    }

    bool InstallWideIndices()
    {
        if (!wxl::hook::Install("M2SetModelIndices", off::kSetModelIndices,
                                &hkSetModelIndices, &g_origSetModelIndices))
            return false;
        if (!wxl::hook::Install("M2SharedSetIndices", off::kSharedSetIndices,
                                &hkSharedSetIndices, &g_origSharedSetIndices))
            return false;
        if (!wxl::hook::Install("M2DrawBatch", gxoff::kDrawTriangleBatch,
                                &hkDrawBatch, &g_origDrawBatch))
            return false;
        if (!wxl::hook::Install("GxDeviceDraw", gxoff::kGxDeviceDraw,
                                &hkDeviceDraw, &g_origDeviceDraw))
            return false;
        if (!wxl::hook::Install("M2SharedSetVertices", off::kSharedSetVertices,
                                &hkSharedSetVertices, &g_origSharedSetVertices))
            return false;
        if (!wxl::hook::Install("M2HitTestGeometry", off::kHitTestGeometry,
                                &hkHitTestGeometry, &g_wideSkins.geometryNext))
            return false;
        if (!wxl::hook::Install("M2FillHitTestVerticesSse", off::kFillHitTestVerticesSse,
                                &hkFillPickingVertices<0>, &g_wideSkins.fillNext[0]))
            return false;
        if (!wxl::hook::Install("M2FillHitTestVerticesScalar", off::kFillHitTestVerticesScalar,
                                &hkFillPickingVertices<1>, &g_wideSkins.fillNext[1]))
            return false;
        if (!wxl::hook::Install("M2FillHitTestVerticesSingle", off::kFillHitTestVerticesSingle,
                                &hkFillPickingVertices<2>, &g_wideSkins.fillNext[2]))
            return false;
        if (!wxl::hook::Install("M2SceneTriangleHitTest", off::kSceneTriangleHitTest,
                                &hkSceneTriangleHitTest, &g_origTriangleHitTest))
            return false;
        WLOG_INFO("m2native-indices: submesh triangle starts read and drawn as "
                  "(level << 16) | indexStart; dense wide-vertex refill registered; "
                  "section vertex windows apply to local indices and validated single-copy shared conversions; "
                  "picking repairs validated wide positions and uses bounded local triangle chunks; "
                  "unadmitted noted skins keep the triangle-start remap and skip crossing sections");
        return true;
    }
}

WXL_REGISTER_FEATURE("m2native-indices", true, InstallWideIndices)
