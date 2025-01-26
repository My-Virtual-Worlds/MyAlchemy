/**
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
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

#include "llgroupactions.h"

#include "message.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llgroupmgr.h"
#include "llfloatergroupprofile.h"
#include "llfloaterimcontainer.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llstartup.h"
#include "llstatusbar.h"    // can_afford_transaction()
#include "groupchatlistener.h"
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0)
#include "llslurl.h"
#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvhandler.h"
// [/RLVa:KB]

//
// Globals
//
static GroupChatListener sGroupChatListener;

class LLGroupCommandHandler : public LLCommandHandler
{
public:
    // requires trusted browser to trigger
    LLGroupCommandHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }

    virtual bool canHandleUntrusted(
        const LLSD& params,
        const LLSD& query_map,
        LLMediaCtrl* web,
        const std::string& nav_type)
    {
        if (params.size() < 1)
        {
            return true; // don't block, will fail later
        }

        if (nav_type == NAV_TYPE_CLICKED
            || nav_type == NAV_TYPE_EXTERNAL)
        {
            return true;
        }

        const std::string verb = params[0].asString();
        if (verb == "create")
        {
            return false;
        }
        return true;
    }

    bool handle(const LLSD& tokens,
                const LLSD& query_map,
                const std::string& grid,
                LLMediaCtrl* web)
    {
        if (LLStartUp::getStartupState() < STATE_STARTED)
        {
            return true;
        }

        if (tokens.size() < 1)
        {
            return false;
        }

        if (tokens[0].asString() == "create")
        {
            LLGroupActions::createGroup();
            return true;
        }

        if (tokens.size() < 2)
        {
            return false;
        }

        if (tokens[0].asString() == "list")
        {
            if (tokens[1].asString() == "show")
            {
                LLSD params;
                params["people_panel_tab_name"] = "groups_panel";
                LLFloaterSidePanelContainer::showPanel("people", "panel_people", params);
                return true;
            }
            return false;
        }

        LLUUID group_id;
        if (!group_id.set(tokens[0].asStringRef(), FALSE))
        {
            return false;
        }

        if (tokens[1].asString() == "about")
        {
            if (group_id.isNull())
                return true;

            LLGroupActions::show(group_id);

            return true;
        }
        if (tokens[1].asString() == "inspect")
        {
            if (group_id.isNull())
                return true;
            LLGroupActions::inspect(group_id);
            return true;
        }
        return false;
    }
};
LLGroupCommandHandler gGroupHandler;

// This object represents a pending request for specified group member information
// which is needed to check whether avatar can leave group
class LLFetchGroupMemberData : public LLGroupMgrObserver
{
public:
    LLFetchGroupMemberData(const LLUUID& group_id) :
        mGroupId(group_id),
        mRequestProcessed(false),
        LLGroupMgrObserver(group_id)
    {
        LL_INFOS() << "Sending new group member request for group_id: "<< group_id << LL_ENDL;
        LLGroupMgr* mgr = LLGroupMgr::getInstance();
        // register ourselves as an observer
        mgr->addObserver(this);
        // send a request
        mgr->sendGroupPropertiesRequest(group_id);
        mgr->sendCapGroupMembersRequest(group_id);
    }

    ~LLFetchGroupMemberData()
    {
        if (!mRequestProcessed)
        {
            // Request is pending
            LL_WARNS() << "Destroying pending group member request for group_id: "
                << mGroupId << LL_ENDL;
        }
        // Remove ourselves as an observer
        LLGroupMgr::getInstance()->removeObserver(this);
    }

    void changed(LLGroupChange gc)
    {
        if (gc == GC_PROPERTIES && !mRequestProcessed)
        {
            LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
            if (!gdatap)
            {
                LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData() was NULL" << LL_ENDL;
            }
            else if (!gdatap->isMemberDataComplete())
            {
                LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData()->isMemberDataComplete() was FALSE" << LL_ENDL;
                processGroupData();
                mRequestProcessed = true;
            }
        }
    }

    LLUUID getGroupId() { return mGroupId; }
    virtual void processGroupData() = 0;
protected:
    LLUUID mGroupId;
    bool mRequestProcessed;
};

class LLFetchLeaveGroupData: public LLFetchGroupMemberData
{
public:
     LLFetchLeaveGroupData(const LLUUID& group_id)
         : LLFetchGroupMemberData(group_id)
     {}
     void processGroupData()
     {
         LLGroupActions::processLeaveGroupDataResponse(mGroupId);
     }
     void changed(LLGroupChange gc)
     {
         if (gc == GC_PROPERTIES && !mRequestProcessed)
         {
             LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
             if (!gdatap)
             {
                 LL_WARNS() << "GroupData was NULL" << LL_ENDL;
             }
             else
             {
                 processGroupData();
                 mRequestProcessed = true;
             }
         }
     }
};

LLFetchLeaveGroupData* gFetchLeaveGroupData = NULL;

// static
void LLGroupActions::search()
{
    LLFloaterReg::showInstance("search", LLSD().with("category", "groups"));
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
    // create a new group voice session
    LLGroupData gdata;

    if (!gAgent.getGroupData(group_id, gdata))
    {
        LL_WARNS() << "Error getting group data" << LL_ENDL;
        return;
    }

// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
    if (!RlvActions::canStartIM(group_id))
    {
        make_ui_sound("UISndInvalidOp");
        RlvUtil::notifyBlocked(RlvStringKeys::Blocked::StartIm, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
        return;
    }
// [/RLVa:KB]

    LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, LLSD());
    if (session_id == LLUUID::null)
    {
        LL_WARNS() << "Error adding session" << LL_ENDL;
        return;
    }

    // start the call
    gIMMgr->autoStartCallOnStartup(session_id);

    make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
    if (!gAgent.canJoinGroups())
    {
        LLNotificationsUtil::add("JoinedTooManyGroups");
        return;
    }

    LLGroupMgrGroupData* gdatap =
        LLGroupMgr::getInstance()->getGroupData(group_id);

    if (gdatap)
    {
        S32 cost = gdatap->mMembershipFee;
        LLSD args;
        args["COST"] = llformat("%d", cost);
        args["NAME"] = gdatap->mName;
        LLSD payload;
        payload["group_id"] = group_id;

        if (can_afford_transaction(cost))
        {
            if(cost > 0)
                LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
            else
                LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);

        }
        else
        {
            LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
        }
    }
    else
    {
        LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData(" << group_id
            << ") was NULL" << LL_ENDL;
    }
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

    if (option == 1)
    {
        // user clicked cancel
        return false;
    }

    LLGroupMgr::getInstance()->
        sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
    return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
//  if (group_id.isNull())
// [RLVa:KB] - Checked: RLVa-1.3.0
    if ( (group_id.isNull()) || ((gAgent.getGroupID() == group_id) && (!RlvActions::canChangeActiveGroup())) )
// [/RLVa:KB]
    {
        return;
    }

    LLGroupData group_data;
    if (gAgent.getGroupData(group_id, group_data))
    {
        LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
        if (!gdatap || !gdatap->isMemberDataComplete())
        {
            if (gFetchLeaveGroupData != NULL)
            {
                delete gFetchLeaveGroupData;
                gFetchLeaveGroupData = NULL;
            }
            gFetchLeaveGroupData = new LLFetchLeaveGroupData(group_id);
        }
        else
        {
            processLeaveGroupDataResponse(group_id);
        }
    }
}

//static
void LLGroupActions::processLeaveGroupDataResponse(const LLUUID group_id)
{
    LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
    LLUUID agent_id = gAgent.getID();
    LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.find(agent_id);
    //get the member data for the group
    if ( mit != gdatap->mMembers.end() )
    {
        LLGroupMemberData* member_data = (*mit).second.get();

        if ( member_data && member_data->isOwner() && gdatap->mMemberCount == 1)
        {
            LLNotificationsUtil::add("OwnerCannotLeaveGroup");
            return;
        }
    }
    LLSD args;
    args["GROUP"] = gdatap->mName;
    LLSD payload;
    payload["group_id"] = group_id;
    if (gdatap->mMembershipFee > 0)
    {
        args["COST"] = gdatap->mMembershipFee;
    LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
}
    else
    {
        LLNotificationsUtil::add("GroupLeaveConfirmMemberNoFee", args, payload, onLeaveGroup);
    }

}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
// [RLVa:KB] - Checked: RLVa-1.3.0
    if ( (!RlvActions::canChangeActiveGroup()) && (gRlvHandler.getAgentGroup() != group_id) )
    {
        return;
    }
// [/RLVa:KB]

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_ActivateGroup);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->addUUIDFast(_PREHASH_GroupID, group_id);
    gAgent.sendReliableMessage();
}

static bool isGroupUIVisible()
{
    static LLPanel* panel = 0;
    if(!panel)
        panel = LLFloaterSidePanelContainer::getPanel("people", "panel_group_info_sidetray");
    if(!panel)
        return false;
    return panel->isInVisibleChain();
}

// static
void LLGroupActions::inspect(const LLUUID& group_id)
{
    LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
}

// static
void LLGroupActions::show(const LLUUID &group_id, bool expand_notices_tab)
{
    if (group_id.isNull())
        return;

    LLSD params;
    params["group_id"] = group_id;
    if (expand_notices_tab)
    {
        params["action"] = "show_notices";
    }

    if (gSavedSettings.getBool("ShowGroupFloaters"))
    {
        LLFloaterGroupProfile::showInstance(params, TRUE);
    }
    else
    {
        LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
        LLFloater *floater = LLFloaterReg::getTypedInstance<LLFloaterSidePanelContainer>("people");
        if (!floater->isFrontmost())
        {
            floater->setVisibleAndFrontmost(TRUE, params);
        }
    }
}


// [SL:KB] - Patch: Notification-GroupCreateNotice | Checked: 2012-02-16 (Catznip-3.2)
// static
void LLGroupActions::showNotices(const LLUUID& group_id)
{
    if (group_id.isNull())
        return;

    LLSD sdParams;
    sdParams["group_id"] = group_id;
    sdParams["action"] = "view_notices";

    if (gSavedSettings.getBool("ShowGroupFloaters"))
    {
        LLFloaterGroupProfile::showInstance(sdParams, TRUE);
    }
    else
    {
        LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", sdParams);
    }
}

// static
void LLGroupActions::viewChatHistory(const LLUUID& group_id)
{
    LLFloaterReg::showInstance("preview_conversation", group_id, true);
}
// [/SL:KB]

void LLGroupActions::refresh_notices(const LLUUID& group_id)
{
    LLSD params;
    params["group_id"] = group_id;
    params["action"] = "refresh_notices";

    if (gSavedSettings.getBool("ShowGroupFloaters"))
    {
        if (LLFloaterReg::instanceVisible("group_profile", LLSD(group_id)))
        {
            LLFloaterGroupProfile::showInstance(params, FALSE);
        }
    }
    else
    {
        if (isGroupUIVisible())
        {
            LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
        }
    }
}

//static
void LLGroupActions::refresh(const LLUUID& group_id)
{
    LLSD params;
    params["group_id"] = group_id;
    params["action"] = "refresh";

    if (gSavedSettings.getBool("ShowGroupFloaters"))
    {
        if (LLFloaterReg::instanceVisible("group_profile", LLSD(group_id)))
        {
            LLFloaterGroupProfile::showInstance(params, TRUE);
        }
    }
    else
    {
        if (isGroupUIVisible())
        {
            LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
        }
    }
}

//static
void LLGroupActions::createGroup()
{
    LLSD params;
    params["group_id"] = LLUUID::null;
    params["action"] = "create";

    if (gSavedSettings.getBool("ShowGroupFloaters"))
    {
        LLFloaterGroupProfile::showInstance(params, TRUE);
    }
    else
    {
        LLFloaterSidePanelContainer::showPanel("people", "panel_group_creation_sidetray", params);
    }
}
//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
    LLFloaterReg::hideInstance("group_profile", LLSD(group_id));

    if (isGroupUIVisible())
    {
        LLSD params;
        params["group_id"] = group_id;
        params["action"] = "close";
        LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
    }
}


// static
LLUUID LLGroupActions::startIM(const LLUUID& group_id)
{
    if (group_id.isNull()) return LLUUID::null;

// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
    if (!RlvActions::canStartIM(group_id))
    {
        make_ui_sound("UISndInvalidOp");
        RlvUtil::notifyBlocked(RlvStringKeys::Blocked::StartIm, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
        return LLUUID::null;
    }
// [/RLVa:KB]

    LLGroupData group_data;
    if (gAgent.getGroupData(group_id, group_data))
    {
        // Unmute the group if the user tries to start a session with it.
        LLMuteList::instance().removeGroup(group_id);
        LLUUID session_id = gIMMgr->addSession(
            group_data.mName,
            IM_SESSION_GROUP_START,
            group_id);
        if (session_id.notNull())
        {
            LLFloaterIMContainer::getInstance()->showConversation(session_id);
        }
        make_ui_sound("UISndStartIM");
        return session_id;
    }
    else
    {
        // this should never happen, as starting a group IM session
        // relies on you belonging to the group and hence having the group data
        make_ui_sound("UISndInvalidOp");
        return LLUUID::null;
    }
}

// [SL:KB] - Patch: Chat-GroupSnooze | Checked: Catznip-3.3

static void close_group_im(const LLUUID& group_id, LLIMModel::LLIMSession::SCloseAction close_action, int snooze_duration = -1)
{
    if (group_id.isNull())
        return;

    LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
    if (session_id.notNull())
    {
        LLIMModel::LLIMSession* pIMSession = LLIMModel::getInstance()->findIMSession(session_id);
        if (pIMSession)
        {
            pIMSession->mCloseAction = close_action;
            pIMSession->mSnoozeDuration = snooze_duration;
        }

        gIMMgr->leaveSession(session_id);
    }
}

void LLGroupActions::leaveIM(const LLUUID& group_id)
{
    close_group_im(group_id, LLIMModel::LLIMSession::SCloseAction::CLOSE_LEAVE);
}

void LLGroupActions::snoozeIM(const LLUUID& group_id, int snooze_duration /*=-1*/)
{
    close_group_im(group_id, LLIMModel::LLIMSession::SCloseAction::CLOSE_SNOOZE, snooze_duration);
}

void LLGroupActions::endIM(const LLUUID& group_id)
{
    close_group_im(group_id, LLIMModel::LLIMSession::SCloseAction::CLOSE_DEFAULT);
}

// [/SL:KB]

// static
//void LLGroupActions::endIM(const LLUUID& group_id)
//{
//  if (group_id.isNull())
//      return;
//
//  LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
//  if (session_id.notNull())
//  {
//      gIMMgr->leaveSession(session_id);
//  }
//}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
    // *TODO: Move all the LLAgent group stuff into another class, such as
    // this one.
    return gAgent.isInGroup(group_id);
}

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
    if(group_id.isNull() || avatar_id.isNull())
    {
        return false;
    }

    LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
    if(!group_data)
    {
        return false;
    }

    if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
    {
        return false;
    }

    return true;
}

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLUUID group_id = notification["payload"]["group_id"].asUUID();
    if(option == 0)
    {
        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_LeaveGroupRequest);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
        msg->nextBlockFast(_PREHASH_GroupData);
        msg->addUUIDFast(_PREHASH_GroupID, group_id);
        gAgent.sendReliableMessage();
    }
    return false;
}
