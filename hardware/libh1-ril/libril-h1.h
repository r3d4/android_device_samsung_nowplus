/* /device/samsung/nowplus/hardware/libsecril/libril-h1.h
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
#ifndef _LIBRIL_H1_
#define _LIBRIL_H1_

#define NO_TRIGGER  -1

#define LIBSEC_RIL_PATH      "/system/lib/libsec-ril.so"
#define LIBSEC_RILC_IP       "127.0.0.1"
#define LIBSEC_RILC_PORT     7203


// prototypes
void RIL_onRequestComplete(RIL_Token t, RIL_Errno e, void *response, size_t responselen);
void RIL_onUnsolicitedResponse(int unsolResponse, const void *data, size_t datalen);
void RIL_requestTimedCallback (RIL_TimedCallback callback, void *param, const struct timeval *relativeTime);


void onRequest (int request, void *data, size_t datalen, RIL_Token t);
RIL_RadioState currentState();
int onSupports (int requestCode);
void onCancel (RIL_Token t);
const char *getVersion();

const char *requestToString(int request);
static void fix3G();

//functions for ipc via libsecril-client
int ipc_connect();
int ipc_disconnect(int hSocket);
int ipc_send(int hSocket, unsigned char* data, int length);
int ipc_send_data( unsigned char* data, int length, int dir);

struct RIL_Env {
    void (*OnRequestComplete)(RIL_Token t, RIL_Errno e,
                           void *response, size_t responselen);
    void (*OnUnsolicitedResponse)(int unsolResponse, const void *data,
                                    size_t datalen);
    void (*RequestTimedCallback) (RIL_TimedCallback callback,
                                   void *param, const struct timeval *relativeTime);
};

typedef struct RequestInfo {
    int32_t token;      //this is not RIL_Token
    int *requestNumber;
} RequestInfo;

struct myTrigger {
    int token;
    int (*action)();
    char *message;
};

enum {
    MODEM_TO_PDA,
    PDA_TO_MODEM
};






#endif