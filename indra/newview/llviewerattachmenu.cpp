/**
 * @file llviewerattachmenu.cpp
 * @brief "Attach to" / "Attach to HUD" submenus.
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

#include "llviewerattachmenu.h"

// project includes
#include "llagent.h"
#include "llinventorybridge.h" // for rez_attachment()
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llviewermenu.h" // for gMenuHolder
#include "llvoavatarself.h"

// linden libraries
#include "llmenugl.h"
#include "lltrans.h"

// static
void LLViewerAttachMenu::populateMenus(const std::string& attach_to_menu_name, const std::string& attach_to_hud_menu_name)
{
    // *TODO: share this code with other similar menus
    // (inventory panel context menu, in-world object menu).

    if (attach_to_menu_name.empty() || attach_to_hud_menu_name.empty() || !isAgentAvatarValid()) return;

    LLContextMenu* attach_menu = gMenuHolder->getChild<LLContextMenu>(attach_to_menu_name);
    LLContextMenu* attach_hud_menu = gMenuHolder->getChild<LLContextMenu>(attach_to_hud_menu_name);

    if (!attach_menu || attach_menu->getChildCount() != 0 ||
        !attach_hud_menu || attach_hud_menu->getChildCount() != 0)
    {
        return;
    }

    // Populate "Attach to..." / "Attach to HUD..." submenus.
    for (const auto& attach_pair : gAgentAvatarp->mAttachmentPoints)
    {
        LLViewerJointAttachment* attachment = attach_pair.second;
        LLMenuItemCallGL::Params p;
        std::string submenu_name = attachment->getName();
        std::string translated_submenu_name;

        if (LLTrans::findString(translated_submenu_name, submenu_name))
        {
            p.name = attachment->getIsHUDAttachment() ? translated_submenu_name : fmt::format(FMT_STRING("{} ({})"), translated_submenu_name, attach_pair.first);
        }
        else
        {
            p.name = attachment->getIsHUDAttachment() ? submenu_name : fmt::format(FMT_STRING("{} ({})"), submenu_name, attach_pair.first);
        }

        LLSD cbparams;
        cbparams["index"] = attach_pair.first;
        cbparams["label"] = p.name;
        p.on_click.function_name = "Object.Attach";
        p.on_click.parameter = LLSD(attachment->getName());
        p.on_enable.function_name = "Attachment.Label";
        p.on_enable.parameter = cbparams;

        LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(p);
        LLView* parent_menu = attachment->getIsHUDAttachment() ? attach_hud_menu : attach_menu;
        parent_menu->addChild(item);
    }
}

// static
void LLViewerAttachMenu::attachObjects(const uuid_vec_t& items, const std::string& joint_name)
{
    LLViewerJointAttachment* attachmentp = NULL;
    for (const auto& attach_pair : gAgentAvatarp->mAttachmentPoints)
    {
        LLViewerJointAttachment* attachment = attach_pair.second;
        if (attachment->getName() == joint_name)
        {
            attachmentp = attachment;
            break;
        }
    }
    if (attachmentp == NULL)
    {
        return;
    }

    for (const LLUUID& id : items)
    {
        LLViewerInventoryItem* item = (LLViewerInventoryItem*)gInventory.getLinkedItem(id);
        if(item && gInventory.isObjectDescendentOf(id, gInventory.getRootFolderID()))
        {
            rez_attachment(item, attachmentp); // don't replace if called from an "Attach To..." menu
        }
        else if(item && item->isFinished())
        {
            // must be in library. copy it to our inventory and put it on.
//          LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(rez_attachment_cb, _1, attachmentp));
// [SL:KB] - Patch: Appearance-DnDWear | Checked: 2013-02-04 (Catznip-3.4)
            LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(rez_attachment_cb, _1, attachmentp, false));
// [/SL;KB]
            copy_inventory_item(gAgent.getID(),
                                item->getPermissions().getOwner(),
                                item->getUUID(),
                                LLUUID::null,
                                std::string(),
                                cb);
        }
    }
}
