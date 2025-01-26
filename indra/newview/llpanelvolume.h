/**
 * @file llpanelvolume.h
 * @brief Object editing (position, scale, etc.) in the tools floater
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLPANELVOLUME_H
#define LL_LLPANELVOLUME_H

#include "v3math.h"
#include "llpanel.h"
#include "llpointer.h"
#include "llvolume.h"
#include "lltextureentry.h"

class LLSpinCtrl;
class LLCheckBoxCtrl;
class LLTextBox;
class LLUICtrl;
class LLButton;
class LLMenuButton;
class LLViewerObject;
class LLComboBox;
class LLColorSwatchCtrl;
class LLTextureCtrl;
class LLVOVolume;

class LLPanelVolume final : public LLPanel
{
public:
    LLPanelVolume();
    virtual ~LLPanelVolume();

    virtual void    draw();
    virtual void    clearCtrls();

    virtual BOOL    postBuild();

    void            refresh();

    void            sendIsLight();

    // when an object is becoming a refleciton probe, present a dialog asking for confirmation
    // otherwise, send the reflection probe update immediately
    void            sendIsReflectionProbe();

    // callback for handling response of the ok/cancel/ignore dialog for making an object a reflection probe
    void            doSendIsReflectionProbe(const LLSD& notification, const LLSD& response);

    void            sendIsFlexible();

    static bool     precommitValidate(const LLSD& data);

    static void     onCommitIsLight(        LLUICtrl* ctrl, void* userdata);
    static void     onCommitLight(          LLUICtrl* ctrl, void* userdata);
    static void     onCommitIsReflectionProbe(LLUICtrl* ctrl, void* userdata);
    static void     onCommitProbe(LLUICtrl* ctrl, void* userdata);
    void            onCommitIsFlexible(     LLUICtrl* ctrl, void* userdata);
    static void     onCommitFlexible(       LLUICtrl* ctrl, void* userdata);
    void            onCommitAnimatedMeshCheckbox(LLUICtrl* ctrl, void* userdata);
    static void     onCommitPhysicsParam(       LLUICtrl* ctrl, void* userdata);
    static void     onCommitMaterial(       LLUICtrl* ctrl, void* userdata);

    void        onLightCancelColor(const LLSD& data);
    void        onLightSelectColor(const LLSD& data);

    void        onLightCancelTexture(const LLSD& data);
    void        onLightSelectTexture(const LLSD& data);

    static void    setLightTextureID(const LLUUID &asset_id, const LLUUID &item_id, LLVOVolume* volobjp);

    void            onCopyFeatures();
    void            onPasteFeatures();
    void            onCopyLight();
    void            onPasteLight();
    void            onClickPipetteFeatures();
    void            onClickPipetteLight();

    void onFeaturesSelect(bool success, LLViewerObject* obj, const LLTextureEntry& te);
    void onLightSelect(bool success, LLViewerObject* obj, const LLTextureEntry& te);

    void        menuDoToSelected(const LLSD& userdata);
    bool        menuEnableItem(const LLSD& userdata);

protected:
    void            getState();
    void            refreshCost();

protected:
    void            sendPhysicsShapeType(LLUICtrl* ctrl, void* userdata);
    void            sendPhysicsGravity(LLUICtrl* ctrl, void* userdata);
    void            sendPhysicsFriction(LLUICtrl* ctrl, void* userdata);
    void            sendPhysicsRestitution(LLUICtrl* ctrl, void* userdata);
    void            sendPhysicsDensity(LLUICtrl* ctrl, void* userdata);

    void            handleResponseChangeToFlexible(const LLSD &pNotification, const LLSD &pResponse);


    //Animesh
    LLCheckBoxCtrl* mCheckAnimesh = nullptr;

    LLTextBox* mLabelEditObject = nullptr;
    LLTextBox* mLabelSelectSingle = nullptr;

    // Light
    LLCheckBoxCtrl*    mCheckLight       = nullptr;
    LLTextBox*         mLabelColor       = nullptr;
    LLTextureCtrl*     mLightTextureCtrl = nullptr;
    LLColorSwatchCtrl* mLightColorSwatch = nullptr;
    LLSpinCtrl*        mLightIntensity   = nullptr;
    LLSpinCtrl*        mLightRadius      = nullptr;
    LLSpinCtrl*        mLightFalloff     = nullptr;
    LLSpinCtrl*        mLightFOV         = nullptr;
    LLSpinCtrl*        mLightFocus       = nullptr;
    LLSpinCtrl*        mLightAmbiance    = nullptr;

    // Flexibile
    LLCheckBoxCtrl* mCheckFlexible1D = nullptr;
    LLSpinCtrl*     mSpinSections = nullptr;
    LLSpinCtrl*     mSpinGravity  = nullptr;
    LLSpinCtrl*     mSpinTension  = nullptr;
    LLSpinCtrl*     mSpinFriction = nullptr;
    LLSpinCtrl*     mSpinWind     = nullptr;
    LLSpinCtrl*     mSpinForceX   = nullptr;
    LLSpinCtrl*     mSpinForceY   = nullptr;
    LLSpinCtrl*     mSpinForceZ   = nullptr;

    S32         mComboMaterialItemCount;
    LLComboBox*     mComboMaterial = nullptr;


    LLColor4        mLightSavedColor;
    LLPointer<LLViewerObject> mObject;
    LLPointer<LLViewerObject> mRootObject;

    LLTextBox*      mLabelPhysicsShapeType = nullptr;
    LLComboBox*     mComboPhysicsShapeType = nullptr;
    LLSpinCtrl*     mSpinPhysicsGravity    = nullptr;
    LLSpinCtrl*     mSpinPhysicsFriction   = nullptr;
    LLSpinCtrl*     mSpinPhysicsDensity    = nullptr;
    LLSpinCtrl*     mSpinPhysicsRestitution = nullptr;

    LLButton* mBtnCopyFeatures = nullptr;
    LLButton* mBtnPasteFeatures = nullptr;
    LLButton* mBtnPipetteFeatures = nullptr;
    LLButton* mBtnCopyLight = nullptr;
    LLButton* mBtnPasteLight = nullptr;
    LLButton* mBtnPipetteLight = nullptr;

    LLSD            mClipboardParams;
};

#endif
