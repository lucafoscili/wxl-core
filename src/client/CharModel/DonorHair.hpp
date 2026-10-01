// Donor hair under helms: the decision, free of client addresses so it can be tested.
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

#pragma once

#include <cstdint>

/**
 * A stock character swaps its hairstyle for the "no hair" geoset under a helm that hides hair for
 * its race; its scalp stays. A custom body (a donor) marks the hair VOLUME it can lose -- strand
 * cards -- with one reserved geoset id, and keeps its roots (the hair cap) in the base geoset, so
 * the same helm hides the volume and leaves a hairline instead of baldness. The id sits above the
 * 0..2000 range the client's geoset pass blanket-hides, and clear of retail-era groups such as
 * the HD bodies' bare feet (2001, which heel items hide), so only this feature ever toggles it.
 */
namespace wxl::client::donorhair
{
    /// The geoset id a donor's helm-hidden hair volume carries (Velora's writer assigns it).
    constexpr uint32_t kHairVolumeGeoset = 9001;

    /// How many vertex-window levels a section id can carry above it ((level << 16) | id).
    constexpr uint32_t kLevels = 4;

    /// Whether a helm's HelmetGeosetVisData hair mask hides hair for this race.
    constexpr bool HidesHair(uint32_t hairRaceMask, uint32_t race)
    {
        return race < 32 && (hairRaceMask & (1u << race)) != 0;
    }

    /// The section id the volume presents at a vertex-window level.
    constexpr uint32_t VolumeId(uint32_t level) { return (level << 16) | kHairVolumeGeoset; }
}
