// Custom bodies: character models that are not stock race models, as the CharModel features see them.
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

#include "client/CharModel/SkinAlpha.hpp"
#include "game/M2.hpp"
#include "offsets/game/M2.hpp"

#include <cstdint>
#include <cstring>

namespace wxl::client::custombody
{
    /// The character component's model path stem, or null while it has no model.
    inline const char* ModelStem(void* component)
    {
        namespace m2 = wxl::offsets::game::m2;
        if (!component)
            return nullptr;
        auto* base = static_cast<uint8_t*>(component);
        void* instance = *reinterpret_cast<void**>(base + m2::kOffCharComponentInstance);
        if (!instance)
            return nullptr;
        void* shared = *reinterpret_cast<void**>(static_cast<uint8_t*>(instance) + m2::kOffInstShared);
        if (!shared)
            return nullptr;
        const char* stem = wxl::game::m2::M2Model(shared).GetPathStem();
        const size_t bound = m2::kOffModelHeader - m2::kOffModelPathStem;
        return stem && std::memchr(stem, 0, bound) ? stem : nullptr;
    }

    /// Whether the component wears a custom body (a donor), not a stock race model.
    inline bool Is(void* component)
    {
        return wxl::client::skinalpha::IsCustomBody(ModelStem(component));
    }
}
