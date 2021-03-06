/* Keyleds -- Gaming keyboard tool
 * Copyright (C) 2017 Julien Hartmann, juli1.hartmann@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "keyledsd/PluginHelper.h"
#include "keyledsd/tools/utils.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

using namespace std::literals::chrono_literals;

static constexpr float pi = 3.14159265358979f;
static constexpr unsigned int accuracy = 1024;
static constexpr auto transparent = keyleds::RGBAColor{0, 0, 0, 0};

static_assert(accuracy && ((accuracy & (accuracy - 1)) == 0),
              "accuracy must be a power of two");

/****************************************************************************/

namespace keyleds::plugin {

class WaveEffect final : public SimpleEffect
{
    using KeyGroup = KeyDatabase::KeyGroup;
public:
    explicit WaveEffect(EffectService & service, milliseconds period)
     : m_service(service),
       m_period(period),
       m_keys(getConfig<KeyGroup>(service, "group")),
       m_phases(computePhases(service.keyDB(), m_keys,
                getConfig<unsigned long>(service, "length").value_or(1000u),
                float(getConfig<unsigned long>(service, "direction").value_or(0)))),
       m_colors(generateColorTable(
           getConfig<std::vector<RGBAColor>>(service, "colors").value_or(std::vector<RGBAColor>{})
       )),
       m_buffer(*service.createRenderTarget())
    {
        std::fill(m_buffer.begin(), m_buffer.end(), transparent);
    }

    static WaveEffect * create(EffectService & service)
    {
        const auto & bounds = service.keyDB().bounds();
        if (!(bounds.x0 < bounds.x1 && bounds.y0 < bounds.y1)) {
            service.log(logging::info::value, "effect requires a valid layout");
            return nullptr;
        }
        auto period = getConfig<milliseconds>(service, "period").value_or(10s);
        if (period < 1s) {
            service.log(logging::info::value, "minimum value for period is 1000ms");
            return nullptr;
        }
        return new WaveEffect(service, period);
    }

    void render(milliseconds elapsed, RenderTarget & target) override
    {
        m_time += elapsed;
        if (m_time >= m_period) { m_time -= m_period; }

        auto t = accuracy * m_time / m_period;

        if (m_keys) {
            assert(m_keys->size() == m_phases.size());
            for (KeyGroup::size_type idx = 0; idx < m_keys->size(); ++idx) {
                auto tphi = (t >= m_phases[idx] ? 0 : accuracy) + t - m_phases[idx];

                m_buffer[(*m_keys)[idx].index] = m_colors[tphi];
            }
        } else {
            const auto & keyDB = m_service.keyDB();
            assert(keyDB.size() == m_phases.size());
            for (KeyDatabase::size_type idx = 0; idx < keyDB.size(); ++idx) {
                auto tphi = (t >= m_phases[idx] ? 0 : accuracy) + t - m_phases[idx];

                m_buffer[keyDB[idx].index] = m_colors[tphi];
            }
        }
        blend(target, m_buffer);
    }

private:
    static std::vector<unsigned>
    computePhases(const KeyDatabase & keyDB, const std::optional<KeyGroup> & keys,
                  const unsigned long length, const float direction)
    {
        auto freqX = length > 0
                   ? 1000.0f / float(length) * std::sin(2.0f * pi / 360.0f * direction)
                   : 0.0f;
        auto freqY = length > 0
                   ? 1000.0f / float(length) * std::cos(2.0f * pi / 360.0f * direction)
                   : 0.0f;
        auto bounds = keyDB.bounds();

        auto keyPhase = [&](const auto & key) {
            auto x = (key.position.x0 + key.position.x1) / 2u;
            auto y = (key.position.y0 + key.position.y1) / 2u;

            // Reverse Y axis as keyboard layout uses top<down
            auto xpos = float(x - bounds.x0) / float(bounds.x1 - bounds.x0);
            auto ypos = 1.0f - float(y - bounds.x0) / float(bounds.x1 - bounds.x0);

            auto phase = std::fmod(freqX * xpos + freqY * ypos, 1.0f);
            if (phase < 0.0f) { phase += 1.0f; }
            return unsigned(phase * accuracy);
        };

        auto phases = std::vector<unsigned>();
        if (keys) {
            phases.resize(keys->size());
            std::transform(keys->begin(), keys->end(), phases.begin(), keyPhase);
        } else {
            phases.resize(keyDB.size());
            std::transform(keyDB.begin(), keyDB.end(), phases.begin(), keyPhase);
        }
        return phases;
    }

    static std::vector<RGBAColor> generateColorTable(const std::vector<RGBAColor> & colors)
    {
        std::vector<RGBAColor> table(accuracy);

        for (std::vector<RGBAColor>::size_type range = 0; range < colors.size(); ++range) {
            auto first = range * table.size() / colors.size();
            auto last = (range + 1) * table.size() / colors.size();
            auto colorA = colors[range];
            auto colorB = colors[range + 1 >= colors.size() ? 0 : range + 1];

            for (std::vector<RGBAColor>::size_type idx = first; idx < last; ++idx) {
                float ratio = float(idx - first) / float(last - first);

                table[idx] = RGBAColor{
                    RGBAColor::channel_type(colorA.red * (1.0f - ratio) + colorB.red * ratio),
                    RGBAColor::channel_type(colorA.green * (1.0f - ratio) + colorB.green * ratio),
                    RGBAColor::channel_type(colorA.blue * (1.0f - ratio) + colorB.blue * ratio),
                    RGBAColor::channel_type(colorA.alpha * (1.0f - ratio) + colorB.alpha * ratio),
                };
            }
        }
        return table;
    }

private:
    const EffectService &           m_service;
    const milliseconds              m_period;   ///< total duration of a cycle.
    const std::optional<KeyGroup>   m_keys;     ///< what keys the effect applies to.
    const std::vector<unsigned>     m_phases;   ///< one per key in m_keys or one per key in m_buffer.
                                                ///< From 0 (no phase shift) to 1000 (2*pi shift)
    const std::vector<RGBAColor>    m_colors;   ///< pre-computed color samples.

    RenderTarget &                  m_buffer;   ///< this plugin's rendered state
    milliseconds                    m_time = 0ms; ///< time since beginning of current cycle.
};

KEYLEDSD_SIMPLE_EFFECT("wave", WaveEffect);

} // namespace keyleds::plugin
