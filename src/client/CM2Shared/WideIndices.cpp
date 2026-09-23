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

    /**
     * @brief The submesh's first index into the skin's triangle array.
     * @param section  the submesh being placed.
     * @param skin     the skin profile it belongs to.
     * @return the widened start when level is a high half, else the client's own 16-bit value.
     */
    uint32_t TriangleStart(const M2SkinSection& section, const M2SkinProfile& skin)
    {
        const uint32_t wide = (static_cast<uint32_t>(section.level) << 16) | section.indexStart;
        // Phrased as a subtraction so a garbage level cannot wrap the bound it is being checked against.
        if (wide <= skin.indexCount && section.indexCount <= skin.indexCount - wide) return wide;
        return section.indexStart;
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

    // Shared with picking: a converted binding is recorded only after a completed refill.
    struct WideSkinNote
    {
        const M2SkinProfile* skin;
        const uint16_t* indices;
        uint32_t indexCount;
        uint32_t vertexCount;
        void* model;
        void* convertedSharedIb;
    };
    constexpr size_t kMaxWideSkins = 64;
    WideSkinNote g_wideSkins[kMaxWideSkins] = {};
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
                n = { skin, skin->indices, skin->indexCount, skin->vertexCount, nullptr, nullptr };
            return &n;
        }
        if (g_wideSkinCount == kMaxWideSkins) return nullptr;
        WideSkinNote& n = g_wideSkins[g_wideSkinCount++];
        n = { skin, skin->indices, skin->indexCount, skin->vertexCount, nullptr, nullptr };
        return &n;
    }

    void ClearSharedConversion(void* model)
    {
        for (size_t i = 0; i < g_wideSkinCount; ++i)
            if (g_wideSkins[i].model == model) g_wideSkins[i].convertedSharedIb = nullptr;
    }

    WideSkinNote* ConvertedBinding(void* buffer)
    {
        if (buffer)
            for (size_t i = 0; i < g_wideSkinCount; ++i)
                if (g_wideSkins[i].convertedSharedIb == buffer) return &g_wideSkins[i];
        return nullptr;
    }

    // This is a fill-time check, not a per-draw scan of all triangle payloads.
    bool CanWindowSharedIndices(void* model, const M2SkinProfile* skin)
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
            for (uint32_t k = 0; k < s.indexCount; ++k)
                if (window::LocalIndex(skin->indices[start + k], s.vertexStart) >= s.vertexCount) return false;
            for (uint32_t k = 0; k < s.vertexCount; ++k)
                if (skin->vertexLookup[first + k] != uint16_t(first + k)) return false;
            first += s.vertexCount;
            written += s.indexCount;
        }
        return first == skin->vertexCount && written == skin->indexCount;
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
                             WideSkinNote* windowNote = nullptr)
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
        if (windowNote)
        {
            windowNote->model = model;
            windowNote->convertedSharedIb = buffer;
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
    void RefillWideVertices(void* model, const M2SkinProfile& skin)
    {
        void* device = wxl::game::gx::RawGraphicsDevice();
        void* buffer = *At<void*>(model, off::kOffSharedVertexBuf);
        auto* header = *At<M2Header*>(model, off::kOffModelHeader);
        if (!device || !buffer || !header || !skin.bones) return;

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
        auto* skin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        const bool rebuilding = model && !BufferHolds(*At<void*>(model, off::kOffSharedIndexBuf));
        if (rebuilding) ClearSharedConversion(model);

        // The original owns creating the pool/buffer pair and sizing it, so it always runs first.
        const uint32_t result = g_origSharedSetIndices(model, edx);
        if (!result || !rebuilding) return result;
        const bool windows = CanWindowSharedIndices(model, skin);
        if (!UsesWideStarts(skin) && !windows) return result;
        WideSkinNote* note = NoteWideSkin(skin);
        RefillSharedIndices(model, *skin, UsesGlobalIndices(model), windows ? note : nullptr);
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

    // ---- picking: the scene's ray-versus-geometry pass hands the triangle test a [begin, end) pair it
    // computed from the submesh's 16-bit start, so a widened submesh is tested 65536 indices too early
    // and the test walks off its scratch window (crash at 0x0081D569 on a 44k-triangle model, 2026-09-07).
    // A wide skin is noted when its index buffer is filled (not before, and not past the registry); a pair that lands inside a noted skin's
    // triangle array and matches a submesh's low-16 start and count is remapped to the widened start.
    // The vertex side has no such repair. The test subtracts the section's 16-bit vertex start from
    // each 16-bit index as a signed value, so in a section whose vertices pass a multiple of 65536 the
    // wrapped indices fall below the start and the test reads before its scratch buffer (ERROR #132 at
    // 0x0081D55C on the cursor's first hover over Kasumi's hair, 2026-09-23). Until the test is handed a
    // rebased copy with vertex base 0, a matched section that crosses is skipped: currentHit returned
    // unchanged, with *bestDepth untouched, is the stock result for a range that hits nothing
    // (0x0081D66E), so a hit found earlier in the walk survives. The skipped section cannot be hovered
    // meanwhile, and a section wholly above 65536 still picks against positions read through the 16-bit
    // vertex lookup.
    off::M2_SceneTriangleHitTestFn g_origTriangleHitTest = nullptr;

    int __fastcall hkSceneTriangleHitTest(void* scratch, void* edx, uint16_t* indexBegin, uint16_t* indexEnd,
                                          int vertexBase, float* point, int mode, int candidate,
                                          float* bestDepth, int currentHit)
    {
        if (indexBegin && indexEnd > indexBegin)
        {
            for (size_t i = 0; i < g_wideSkinCount; ++i)
            {
                const WideSkinNote& n = g_wideSkins[i];
                if (indexBegin < n.indices || indexBegin >= n.indices + n.indexCount) continue;
                const M2SkinProfile& skin = *n.skin;
                if (skin.indices != n.indices || skin.indexCount != n.indexCount || !skin.submeshes) break;
                const uint32_t low   = static_cast<uint32_t>(indexBegin - n.indices);
                const uint32_t count = static_cast<uint32_t>(indexEnd - indexBegin);
                if (low > 0xFFFFu) break;      // already a full address: not a truncated one
                for (uint32_t k = 0; k < skin.submeshCount; ++k)
                {
                    const M2SkinSection& sec = skin.submeshes[k];
                    if (sec.indexStart != low || sec.indexCount != count) continue;
                    if (window::CrossesWrap(sec.vertexStart, sec.vertexCount))
                    {
                        // Said once. The wide start is walked only for this message, so the per-frame
                        // cost of the guard stays the one comparison above.
                        static bool warnedCrossing = false;
                        if (!warnedCrossing)
                        {
                            warnedCrossing = true;
                            uint32_t wideStart = 0;
                            const bool wideValid = WideVertexStart(skin, k, wideStart);
                            WLOG_WARN("m2native-indices: picking skips section %u of model=%p skin=%p "
                                      "(vertexStart low=%u wide=%u wideValid=%u, vertexCount=%u) until the "
                                      "hit test handles wide skins: its 16-bit indices wrap below its 16-bit "
                                      "start; later skips are silent",
                                      k, n.model, n.skin, unsigned(sec.vertexStart), wideStart,
                                      unsigned(wideValid), unsigned(sec.vertexCount));
                        }
                        return currentHit;
                    }
                    const uint32_t wide = TriangleStart(sec, skin);
                    if (wide != sec.indexStart)
                    {
                        indexBegin = const_cast<uint16_t*>(n.indices) + wide;
                        indexEnd   = indexBegin + count;
                    }
                    break;
                }
                break;
            }
        }
        return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase, point, mode, candidate, bestDepth, currentHit);
    }

    uint32_t __fastcall hkSharedSetVertices(void* model, void* edx, int texCoordSet)
    {
        auto* skin = model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;
        const bool rebuilding = model && !BufferHolds(*At<void*>(model, off::kOffSharedVertexBuf));

        // The original owns creating and sizing the pool and buffer, so it always runs first.
        const uint32_t result = g_origSharedSetVertices(model, edx, texCoordSet);
        if (!result || !rebuilding || !NeedsWideVertices(skin)) return result;
        RefillWideVertices(model, *skin);
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
        if (!wxl::hook::Install("M2SceneTriangleHitTest", off::kSceneTriangleHitTest,
                                &hkSceneTriangleHitTest, &g_origTriangleHitTest))
            return false;
        WLOG_INFO("m2native-indices: submesh triangle starts read and drawn as "
                  "(level << 16) | indexStart; dense wide-vertex refill registered; "
                  "section vertex windows apply to local indices and validated single-copy shared conversions; "
                  "picking skips noted sections whose vertices cross a 16-bit wrap");
        return true;
    }
}

WXL_REGISTER_FEATURE("m2native-indices", true, InstallWideIndices)
