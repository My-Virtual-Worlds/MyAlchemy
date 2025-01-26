/*
* @file llfloatergroupprofile.h
* @brief Floater that holds panel
*
* Copyright (c) 2017, Cinder Roxley <cinder@sdf.org>
*
* Permission is hereby granted, free of charge, to any person or organization
* obtaining a copy of the software and accompanying documentation covered by
* this license (the "Software") to use, reproduce, display, distribute,
* execute, and transmit the Software, and to prepare derivative works of the
* Software, and to permit third-parties to whom the Software is furnished to
* do so, all subject to the following:
*
* The copyright notices in the Software and this entire statement, including
* the above license grant, this restriction and the following disclaimer,
* must be included in all copies of the Software, in whole or in part, and
* all derivative works of the Software, unless such copies or derivative
* works are solely in the form of machine-executable object code generated by
* a source language processor.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*
*/

#include "llviewerprecompiledheaders.h"
#include "llfloatergroupprofile.h"

#include "llfloaterreg.h"
#include "lltrans.h"

#include "llpanelgroup.h"

LLFloaterGroupProfile::LLFloaterGroupProfile(LLSD const& key)
    : LLFloater(key)
{
}

LLFloaterGroupProfile::~LLFloaterGroupProfile()
{
}

BOOL LLFloaterGroupProfile::postBuild()
{
    mGroupPanel = getChild<LLPanel>("panel_group_info_sidetray");
    mCreateGroupPanel = getChild<LLPanel>("panel_group_creation_sidetray");
    return TRUE;
}

void LLFloaterGroupProfile::onOpen(const LLSD& key)
{
    if (key.isMap())
    {
        setKey(key["group_id"].asUUID());
    }

    mCreatingGroup = (key.has("action") && key.get("action").asString() == "create");
    if (mCreatingGroup)
    {
        mCreateGroupPanel->onOpen(key);
        mCreateGroupPanel->setVisible(true);
        mGroupPanel->setVisible(false);
        setTitle(getString("title_create_group"));
    }
    else
    {
        mGroupPanel->onOpen(key);
        mGroupPanel->setVisible(true);
        mCreateGroupPanel->setVisible(false);
    }
}

void LLFloaterGroupProfile::setGroupName(const std::string& group_name)
{
    if (mCreatingGroup)
    {
        setTitle(getString("title_create_group"));
    }
    else
    {
        setTitle(group_name.empty() ? LLTrans::getString("LoadingData") : group_name);
    }
}

void LLFloaterGroupProfile::createGroup() const
{
    LLSD params;
    params["group_id"] = LLUUID::null;
    params["action"] = "create";
    getChild<LLPanel>("panel_group_info_sidetray")->onOpen(params);
}

//static
LLFloater* LLFloaterGroupProfile::showInstance(const LLSD& key, BOOL focus)
{
    // [RLVa:KB] - Checked: 2010-02-28 (RLVa-1.4.0a) | Modified: RLVa-1.2.0a
    if (!LLFloaterReg::canShowInstance("group_profile", key["group_id"].asUUID()))
        // [/RLVa:KB]
        return nullptr;//
    LLFloater* instance = LLFloaterReg::getInstance("group_profile", key["group_id"].asUUID());
    if (instance)
    {
        instance->openFloater(key);
        if (focus)
            instance->setFocus(TRUE);
    }
    return instance;
}