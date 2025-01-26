/**
 * @file llagentui.cpp
 * @brief Utility methods to process agent's data as slurl's etc. before displaying
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llagentui.h"

// Library includes
#include "llparcel.h"

// Viewer includes
#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llviewerparcelmgr.h"
#include "llvoavatarself.h"
#include "llslurl.h"
// [RLVa:KB] - Checked: 2010-04-04 (RLVa-1.2.0d)
#include "rlvhandler.h"
// [/RLVa:KB]

//static
void LLAgentUI::buildFullname(std::string& name)
{
    if (isAgentAvatarValid())
        name = gAgentAvatarp->getFullname();
}

//static
void LLAgentUI::buildSLURL(LLSLURL& slurl, const bool escaped /*= true*/)
{
    LLSLURL return_slurl;
    LLViewerRegion *regionp = gAgent.getRegion();
    if (regionp)
    {
        return_slurl = LLSLURL(regionp->getHGGrid(), regionp->getName(), gAgent.getPositionAgent());
    }
    slurl = return_slurl;
}

//static
BOOL LLAgentUI::checkAgentDistance(const LLVector3& pole, F32 radius)
{
    F32 delta_x = gAgent.getPositionAgent().mV[VX] - pole.mV[VX];
    F32 delta_y = gAgent.getPositionAgent().mV[VY] - pole.mV[VY];

    return  sqrt( delta_x* delta_x + delta_y* delta_y ) < radius;
}
BOOL LLAgentUI::buildLocationString(std::string& str, ELocationFormat fmt,const LLVector3& agent_pos_region)
{
    LLViewerRegion* region = gAgent.getRegion();
    LLViewerParcelMgr& parcelMGr = LLViewerParcelMgr::instance();
    LLParcel* parcel = parcelMGr.getAgentParcel();

    if (!region || !parcel) return FALSE;

    std::string remote_grid = LLGridManager::instance().getGridByProbing(region->getHGGrid());
    std::string cur_grid = LLGridManager::instance().getGrid();
    bool is_hypergrid = remote_grid != cur_grid;

    S32 pos_x = S32(agent_pos_region.mV[VX] + 0.5f);
    S32 pos_y = S32(agent_pos_region.mV[VY] + 0.5f);
    S32 pos_z = S32(agent_pos_region.mV[VZ] + 0.5f);

    // Round the numbers based on the velocity
    F32 velocity_mag_sq = gAgent.getVelocity().magVecSquared();

    const F32 FLY_CUTOFF = 6.f;     // meters/sec
    const F32 FLY_CUTOFF_SQ = FLY_CUTOFF * FLY_CUTOFF;
    const F32 WALK_CUTOFF = 1.5f;   // meters/sec
    const F32 WALK_CUTOFF_SQ = WALK_CUTOFF * WALK_CUTOFF;

    if (velocity_mag_sq > FLY_CUTOFF_SQ)
    {
        pos_x -= pos_x % 4;
        pos_y -= pos_y % 4;
    }
    else if (velocity_mag_sq > WALK_CUTOFF_SQ)
    {
        pos_x -= pos_x % 2;
        pos_y -= pos_y % 2;
    }

    // create a default name and description for the landmark
    bool rlva_has_showloc = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
    const std::string& parcel_name = rlva_has_showloc  ? RlvStrings::getString(RlvStringKeys::Hidden::Parcel) : parcelMGr.getAgentParcelName();
    const std::string& region_name = rlva_has_showloc ? RlvStrings::getString(RlvStringKeys::Hidden::Region) : region->getName();
    const std::string& grid_name = rlva_has_showloc ? RlvStrings::getString(RlvStringKeys::Hidden::Grid) : region->getHGGridName();
// [RLVa:KB] - Checked: 2010-04-04 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
    // RELEASE-RLVa: [SL-2.0.0] Check ELocationFormat to make sure our switch still makes sense
    if (rlva_has_showloc)
    {
        if (LOCATION_FORMAT_NO_MATURITY == fmt)
            fmt = LOCATION_FORMAT_LANDMARK;
        else if (LOCATION_FORMAT_FULL == fmt)
            fmt = LOCATION_FORMAT_NO_COORDS;
    }
// [/RLVa:KB]
    const std::string& sim_access_string = region->getSimAccessString();
    std::string buffer;
    if( parcel_name.empty() )
    {
        // the parcel doesn't have a name
        switch (fmt)
        {
        case LOCATION_FORMAT_LANDMARK:
            buffer = llformat("%.100s", region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL:
            buffer = is_hypergrid
                ? llformat("%s, %s", region_name.c_str(), grid_name.c_str())
                : llformat("%s", region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL_COORDS:
            buffer = is_hypergrid
                ? llformat("%s (%d, %d, %d), %s", region_name.c_str(), pos_x, pos_y, pos_z, grid_name.c_str())
                : llformat("%s (%d, %d, %d)", region_name.c_str(), pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_COORDS:
            buffer = is_hypergrid
                ? llformat("%s, %s%s%s", region_name.c_str(), grid_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str())
                : llformat("%s%s%s", region_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str());
            break;
        case LOCATION_FORMAT_NO_MATURITY:
            buffer = is_hypergrid
                ? llformat("%s (%d, %d, %d), %s", region_name.c_str(), pos_x, pos_y, pos_z, grid_name.c_str())
                : llformat("%s (%d, %d, %d)", region_name.c_str(), pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_FULL:
            buffer = is_hypergrid
                ? llformat("%s (%d, %d, %d), %s%s%s", region_name.c_str(), pos_x, pos_y, pos_z, grid_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str())
                : llformat("%s (%d, %d, %d)%s%s", region_name.c_str(), pos_x, pos_y, pos_z, sim_access_string.empty() ? "" : " - ", sim_access_string.c_str());
            break;
        }
    }
    else
    {
        // the parcel has a name, so include it in the landmark name
        switch (fmt)
        {
        case LOCATION_FORMAT_LANDMARK:
            buffer = llformat("%.100s", parcel_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL:
            buffer = is_hypergrid
                ? llformat("%s, %s, %s", parcel_name.c_str(), region_name.c_str(), grid_name.c_str())
                : llformat("%s, %s", parcel_name.c_str(), region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL_COORDS:
            buffer = llformat("%s (%d, %d, %d)", parcel_name.c_str(), pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_MATURITY:
            buffer = is_hypergrid
                ? llformat("%s, %s (%d, %d, %d), %s", parcel_name.c_str(), region_name.c_str(), pos_x, pos_y, pos_z, grid_name.c_str())
                : llformat("%s, %s (%d, %d, %d)", parcel_name.c_str(), region_name.c_str(), pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_COORDS:
            buffer = is_hypergrid
                ? llformat("%s, %s, %s%s%s", parcel_name.c_str(), region_name.c_str(), grid_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str())
                : llformat("%s, %s%s%s", parcel_name.c_str(), region_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str());
                break;
        case LOCATION_FORMAT_FULL:
            buffer = is_hypergrid
                ? llformat("%s, %s (%d, %d, %d), %s%s%s", parcel_name.c_str(), region_name.c_str(), pos_x, pos_y, pos_z, grid_name.c_str(), sim_access_string.empty() ? "" : " - ", sim_access_string.c_str())
                : llformat("%s, %s (%d, %d, %d)%s%s", parcel_name.c_str(), region_name.c_str(), pos_x, pos_y, pos_z, sim_access_string.empty() ? "" : " - ", sim_access_string.c_str());
            break;
        }
    }
    str = std::move(buffer);
    return TRUE;
}
BOOL LLAgentUI::buildLocationString(std::string& str, ELocationFormat fmt)
{
    return buildLocationString(str,fmt, gAgent.getPositionAgent());
}
