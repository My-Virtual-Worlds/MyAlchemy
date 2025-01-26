/**
 * @file llfloaterpreferencesgraphicsadvanced.cpp
 * @brief floater for adjusting camera position
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2021, Linden Research, Inc.
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
#include "llfloaterpreferencesgraphicsadvanced.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfeaturemanager.h"
#include "llfloaterpreference.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvoavatar.h"
#include "pipeline.h"
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvactions.h"
// [/RLVa:KB]


LLFloaterPreferenceGraphicsAdvanced::LLFloaterPreferenceGraphicsAdvanced(const LLSD& key)
    : LLFloater(key)
{
    mCommitCallbackRegistrar.add("Pref.RenderOptionUpdate",            boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onRenderOptionEnable, this));
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxNonImpostors", boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors,this));
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxComplexity",   boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity,this));

    mCommitCallbackRegistrar.add("Pref.Cancel", boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onBtnCancel, this, _2));
    mCommitCallbackRegistrar.add("Pref.OK",     boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onBtnOK, this, _2));

    gSavedSettings.getControl("RenderAvatarMaxNonImpostors")->getSignal()->connect(boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateIndirectMaxNonImpostors, this, _2));
}

LLFloaterPreferenceGraphicsAdvanced::~LLFloaterPreferenceGraphicsAdvanced()
{
    mComplexityChangedSignal.disconnect();
    mLODFactorChangedSignal.disconnect();
}

BOOL LLFloaterPreferenceGraphicsAdvanced::postBuild()
{
    // Don't do this on Mac as their braindead GL versioning
    // sets this when 8x and 16x are indeed available
    //
#if !LL_DARWIN
    LLCheckBoxCtrl *use_HiDPI = getChild<LLCheckBoxCtrl>("use HiDPI");
    use_HiDPI->setVisible(FALSE);
#endif

    mComplexityChangedSignal = gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateComplexityText, this));
    mLODFactorChangedSignal = gSavedSettings.getControl("RenderVolumeLODFactor")->getCommitSignal()->connect(boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateObjectMeshDetailText, this));
    return TRUE;
}

void LLFloaterPreferenceGraphicsAdvanced::onOpen(const LLSD& key)
{
    refresh();
}

void LLFloaterPreferenceGraphicsAdvanced::onClickCloseBtn(bool app_quitting)
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->cancel();
    }
    updateMaxComplexity();
}

void LLFloaterPreferenceGraphicsAdvanced::onRenderOptionEnable()
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->refresh();
    }

    refreshEnabledGraphics();
}

void LLFloaterPreferenceGraphicsAdvanced::onAdvancedAtmosphericsEnable()
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->refresh();
    }

    refreshEnabledGraphics();
}

void LLFloaterPreferenceGraphicsAdvanced::refresh()
{
    getChild<LLUICtrl>("fsaa")->setValue((LLSD::Integer)  gSavedSettings.getU32("RenderFSAASamples"));

    // sliders and their text boxes
    //  mPostProcess = gSavedSettings.getS32("RenderGlowResolutionPow");
    // slider text boxes
    updateSliderText(getChild<LLSliderCtrl>("ObjectMeshDetail",     true), getChild<LLTextBox>("ObjectMeshDetailText",      true));
    updateSliderText(getChild<LLSliderCtrl>("FlexibleMeshDetail",   true), getChild<LLTextBox>("FlexibleMeshDetailText",    true));
    updateSliderText(getChild<LLSliderCtrl>("TreeMeshDetail",       true), getChild<LLTextBox>("TreeMeshDetailText",        true));
    updateSliderText(getChild<LLSliderCtrl>("AvatarMeshDetail",     true), getChild<LLTextBox>("AvatarMeshDetailText",      true));
    updateSliderText(getChild<LLSliderCtrl>("AvatarPhysicsDetail",  true), getChild<LLTextBox>("AvatarPhysicsDetailText",       true));
    updateSliderText(getChild<LLSliderCtrl>("TerrainMeshDetail",    true), getChild<LLTextBox>("TerrainMeshDetailText",     true));
    updateSliderText(getChild<LLSliderCtrl>("RenderPostProcess",    true), getChild<LLTextBox>("PostProcessText",           true));
    updateSliderText(getChild<LLSliderCtrl>("SkyMeshDetail",        true), getChild<LLTextBox>("SkyMeshDetailText",         true));
    LLAvatarComplexityControls::setIndirectControls();
    setMaxNonImpostorsText(
        gSavedSettings.getU32("RenderAvatarMaxNonImpostors"),
        getChild<LLTextBox>("IndirectMaxNonImpostorsText", true));
    LLAvatarComplexityControls::setText(
        gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
    refreshEnabledState();
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledGraphics()
{
    refreshEnabledState();
}

void LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity()
{
    // Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(
        getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText"));
}

void LLFloaterPreferenceGraphicsAdvanced::updateComplexityText()
{
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
}

void LLFloaterPreferenceGraphicsAdvanced::updateObjectMeshDetailText()
{
    updateSliderText(getChild<LLSliderCtrl>("ObjectMeshDetail", true), getChild<LLTextBox>("ObjectMeshDetailText", true));
}

void LLFloaterPreferenceGraphicsAdvanced::updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box)
{
    if (text_box == NULL || ctrl== NULL)
        return;

    // get range and points when text should change
    F32 value = (F32)ctrl->getValue().asReal();
    F32 min = ctrl->getMinValue();
    F32 max = ctrl->getMaxValue();
    F32 range = max - min;
    llassert(range > 0);
    F32 midPoint = min + range / 3.0f;
    F32 highPoint = min + (2.0f * range / 3.0f);

    // choose the right text
    if (value < midPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityLow"));
    }
    else if (value < highPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityMid"));
    }
    else
    {
        text_box->setText(LLTrans::getString("GraphicsQualityHigh"));
    }
}

void LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors()
{
    // Called when the IndirectMaxNonImpostors control changes
    // Responsible for fixing the slider label (IndirectMaxNonImpostorsText) and setting RenderAvatarMaxNonImpostors
    LLSliderCtrl* ctrl = getChild<LLSliderCtrl>("IndirectMaxNonImpostors",true);
    U32 value = ctrl->getValue().asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
    LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
    setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreferenceGraphicsAdvanced::updateIndirectMaxNonImpostors(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();
    if ((value != 0) && (value != gSavedSettings.getU32("IndirectMaxNonImpostors")))
    {
        gSavedSettings.setU32("IndirectMaxNonImpostors", value);
        setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
    }
}

void LLFloaterPreferenceGraphicsAdvanced::setMaxNonImpostorsText(U32 value, LLTextBox* text_box)
{
    if (0 == value)
    {
        text_box->setText(LLTrans::getString("no_limit"));
    }
    else
    {
        text_box->setText(llformat("%d", value));
    }
}

void LLFloaterPreferenceGraphicsAdvanced::disableUnavailableSettings()
{
    LLComboBox* ctrl_shadows = getChild<LLComboBox>("ShadowDetail");
    LLTextBox* shadows_text = getChild<LLTextBox>("RenderShadowDetailText");
    LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
    LLComboBox* ctrl_anisotropic = getChild<LLComboBox>("anisotropic_filter");

    // disabled deferred SSAO
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
    {
        ctrl_ssao->setEnabled(FALSE);
        ctrl_ssao->setValue(FALSE);
    }

    // disabled deferred shadows
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
    {
        ctrl_shadows->setEnabled(FALSE);
        ctrl_shadows->setValue(0);
        shadows_text->setEnabled(FALSE);
    }

    if (!LLFeatureManager::instance().isFeatureAvailable("RenderAnisotropicLevel"))
    {
        ctrl_anisotropic->setEnabled(FALSE);
    }
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledState()
{
    // WindLight
    //LLCheckBoxCtrl* ctrl_wind_light = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
    //ctrl_wind_light->setEnabled(TRUE);
    LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");
    LLTextBox* sky_text = getChild<LLTextBox>("SkyMeshDetailText");
    sky->setEnabled(TRUE);
    sky_text->setEnabled(TRUE);

    BOOL enabled = TRUE;

    LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
    LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
    LLComboBox* ctrl_shadow = getChild<LLComboBox>("ShadowDetail");
    LLTextBox* shadow_text = getChild<LLTextBox>("RenderShadowDetailText");

    // note, okay here to get from ctrl_deferred as it's twin, ctrl_deferred2 will alway match it
    enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO");// && (ctrl_deferred->get() ? TRUE : FALSE);

    //ctrl_deferred->set(gSavedSettings.getBOOL("RenderDeferred"));

    ctrl_ssao->setEnabled(enabled);
    ctrl_dof->setEnabled(enabled);

    enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

    ctrl_shadow->setEnabled(enabled);
    shadow_text->setEnabled(enabled);

    // Hardware settings

    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderVBOEnable"))
    {
        getChildView("vbo")->setEnabled(FALSE);
    }

    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderCompressTextures"))
    {
        getChildView("texture compression")->setEnabled(FALSE);
    }

    // AF Filtering
    LLComboBox* af_combo = getChild<LLComboBox>("anisotropic_filter");
    if (2.f > gGLManager.mMaxAnisotropy) {
        af_combo->remove("2x");
    }
    if (4.f > gGLManager.mMaxAnisotropy) {
        af_combo->remove("4x");
    }
    if (8.f > gGLManager.mMaxAnisotropy) {
        af_combo->remove("8x");
    }
    if (16.f > gGLManager.mMaxAnisotropy) {
        af_combo->remove("16x");
    }

    getChildView("antialiasing restart")->setVisible(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred"));

    // now turn off any features that are unavailable
    disableUnavailableSettings();
}

void LLFloaterPreferenceGraphicsAdvanced::onBtnOK(const LLSD& userdata)
{
    LLFloaterPreference* instance = LLFloaterReg::getTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->onBtnOK(userdata);
    }
}

void LLFloaterPreferenceGraphicsAdvanced::onBtnCancel(const LLSD& userdata)
{
    LLFloaterPreference* instance = LLFloaterReg::getTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->onBtnCancel(userdata);
    }
}
