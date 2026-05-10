/*
 * tomography.cpp
 *
 * Zero-lag tomography pipeline
 */

#include "tomography.h"

#include <vector>
#include <mutex>
#include <cmath>
#include <glm/glm.hpp>

#include "globals.h"
#include "GUI.h"
#include "tickbox.h"
#include "radio.h"
#include "config.h"
#include "draw_utils.h"
#include "render_pipeline.h"
#include "model/simulation.h"

namespace tomography
{

struct TomoState
{
    bool initialized = false;
    bool needsUpdate = true;

    float sliderPosition = 0.5f;
    float lastUpdatedPosition = -1.0f;

    std::vector<VoxelSnapshot> snapshot;
};

static TomoState state;

std::mutex gVoxelBufferMutex;

static inline int mod(int x, int m)
{
    return (x % m + m) % m;
}

void init()
{
    state.snapshot.clear();

    state.sliderPosition = 0.5f;
    state.lastUpdatedPosition = -1.0f;

    const int EL = automaton::EL;
    if (EL > 0) {
        unsigned center = static_cast<unsigned>(0.5f * (EL - 1) + 0.5f);
        tomo_x = center;
        tomo_y = center;
        tomo_z = center;
    }

    state.needsUpdate = true;
    state.initialized = true;
}

void shutdown()
{
    std::lock_guard<std::mutex> lock(gVoxelBufferMutex);

    state.snapshot.clear();

    state.initialized = false;
}

void requestUpdate()
{
    state.needsUpdate = true;
    tomographyNeedsUpdate = true;
}

float getSlicePosition()
{
    return state.sliderPosition;
}

void setSlicePosition(float value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    state.sliderPosition = value;

    const int EL = automaton::EL;
    if (EL > 0) {
        unsigned idx = static_cast<unsigned>(value * (EL - 1) + 0.5f);
        tomo_x = idx;
        tomo_y = idx;
        tomo_z = idx;
    }

    state.needsUpdate = true;
}

void updateSnapshot()
{
    try
    {
        if (!state.initialized)
            return;

        if (automaton::EL == 0)
            return;

        if (voxels.empty())
            return;

        if (!tomoEnable)
            return;

        if (!tomoEnable->getState())
        {
            state.snapshot.clear();
            state.needsUpdate = false;
            return;
        }

        if (tomoDirs.size() < 3)
            return;

        if (std::abs(
                state.sliderPosition -
                state.lastUpdatedPosition) < 0.0001f
            && !state.needsUpdate)
        {
            return;
        }

        std::vector<VoxelSnapshot> nextSnapshot;

        const int EL = automaton::EL;

        nextSnapshot.reserve(
            static_cast<size_t>(EL) *
            static_cast<size_t>(EL));

        std::unique_lock<std::mutex> lock(
            gVoxelBufferMutex,
            std::defer_lock);

        if (!lock.try_lock())
            return;

        unsigned sliceIndex =
            static_cast<unsigned>(
                state.sliderPosition *
                (EL - 1) + 0.5f);

        int plane = 0;

        if (tomoDirs[0].isSelected())
            plane = 0;
        else if (tomoDirs[1].isSelected())
            plane = 1;
        else
            plane = 2;

        const int CENTER_INT = EL / 2;

        const float CELL_SPACING =
            0.5f / static_cast<float>(EL);

        const size_t totalVoxels =
            voxels.size();

        for (unsigned a = 0; a < (unsigned)EL; ++a)
        {
            for (unsigned b = 0; b < (unsigned)EL; ++b)
            {
                unsigned x;
                unsigned y;
                unsigned z;

                if (plane == 0)
                {
                    x = a;
                    y = b;
                    z = sliceIndex;
                }
                else if (plane == 1)
                {
                    x = sliceIndex;
                    y = a;
                    z = b;
                }
                else
                {
                    x = b;
                    y = sliceIndex;
                    z = a;
                }

                size_t idx =
                    static_cast<size_t>(x) * EL * EL +
                    static_cast<size_t>(y) * EL +
                    static_cast<size_t>(z);

                if (idx >= totalVoxels)
                    continue;

                uint32_t color =
                    voxels[idx];

                if (color == 0)
                    continue;

                float px =
                    (mod(
                        (int)x + gConfig.view.vis_dx,
                        EL) - CENTER_INT)
                    * CELL_SPACING;

                float py =
                    (mod(
                        (int)y + gConfig.view.vis_dy,
                        EL) - CENTER_INT)
                    * CELL_SPACING;

                float pz =
                    (mod(
                        (int)z + gConfig.view.vis_dz,
                        EL) - CENTER_INT)
                    * CELL_SPACING;

                nextSnapshot.push_back({
                    glm::vec3(px, py, pz),
                    color
                });
            }
        }

        state.snapshot.swap(nextSnapshot);

        state.lastUpdatedPosition =
            state.sliderPosition;

        state.needsUpdate = false;
    }
    catch (...)
    {
        state.snapshot.clear();
    }
}

void update()
{
    updateSnapshot();
}

bool isVoxelVisible(unsigned x,
                    unsigned y,
                    unsigned z)
{
    if (!tomoEnable)
        return true;

    if (!tomoEnable->getState())
        return true;

    if (tomoDirs.size() < 3)
        return true;

    if (tomoDirs[0].isSelected())
        return z == tomo_z;

    if (tomoDirs[1].isSelected())
        return x == tomo_x;

    if (tomoDirs[2].isSelected())
        return y == tomo_y;

    return true;
}

void renderTomoPlane()
{
    // Rendering moved to GUI_3D.cpp
}

void renderSlice()
{
    renderTomoPlane();
}

void renderControls()
{
    // UI handled elsewhere
}

const std::vector<VoxelSnapshot>& getSnapshot()
{
    return state.snapshot;
}

} // namespace tomography