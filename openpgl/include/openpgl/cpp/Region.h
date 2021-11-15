// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../openpgl.h"

#include "Distribution.h"

namespace openpgl
{
namespace cpp
{

/**
 * @brief 
 * 
 * @deprecated Thinking about hiding the existance of the Region structure from the user
 */
struct Region
{
    Region(PGLRegion regionHandle);

    /**
     * @brief Returns the internal flag if the current Region is valid or not.
     * A Region is not valid if the containing appproximation of the local radiance distribution 
     * is not trained yet or if a previous training pass was erroneous and resulted in an invalid
     * approximation (e.g., caused by numerical problems).  
     * 
     * @return true 
     * @return false 
     */
    //bool GetValid() const;

    /**
     * @brief Returns a copy of the approximation of the local radiance Distribution stored inside the spatial Region.
     * Based on the used ibnternal representation the Distribution can be constant accross the spatial area covered by
     * the Region or dependent on a specific position inside the Region (e.g., parallax-aware mixtures).
     * 
     * @param samplePosition sampling/querying positing inside the Region
     * @param useParallaxComp if parallax compensation (if supported) should be used
     * @return Distribution 
     */
    //Distribution GetDistribution(pgl_point3f samplePosition, const bool &useParallaxComp) const;

    friend class SurfaceSamplingDistribution;
    friend class VolumeSamplingDistribution;
    private:
        PGLRegion m_regionHandle{nullptr};
};

OPENPGL_INLINE Region::Region(PGLRegion regionHandle)
{
    m_regionHandle = regionHandle;
}
/*
OPENPGL_INLINE bool Region::GetValid()const
{
    if(m_regionHandle)
        return pglRegionGetValid(m_regionHandle);
    else
        return false;
}


OPENPGL_INLINE Distribution Region::GetDistribution(pgl_point3f samplePosition, const bool &useParallaxComp) const
{
    OPENPGL_ASSERT(m_regionHandle);
    PGLDistribution distributionHandle = pglRegionGetDistribution(m_regionHandle, samplePosition, useParallaxComp);
    return Distribution(distributionHandle);
}
*/
}
}