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

//#define LOG_NDEBUG 0
#define LOG_TAG "H1RIL"

#include <utils/Log.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <linux/capability.h>
#include <linux/prctl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "libril-h1.h"

//#define DUMP

#define SOCKET_ERROR        -1

//connect to libsecril-client via socket
int ipc_connect()
{
    int hSocket;                 /* handle to socket */
    struct sockaddr_in Address;  /* Internet socket address stuct */
    char strHostIp[] = LIBSEC_RILC_IP;
    int nHostPort    = LIBSEC_RILC_PORT;

    hSocket=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(hSocket == SOCKET_ERROR)
    {
        LOGE("Could not make a socket");
        return 0;
    }

    /* fill address struct */
    Address.sin_addr.s_addr=inet_addr(strHostIp);
    Address.sin_port=htons(nHostPort);
    Address.sin_family=AF_INET;

    /* connect to host */
    if(connect(hSocket,(struct sockaddr*)&Address,sizeof(Address)) == SOCKET_ERROR)
    {
        LOGE("Could not connect to host");
        return 0;
    }

    LOGV("ipc_connect() connect to %s on port %d",strHostIp,nHostPort);
    return hSocket;
}

int ipc_disconnect(int hSocket)
{
    if(close(hSocket) == SOCKET_ERROR)
    {
        LOGE("Could not close socket");
        return 0;
    }
    LOGV("ipc_disconnect() close socket");
    return 1;

}
int ipc_send(int hSocket, unsigned char* data, int length)
{
    char *pbuf;
    int i;
#ifdef DUMP
    pbuf = (char*)malloc(length*3+1);
    if(pbuf)
    {
        for(i=0; i<length; i++)
        {
            sprintf(&pbuf[3*i],"%02x ", data[i]);
        }
        pbuf[3*i]='\0';
        LOGV("ipc_send():%d bytes: { %s }", length, pbuf);
        free(pbuf);
    }
#else
    LOGV("ipc_send(): %d bytes", length);
#endif

    /* write what we received back to the server */
    int sz=write(hSocket, data, length);
    if( sz!= length)
    {
        LOGE("Could not wite all data %d!=%d", sz, length);
    }

    return sz;
}


int ipc_send_data( unsigned char* data, int length, int dir)
{
    unsigned char hdr1[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned char hdr2[] = { 0x01, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int written = 0;
//ipctool 09 00 35 00 05 01 03 82 00
    if(dir == PDA_TO_MODEM)
    {
        hdr1[0] = 0x00;
        hdr2[0] = 0x00;
    }
    hdr2[2] = length;

    int hSocket = ipc_connect();
    if(hSocket)
    {
        data[0] = length;

        ipc_send(hSocket, hdr1, sizeof(hdr1));
        ipc_send(hSocket, hdr2, sizeof(hdr2));
        written = ipc_send(hSocket, data, length);

        ipc_disconnect(hSocket);
    }

    return length - written;
}
