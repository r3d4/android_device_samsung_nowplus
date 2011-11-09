
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <unistd.h>

#define LOG_TAG "MessageQueue"
#include <utils/Log.h>

#include "MessageQueue.h"

MessageQueue::MessageQueue()
{
    int fds[2] = {-1,-1};

    pipe(fds);

    this->fd_read = fds[0];
    this->fd_write = fds[1];
}

MessageQueue::~MessageQueue()
{
    int err = 0;
    err = close(this->fd_read);
    if(err != 0){
        LOGE ("Error:fds failed to close 0\n");
    }

    err = close(this->fd_write);
    if(err != 0){
        LOGE ("Error:fds[1] failed to close 0\n");
    }
}

int MessageQueue::get(Message* msg)
{
    char* p = (char*) msg;
    size_t read_bytes = 0;

    while( read_bytes  < sizeof(msg) )
    {
        int err = read(this->fd_read, p, sizeof(*msg) - read_bytes);
        
        if( err < 0 ) {
            LOGE("read() error: %s", strerror(errno));
            return -1;
        }
        else
            read_bytes += err;
    }

    LOGD("MQ.get(%d,%p,%p,%p,%p)", msg->command, msg->arg1,msg->arg2,msg->arg3,msg->arg4);    

    return 0;
}

int MessageQueue::put(Message* msg)
{
    char* p = (char*) msg;
    size_t bytes = 0;

    LOGD("MQ.put(%d,%p,%p,%p,%p)", msg->command, msg->arg1,msg->arg2,msg->arg3,msg->arg4);

    while( bytes  < sizeof(msg) )
    {
        int err = write(this->fd_write, p, sizeof(*msg) - bytes);
    	 
        if( err < 0 ) {
            LOGE("write() error: %s", strerror(errno));
            return -1;
        }
        else
            bytes += err;
    }

    LOGD("MessageQueue::put EXIT");

    return 0;    
}


bool MessageQueue::isEmpty()
{
    struct pollfd pfd;

    pfd.fd = this->fd_read;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if( -1 == poll(&pfd,1,0) )
    {
        LOGE("poll() error: %s", strerror(errno));
    }

    return (pfd.revents & POLLIN) == 0;
}
