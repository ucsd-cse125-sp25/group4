#ifndef _AUDIO_ENGINE_H_
#define _AUDIO_ENGINE_H_

#include "fmod_studio.hpp"
#include "fmod.hpp"
#include <string>
#include <map>
#include <vector>
#include <math.h>
#include <iostream>

using namespace std;

struct Vector3 {
    float x, y, z;
};

struct Implementation {
    Implementation();
    ~Implementation();

    void Update();

    FMOD::Studio::System* mpStudioSystem;
    FMOD::System* mpSystem;

    int mnNextChannelId = 0;

    typedef map<string, FMOD::Sound*> SoundMap;
    typedef map<int, FMOD::Channel*> ChannelMap;
    
    SoundMap mSounds;
    ChannelMap mChannels;
};

class CAudioEngine {
public:
    static void Init();
    static void Update();
    static void Shutdown();
    static int ErrorCheck(FMOD_RESULT result);

    void LoadSound(const string& strSoundName, bool b3D = false, bool bLooping = false);
    int PlayOneSound(const string& strSoundName, const Vector3& vPosition = {0, 0, 0}, float volume = 1);
    int PlayOneSound(const string& strSoundName, float volume = 1);
    void StopChannel(int channel);
    FMOD_VECTOR VectorToFmod(const Vector3& vPosition);
};

#endif
