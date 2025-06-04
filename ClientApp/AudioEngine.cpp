#include "AudioEngine.h"

Implementation::Implementation() {
	mpStudioSystem = NULL;
	CAudioEngine::ErrorCheck(FMOD::Studio::System::create(&mpStudioSystem));
	CAudioEngine::ErrorCheck(mpStudioSystem->initialize(32, FMOD_STUDIO_INIT_LIVEUPDATE, FMOD_INIT_PROFILE_ENABLE, NULL));

	mpSystem = NULL;
	CAudioEngine::ErrorCheck(mpStudioSystem->getCoreSystem(&mpSystem));
}

Implementation::~Implementation() {
	CAudioEngine::ErrorCheck(mpStudioSystem->unloadAll());
	CAudioEngine::ErrorCheck(mpStudioSystem->release());
}

void Implementation::Update() {
    vector<ChannelMap::iterator> pStoppedChannels;
    for (auto it = mChannels.begin(), itEnd = mChannels.end(); it != itEnd; ++it)
    {
        bool bIsPlaying = false;
        it->second->isPlaying(&bIsPlaying);
        if (!bIsPlaying)
        {
            pStoppedChannels.push_back(it);
        }
    }
    for (auto& it : pStoppedChannels)
    {
        mChannels.erase(it);
    }
	CAudioEngine::ErrorCheck(mpStudioSystem->update());
}

Implementation* sgpImplementation = nullptr;

void CAudioEngine::Init() {
	sgpImplementation = new Implementation;
}

void CAudioEngine::Update() {
	sgpImplementation->Update();
}

void CAudioEngine::LoadSound(const string& strSoundName, bool bLooping) {
    auto tFoundIt = sgpImplementation->mSounds.find(strSoundName);
    if (tFoundIt != sgpImplementation->mSounds.end())
        return;

    FMOD_MODE eMode = FMOD_DEFAULT;
    eMode |= bLooping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;

    FMOD::Sound* pSound = nullptr;
    CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->createSound(strSoundName.c_str(), eMode, nullptr, &pSound));
    if (pSound) {
        sgpImplementation->mSounds[strSoundName] = pSound;
    }
}

int CAudioEngine::PlayOneSound(const string& strSoundName, float volume)
{
    int nChannelId = sgpImplementation->mnNextChannelId++;
    auto tFoundIt = sgpImplementation->mSounds.find(strSoundName);
    if (tFoundIt == sgpImplementation->mSounds.end())
    {
        LoadSound(strSoundName);
        tFoundIt = sgpImplementation->mSounds.find(strSoundName);
        if (tFoundIt == sgpImplementation->mSounds.end())
        {
            return nChannelId;
        }
    }
    FMOD::Channel* pChannel = nullptr;
    CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->playSound(tFoundIt->second, nullptr, true, &pChannel));
    if (pChannel)
    {
        CAudioEngine::ErrorCheck(pChannel->setVolume(volume));
        CAudioEngine::ErrorCheck(pChannel->setPaused(false));
    }
    return nChannelId;
}

int CAudioEngine::ErrorCheck(FMOD_RESULT result) {
    if (result != FMOD_OK) {
        cout << "FMOD ERROR " << result << endl;
        return 1;
    }

    // cout << "FMOD all good" << endl;

    return 0;
}

void CAudioEngine::Shutdown() {
    delete sgpImplementation;
}
