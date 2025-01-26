/**
 * @file ALFloaterExploreSounds.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alfloaterexploresounds.h"

#include "alassetblocklist.h"
#include "llfloaterblocked.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarnamecache.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llstring.h"
#include "lltrans.h"
#include "rlvhandler.h"

static const size_t num_collision_sounds = 28;
const LLUUID collision_sounds[num_collision_sounds] =
{
    LLUUID("dce5fdd4-afe4-4ea1-822f-dd52cac46b08"),
    LLUUID("51011582-fbca-4580-ae9e-1a5593f094ec"),
    LLUUID("68d62208-e257-4d0c-bbe2-20c9ea9760bb"),
    LLUUID("75872e8c-bc39-451b-9b0b-042d7ba36cba"),
    LLUUID("6a45ba0b-5775-4ea8-8513-26008a17f873"),
    LLUUID("992a6d1b-8c77-40e0-9495-4098ce539694"),
    LLUUID("2de4da5a-faf8-46be-bac6-c4d74f1e5767"),
    LLUUID("6e3fb0f7-6d9c-42ca-b86b-1122ff562d7d"),
    LLUUID("14209133-4961-4acc-9649-53fc38ee1667"),
    LLUUID("bc4a4348-cfcc-4e5e-908e-8a52a8915fe6"),
    LLUUID("9e5c1297-6eed-40c0-825a-d9bcd86e3193"),
    LLUUID("e534761c-1894-4b61-b20c-658a6fb68157"),
    LLUUID("8761f73f-6cf9-4186-8aaa-0948ed002db1"),
    LLUUID("874a26fd-142f-4173-8c5b-890cd846c74d"),
    LLUUID("0e24a717-b97e-4b77-9c94-b59a5a88b2da"),
    LLUUID("75cf3ade-9a5b-4c4d-bb35-f9799bda7fb2"),
    LLUUID("153c8bf7-fb89-4d89-b263-47e58b1b4774"),
    LLUUID("55c3e0ce-275a-46fa-82ff-e0465f5e8703"),
    LLUUID("24babf58-7156-4841-9a3f-761bdbb8e237"),
    LLUUID("aca261d8-e145-4610-9e20-9eff990f2c12"),
    LLUUID("0642fba6-5dcf-4d62-8e7b-94dbb529d117"),
    LLUUID("25a863e8-dc42-4e8a-a357-e76422ace9b5"),
    LLUUID("9538f37c-456e-4047-81be-6435045608d4"),
    LLUUID("8c0f84c3-9afd-4396-b5f5-9bca2c911c20"),
    LLUUID("be582e5d-b123-41a2-a150-454c39e961c8"),
    LLUUID("c70141d4-ba06-41ea-bcbc-35ea81cb8335"),
    LLUUID("7d1826f4-24c4-4aac-8c2e-eff45df37783"),
    LLUUID("063c97d3-033a-4e9b-98d8-05c8074922cb")
};

ALFloaterExploreSounds::ALFloaterExploreSounds(const LLSD& key)
:   LLFloater(key), LLEventTimer(0.25f)
{
}

ALFloaterExploreSounds::~ALFloaterExploreSounds()
{
    for (blacklist_avatar_name_cache_connection_map_t::iterator it = mBlacklistAvatarNameCacheConnections.begin(); it != mBlacklistAvatarNameCacheConnections.end(); ++it)
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
    }
    mBlacklistAvatarNameCacheConnections.clear();

    mLocalPlayingAudioSourceIDs.clear();
}

BOOL ALFloaterExploreSounds::postBuild()
{
    getChild<LLButton>("play_locally_btn")->setClickedCallback(boost::bind(&ALFloaterExploreSounds::handlePlayLocally, this));
    getChild<LLButton>("look_at_btn")->setClickedCallback(boost::bind(&ALFloaterExploreSounds::handleLookAt, this));
    getChild<LLButton>("stop_btn")->setClickedCallback(boost::bind(&ALFloaterExploreSounds::handleStop, this));
    getChild<LLButton>("block_btn")->setClickedCallback(boost::bind(&ALFloaterExploreSounds::blacklistSound, this));

    mStopLocalButton = getChild<LLButton>("stop_locally_btn");
    mStopLocalButton->setClickedCallback(boost::bind(&ALFloaterExploreSounds::handleStopLocally, this));

    mHistoryScroller = getChild<LLScrollListCtrl>("sound_list");
    mHistoryScroller->setCommitCallback(boost::bind(&ALFloaterExploreSounds::handleSelection, this));
    mHistoryScroller->setDoubleClickCallback(boost::bind(&ALFloaterExploreSounds::handlePlayLocally, this));
    mHistoryScroller->sortByColumn("playing", TRUE);

    mCollisionSounds = getChild<LLCheckBoxCtrl>("collision_chk");
    mRepeatedAssets = getChild<LLCheckBoxCtrl>("repeated_asset_chk");
    mAvatarSounds = getChild<LLCheckBoxCtrl>("avatars_chk");
    mObjectSounds = getChild<LLCheckBoxCtrl>("objects_chk");
    mPaused = getChild<LLCheckBoxCtrl>("pause_chk");

    return LLFloater::postBuild();
}

void ALFloaterExploreSounds::handleSelection()
{
    size_t num_selected = mHistoryScroller->getAllSelected().size();
    bool multiple = (num_selected > 1);
    childSetEnabled("look_at_btn", (num_selected && !multiple));
    childSetEnabled("play_locally_btn", num_selected);
    childSetEnabled("stop_btn", num_selected);
    childSetEnabled("block_btn", num_selected);
}

LLSoundHistoryItem ALFloaterExploreSounds::getItem(const LLUUID& itemID)
{
    if (!gAudiop)
    {
        LLSoundHistoryItem item;
        item.mID = LLUUID::null;
        return item;
    }

    const auto& sound_log = gAudiop->getSoundLog();
    auto found = sound_log.find(itemID);
    if (found != sound_log.end())
    {
        return *found->second;
    }
    else
    {
        // If log is paused, hopefully we can find it in mLastHistory
        std::list<LLSoundHistoryItem>::iterator iter = mLastHistory.begin();
        std::list<LLSoundHistoryItem>::iterator end = mLastHistory.end();
        for ( ; iter != end; ++iter)
        {
            if ((*iter).mID == itemID)
            {
                return (*iter);
            }
        }
    }
    LLSoundHistoryItem item;
    item.mID = LLUUID::null;
    return item;
}

class LLSoundHistoryItemCompare
{
public:
    bool operator() (LLSoundHistoryItem first, LLSoundHistoryItem second)
    {
        if (first.mPlaying)
        {
            if (second.mPlaying)
            {
                return (first.mTimeStarted > second.mTimeStarted);
            }
            else
            {
                return true;
            }
        }
        else if (second.mPlaying)
        {
            return false;
        }
        else
        {
            return (first.mTimeStopped > second.mTimeStopped);
        }
    }
};

BOOL ALFloaterExploreSounds::tick()
{
    static const std::string str_playing =  getString("Playing");
    static LLUIString str_not_playing = getString("NotPlaying");
    static const std::string str_type_ui =  getString("Type_UI");
    static const std::string str_type_avatar = getString("Type_Avatar");
    static const std::string str_type_trigger_sound = getString("Type_llTriggerSound");
    static const std::string str_type_loop_sound = getString("Type_llLoopSound");
    static const std::string str_type_play_sound = getString("Type_llPlaySound");
    static const std::string str_unknown_name = LLTrans::getString("AvatarNameWaiting");

    bool show_collision_sounds = mCollisionSounds->get();
    bool show_repeated_assets = mRepeatedAssets->get();
    bool show_avatars = mAvatarSounds->get();
    bool show_objects = mObjectSounds->get();

    std::list<LLSoundHistoryItem> history;
    if (mPaused->get())
    {
        history = mLastHistory;
    }
    else
    {
        if (gAudiop)
        {
            for (const auto& sound_pair : gAudiop->getSoundLog())
            {
                history.push_back(*sound_pair.second);
            }
            LLSoundHistoryItemCompare c;
            history.sort(c);
        }
        mLastHistory = history;
    }

    // Save scroll pos and selection so they can be restored
    S32 scroll_pos = mHistoryScroller->getScrollPos();
    uuid_vec_t selected_ids;
    std::vector<LLScrollListItem*> selected_items = mHistoryScroller->getAllSelected();
    std::vector<LLScrollListItem*>::iterator selection_iter = selected_items.begin();
    std::vector<LLScrollListItem*>::iterator selection_end = selected_items.end();
    for (; selection_iter != selection_end; ++selection_iter)
    {
        selected_ids.push_back((*selection_iter)->getUUID());
    }

    mHistoryScroller->clearRows();

    std::list<LLUUID> unique_asset_list;

    std::list<LLSoundHistoryItem>::iterator iter = history.begin();
    std::list<LLSoundHistoryItem>::iterator end = history.end();
    for ( ; iter != end; ++iter)
    {
        LLSoundHistoryItem item = (*iter);

        bool is_avatar = item.mOwnerID == item.mSourceID;
        if (is_avatar && !show_avatars)
        {
            continue;
        }

        bool is_object = !is_avatar;
        if (is_object && !show_objects)
        {
            continue;
        }

        bool is_repeated_asset = std::find(unique_asset_list.begin(), unique_asset_list.end(), item.mAssetID) != unique_asset_list.end();
        if (is_repeated_asset && !show_repeated_assets)
        {
            continue;
        }

        if (!item.mReviewed)
        {
            item.mReviewedCollision = std::find(&collision_sounds[0], &collision_sounds[num_collision_sounds], item.mAssetID) != &collision_sounds[num_collision_sounds];
            item.mReviewed = true;
        }

        bool is_collision_sound = item.mReviewedCollision;
        if (is_collision_sound && !show_collision_sounds)
        {
            continue;
        }

        unique_asset_list.push_back(item.mAssetID);

        LLSD element;
        element["id"] = item.mID;

        LLSD& playing_column = element["columns"][0];
        playing_column["column"] = "playing";
        if (item.mPlaying)
        {
            playing_column["value"] = " " + str_playing;
        }
        else
        {
            LLStringUtil::format_map_t format_args;
            format_args["TIME"] = llformat("%.1f", static_cast<F32>((LLTimer::getElapsedSeconds() - item.mTimeStopped) / 60.0));
            str_not_playing.setArgs(format_args);
            playing_column["value"] = str_not_playing.getString();
        }

        LLSD& type_column = element["columns"][1];
        type_column["column"] = "type";
        if (item.mType == LLAudioEngine::AUDIO_TYPE_UI)
        {
            // this shouldn't happen for now, as UI is forbidden in the log
            type_column["value"] = str_type_ui;
        }
        else
        {
            std::string type;

            if (is_avatar)
            {
                type = str_type_avatar;
            }
            else
            {
                if (item.mIsTrigger)
                {
                    type = str_type_trigger_sound;
                }
                else
                {
                    if (item.mIsLooped)
                    {
                        type = str_type_loop_sound;
                    }
                    else
                    {
                        type = str_type_play_sound;
                    }
                }
            }

            type_column["value"] = type;
        }

        LLSD& owner_column = element["columns"][2];
        owner_column["column"] = "owner";
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(item.mOwnerID, &av_name))
        {
            owner_column["value"] = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name);
        }
        else
        {
            owner_column["value"] = str_unknown_name;
        }

        LLSD& sound_column = element["columns"][3];
        sound_column["column"] = "sound";
        sound_column["value"] = item.mAssetID.asString().substr(0,16);

        mHistoryScroller->addElement(element, ADD_BOTTOM);
    }

    mHistoryScroller->selectMultiple(selected_ids);
    mHistoryScroller->setScrollPos(scroll_pos);

    // Clean up stopped local audio source IDs
    uuid_vec_t stopped_audio_src_ids;
    uuid_vec_t::iterator audio_src_id_iter = mLocalPlayingAudioSourceIDs.begin();
    uuid_vec_t::iterator audio_src_id_end = mLocalPlayingAudioSourceIDs.end();
    for (; audio_src_id_iter != audio_src_id_end; ++audio_src_id_iter)
    {
        LLUUID audio_src_id = *audio_src_id_iter;
        LLAudioSource* audio_source = gAudiop->findAudioSource(audio_src_id);
        if (!audio_source || audio_source->isDone())
        {
            stopped_audio_src_ids.push_back(audio_src_id);
        }
    }

    for (uuid_vec_t::iterator stopped_audio_src_ids_iter = stopped_audio_src_ids.begin();
         stopped_audio_src_ids_iter != stopped_audio_src_ids.end(); ++stopped_audio_src_ids_iter)
    {
        uuid_vec_t::iterator find_iter = std::find(mLocalPlayingAudioSourceIDs.begin(), mLocalPlayingAudioSourceIDs.end(), *stopped_audio_src_ids_iter);
        if (find_iter != mLocalPlayingAudioSourceIDs.end())
        {
            mLocalPlayingAudioSourceIDs.erase(find_iter);
        }
    }

    mStopLocalButton->setEnabled(mLocalPlayingAudioSourceIDs.size() > 0);

    return FALSE;
}

void ALFloaterExploreSounds::handlePlayLocally()
{
    std::vector<LLScrollListItem*> selection = mHistoryScroller->getAllSelected();
    std::vector<LLScrollListItem*>::iterator selection_iter = selection.begin();
    std::vector<LLScrollListItem*>::iterator selection_end = selection.end();
    uuid_vec_t asset_list;
    for ( ; selection_iter != selection_end; ++selection_iter)
    {
        LLSoundHistoryItem item = getItem((*selection_iter)->getValue());
        if (item.mID.isNull())
        {
            continue;
        }

        // Unique assets only
        if (std::find(asset_list.begin(), asset_list.end(), item.mAssetID) == asset_list.end())
        {
            asset_list.push_back(item.mAssetID);
            LLUUID audio_source_id = LLUUID::generateNewID();
            gAudiop->triggerSound(item.mAssetID, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI, LLVector3d::zero, LLUUID::null, audio_source_id);
            mLocalPlayingAudioSourceIDs.push_back(audio_source_id);
        }
    }

    mStopLocalButton->setEnabled(mLocalPlayingAudioSourceIDs.size() > 0);
}

void ALFloaterExploreSounds::handleLookAt()
{
    LLUUID selection = mHistoryScroller->getSelectedValue().asUUID();
    LLSoundHistoryItem item = getItem(selection); // Single item only
    if (item.mID.isNull())
    {
        return;
    }

    LLVector3d pos_global = item.mPosition;

    // Try to find object position
    if (item.mSourceID.notNull())
    {
        LLViewerObject* object = gObjectList.findObject(item.mSourceID);
        if (object)
        {
            pos_global = object->getPositionGlobal();
        }
    }

    // Move the camera
    // Find direction to self (reverse)
    LLVector3d cam = gAgent.getPositionGlobal() - pos_global;
    cam.normalize();
    // Go 4 meters back and 3 meters up
    cam *= 4.0;
    cam += pos_global;
    cam += LLVector3d(0.0, 0.0, 3.0);

    gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
    gAgentCamera.setCameraPosAndFocusGlobal(cam, pos_global, item.mSourceID);
    gAgentCamera.setCameraAnimating(FALSE);
}

void ALFloaterExploreSounds::handleStop()
{
    if (!gAudiop)
        return;

    auto& sound_log = gAudiop->getSoundLog();

    std::vector<LLScrollListItem*> selection = mHistoryScroller->getAllSelected();
    for (const auto& selection_item : selection)
    {
        LLSoundHistoryItem item = getItem(selection_item->getValue());
        if (item.mID.notNull() && item.mPlaying)
        {
            LLAudioSource* audio_source = gAudiop->findAudioSource(item.mSourceID);
            if (audio_source)
            {
                S32 type = item.mType;
                audio_source->setType(LLAudioEngine::AUDIO_TYPE_UI);
                audio_source->play(LLUUID::null);
                audio_source->setType(type);
            }
            else
            {
                LL_WARNS("SoundExplorer") << "audio source for source ID " << item.mSourceID << " already gone but still marked as playing. Fixing ..." << LL_ENDL;
                auto iter = sound_log.find(item.mID);
                if (iter != sound_log.end())
                {
                    iter->second->mPlaying = false;
                    iter->second->mTimeStopped = LLTimer::getElapsedSeconds();
                }
                else
                {
                    for (auto& histItem : mLastHistory)
                    {
                        if (histItem.mID == item.mID)
                        {
                            histItem.mPlaying = false;
                            histItem.mTimeStopped = LLTimer::getElapsedSeconds();
                            break;
                        }
                    }
                }
                continue;
            }
        }
    }
}

void ALFloaterExploreSounds::handleStopLocally()
{
    uuid_vec_t::iterator audio_source_id_iter = mLocalPlayingAudioSourceIDs.begin();
    uuid_vec_t::iterator audio_source_id_end = mLocalPlayingAudioSourceIDs.end();
    for (; audio_source_id_iter != audio_source_id_end; ++audio_source_id_iter)
    {
        LLUUID audio_source_id = *audio_source_id_iter;
        LLAudioSource* audio_source = gAudiop->findAudioSource(audio_source_id);
        if (audio_source && !audio_source->isDone())
        {
            audio_source->play(LLUUID::null);
        }
    }

    mLocalPlayingAudioSourceIDs.clear();
}

//add sound to blacklist
void ALFloaterExploreSounds::blacklistSound()
{
    std::vector<LLScrollListItem*> selection = mHistoryScroller->getAllSelected();
    std::vector<LLScrollListItem*>::iterator selection_iter = selection.begin();
    std::vector<LLScrollListItem*>::iterator selection_end = selection.end();

    for ( ; selection_iter != selection_end; ++selection_iter)
    {
        LLSoundHistoryItem item = getItem((*selection_iter)->getValue());
        if (item.mID.isNull())
        {
            continue;
        }

        std::string location = "Unknown";
        LLViewerRegion* cur_region = gAgent.getRegion();
        if (cur_region)
        {
            location = cur_region->getName();
            //if(!item.mPosition.isNull())
            //{
            //  LLVector3 cur_pos(item.mPosition - cur_region->getOriginGlobal());
            //  location = fmt::format(FMT_STRING("{:s} ({:d}, {:d}, {:d})"), cur_region->getName(), ll_round(cur_pos.mV[VX]), ll_round(cur_pos.mV[VY]), ll_round(cur_pos.mV[VZ]));
            //}
            //else
            //{
            //  location = cur_region->getName();
            //}
        }

        ALAssetBlocklist::instance().addEntry(item.mAssetID, item.mOwnerID, location, LLAssetType::AT_SOUND);
    }

    handleStop();
}
