/*
** Copyright 2010, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <math.h>

#define LOG_NDEBUG 0

#define LOG_TAG "AudioHardware"

#include <utils/Log.h>
#include <utils/String8.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "AudioHardware.h"
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

extern "C" {
#include "alsa_audio.h"
}

#ifdef HAVE_FM_RADIO
#define Si4709_IOC_MAGIC  0xFA
#define Si4709_IOC_VOLUME_SET                       _IOW(Si4709_IOC_MAGIC, 15, __u8)
#endif



namespace android {

const uint32_t AudioHardware::inputSamplingRates[] = {
        8000, 11025, 16000, 22050, 44100
};

//  trace driver operations for dump
//
#define DRIVER_TRACE

enum {
    DRV_NONE,
    DRV_PCM_OPEN,
    DRV_PCM_CLOSE,
    DRV_PCM_WRITE,
    DRV_PCM_READ,
    DRV_MIXER_OPEN,
    DRV_MIXER_CLOSE,
    DRV_MIXER_GET,
    DRV_MIXER_SEL
};

#ifdef DRIVER_TRACE
#define TRACE_DRIVER_IN(op) mDriverOp = op;
#define TRACE_DRIVER_OUT mDriverOp = DRV_NONE;
#else
#define TRACE_DRIVER_IN(op)
#define TRACE_DRIVER_OUT
#endif

// ----------------------------------------------------------------------------

AudioHardware::AudioHardware() :
    mInit(false),
    mMicMute(false),
    mPcm(NULL),
    mMixer(NULL),
    mPcmOpenCnt(0),
    mMixerOpenCnt(0),
    mInCallAudioMode(false),
    mVoiceVol(1.0f),
    mInputSource(AUDIO_SOURCE_DEFAULT),
    mBluetoothNrec(true),
#ifdef HAVE_FM_RADIO
    mFmFd(-1),
    mFmVolume(1),
    mFmResumeAfterCall(false),
#endif
    mDriverOp(DRV_NONE)
{
    mInit = true;
}

AudioHardware::~AudioHardware()
{
    for (size_t index = 0; index < mInputs.size(); index++) {
        closeInputStream(mInputs[index].get());
    }
    mInputs.clear();
    closeOutputStream((AudioStreamOut*)mOutput.get());

    if (mMixer) {
        TRACE_DRIVER_IN(DRV_MIXER_CLOSE)
        mixer_close(mMixer);
        TRACE_DRIVER_OUT
    }
    if (mPcm) {
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        pcm_close(mPcm);
        TRACE_DRIVER_OUT
    }

    mInit = false;
}

status_t AudioHardware::initCheck()
{
    return mInit ? NO_ERROR : NO_INIT;
}


AudioStreamOut* AudioHardware::openOutputStream(
    uint32_t devices, int *format, uint32_t *channels,
    uint32_t *sampleRate, status_t *status)
{
    sp <AudioStreamOutALSA> out;
    status_t rc;

    { // scope for the lock
        Mutex::Autolock lock(mLock);

        // only one output stream allowed
        if (mOutput != 0) {
            if (status) {
                *status = INVALID_OPERATION;
            }
            return NULL;
        }

        out = new AudioStreamOutALSA();

        rc = out->set(this, devices, format, channels, sampleRate);
        if (rc == NO_ERROR) {
            mOutput = out;
        }
    }

    if (rc != NO_ERROR) {
        if (out != 0) {
            out.clear();
        }
    }
    if (status) {
        *status = rc;
    }

    return out.get();
}

void AudioHardware::closeOutputStream(AudioStreamOut* out) {
    sp <AudioStreamOutALSA> spOut;
    {
        Mutex::Autolock lock(mLock);
        if (mOutput == 0 || mOutput.get() != out) {
            LOGW("Attempt to close invalid output stream");
            return;
        }
        spOut = mOutput;
        mOutput.clear();
    }
    spOut.clear();
}

AudioStreamIn* AudioHardware::openInputStream(
    uint32_t devices, int *format, uint32_t *channels,
    uint32_t *sampleRate, status_t *status,
    AudioSystem::audio_in_acoustics acoustic_flags)
{
    // check for valid input source
    if (!AudioSystem::isInputDevice((AudioSystem::audio_devices)devices)) {
        if (status) {
            *status = BAD_VALUE;
        }
        return NULL;
    }

    status_t rc = NO_ERROR;
    sp <AudioStreamInALSA> in;

    { // scope for the lock
        Mutex::Autolock lock(mLock);

        in = new AudioStreamInALSA();
        rc = in->set(this, devices, format, channels, sampleRate, acoustic_flags);
        if (rc == NO_ERROR) {
            mInputs.add(in);
        }
    }

    if (rc != NO_ERROR) {
        if (in != 0) {
            in.clear();
        }
    }
    if (status) {
        *status = rc;
    }

    LOGV("AudioHardware::openInputStream()%p", in.get());
    return in.get();
}

void AudioHardware::closeInputStream(AudioStreamIn* in) {

    sp<AudioStreamInALSA> spIn;
    {
        Mutex::Autolock lock(mLock);

        ssize_t index = mInputs.indexOf((AudioStreamInALSA *)in);
        if (index < 0) {
            LOGW("Attempt to close invalid input stream");
            return;
        }
        spIn = mInputs[index];
        mInputs.removeAt(index);
    }
    LOGV("AudioHardware::closeInputStream()%p", in);
    spIn.clear();
}


status_t AudioHardware::setMode(int mode)
{
    sp<AudioStreamOutALSA> spOut;
    sp<AudioStreamInALSA> spIn;
    status_t status;

    // Mutex acquisition order is always out -> in -> hw
    AutoMutex lock(mLock);

    spOut = mOutput;
    while (spOut != 0) {
        if (!spOut->checkStandby()) {
            int cnt = spOut->prepareLock();
            mLock.unlock();
            spOut->lock();
            mLock.lock();
            // make sure that another thread did not change output state while the
            // mutex is released
            if ((spOut == mOutput) && (cnt == spOut->standbyCnt())) {
                break;
            }
            spOut->unlock();
            spOut = mOutput;
        } else {
            spOut.clear();
        }
    }
    // spOut is not 0 here only if the output is active

    spIn = getActiveInput_l();
    while (spIn != 0) {
        int cnt = spIn->prepareLock();
        mLock.unlock();
        spIn->lock();
        mLock.lock();
        // make sure that another thread did not change input state while the
        // mutex is released
        if ((spIn == getActiveInput_l()) && (cnt == spIn->standbyCnt())) {
            break;
        }
        spIn->unlock();
        spIn = getActiveInput_l();
    }
    // spIn is not 0 here only if the input is active

    int prevMode = mMode;
    status = AudioHardwareBase::setMode(mode);
    LOGV("setMode() : new %d, old %d", mMode, prevMode);
    if (status == NO_ERROR) {
        //enable voice call
        if ((mMode == AudioSystem::MODE_IN_CALL) && !mInCallAudioMode) {
            if (spOut != 0) {
                LOGV("setMode() in call force output standby");
                spOut->doStandby_l();
            }
            if (spIn != 0) {
                LOGV("setMode() in call force input standby");
                spIn->doStandby_l();
            }

            LOGV("setMode() openPcmOut_l()");
            openPcmOut_l();
            openMixer_l();

            setInputRoute(mInputSource, AudioSystem::DEVICE_IN_BUILTIN_MIC);
            setOutputRoute(ROUTE_OUT_VOICECALL, AudioSystem::DEVICE_OUT_EARPIECE);
            setVoiceVolume_l(mVoiceVol);

            mInCallAudioMode = true;
        }
        //disable voice call
        if (mMode != AudioSystem::MODE_IN_CALL && mInCallAudioMode) {
            setInputRoute(mInputSource, AudioSystem::DEVICE_IN_BUILTIN_MIC);
            setOutputRoute(ROUTE_OUT_PLAYBACK, AudioSystem::DEVICE_OUT_SPEAKER);
            LOGV("setMode() closePcmOut_l()");
            closeMixer_l();
            closePcmOut_l();

            if (spOut != 0) {
                LOGV("setMode() off call force output standby");
                spOut->doStandby_l();
            }
            if (spIn != 0) {
                LOGV("setMode() off call force input standby");
                spIn->doStandby_l();
            }

            mInCallAudioMode = false;
        }

    }

    if (spIn != 0) {
        spIn->unlock();
    }
    if (spOut != 0) {
        spOut->unlock();
    }

#ifdef HAVE_FM_RADIO
    if (mFmResumeAfterCall) {
        mFmResumeAfterCall = false;

        enableFMRadio();
    }
#endif
    return status;
}

status_t AudioHardware::setMicMute(bool state)
{
    LOGV("setMicMute(%d) mMicMute %d", state, mMicMute);
    sp<AudioStreamInALSA> spIn;
    {
        AutoMutex lock(mLock);
        if (mMicMute != state) {
            mMicMute = state;
            if (mMode == AudioSystem::MODE_IN_CALL) {
                LOGV("not implemented");
            } else {
                LOGV("not implemented");
            }
        }
    }

    if (spIn != 0) {
        spIn->standby();
    }

    return NO_ERROR;
}

status_t AudioHardware::getMicMute(bool* state)
{
    *state = mMicMute;
    return NO_ERROR;
}

status_t AudioHardware::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 value;
    String8 key;
    const char BT_NREC_KEY[] = "bt_headset_nrec";
    const char BT_NREC_VALUE_ON[] = "on";

    key = String8(BT_NREC_KEY);
    if (param.get(key, value) == NO_ERROR) {
        if (value == BT_NREC_VALUE_ON) {
            mBluetoothNrec = true;
        } else {
            mBluetoothNrec = false;
            LOGD("Turning noise reduction and echo cancellation off for BT "
                 "headset");
        }
        param.remove(String8(BT_NREC_KEY));
    }

#ifdef HAVE_FM_RADIO
    // fm radio on
    key = String8(AudioParameter::keyFmOn);
    if (param.get(key, value) == NO_ERROR) {
        enableFMRadio();
    }
    param.remove(key);

    // fm radio off
    key = String8(AudioParameter::keyFmOff);
    if (param.get(key, value) == NO_ERROR) {
        disableFMRadio();
    }
    param.remove(key);
#endif

    return NO_ERROR;
}

String8 AudioHardware::getParameters(const String8& keys)
{
    AudioParameter request = AudioParameter(keys);
    AudioParameter reply = AudioParameter();

    LOGV("getParameters() %s", keys.string());

    return reply.toString();
}

size_t AudioHardware::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    if (format != AudioSystem::PCM_16_BIT) {
        LOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if (channelCount < 1 || channelCount > 2) {
        LOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }
    if (sampleRate != 8000 && sampleRate != 11025 && sampleRate != 16000 &&
            sampleRate != 22050 && sampleRate != 44100) {
        LOGW("getInputBufferSize bad sample rate: %d", sampleRate);
        return 0;
    }

    return AudioStreamInALSA::getBufferSize(sampleRate, channelCount);
}

void AudioHardware::setOutputVolume(uint32_t device, uint32_t volume)
{
    const char *name = "DAC Voice Digital Downlink Volume";
    struct mixer_ctl *ctl;

    LOGD("### setOutputVolume: device= %d, volume= %d", device, volume);

    if (mMixer) {
        ctl = mixer_get_control(mMixer, name, 0);
        if (ctl)
            mixer_ctl_set(ctl, volume);
    }
}

status_t AudioHardware::setVoiceVolume(float volume)
{
    AutoMutex lock(mLock);

    setVoiceVolume_l(volume);

    return NO_ERROR;
}

void AudioHardware::setVoiceVolume_l(float volume)
{
    LOGD("### setVoiceVolume: %f", volume);

    mVoiceVol = volume;

    if (AudioSystem::MODE_IN_CALL == mMode) {
        uint32_t device = AudioSystem::DEVICE_OUT_EARPIECE;
        if (mOutput != 0) {
            device = mOutput->device();
        }
        setOutputVolume(device, 100*volume);
    }
}

status_t AudioHardware::setMasterVolume(float volume)
{
    LOGV("Set master volume to %f.\n", volume);
    // We return an error code here to let the audioflinger do in-software
    // volume on top of the maximum volume that we set through the SND API.
    // return error - software mixer will handle it
    return -1;
}

#ifdef HAVE_FM_RADIO
status_t AudioHardware::setFmVolume(float v)
{
    mFmVolume = v;
    if (mFmFd > 0) {
        __u8 fmVolume = (AudioSystem::logToLinear(v) + 5) / 7;
        LOGD("%s %f %d", __func__, v, (int) fmVolume);
        if (ioctl(mFmFd, Si4709_IOC_VOLUME_SET, &fmVolume) < 0) {
            LOGE("set_volume_fm error.");
            return -EIO;
        }
    }
    return NO_ERROR;
}
#endif

static const int kDumpLockRetries = 50;
static const int kDumpLockSleep = 20000;

static bool tryLock(Mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.tryLock() == NO_ERROR) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

status_t AudioHardware::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\tAudioHardware maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\tInit %s\n", (mInit) ? "OK" : "Failed");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tMic Mute %s\n", (mMicMute) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmPcm: %p\n", mPcm);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmPcmOpenCnt: %d\n", mPcmOpenCnt);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmMixer: %p\n", mMixer);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmMixerOpenCnt: %d\n", mMixerOpenCnt);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tIn Call Audio Mode %s\n", (mInCallAudioMode) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tInput source %d\n", mInputSource);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);

    snprintf(buffer, SIZE, "\n\tmOutput %p dump:\n", mOutput.get());
    result.append(buffer);
    write(fd, result.string(), result.size());
    if (mOutput != 0) {
        mOutput->dump(fd, args);
    }

    snprintf(buffer, SIZE, "\n\t%d inputs opened:\n", mInputs.size());
    write(fd, buffer, strlen(buffer));
    for (size_t i = 0; i < mInputs.size(); i++) {
        snprintf(buffer, SIZE, "\t- input %d dump:\n", i);
        write(fd, buffer, strlen(buffer));
        mInputs[i]->dump(fd, args);
    }

    return NO_ERROR;
}

status_t AudioHardware::setStandby()
{
    struct mixer_ctl *ctl;
    if (mMixer) {
        LOGD("AudioHardware set codec to standby.");
        // powerdown MAX9877
        ctl = mixer_get_control(mMixer, "Amp Enable", 0);
        if(ctl)
            mixer_ctl_select(ctl, "OFF");
        // powerdown TWL5030 audio codec
        ctl = mixer_get_control(mMixer, "Idle Mode", 0);
        if(ctl)
            mixer_ctl_select(ctl, "off");
    }
    return NO_ERROR;
}

status_t AudioHardware::setIncallPath_l(uint32_t device)
{
    LOGV("setIncallPath_l: device %x", device);

    if (mMode == AudioSystem::MODE_IN_CALL) {
        LOGD("### incall mode route (%d)", device);
        if (mMixer != NULL) {
            setOutputRoute(ROUTE_OUT_VOICECALL, device);
        }
    }

    return NO_ERROR;
}

// set alsa audio path (see output of alsa_amixer)
//  path: alsa '.. Path' mixer control to set
//  route: value to set for path ("OFF", "RCV"  "SPK" ...)
//  maximOutMode: value of 'MAX9877 Output Mode' ("INA -> SPK", ...)
//  ampEnable: value of "Amp Enable" control (AMP_ENABLE, AMP_DISABLE, ...)
status_t AudioHardware::setOutputRoute( AudioHardware::RouteType path, uint32_t device)
{
    const char* MAX9877_MODE = "MAX9877 Output Mode";
    const char* MAX9877_ENABLE = "Amp Enable";
    char *max9877Mode = (char*)getMax9877ModeFromDevice(device);
    char *twlAudioRoute = (char*)getOutputRouteFromDevice(device);
    AudioHardware::MixerConfig *pCfg;
    char *twlAudioPath;
    const char *ampEnable;

    if (mMixer != NULL)
    {
        LOGV("setOutputRoute() mixer is open");

        switch(path)
        {
            case ROUTE_OUT_FMRADIO:
                twlAudioPath = (char*)"FM Radio Path";
                twlAudioRoute = (char*)getFmOutputRouteFromDevice(device);
                break;
            case ROUTE_OUT_VOICECALL:
                twlAudioPath = (char*)"Voice Call Path";
                break;
            case ROUTE_OUT_PLAYBACK:
            default:
                twlAudioPath = (char*)"Playback Path";
        }
        //earpiece not connected to MAX9877
        if(device==AudioSystem::DEVICE_OUT_EARPIECE)
            ampEnable = "OFF";
        else
            ampEnable = "ON";

        AudioHardware::MixerConfig mixerSequenceOut[] = {
             {   MAX9877_ENABLE,    "OFF",          1000},   //power of AMP before
             {   twlAudioPath,      twlAudioRoute,  0},
             {   MAX9877_MODE,      max9877Mode,    0},
             {   MAX9877_ENABLE,    ampEnable,      1000},
             {   NULL,              NULL,           0} };

        //apply mixer setting
        for(pCfg = &mixerSequenceOut[0]; pCfg->ctl; pCfg++)
        {
            TRACE_DRIVER_IN(DRV_MIXER_GET)
            struct mixer_ctl *ctl= mixer_get_control(mMixer, pCfg->ctl, 0);
            TRACE_DRIVER_OUT
            if (ctl != NULL) {
                LOGV("setOutputRoute() '%s' '%s'", pCfg->ctl, pCfg->val);
                TRACE_DRIVER_IN(DRV_MIXER_SEL)
                mixer_ctl_select(ctl, pCfg->val);
                TRACE_DRIVER_OUT
                //usleep(pCfg->delay);
            } else {
                LOGE("setOutputRoute() could not get %s mixer ctl", pCfg->ctl);
            }
        }

    } else {
        LOGE("setOutputRoute() mixer is not open");
    }
    return NO_ERROR;
}

#ifdef HAVE_FM_RADIO
void AudioHardware::enableFMRadio() {
    LOGV("AudioHardware::enableFMRadio() Turning FM Radio ON");

    if (mMode == AudioSystem::MODE_IN_CALL) {
        LOGV("AudioHardware::enableFMRadio() Call is active. Delaying FM enable.");
        mFmResumeAfterCall = true;
    }
    else {
        openPcmOut_l();
        openMixer_l();
        //setInputSource_l(AUDIO_SOURCE_DEFAULT);

        if (mMixer != NULL) {
            LOGV("AudioHardware::enableFMRadio() FM Radio is ON, calling setFMRadioPath_l()");
            setFMRadioPath_l(mOutput->device());
        }

        if (mFmFd < 0) {
            mFmFd = open("/dev/radio0", O_RDWR);
            // In case setFmVolume was called before FM was enabled, we save the volume and call it here.
            setFmVolume(mFmVolume);
        }
    }
}

void AudioHardware::disableFMRadio() {
    LOGV("AudioHardware::disableFMRadio() Turning FM Radio OFF");


    if (mMixer != NULL) {
        // Disable FM radio flag to allow the codec to be turned off
        // (the flag is automatically set by the kernel driver when FM is enabled)
        // No need to turn off the FM Radio path as the kernel driver will handle that
#if 0
// todo: disable codec
//how to disable codec on nowplus,
//have to add kernel function?
        TRACE_DRIVER_IN(DRV_MIXER_GET)
        struct mixer_ctl *ctl = mixer_get_control(mMixer, "Codec Status", 0);
        TRACE_DRIVER_OUT

        if (ctl != NULL) {
            TRACE_DRIVER_IN(DRV_MIXER_SEL)
            mixer_ctl_select(ctl, "FMR_FLAG_CLEAR");
            TRACE_DRIVER_OUT
        }
#endif
        closeMixer_l();
        closePcmOut_l();
    }

    if (mFmFd > 0) {
        close(mFmFd);
        mFmFd = -1;
    }
}

status_t AudioHardware::setFMRadioPath_l(uint32_t device)
{
    LOGV("setFMRadioPath_l() device %x", device);

    // AudioPath path;
    // const char *fmpath;

    if (device != AudioSystem::DEVICE_OUT_SPEAKER && (device & AudioSystem::DEVICE_OUT_SPEAKER) != 0) {
        /* Fix the case where we're on headset and the system has just played a
         * notification sound to both the speaker and the headset. The device
         * now is an ORed value and we need to get back its original value.
         */
         device -= AudioSystem::DEVICE_OUT_SPEAKER;
         LOGD("setFMRadioPath_l() device removed speaker %x", device);
    }

    setOutputRoute( ROUTE_OUT_FMRADIO, device);

    return NO_ERROR;
}
#endif

struct pcm *AudioHardware::openPcmOut_l()
{
    LOGD("openPcmOut_l() mPcmOpenCnt: %d", mPcmOpenCnt);
    if (mPcmOpenCnt++ == 0) {
        if (mPcm != NULL) {
            LOGE("openPcmOut_l() mPcmOpenCnt == 0 and mPcm == %p\n", mPcm);
            mPcmOpenCnt--;
            return NULL;
        }
        unsigned flags = PCM_OUT;

        flags |= (AUDIO_HW_OUT_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
        flags |= (AUDIO_HW_OUT_PERIOD_CNT - PCM_PERIOD_CNT_MIN) << PCM_PERIOD_CNT_SHIFT;

        TRACE_DRIVER_IN(DRV_PCM_OPEN)
        mPcm = pcm_open(flags);
        TRACE_DRIVER_OUT
        if (!pcm_ready(mPcm)) {
            LOGE("openPcmOut_l() cannot open pcm_out driver: %s\n", pcm_error(mPcm));
            TRACE_DRIVER_IN(DRV_PCM_CLOSE)
            pcm_close(mPcm);
            TRACE_DRIVER_OUT
            mPcmOpenCnt--;
            mPcm = NULL;
        }
    }
    return mPcm;
}

void AudioHardware::closePcmOut_l()
{
    LOGD("closePcmOut_l() mPcmOpenCnt: %d", mPcmOpenCnt);
    if (mPcmOpenCnt == 0) {
        LOGE("closePcmOut_l() mPcmOpenCnt == 0");
        return;
    }

    if (--mPcmOpenCnt == 0) {
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        pcm_close(mPcm);
        TRACE_DRIVER_OUT
        mPcm = NULL;
    }
}

struct mixer *AudioHardware::openMixer_l()
{
    LOGV("openMixer_l() mMixerOpenCnt: %d", mMixerOpenCnt);
    if (mMixerOpenCnt++ == 0) {
        if (mMixer != NULL) {
            LOGE("openMixer_l() mMixerOpenCnt == 0 and mMixer == %p\n", mMixer);
            mMixerOpenCnt--;
            return NULL;
        }
        TRACE_DRIVER_IN(DRV_MIXER_OPEN)
        mMixer = mixer_open();
        TRACE_DRIVER_OUT
        if (mMixer == NULL) {
            LOGE("openMixer_l() cannot open mixer");
            mMixerOpenCnt--;
            return NULL;
        }
    }
    return mMixer;
}

void AudioHardware::closeMixer_l()
{
    LOGV("closeMixer_l() mMixerOpenCnt: %d", mMixerOpenCnt);
    if (mMixerOpenCnt == 0) {
        LOGE("closeMixer_l() mMixerOpenCnt == 0");
        return;
    }

    if (--mMixerOpenCnt == 0) {
        // put codec in standby
        setStandby();
        TRACE_DRIVER_IN(DRV_MIXER_CLOSE)
        mixer_close(mMixer);
        TRACE_DRIVER_OUT
        mMixer = NULL;
    }
}
const char *AudioHardware::getFmOutputRouteFromDevice(uint32_t device)
{
    switch(device){
         case AudioSystem::DEVICE_OUT_SPEAKER:
            LOGD("setFMRadioPath_l() fmradio speaker route");
            return "SPK";
        case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
        case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
            LOGD("setFMRadioPath_l() fmradio headphone route");
            return "HP";
        default:
            LOGE("setFMRadioPath_l() fmradio error, route = [%d]", device);
            return "HP";
    }
}

const char *AudioHardware::getOutputRouteFromDevice(uint32_t device)
{
    switch (device) {
    case AudioSystem::DEVICE_OUT_EARPIECE:
        return "RCV";
    case AudioSystem::DEVICE_OUT_SPEAKER:
        return "SPK";
    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
        return "HP";
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADPHONE):
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADSET):
        return "SPK_HP";
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return "BT";
    default:
        return "OFF";
    }
}
const char *AudioHardware::getMax9877ModeFromDevice(uint32_t device)
{
    switch (device) {
    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
         return "INB -> HP";
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADPHONE):
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADSET):
        return "INA -> SPK and HP";
    default:
        return "INA -> SPK";
    }

}

const char *AudioHardware::getVoiceRouteFromDevice(uint32_t device)
{
    switch (device) {
    case AudioSystem::DEVICE_OUT_EARPIECE:
        return "RCV";
    case AudioSystem::DEVICE_OUT_SPEAKER:
        return "SPK";
    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
        if (device == AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) {
            return "HP_NO_MIC";
        } else {
            return "HP";
        }
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return "BT";
    default:
        return "OFF";
    }
}

const char *AudioHardware::getInputRouteFromDevice(uint32_t device)
{
    if (mMicMute) {
        return "off";
    }

    switch (device) {
    case AudioSystem::DEVICE_IN_BUILTIN_MIC:
        return "MAIN";
    case AudioSystem::DEVICE_IN_WIRED_HEADSET:
        return "HP";
    // case AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        // return "BT Sco Mic";
// #ifdef HAVE_FM_RADIO
    // case AudioSystem::DEVICE_IN_FM_RX:
        // return "FM Radio";
    // case AudioSystem::DEVICE_IN_FM_RX_A2DP:
        // return "FM Radio A2DP";
// #endif
    default:
        return "off";
    }
}

uint32_t AudioHardware::getInputSampleRate(uint32_t sampleRate)
{
    uint32_t i;
    uint32_t prevDelta;
    uint32_t delta;

    for (i = 0, prevDelta = 0xFFFFFFFF; i < sizeof(inputSamplingRates)/sizeof(uint32_t); i++, prevDelta = delta) {
        delta = abs(sampleRate - inputSamplingRates[i]);
        if (delta > prevDelta) break;
    }
    // i is always > 0 here
    return inputSamplingRates[i-1];
}

// getActiveInput_l() must be called with mLock held
sp <AudioHardware::AudioStreamInALSA> AudioHardware::getActiveInput_l()
{
    sp< AudioHardware::AudioStreamInALSA> spIn;

    for (size_t i = 0; i < mInputs.size(); i++) {
        // return first input found not being in standby mode
        // as only one input can be in this state
        if (!mInputs[i]->checkStandby()) {
            spIn = mInputs[i];
            break;
        }
    }

    return spIn;
}

status_t AudioHardware::setInputRoute(audio_source source, uint32_t device)
{
    LOGV("setInputSource_l(%d)", source);
    if (source != mInputSource) {
        if ((source == AUDIO_SOURCE_DEFAULT) || (mMode != AudioSystem::MODE_IN_CALL)) {
            if (mMixer)
            {
                const char* sourceName;
                switch (source) {
                case AUDIO_SOURCE_VOICE_RECOGNITION:
                case AUDIO_SOURCE_CAMCORDER:
                case AUDIO_SOURCE_DEFAULT: // intended fall-through
                case AUDIO_SOURCE_MIC:
                    sourceName = "Memo Path";
                    break;
                case AUDIO_SOURCE_VOICE_COMMUNICATION:
                    sourceName = "Calling Memo Path";
                    break;
                case AUDIO_SOURCE_VOICE_UPLINK:   // intended fall-through
                case AUDIO_SOURCE_VOICE_DOWNLINK: // intended fall-through
                case AUDIO_SOURCE_VOICE_CALL:     // intended fall-through
                default:
                    return NO_INIT;
                }
                TRACE_DRIVER_IN(DRV_MIXER_GET)
                struct mixer_ctl *ctl= mixer_get_control(mMixer, sourceName, 0);
                TRACE_DRIVER_OUT
                if (ctl == NULL) {
                    return NO_INIT;
                }
                const char *route = getInputRouteFromDevice(device);
                LOGV("setOutputRoute() '%s' '%s'",sourceName, route);
                TRACE_DRIVER_IN(DRV_MIXER_SEL)
                mixer_ctl_select(ctl, route);
                TRACE_DRIVER_OUT
            }
        }
        mInputSource = source;
    }

    return NO_ERROR;
}


//------------------------------------------------------------------------------
//  AudioStreamOutALSA
//------------------------------------------------------------------------------

AudioHardware::AudioStreamOutALSA::AudioStreamOutALSA() :
    mHardware(0), mPcm(0), mMixer(0), mRouteCtl(0),
    mStandby(true), mDevices(0), mChannels(AUDIO_HW_OUT_CHANNELS),
    mSampleRate(AUDIO_HW_OUT_SAMPLERATE), mBufferSize(AUDIO_HW_OUT_PERIOD_BYTES),
    mDriverOp(DRV_NONE), mStandbyCnt(0), mSleepReq(false)
{
}

status_t AudioHardware::AudioStreamOutALSA::set(
    AudioHardware* hw, uint32_t devices, int *pFormat,
    uint32_t *pChannels, uint32_t *pRate)
{
    int lFormat = pFormat ? *pFormat : 0;
    uint32_t lChannels = pChannels ? *pChannels : 0;
    uint32_t lRate = pRate ? *pRate : 0;

    mHardware = hw;
    mDevices = devices;

    // fix up defaults
    if (lFormat == 0) lFormat = format();
    if (lChannels == 0) lChannels = channels();
    if (lRate == 0) lRate = sampleRate();

    // check values
    if ((lFormat != format()) ||
        (lChannels != channels()) ||
        (lRate != sampleRate())) {
        if (pFormat) *pFormat = format();
        if (pChannels) *pChannels = channels();
        if (pRate) *pRate = sampleRate();
        return BAD_VALUE;
    }

    if (pFormat) *pFormat = lFormat;
    if (pChannels) *pChannels = lChannels;
    if (pRate) *pRate = lRate;

    mChannels = lChannels;
    mSampleRate = lRate;
    mBufferSize = AUDIO_HW_OUT_PERIOD_BYTES;

    return NO_ERROR;
}

AudioHardware::AudioStreamOutALSA::~AudioStreamOutALSA()
{
    standby();
}

ssize_t AudioHardware::AudioStreamOutALSA::write(const void* buffer, size_t bytes)
{
    //    LOGV("AudioStreamOutALSA::write(%p, %u)", buffer, bytes);
    status_t status = NO_INIT;
    const uint8_t* p = static_cast<const uint8_t*>(buffer);
    int ret;

    if (mHardware == NULL) return NO_INIT;

    if (mSleepReq) {
        // 10ms are always shorter than the time to reconfigure the audio path
        // which is the only condition when mSleepReq would be true.
        usleep(10000);
    }

    { // scope for the lock

        AutoMutex lock(mLock);

        if (mStandby) {
            AutoMutex hwLock(mHardware->lock());

            LOGD("AudioHardware pcm playback is exiting standby.");
            acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioOutLock");

            sp<AudioStreamInALSA> spIn = mHardware->getActiveInput_l();
            while (spIn != 0) {
                int cnt = spIn->prepareLock();
                mHardware->lock().unlock();
                // Mutex acquisition order is always out -> in -> hw
                spIn->lock();
                mHardware->lock().lock();
                // make sure that another thread did not change input state
                // while the mutex is released
                if ((spIn == mHardware->getActiveInput_l()) &&
                        (cnt == spIn->standbyCnt())) {
                    LOGV("AudioStreamOutALSA::write() force input standby");
                    spIn->close_l();
                    break;
                }
                spIn->unlock();
                spIn = mHardware->getActiveInput_l();
            }
            // spIn is not 0 here only if the input was active and has been
            // closed above

            // open output before input
            open_l();

            if (spIn != 0) {
                if (spIn->open_l() != NO_ERROR) {
                    spIn->doStandby_l();
                }
                spIn->unlock();
            }
            if (mPcm == NULL) {
                release_wake_lock("AudioOutLock");
                goto Error;
            }
            mStandby = false;
        }

        TRACE_DRIVER_IN(DRV_PCM_WRITE)
        ret = pcm_write(mPcm,(void*) p, bytes);
        TRACE_DRIVER_OUT

        if (ret == 0) {
            return bytes;
        }
        LOGW("write error: %d", errno);
        status = -errno;
    }
Error:

    standby();

    // Simulate audio output timing in case of error
    usleep((((bytes * 1000) / frameSize()) * 1000) / sampleRate());

    return status;
}

status_t AudioHardware::AudioStreamOutALSA::standby()
{
    if (mHardware == NULL) return NO_INIT;

    AutoMutex lock(mLock);

    { // scope for the AudioHardware lock
        AutoMutex hwLock(mHardware->lock());

        doStandby_l();
    }

    return NO_ERROR;
}

void AudioHardware::AudioStreamOutALSA::doStandby_l()
{
    mStandbyCnt++;

    if (!mStandby) {
        LOGD("AudioHardware pcm playback is going to standby.");
        release_wake_lock("AudioOutLock");
        mStandby = true;
    }

    close_l();
}

void AudioHardware::AudioStreamOutALSA::close_l()
{

    if (mMixer) {
        // powerdown MAX9877
        TRACE_DRIVER_IN(DRV_MIXER_GET)
        struct mixer_ctl *ctl= mixer_get_control(mMixer, "Amp Enable", 0);
        TRACE_DRIVER_OUT
        if(ctl)
        {
            TRACE_DRIVER_IN(DRV_MIXER_SEL)
            mixer_ctl_select(ctl, "OFF");
            TRACE_DRIVER_OUT
        }
        mHardware->closeMixer_l();
        mMixer = NULL;
        mRouteCtl = NULL;
    }
    if (mPcm) {
        mHardware->closePcmOut_l();
        mPcm = NULL;
    }
}

status_t AudioHardware::AudioStreamOutALSA::open_l()
{
    LOGV("open pcm_out driver");
    mPcm = mHardware->openPcmOut_l();
    if (mPcm == NULL) {
        return NO_INIT;
    }

    mMixer = mHardware->openMixer_l();

    if (mHardware->mode() != AudioSystem::MODE_IN_CALL)
    {
        mHardware->setOutputRoute( ROUTE_OUT_PLAYBACK, mDevices);
    }

    return NO_ERROR;
}

status_t AudioHardware::AudioStreamOutALSA::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\t\tAudioStreamOutALSA maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\t\tmHardware: %p\n", mHardware);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmPcm: %p\n", mPcm);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmMixer: %p\n", mMixer);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmRouteCtl: %p\n", mRouteCtl);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tStandby %s\n", (mStandby) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDevices: 0x%08x\n", mDevices);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmChannels: 0x%08x\n", mChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmSampleRate: %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmBufferSize: %d\n", mBufferSize);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);

    ::write(fd, result.string(), result.size());

    return NO_ERROR;
}

bool AudioHardware::AudioStreamOutALSA::checkStandby()
{
    return mStandby;
}

status_t AudioHardware::AudioStreamOutALSA::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);

    status_t status = NO_ERROR;
    int device;
    LOGD("AudioStreamOutALSA::setParameters() %s", keyValuePairs.string());

    if (mHardware == NULL) return NO_INIT;

    {
        AutoMutex lock(mLock);

        if (param.getInt(String8(AudioParameter::keyRouting), device) == NO_ERROR)
        {
            if (device != 0) {
                AutoMutex hwLock(mHardware->lock());

                if (mDevices != (uint32_t)device) {
                    mDevices = (uint32_t)device;

                    if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                        doStandby_l();
                    }
                }
                if (mHardware->mode() == AudioSystem::MODE_IN_CALL) {
                    mHardware->setIncallPath_l(device);
                }
            }
            param.remove(String8(AudioParameter::keyRouting));
        }
    }

    if (param.size()) {
        status = BAD_VALUE;
    }

    return status;
}

String8 AudioHardware::AudioStreamOutALSA::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    LOGV("AudioStreamOutALSA::getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioHardware::AudioStreamOutALSA::getRenderPosition(uint32_t *dspFrames)
{
    //TODO
    return INVALID_OPERATION;
}

int AudioHardware::AudioStreamOutALSA::prepareLock()
{
    // request sleep next time write() is called so that caller can acquire
    // mLock
    mSleepReq = true;
    return mStandbyCnt;
}

void AudioHardware::AudioStreamOutALSA::lock()
{
    mLock.lock();
    mSleepReq = false;
}

void AudioHardware::AudioStreamOutALSA::unlock() {
    mLock.unlock();
}

//------------------------------------------------------------------------------
//  AudioStreamInALSA
//------------------------------------------------------------------------------

AudioHardware::AudioStreamInALSA::AudioStreamInALSA() :
    mHardware(0), mPcm(0), mMixer(0), mRouteCtl(0),
    mStandby(true), mDevices(0), mChannels(AUDIO_HW_IN_CHANNELS), mChannelCount(2),
    mSampleRate(AUDIO_HW_IN_SAMPLERATE), mBufferSize(AUDIO_HW_IN_PERIOD_BYTES),
    mDownSampler(NULL), mChannelMixer(NULL), mReadStatus(NO_ERROR),
    mInPcmInBuf(0), mPcmIn(NULL), mDriverOp(DRV_NONE),
    mStandbyCnt(0), mSleepReq(false)
{
}

status_t AudioHardware::AudioStreamInALSA::set(
    AudioHardware* hw, uint32_t devices, int *pFormat,
    uint32_t *pChannels, uint32_t *pRate, AudioSystem::audio_in_acoustics acoustics)
{
    if (!pFormat || !pChannels || !pRate)
        return BAD_VALUE;

    if (*pFormat != AUDIO_HW_IN_FORMAT) {
        LOGD("Invalid audio input format %d.", *pFormat);
        *pFormat = AUDIO_HW_IN_FORMAT;
        return BAD_VALUE;
    }

    uint32_t rate = AudioHardware::getInputSampleRate(*pRate);
    if (rate != *pRate) {
        LOGD("Invalid audio input sample rate %d.", *pRate);
        *pRate = rate;
        return BAD_VALUE;
    }

    if (*pChannels != AudioSystem::CHANNEL_IN_MONO &&
        *pChannels != AudioSystem::CHANNEL_IN_STEREO) {
         LOGD("Invalid audio input channels %d.", *pChannels);
        *pChannels = AUDIO_HW_IN_CHANNELS;
        return BAD_VALUE;
    }

    mHardware = hw;

    LOGV("AudioStreamInALSA::set(%d, %d, %u)", *pFormat, *pChannels, *pRate);
    BufferProvider *bufferProvider = this;
    mDevices = devices;
    mInputChannels = AUDIO_HW_IN_CHANNELS;
    mInputChannelCount = 2;
    mChannels = *pChannels;
    mChannelCount = AudioSystem::popCount(mChannels);
    delete mChannelMixer;
    mChannelMixer = NULL;
    if (mChannels != AUDIO_HW_IN_CHANNELS) {
        mChannelMixer = new AudioHardware::ChannelMixer(mChannelCount,
                                    mInputChannelCount, AUDIO_HW_IN_PERIOD_SZ, this);
        status_t status = mChannelMixer->initCheck();
        if (status != NO_ERROR) {
            delete mChannelMixer;
            LOGW("AudioStreamInALSA::set() channel mixer init failed: %d", status);
            return status;
        }

        bufferProvider = mChannelMixer;
        if (!mPcmIn)
            mPcmIn = new int16_t[AUDIO_HW_IN_PERIOD_SZ * mInputChannelCount];
        if (!mPcmIn)
            return NO_MEMORY;
    }
    mBufferSize = getBufferSize(rate, mChannelCount);
    mSampleRate = rate;
    delete mDownSampler;
    mDownSampler = NULL;
    if (mSampleRate != AUDIO_HW_IN_SAMPLERATE) {
        mDownSampler = new AudioHardware::DownSampler(mSampleRate,
                                                  mChannelCount,
                                                  AUDIO_HW_IN_PERIOD_SZ,
                                                  bufferProvider);
        status_t status = mDownSampler->initCheck();
        if (status != NO_ERROR) {
            delete mDownSampler;
            LOGW("AudioStreamInALSA::set() downsampler init failed: %d", status);
            return status;
        }

        if (!mPcmIn)
            mPcmIn = new int16_t[AUDIO_HW_IN_PERIOD_SZ * mInputChannelCount];
        if (!mPcmIn)
            return NO_MEMORY;
    }
    return NO_ERROR;
}

AudioHardware::AudioStreamInALSA::~AudioStreamInALSA()
{
    standby();
    if (mDownSampler != NULL)
        delete mDownSampler;
    if (mChannelMixer != NULL)
        delete mChannelMixer;
    if (mPcmIn != NULL)
        delete[] mPcmIn;
}

ssize_t AudioHardware::AudioStreamInALSA::read(void* buffer, ssize_t bytes)
{
    //    LOGV("AudioStreamInALSA::read(%p, %u)", buffer, bytes);
    status_t status = NO_INIT;
    int ret;

    if (mHardware == NULL) return NO_INIT;

    if (mSleepReq) {
        // 10ms are always shorter than the time to reconfigure the audio path
        // which is the only condition when mSleepReq would be true.
        usleep(10000);
    }

    { // scope for the lock
        AutoMutex lock(mLock);

        if (mStandby) {
            AutoMutex hwLock(mHardware->lock());

            LOGD("AudioHardware pcm capture is exiting standby.");
            acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioInLock");

            sp<AudioStreamOutALSA> spOut = mHardware->output();
            while (spOut != 0) {
                if (!spOut->checkStandby()) {
                    int cnt = spOut->prepareLock();
                    mHardware->lock().unlock();
                    mLock.unlock();
                    // Mutex acquisition order is always out -> in -> hw
                    spOut->lock();
                    mLock.lock();
                    mHardware->lock().lock();
                    // make sure that another thread did not change output state
                    // while the mutex is released
                    if ((spOut == mHardware->output()) && (cnt == spOut->standbyCnt())) {
                        LOGV("AudioStreamInALSA::read() force output standby");
                        spOut->close_l();
                        break;
                    }
                    spOut->unlock();
                    spOut = mHardware->output();
                } else {
                    spOut.clear();
                }
            }
            // spOut is not 0 here only if the output was active and has been
            // closed above

            // open output before input
            if (spOut != 0) {
                if (spOut->open_l() != NO_ERROR) {
                    spOut->doStandby_l();
                }
                spOut->unlock();
            }

            open_l();

            if (mPcm == NULL) {
                release_wake_lock("AudioInLock");
                goto Error;
            }
            mStandby = false;
        }


        if (mDownSampler != NULL) {
            size_t frames = bytes / frameSize();
            size_t framesIn = 0;
            mReadStatus = 0;
            do {
                size_t outframes = frames - framesIn;
                mDownSampler->resample(
                        (int16_t *)buffer + (framesIn * mChannelCount),
                        &outframes);
                framesIn += outframes;
            } while ((framesIn < frames) && mReadStatus == 0);
            ret = mReadStatus;
            bytes = framesIn * frameSize();
        } else if (mChannelMixer != NULL) {
            size_t frames = bytes / frameSize();
            size_t framesIn = 0;
            mReadStatus = 0;
            do {
                size_t outframes = frames - framesIn;
                mChannelMixer->mix(
                        (int16_t *)buffer + (framesIn * mChannelCount),
                        &outframes);
                framesIn += outframes;
            } while ((framesIn < frames) && mReadStatus == 0);
            ret = mReadStatus;
            bytes = framesIn * frameSize();
        } else {
            TRACE_DRIVER_IN(DRV_PCM_READ)
            ret = pcm_read(mPcm, buffer, bytes);
            TRACE_DRIVER_OUT
        }

        if (ret == 0) {
            return bytes;
        }

        LOGW("read error: %d", ret);
        status = ret;
    }

Error:

    standby();

    // Simulate audio output timing in case of error
    usleep((((bytes * 1000) / frameSize()) * 1000) / sampleRate());

    return status;
}

status_t AudioHardware::AudioStreamInALSA::standby()
{
    if (mHardware == NULL) return NO_INIT;

    AutoMutex lock(mLock);

    { // scope for AudioHardware lock
        AutoMutex hwLock(mHardware->lock());

        doStandby_l();
    }
    return NO_ERROR;
}

void AudioHardware::AudioStreamInALSA::doStandby_l()
{
    mStandbyCnt++;

    if (!mStandby) {
        LOGD("AudioHardware pcm capture is going to standby.");
        release_wake_lock("AudioInLock");
        mStandby = true;
    }
    close_l();
}

void AudioHardware::AudioStreamInALSA::close_l()
{
    if (mMixer) {
        mHardware->closeMixer_l();
        mMixer = NULL;
        mRouteCtl = NULL;
    }

    if (mPcm) {
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        pcm_close(mPcm);
        TRACE_DRIVER_OUT
        mPcm = NULL;
    }
}

status_t AudioHardware::AudioStreamInALSA::open_l()
{
    unsigned flags = PCM_IN;

    flags |= (AUDIO_HW_IN_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
    flags |= (AUDIO_HW_IN_PERIOD_CNT - PCM_PERIOD_CNT_MIN)
            << PCM_PERIOD_CNT_SHIFT;

    LOGV("open pcm_in driver");
    TRACE_DRIVER_IN(DRV_PCM_OPEN)
    mPcm = pcm_open(flags);
    TRACE_DRIVER_OUT
    if (!pcm_ready(mPcm)) {
        LOGE("cannot open pcm_in driver: %s\n", pcm_error(mPcm));
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        pcm_close(mPcm);
        TRACE_DRIVER_OUT
        mPcm = NULL;
        return NO_INIT;
    }

    if (mDownSampler != NULL) {
        mInPcmInBuf = 0;
        mDownSampler->reset();
    }

    mMixer = mHardware->openMixer_l();
#if 1
    if (mHardware->mode() != AudioSystem::MODE_IN_CALL)
        mHardware->setInputRoute( mHardware->mInputSource, mDevices);
        //mHardware->setOutputRoute(ROUTE_IN_NORMAL, mDevices);
#else
    if (mMixer) {
        TRACE_DRIVER_IN(DRV_MIXER_GET)
        mRouteCtl = mixer_get_control(mMixer, "Memo Path", 0);
        TRACE_DRIVER_OUT
    }

    if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
        const char *route = mHardware->getInputRouteFromDevice(mDevices);
        LOGV("read() wakeup setting route %s", route);
        if (mRouteCtl) {
            TRACE_DRIVER_IN(DRV_MIXER_SEL)
            mixer_ctl_select(mRouteCtl, route);
            TRACE_DRIVER_OUT
        }
    }
#endif
    return NO_ERROR;
}

status_t AudioHardware::AudioStreamInALSA::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\t\tAudioStreamInALSA maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\t\tmHardware: %p\n", mHardware);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmPcm: %p\n", mPcm);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmMixer: %p\n", mMixer);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tStandby %s\n", (mStandby) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDevices: 0x%08x\n", mDevices);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmChannels: 0x%08x\n", mChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmSampleRate: %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmBufferSize: %d\n", mBufferSize);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);
    write(fd, result.string(), result.size());

    return NO_ERROR;
}

bool AudioHardware::AudioStreamInALSA::checkStandby()
{
    return mStandby;
}

status_t AudioHardware::AudioStreamInALSA::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status = NO_ERROR;
    int value;

    LOGD("AudioStreamInALSA::setParameters() %s", keyValuePairs.string());

    if (mHardware == NULL) return NO_INIT;

    {
        AutoMutex lock(mLock);

        if (param.getInt(String8(AudioParameter::keyInputSource), value) == NO_ERROR) {
            AutoMutex hwLock(mHardware->lock());

            mHardware->openMixer_l();
            mHardware->setInputRoute((audio_source)value, mDevices);
            mHardware->closeMixer_l();

            param.remove(String8(AudioParameter::keyInputSource));
        }

        if (param.getInt(String8(AudioParameter::keyRouting), value) == NO_ERROR)
        {
            if (value != 0) {
                AutoMutex hwLock(mHardware->lock());

                if (mDevices != (uint32_t)value) {
                    mDevices = (uint32_t)value;
                    if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                        doStandby_l();
                    }
                }
            }
            param.remove(String8(AudioParameter::keyRouting));
        }
    }


    if (param.size()) {
        status = BAD_VALUE;
    }

    return status;

}

String8 AudioHardware::AudioStreamInALSA::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    LOGV("AudioStreamInALSA::getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioHardware::AudioStreamInALSA::getNextBuffer(AudioHardware::BufferProvider::Buffer* buffer)
{
    if (mPcm == NULL) {
        LOGE("getNextBuffer(): mPcm not init");
        buffer->raw = NULL;
        buffer->frameCount = 0;
        mReadStatus = NO_INIT;
        return NO_INIT;
    }

    if (mInPcmInBuf == 0) {
        TRACE_DRIVER_IN(DRV_PCM_READ)
        mReadStatus = pcm_read(mPcm,(void*) mPcmIn, AUDIO_HW_IN_PERIOD_BYTES);
        //mReadStatus = pcm_read(mPcm,(void*) mPcmIn, AUDIO_HW_IN_PERIOD_SZ * frameSize());
        TRACE_DRIVER_OUT
        if (mReadStatus != 0) {
            LOGE("getNextBuffer(): pcm_read failed: %s", pcm_error(mPcm));
            buffer->raw = NULL;
            buffer->frameCount = 0;
            return mReadStatus;
        }
        mInPcmInBuf = AUDIO_HW_IN_PERIOD_SZ;
    }

    buffer->frameCount = (buffer->frameCount > mInPcmInBuf) ? mInPcmInBuf : buffer->frameCount;
    buffer->i16 = mPcmIn + (AUDIO_HW_IN_PERIOD_SZ - mInPcmInBuf) * mInputChannelCount;

    return mReadStatus;
}

void AudioHardware::AudioStreamInALSA::releaseBuffer(Buffer* buffer)
{
    mInPcmInBuf -= buffer->frameCount;
}

size_t AudioHardware::AudioStreamInALSA::getBufferSize(uint32_t sampleRate, int channelCount)
{
    size_t ratio;

    switch (sampleRate) {
    case 8000:
    case 11025:
        ratio = 4;
        break;
    case 16000:
    case 22050:
        ratio = 2;
        break;
    case 44100:
    default:
        ratio = 1;
        break;
    }

    return (AUDIO_HW_IN_PERIOD_SZ*channelCount*sizeof(int16_t)) / ratio ;
}

int AudioHardware::AudioStreamInALSA::prepareLock()
{
    // request sleep next time read() is called so that caller can acquire
    // mLock
    mSleepReq = true;
    return mStandbyCnt;
}

void AudioHardware::AudioStreamInALSA::lock()
{
    mLock.lock();
    mSleepReq = false;
}

void AudioHardware::AudioStreamInALSA::unlock() {
    mLock.unlock();
}

//------------------------------------------------------------------------------
//  DownSampler
//------------------------------------------------------------------------------

/*
 * 2.30 fixed point FIR filter coefficients for conversion 44100 -> 22050.
 * (Works equivalently for 22010 -> 11025 or any other halving, of course.)
 *
 * Transition band from about 18 kHz, passband ripple < 0.1 dB,
 * stopband ripple at about -55 dB, linear phase.
 *
 * Design and display in MATLAB or Octave using:
 *
 * filter = fir1(19, 0.5); filter = round(filter * 2**30); freqz(filter * 2**-30);
 */
static const int32_t filter_22khz_coeff[] = {
    2089257, 2898328, -5820678, -10484531,
    19038724, 30542725, -50469415, -81505260,
    152544464, 478517512, 478517512, 152544464,
    -81505260, -50469415, 30542725, 19038724,
    -10484531, -5820678, 2898328, 2089257,
};
#define NUM_COEFF_22KHZ (sizeof(filter_22khz_coeff) / sizeof(filter_22khz_coeff[0]))
#define OVERLAP_22KHZ (NUM_COEFF_22KHZ - 2)

/*
 * Convolution of signals A and reverse(B). (In our case, the filter response
 * is symmetric, so the reversing doesn't matter.)
 * A is taken to be in 0.16 fixed-point, and B is taken to be in 2.30 fixed-point.
 * The answer will be in 16.16 fixed-point, unclipped.
 *
 * This function would probably be the prime candidate for SIMD conversion if
 * you want more speed.
 */
int32_t fir_convolve(const int16_t* a, const int32_t* b, int num_samples)
{
        int32_t sum = 1 << 13;
        for (int i = 0; i < num_samples; ++i) {
                sum += a[i] * (b[i] >> 16);
        }
        return sum >> 14;
}

/* Clip from 16.16 fixed-point to 0.16 fixed-point. */
int16_t clip(int32_t x)
{
    if (x < -32768) {
        return -32768;
    } else if (x > 32767) {
        return 32767;
    } else {
        return x;
    }
}

/*
 * Convert a chunk from 44 kHz to 22 kHz. Will update num_samples_in and num_samples_out
 * accordingly, since it may leave input samples in the buffer due to overlap.
 *
 * Input and output are taken to be in 0.16 fixed-point.
 */
void resample_2_1(int16_t* input, int16_t* output, int* num_samples_in, int* num_samples_out)
{
    if (*num_samples_in < (int)NUM_COEFF_22KHZ) {
        *num_samples_out = 0;
        return;
    }

    int odd_smp = *num_samples_in & 0x1;
    int num_samples = *num_samples_in - odd_smp - OVERLAP_22KHZ;

    for (int i = 0; i < num_samples; i += 2) {
            output[i / 2] = clip(fir_convolve(input + i, filter_22khz_coeff, NUM_COEFF_22KHZ));
    }

    memmove(input, input + num_samples, (OVERLAP_22KHZ + odd_smp) * sizeof(*input));
    *num_samples_out = num_samples / 2;
    *num_samples_in = OVERLAP_22KHZ + odd_smp;
}

/*
 * 2.30 fixed point FIR filter coefficients for conversion 22050 -> 16000,
 * or 11025 -> 8000.
 *
 * Transition band from about 14 kHz, passband ripple < 0.1 dB,
 * stopband ripple at about -50 dB, linear phase.
 *
 * Design and display in MATLAB or Octave using:
 *
 * filter = fir1(23, 16000 / 22050); filter = round(filter * 2**30); freqz(filter * 2**-30);
 */
static const int32_t filter_16khz_coeff[] = {
    2057290, -2973608, 1880478, 4362037,
    -14639744, 18523609, -1609189, -38502470,
    78073125, -68353935, -59103896, 617555440,
    617555440, -59103896, -68353935, 78073125,
    -38502470, -1609189, 18523609, -14639744,
    4362037, 1880478, -2973608, 2057290,
};
#define NUM_COEFF_16KHZ (sizeof(filter_16khz_coeff) / sizeof(filter_16khz_coeff[0]))
#define OVERLAP_16KHZ (NUM_COEFF_16KHZ - 1)

/*
 * Convert a chunk from 22 kHz to 16 kHz. Will update num_samples_in and
 * num_samples_out accordingly, since it may leave input samples in the buffer
 * due to overlap.
 *
 * This implementation is rather ad-hoc; it first low-pass filters the data
 * into a temporary buffer, and then converts chunks of 441 input samples at a
 * time into 320 output samples by simple linear interpolation. A better
 * implementation would use a polyphase filter bank to do these two operations
 * in one step.
 *
 * Input and output are taken to be in 0.16 fixed-point.
 */

#define RESAMPLE_16KHZ_SAMPLES_IN 441
#define RESAMPLE_16KHZ_SAMPLES_OUT 320

void resample_441_320(int16_t* input, int16_t* output, int* num_samples_in, int* num_samples_out)
{
    const int num_blocks = (*num_samples_in - OVERLAP_16KHZ) / RESAMPLE_16KHZ_SAMPLES_IN;
    if (num_blocks < 1) {
        *num_samples_out = 0;
        return;
    }

    for (int i = 0; i < num_blocks; ++i) {
        uint32_t tmp[RESAMPLE_16KHZ_SAMPLES_IN];
        for (int j = 0; j < RESAMPLE_16KHZ_SAMPLES_IN; ++j) {
            tmp[j] = fir_convolve(input + i * RESAMPLE_16KHZ_SAMPLES_IN + j,
                          filter_16khz_coeff,
                          NUM_COEFF_16KHZ);
        }

        const float step_float = (float)RESAMPLE_16KHZ_SAMPLES_IN / (float)RESAMPLE_16KHZ_SAMPLES_OUT;
        const uint32_t step = (uint32_t)(step_float * 32768.0f + 0.5f);  // 17.15 fixed point

        uint32_t in_sample_num = 0;   // 17.15 fixed point
        for (int j = 0; j < RESAMPLE_16KHZ_SAMPLES_OUT; ++j, in_sample_num += step) {
            const uint32_t whole = in_sample_num >> 15;
            const uint32_t frac = (in_sample_num & 0x7fff);  // 0.15 fixed point
            const int32_t s1 = tmp[whole];
            const int32_t s2 = tmp[whole + 1];
            *output++ = clip(s1 + (((s2 - s1) * (int32_t)frac) >> 15));
        }

    }

    const int samples_consumed = num_blocks * RESAMPLE_16KHZ_SAMPLES_IN;
    memmove(input, input + samples_consumed, (*num_samples_in - samples_consumed) * sizeof(*input));
    *num_samples_in -= samples_consumed;
    *num_samples_out = RESAMPLE_16KHZ_SAMPLES_OUT * num_blocks;
}


AudioHardware::DownSampler::DownSampler(uint32_t outSampleRate,
                                    uint32_t channelCount,
                                    uint32_t frameCount,
                                    AudioHardware::BufferProvider* provider)
    :  mStatus(NO_INIT), mProvider(provider), mSampleRate(outSampleRate),
       mChannelCount(channelCount), mFrameCount(frameCount),
       mInLeft(NULL), mInRight(NULL), mTmpLeft(NULL), mTmpRight(NULL),
       mTmp2Left(NULL), mTmp2Right(NULL), mOutLeft(NULL), mOutRight(NULL)

{
    LOGV("AudioHardware::DownSampler() cstor %p SR %d channels %d frames %d",
         this, mSampleRate, mChannelCount, mFrameCount);

    if (mSampleRate != 8000 && mSampleRate != 11025 && mSampleRate != 16000 &&
            mSampleRate != 22050) {
        LOGW("AudioHardware::DownSampler cstor: bad sampling rate: %d", mSampleRate);
        return;
    }

    mInLeft = new int16_t[mFrameCount];
    mInRight = new int16_t[mFrameCount];
    mTmpLeft = new int16_t[mFrameCount];
    mTmpRight = new int16_t[mFrameCount];
    mTmp2Left = new int16_t[mFrameCount];
    mTmp2Right = new int16_t[mFrameCount];
    mOutLeft = new int16_t[mFrameCount];
    mOutRight = new int16_t[mFrameCount];

    mStatus = NO_ERROR;
}

AudioHardware::DownSampler::~DownSampler()
{
    if (mInLeft) delete[] mInLeft;
    if (mInRight) delete[] mInRight;
    if (mTmpLeft) delete[] mTmpLeft;
    if (mTmpRight) delete[] mTmpRight;
    if (mTmp2Left) delete[] mTmp2Left;
    if (mTmp2Right) delete[] mTmp2Right;
    if (mOutLeft) delete[] mOutLeft;
    if (mOutRight) delete[] mOutRight;
}

void AudioHardware::DownSampler::reset()
{
    mInInBuf = 0;
    mInTmpBuf = 0;
    mInTmp2Buf = 0;
    mOutBufPos = 0;
    mInOutBuf = 0;
}


int AudioHardware::DownSampler::resample(int16_t* out, size_t *outFrameCount)
{
    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    if (out == NULL || outFrameCount == NULL) {
        return BAD_VALUE;
    }

    int16_t *outLeft = mTmp2Left;
    int16_t *outRight = mTmp2Left;
    if (mSampleRate == 22050) {
        outLeft = mTmpLeft;
        outRight = mTmpRight;
    } else if (mSampleRate == 8000){
        outLeft = mOutLeft;
        outRight = mOutRight;
    }

    int outFrames = 0;
    int remaingFrames = *outFrameCount;

    if (mInOutBuf) {
        int frames = (remaingFrames > mInOutBuf) ? mInOutBuf : remaingFrames;

        for (int i = 0; i < frames; ++i) {
            out[i] = outLeft[mOutBufPos + i];
        }
        if (mChannelCount == 2) {
            for (int i = 0; i < frames; ++i) {
                out[i * 2] = outLeft[mOutBufPos + i];
                out[i * 2 + 1] = outRight[mOutBufPos + i];
            }
        }
        remaingFrames -= frames;
        mInOutBuf -= frames;
        mOutBufPos += frames;
        outFrames += frames;
    }

    while (remaingFrames) {
        LOGW_IF((mInOutBuf != 0), "mInOutBuf should be 0 here");

        AudioHardware::BufferProvider::Buffer buf;
        buf.frameCount =  mFrameCount - mInInBuf;
        int ret = mProvider->getNextBuffer(&buf);
        if (buf.raw == NULL) {
            *outFrameCount = outFrames;
            return ret;
        }

        for (size_t i = 0; i < buf.frameCount; ++i) {
            mInLeft[i + mInInBuf] = buf.i16[i];
        }
        if (mChannelCount == 2) {
            for (size_t i = 0; i < buf.frameCount; ++i) {
                mInLeft[i + mInInBuf] = buf.i16[i * 2];
                mInRight[i + mInInBuf] = buf.i16[i * 2 + 1];
            }
        }
        mInInBuf += buf.frameCount;
        mProvider->releaseBuffer(&buf);

        /* 44010 -> 22050 */
        {
            int samples_in_left = mInInBuf;
            int samples_out_left;
            resample_2_1(mInLeft, mTmpLeft + mInTmpBuf, &samples_in_left, &samples_out_left);

            if (mChannelCount == 2) {
                int samples_in_right = mInInBuf;
                int samples_out_right;
                resample_2_1(mInRight, mTmpRight + mInTmpBuf, &samples_in_right, &samples_out_right);
            }

            mInInBuf = samples_in_left;
            mInTmpBuf += samples_out_left;
            mInOutBuf = samples_out_left;
        }

        if (mSampleRate == 11025 || mSampleRate == 8000) {
            /* 22050 - > 11025 */
            int samples_in_left = mInTmpBuf;
            int samples_out_left;
            resample_2_1(mTmpLeft, mTmp2Left + mInTmp2Buf, &samples_in_left, &samples_out_left);

            if (mChannelCount == 2) {
                int samples_in_right = mInTmpBuf;
                int samples_out_right;
                resample_2_1(mTmpRight, mTmp2Right + mInTmp2Buf, &samples_in_right, &samples_out_right);
            }


            mInTmpBuf = samples_in_left;
            mInTmp2Buf += samples_out_left;
            mInOutBuf = samples_out_left;

            if (mSampleRate == 8000) {
                /* 11025 -> 8000*/
                int samples_in_left = mInTmp2Buf;
                int samples_out_left;
                resample_441_320(mTmp2Left, mOutLeft, &samples_in_left, &samples_out_left);

                if (mChannelCount == 2) {
                    int samples_in_right = mInTmp2Buf;
                    int samples_out_right;
                    resample_441_320(mTmp2Right, mOutRight, &samples_in_right, &samples_out_right);
                }

                mInTmp2Buf = samples_in_left;
                mInOutBuf = samples_out_left;
            } else {
                mInTmp2Buf = 0;
            }

        } else if (mSampleRate == 16000) {
            /* 22050 -> 16000*/
            int samples_in_left = mInTmpBuf;
            int samples_out_left;
            resample_441_320(mTmpLeft, mTmp2Left, &samples_in_left, &samples_out_left);

            if (mChannelCount == 2) {
                int samples_in_right = mInTmpBuf;
                int samples_out_right;
                resample_441_320(mTmpRight, mTmp2Right, &samples_in_right, &samples_out_right);
            }

            mInTmpBuf = samples_in_left;
            mInOutBuf = samples_out_left;
        } else {
            mInTmpBuf = 0;
        }

        int frames = (remaingFrames > mInOutBuf) ? mInOutBuf : remaingFrames;

        for (int i = 0; i < frames; ++i) {
            out[outFrames + i] = outLeft[i];
        }
        if (mChannelCount == 2) {
            for (int i = 0; i < frames; ++i) {
                out[(outFrames + i) * 2] = outLeft[i];
                out[(outFrames + i) * 2 + 1] = outRight[i];
            }
        }
        remaingFrames -= frames;
        outFrames += frames;
        mOutBufPos = frames;
        mInOutBuf -= frames;
    }

    return 0;
}

//------------------------------------------------------------------------------
//  Channel mixer
//------------------------------------------------------------------------------

AudioHardware::ChannelMixer::ChannelMixer(uint32_t outChannelCount,
                                    uint32_t channelCount,
                                    uint32_t frameCount,
                                    AudioHardware::BufferProvider* provider)
    :  mStatus(NO_INIT), mProvider(provider), mOutChannelCount(outChannelCount),
       mChannelCount(channelCount)
{
    LOGV("AudioHardware::ChannelMixer() cstor %p channels %d frames %d",
         this, mChannelCount, frameCount);

    if (outChannelCount != 1 || channelCount != 2) {
        LOGE("AudioHardware::ChannelMixer cstor: bad conversion: %d => %d",
                                                mChannelCount, outChannelCount);
        return;
    }

    mStatus = NO_ERROR;
}

status_t AudioHardware::ChannelMixer::getNextBuffer(AudioHardware::BufferProvider::Buffer* buffer)
{
    status_t ret;

    if (!mProvider)
        return NO_INIT;

    ret = mProvider->getNextBuffer(buffer);
    if (ret != 0) {
        LOGE("%s: mProvider->getNextBuffer() failed (%d)", __func__, ret);
        return ret;
    }
    if (!buffer->raw)
        return NO_ERROR;

    short *in = buffer->i16;
    short *out = buffer->i16;
    for (unsigned int i = 0; i < buffer->frameCount; ++i, ++out, in += 2)
        out[0] = (in[0] + in[1]) / 2;

    return NO_ERROR;
}

void AudioHardware::ChannelMixer::releaseBuffer(Buffer* buffer)
{
    if (mProvider)
        mProvider->releaseBuffer(buffer);
}

int AudioHardware::ChannelMixer::mix(int16_t* out, size_t *outFrameCount)
{
    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    if (out == NULL || outFrameCount == NULL) {
        return BAD_VALUE;
    }

    int remaingFrames = *outFrameCount;
    while (remaingFrames) {
        AudioHardware::BufferProvider::Buffer buf;
        buf.frameCount = remaingFrames;

        int ret = mProvider->getNextBuffer(&buf);
        if (ret || buf.raw == NULL) {
            *outFrameCount -= remaingFrames;
            return ret;
        }

        remaingFrames -= buf.frameCount;

        int framesRead = buf.frameCount;
        int16_t *inBuf = buf.i16;
        while (framesRead--) {
            out[0] = (int16_t)(((int32_t)inBuf[0] + (int32_t)inBuf[1]) / 2);
            out += 1;
            inBuf += 2;
        }

        mProvider->releaseBuffer(&buf);
    }

    return 0;
}

//------------------------------------------------------------------------------
//  Factory
//------------------------------------------------------------------------------

extern "C" AudioHardwareInterface* createAudioHardware(void) {
    return new AudioHardware();
}

}; // namespace android
