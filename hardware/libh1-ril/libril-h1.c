/* /device/samsung/nowplus/hardware/libsecril/libril-h1.c
**
** Copyright 2011, r3d4
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
**
** impelemts a warpper for libsecril.so to install own reqquest handlers
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <telephony/ril.h>

#define LOG_NDEBUG 0
#define LOG_TAG "H1RIL"

#include <utils/Log.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <linux/capability.h>
#include <linux/prctl.h>

#include "libril-h1.h"
/*
typedef void * RIL_Token;

typedef struct {
    int requestNumber;
    void (*dispatchFunction) (Parcel &p, struct RequestInfo *pRI);
    int(*responseFunction) (Parcel &p, void *response, size_t responselen);
} CommandInfo;

typedef struct RequestInfo {
    int32_t token;      //this is not RIL_Token
    CommandInfo *pCI;
    struct RequestInfo *p_next;
    char cancelled;
    char local;         // responses to local commands do not go back to command process
} RequestInfo;


    RequestInfo *pRI = (RequestInfo *)t;

    pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));

    pRI->local = 1;
    pRI->token = 0xffffffff;        // token is not used in this context
    pRI->pCI = &(s_commands[request]);

    issueLocalRequest(int request, void *data, int len)
*/

static RIL_RadioFunctions s_callbacks = {
    0,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

static struct RIL_Env s_rilEnv = {
    RIL_onRequestComplete,
    RIL_onUnsolicitedResponse,
    RIL_requestTimedCallback
};

RIL_Token   lokalToken;
const RIL_RadioFunctions *secRilFuncs = NULL;   //callbacks to libsec-ril
const struct RIL_Env *secRilEnv = NULL;         //callbacks to libril

static int g3GFixEnable = 1;
static int gSmsFixEnable = 1;

struct myTrigger onCompleteTrigger;
// static int onCompleteTrigger = NO_TRIGGER;
// void (*onCompleteAction)();

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
    char value[PROPERTY_VALUE_MAX];

    LOGV("RIL_Init()\n");

    if(property_get("ro.ril.enable.3gfix", value, ""))
        g3GFixEnable = value[0]-'0';
    if(property_get("ro.ril.enable.smsfix", value, ""))
        gSmsFixEnable = value[0]-'0';

    LOGV("3G Fix %s", g3GFixEnable?"enabled":"disabled");
    LOGV("SMS Fix %s", gSmsFixEnable?"enabled":"disabled");

    RIL_RadioFunctions *h1RilFuncs = &s_callbacks;

    void *dlHandle = dlopen(LIBSEC_RIL_PATH, RTLD_NOW);
    if (dlHandle == NULL) {
        LOGE("sec ril not found %s\n", LIBSEC_RIL_PATH);
        goto err;
    }
    rilInit = (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))dlsym(dlHandle, "RIL_Init");
    if (rilInit == NULL) {
        LOGE("RIL_Init not defined or exported in %s\n", LIBSEC_RIL_PATH);
        goto err1;
    }

    secRilEnv = env;
    struct RIL_Env *h1RilEnv = &s_rilEnv;   //install libril callback wrapper
    secRilFuncs = rilInit(h1RilEnv, argc, argv);
    //secRilFuncs = rilInit(env, argc, argv);
    if(secRilFuncs == NULL) {
        LOGE("get no ril functions !%s\n", LIBSEC_RIL_PATH);
        goto err1;
    }

    h1RilFuncs->version = secRilFuncs->version;
    LOGD("libsec-ril wrapper installed");
    LOGV("    secRilFuncs= 0x%0x, h1RilFuncs= 0x%x\n",
        (int)secRilFuncs, (int)h1RilFuncs);
    LOGV("    secRilEnv= 0x%0x, h1RilEnv= 0x%x\n",
        (int)secRilEnv, (int)h1RilEnv);

err1:
    dlclose(dlHandle);
err:
    //return secRilFuncs;
    return h1RilFuncs; //return wrapped ril functions
}

// --------------- env wrapper --------------------
void RIL_onRequestComplete(RIL_Token t, RIL_Errno e,
                           void *response, size_t responselen)
{
    if(!secRilEnv)
        return;

    RequestInfo *pRI = (RequestInfo *)t;
    int req = *(pRI->requestNumber);
    int token = pRI->token;

    LOGV("RIL_onRequestComplete(): %s (%d), token=%d", requestToString(req),  req, token);

    // not send to libril, if we issued the request
    if(t!=lokalToken)
        secRilEnv->OnRequestComplete(t, e, response, responselen);

    //we have a installed trigger on this request ?
    if(onCompleteTrigger.token == token)
    {
        LOGD("RIL_onRequestComplete(): execute %s trigger", onCompleteTrigger.message);
        onCompleteTrigger.action();    //send ipc command to libsec-ril
        onCompleteTrigger.token = NO_TRIGGER; //delete trigger
    }
}

void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen)
{
    if(!secRilEnv)
        return;

    LOGV("RIL_onUnsolicitedResponse() < %s (%d)", requestToString(unsolResponse), unsolResponse);
    secRilEnv->OnUnsolicitedResponse(unsolResponse, data, datalen);
}

void RIL_requestTimedCallback (RIL_TimedCallback callback,
                               void *param, const struct timeval *relativeTime)
{
    if(!secRilEnv)
        return;

    LOGV("RIL_requestTimedCallback()");
    secRilEnv->RequestTimedCallback(callback, param, relativeTime);
}


// --------------- ril wrapper --------------------


int ipcFix3G()
{
//ipctool 09 00 35 00 05 01 03 82 00
    //ipc header
    unsigned char data[] = { 0x09, 0x00, 0x35, 0x00, 0x05, 0x01, 0x03, 0x82, 0x00 };
    return ipc_send_data( data, sizeof(data), MODEM_TO_PDA);
}

int ipcFixSMS()
{
    unsigned char data[254];
    //fe 00 00 ff 04 06 01 00 00 03 00 02 .....
    //ipc header + data
    unsigned char data1[]={0x00, 0x00, 0x00, 0xff, 0x04, 0x06, 0x01, 0x00, 0x00, 0x03, 0x00, 0x02};

    memset(data, 0, sizeof(data));
    memcpy(&data, data1, sizeof(data1));

    return ipc_send_data( data, sizeof(data), PDA_TO_MODEM);
}

static void requestSMSAcknowledge(void *data, size_t datalen, RIL_Token t)
{
    int ackSuccess = ((int *)data)[0];
    LOGV("requestSMSAcknowledge()");

    if (ackSuccess == 1)
    {
        if(ipcFixSMS() != 0)
        {
            LOGE("requestSMSAcknowledge() ipcFixSMS() failed");
            goto error;
        }
        LOGD("requestSMSAcknowledge() sending SMS_ACKNOWLEDGE\n");
    }
    else if (ackSuccess == 0) {
        LOGD("requestSMSAcknowledge() not sending SMS_ACKNOWLEDGE\n");
    }
    else {
        LOGE("unsupported arg to RIL_REQUEST_SMS_ACKNOWLEDGE\n");
        goto error;
    }

    LOGV("requestSMSAcknowledge() sucsessfull");
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}
// struct myTrigger {
    // int token;
    // int (*action)();
    // char *message;
// };
static void installOnReqCompleteTrigger(RIL_Token t, int (*action)(), char *message)
{
    RequestInfo *pRI = (RequestInfo *)t;
    LOGV("requestRadioPower():install trigger, token=%d", pRI->token);
    onCompleteTrigger.token = pRI->token;
    onCompleteTrigger.action = action;
    onCompleteTrigger.message = message;
}

void onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    if(!secRilFuncs)
        return;

    int replaceRequest = 0; //if set secril request will no be executed
    RequestInfo *pRI = (RequestInfo *)t;

    LOGV("onRequest() > %s (%d), token=%d", requestToString(request), request, pRI->token);

    //do pre processing
    switch(request)
    {
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            if(gSmsFixEnable)
            {
                replaceRequest = 1; //use own handler
                requestSMSAcknowledge(data, datalen, t);
            }
            break;

        case RIL_REQUEST_RADIO_POWER:
            if(g3GFixEnable)
            {
                //set trigger, its called when libsec-ril finshes the request
                int onOff = ((int *)data)[0];
                if(onOff==1)
                    installOnReqCompleteTrigger( t, ipcFix3G, "ipcFix3G");
            }
            break;
    }

    // do sec-ril request
    if(!replaceRequest)
        secRilFuncs->onRequest(request, data, datalen, t);
}

RIL_RadioState currentState()
{
    if(!secRilFuncs)
        return RADIO_STATE_UNAVAILABLE;

    LOGV("currentState()");
    RIL_RadioState ret = secRilFuncs->onStateRequest();

    return ret;
}

int onSupports (int requestCode)
{
    if(!secRilFuncs)
        return 0;
    LOGV("onSupports() requestCode= %d", requestCode);
    int ret = secRilFuncs->supports(requestCode);

    return ret;
}

void onCancel (RIL_Token t)
{
    if(!secRilFuncs)
        return;
    LOGV("onCancel()");
    secRilFuncs->onCancel(t);
}

const char * getVersion(void)
{
    if(!secRilFuncs)
        return "empty h1 secril wrapper";


    const char *ret = secRilFuncs->getVersion();
    LOGV("getVersion() = %s", ret);
    return ret;
}

const char *requestToString(int request) {
/*
 cat libs/telephony/ril_commands.h \
 | egrep "^ *{RIL_" \
 | sed -re 's/\{RIL_([^,]+),[^,]+,([^}]+).+/case RIL_\1: return "\1";/'


 cat libs/telephony/ril_unsol_commands.h \
 | egrep "^ *{RIL_" \
 | sed -re 's/\{RIL_([^,]+),([^}]+).+/case RIL_\1: return "\1";/'

*/
    switch(request) {
        case RIL_REQUEST_GET_SIM_STATUS: return "GET_SIM_STATUS";
        case RIL_REQUEST_ENTER_SIM_PIN: return "ENTER_SIM_PIN";
        case RIL_REQUEST_ENTER_SIM_PUK: return "ENTER_SIM_PUK";
        case RIL_REQUEST_ENTER_SIM_PIN2: return "ENTER_SIM_PIN2";
        case RIL_REQUEST_ENTER_SIM_PUK2: return "ENTER_SIM_PUK2";
        case RIL_REQUEST_CHANGE_SIM_PIN: return "CHANGE_SIM_PIN";
        case RIL_REQUEST_CHANGE_SIM_PIN2: return "CHANGE_SIM_PIN2";
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION: return "ENTER_NETWORK_DEPERSONALIZATION";
        case RIL_REQUEST_GET_CURRENT_CALLS: return "GET_CURRENT_CALLS";
        case RIL_REQUEST_DIAL: return "DIAL";
        case RIL_REQUEST_GET_IMSI: return "GET_IMSI";
        case RIL_REQUEST_HANGUP: return "HANGUP";
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND: return "HANGUP_WAITING_OR_BACKGROUND";
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND: return "HANGUP_FOREGROUND_RESUME_BACKGROUND";
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE: return "SWITCH_WAITING_OR_HOLDING_AND_ACTIVE";
        case RIL_REQUEST_CONFERENCE: return "CONFERENCE";
        case RIL_REQUEST_UDUB: return "UDUB";
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE: return "LAST_CALL_FAIL_CAUSE";
        case RIL_REQUEST_SIGNAL_STRENGTH: return "SIGNAL_STRENGTH";
        case RIL_REQUEST_REGISTRATION_STATE: return "REGISTRATION_STATE";
        case RIL_REQUEST_GPRS_REGISTRATION_STATE: return "GPRS_REGISTRATION_STATE";
        case RIL_REQUEST_OPERATOR: return "OPERATOR";
        case RIL_REQUEST_RADIO_POWER: return "RADIO_POWER";
        case RIL_REQUEST_DTMF: return "DTMF";
        case RIL_REQUEST_SEND_SMS: return "SEND_SMS";
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE: return "SEND_SMS_EXPECT_MORE";
        case RIL_REQUEST_SETUP_DATA_CALL: return "SETUP_DATA_CALL";
        case RIL_REQUEST_SIM_IO: return "SIM_IO";
        case RIL_REQUEST_SEND_USSD: return "SEND_USSD";
        case RIL_REQUEST_CANCEL_USSD: return "CANCEL_USSD";
        case RIL_REQUEST_GET_CLIR: return "GET_CLIR";
        case RIL_REQUEST_SET_CLIR: return "SET_CLIR";
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS: return "QUERY_CALL_FORWARD_STATUS";
        case RIL_REQUEST_SET_CALL_FORWARD: return "SET_CALL_FORWARD";
        case RIL_REQUEST_QUERY_CALL_WAITING: return "QUERY_CALL_WAITING";
        case RIL_REQUEST_SET_CALL_WAITING: return "SET_CALL_WAITING";
        case RIL_REQUEST_SMS_ACKNOWLEDGE: return "SMS_ACKNOWLEDGE";
        case RIL_REQUEST_GET_IMEI: return "GET_IMEI";
        case RIL_REQUEST_GET_IMEISV: return "GET_IMEISV";
        case RIL_REQUEST_ANSWER: return "ANSWER";
        case RIL_REQUEST_DEACTIVATE_DATA_CALL: return "DEACTIVATE_DATA_CALL";
        case RIL_REQUEST_QUERY_FACILITY_LOCK: return "QUERY_FACILITY_LOCK";
        case RIL_REQUEST_SET_FACILITY_LOCK: return "SET_FACILITY_LOCK";
        case RIL_REQUEST_CHANGE_BARRING_PASSWORD: return "CHANGE_BARRING_PASSWORD";
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE: return "QUERY_NETWORK_SELECTION_MODE";
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC: return "SET_NETWORK_SELECTION_AUTOMATIC";
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL: return "SET_NETWORK_SELECTION_MANUAL";
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS : return "QUERY_AVAILABLE_NETWORKS ";
        case RIL_REQUEST_DTMF_START: return "DTMF_START";
        case RIL_REQUEST_DTMF_STOP: return "DTMF_STOP";
        case RIL_REQUEST_BASEBAND_VERSION: return "BASEBAND_VERSION";
        case RIL_REQUEST_SEPARATE_CONNECTION: return "SEPARATE_CONNECTION";
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE: return "SET_PREFERRED_NETWORK_TYPE";
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE: return "GET_PREFERRED_NETWORK_TYPE";
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS: return "GET_NEIGHBORING_CELL_IDS";
        case RIL_REQUEST_SET_MUTE: return "SET_MUTE";
        case RIL_REQUEST_GET_MUTE: return "GET_MUTE";
        case RIL_REQUEST_QUERY_CLIP: return "QUERY_CLIP";
        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE: return "LAST_DATA_CALL_FAIL_CAUSE";
        case RIL_REQUEST_DATA_CALL_LIST: return "DATA_CALL_LIST";
        case RIL_REQUEST_RESET_RADIO: return "RESET_RADIO";
        case RIL_REQUEST_OEM_HOOK_RAW: return "OEM_HOOK_RAW";
        case RIL_REQUEST_OEM_HOOK_STRINGS: return "OEM_HOOK_STRINGS";
        case RIL_REQUEST_SET_BAND_MODE: return "SET_BAND_MODE";
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE: return "QUERY_AVAILABLE_BAND_MODE";
        case RIL_REQUEST_STK_GET_PROFILE: return "STK_GET_PROFILE";
        case RIL_REQUEST_STK_SET_PROFILE: return "STK_SET_PROFILE";
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND: return "STK_SEND_ENVELOPE_COMMAND";
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE: return "STK_SEND_TERMINAL_RESPONSE";
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM: return "STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM";
        case RIL_REQUEST_SCREEN_STATE: return "SCREEN_STATE";
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER: return "EXPLICIT_CALL_TRANSFER";
        case RIL_REQUEST_SET_LOCATION_UPDATES: return "SET_LOCATION_UPDATES";
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION:return"CDMA_SET_SUBSCRIPTION";
        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:return"CDMA_SET_ROAMING_PREFERENCE";
        case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:return"CDMA_QUERY_ROAMING_PREFERENCE";
        case RIL_REQUEST_SET_TTY_MODE:return"SET_TTY_MODE";
        case RIL_REQUEST_QUERY_TTY_MODE:return"QUERY_TTY_MODE";
        case RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE:return"CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE";
        case RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE:return"CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE";
        case RIL_REQUEST_CDMA_FLASH:return"CDMA_FLASH";
        case RIL_REQUEST_CDMA_BURST_DTMF:return"CDMA_BURST_DTMF";
        case RIL_REQUEST_CDMA_SEND_SMS:return"CDMA_SEND_SMS";
        case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:return"CDMA_SMS_ACKNOWLEDGE";
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:return"GSM_GET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:return"GSM_SET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:return "CDMA_GET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:return "CDMA_SET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:return "CDMA_SMS_BROADCAST_ACTIVATION";
        case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY: return"CDMA_VALIDATE_AND_WRITE_AKEY";
        case RIL_REQUEST_CDMA_SUBSCRIPTION: return"CDMA_SUBSCRIPTION";
        case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM: return "CDMA_WRITE_SMS_TO_RUIM";
        case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM: return "CDMA_DELETE_SMS_ON_RUIM";
        case RIL_REQUEST_DEVICE_IDENTITY: return "DEVICE_IDENTITY";
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE: return "EXIT_EMERGENCY_CALLBACK_MODE";
        case RIL_REQUEST_GET_SMSC_ADDRESS: return "GET_SMSC_ADDRESS";
        case RIL_REQUEST_SET_SMSC_ADDRESS: return "SET_SMSC_ADDRESS";
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS: return "REPORT_SMS_MEMORY_STATUS";
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING: return "REPORT_STK_SERVICE_IS_RUNNING";
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: return "UNSOL_RESPONSE_RADIO_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED: return "UNSOL_RESPONSE_CALL_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED: return "UNSOL_RESPONSE_NETWORK_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_NEW_SMS: return "UNSOL_RESPONSE_NEW_SMS";
        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT: return "UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT";
        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM: return "UNSOL_RESPONSE_NEW_SMS_ON_SIM";
        case RIL_UNSOL_ON_USSD: return "UNSOL_ON_USSD";
        case RIL_UNSOL_ON_USSD_REQUEST: return "UNSOL_ON_USSD_REQUEST(obsolete)";
        case RIL_UNSOL_NITZ_TIME_RECEIVED: return "UNSOL_NITZ_TIME_RECEIVED";
        case RIL_UNSOL_SIGNAL_STRENGTH: return "UNSOL_SIGNAL_STRENGTH";
        case RIL_UNSOL_STK_SESSION_END: return "UNSOL_STK_SESSION_END";
        case RIL_UNSOL_STK_PROACTIVE_COMMAND: return "UNSOL_STK_PROACTIVE_COMMAND";
        case RIL_UNSOL_STK_EVENT_NOTIFY: return "UNSOL_STK_EVENT_NOTIFY";
        case RIL_UNSOL_STK_CALL_SETUP: return "UNSOL_STK_CALL_SETUP";
        case RIL_UNSOL_SIM_SMS_STORAGE_FULL: return "UNSOL_SIM_SMS_STORAGE_FUL";
        case RIL_UNSOL_SIM_REFRESH: return "UNSOL_SIM_REFRESH";
        case RIL_UNSOL_DATA_CALL_LIST_CHANGED: return "UNSOL_DATA_CALL_LIST_CHANGED";
        case RIL_UNSOL_CALL_RING: return "UNSOL_CALL_RING";
        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED: return "UNSOL_RESPONSE_SIM_STATUS_CHANGED";
        case RIL_UNSOL_RESPONSE_CDMA_NEW_SMS: return "UNSOL_NEW_CDMA_SMS";
        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS: return "UNSOL_NEW_BROADCAST_SMS";
        case RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL: return "UNSOL_CDMA_RUIM_SMS_STORAGE_FULL";
        case RIL_UNSOL_RESTRICTED_STATE_CHANGED: return "UNSOL_RESTRICTED_STATE_CHANGED";
        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE: return "UNSOL_ENTER_EMERGENCY_CALLBACK_MODE";
        case RIL_UNSOL_CDMA_CALL_WAITING: return "UNSOL_CDMA_CALL_WAITING";
        case RIL_UNSOL_CDMA_OTA_PROVISION_STATUS: return "UNSOL_CDMA_OTA_PROVISION_STATUS";
        case RIL_UNSOL_CDMA_INFO_REC: return "UNSOL_CDMA_INFO_REC";
        case RIL_UNSOL_OEM_HOOK_RAW: return "UNSOL_OEM_HOOK_RAW";
        case RIL_UNSOL_RINGBACK_TONE: return "UNSOL_RINGBACK_TONE";
        case RIL_UNSOL_RESEND_INCALL_MUTE: return "UNSOL_RESEND_INCALL_MUTE";
        default: return "<unknown request>";
    }
}
