// Donor hair under helms: hide a custom body's hair volume where the helm hides a stock hairstyle.
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

#include "config.hpp"
#include "client/CharModel/CustomBody.hpp"
#include "client/CharModel/DonorHair.hpp"
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "offsets/game/DB2.hpp"
#include "offsets/game/M2.hpp"

#include <cstdint>

namespace
{
    namespace m2 = wxl::offsets::game::m2;
    namespace db = wxl::offsets::game::db2;
    namespace dh = wxl::client::donorhair;

    m2::Char_GeosetRenderPrepFn g_origGeosetPrep = nullptr;

    template <class T> T Read(uintptr_t address) { return *reinterpret_cast<const T*>(address); }

    /// A compacted DBC row by id, or 0: (id - minId) into the record table.
    uintptr_t Row(uintptr_t table, uintptr_t minId, uintptr_t maxId, uint32_t id)
    {
        const int32_t low = Read<int32_t>(minId), high = Read<int32_t>(maxId);
        const int32_t value = static_cast<int32_t>(id);
        if (!id || value < low || value > high)
            return 0;
        return Read<uintptr_t>(Read<uintptr_t>(table) + static_cast<uint32_t>(value - low) * 4);
    }

    /// Whether the helm in the component's head slot hides hair for its race and sex.
    bool HelmHidesHair(void* component)
    {
        const auto base = reinterpret_cast<uintptr_t>(component);
        const uint32_t display = Read<uint32_t>(base + m2::kOffCharComponentSlotDisplays + m2::kCharModelSlotHead * 4);
        const uint32_t race = Read<uint32_t>(base + m2::kOffCharComponentRace);
        const uint32_t sex = Read<uint32_t>(base + m2::kOffCharComponentSex);
        if (sex > 1)
            return false;
        const uintptr_t item = Row(db::itemdisplayinfo::kIdTable, db::itemdisplayinfo::kMinId,
                                   db::itemdisplayinfo::kMaxId, display);
        if (!item)
            return false;
        const uint32_t vis = Read<uint32_t>(item + db::itemdisplayinfo::kOffHelmetGeosetVis + sex * 4);
        const uintptr_t row = Row(db::helmetgeosetvisdata::kIdTable, db::helmetgeosetvisdata::kMinId,
                                  db::helmetgeosetvisdata::kMaxId, vis);
        return row && dh::HidesHair(Read<uint32_t>(row + db::helmetgeosetvisdata::kOffHair), race);
    }

    /**
     * @brief After the client's geoset pass: a donor's hair volume follows its helm.
     *
     * The pass leaves the volume's id alone (it only covers 0..2000), so it keeps whatever this
     * says last; every equip, unequip and customization change ends in this pass.
     */
    void __fastcall hkGeosetPrep(void* component, void* edx)
    {
        g_origGeosetPrep(component, edx);
        if (!component || !wxl::client::custombody::Is(component))
            return;
        void* instance = *reinterpret_cast<void**>(static_cast<uint8_t*>(component) + m2::kOffCharComponentInstance);
        if (!instance)
            return;
        const uint32_t visible = HelmHidesHair(component) ? 0 : 1;
        const auto setVisible = reinterpret_cast<m2::M2_SetGeometryVisibleFn>(m2::kSetGeometryVisible);
        for (uint32_t level = 0; level < dh::kLevels; ++level)
            setVisible(instance, nullptr, dh::VolumeId(level), dh::VolumeId(level), visible);
    }

    bool InstallDonorHair()
    {
        if (!wxl::config::Env("WXL_DONOR_HAIR", true))
        {
            WLOG_INFO("donor-hair: disabled by WXL_DONOR_HAIR=0");
            return true;
        }
        wxl::hook::Install("CharGeosetRenderPrep", m2::kCharGeosetRenderPrep, &hkGeosetPrep, &g_origGeosetPrep);
        return true;
    }
}

WXL_REGISTER_FEATURE("donor-hair", true, InstallDonorHair)
