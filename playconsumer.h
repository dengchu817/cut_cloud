#ifndef __PLAY_CONSUMER_H_
#define __PLAY_CONSUMER_H_
#include "rtmpopt.h"
#include "thread.h"
#include "rtmpbuf.h"
#include <string>
class publishsource;
using namespace std;

class playconsumer
{
public:
    playconsumer(RTMP* rtmp, publishsource* source, string url);
    ~playconsumer();
public:
    bool putSharepacket(ShareAVBuffer<char*>* pkt);
    void release();
private:
    int nState;
    RTMP* pRtmp;
    publishsource* pSource;
    string sUrl;
    ShareAVBufferQueue<char*> dataQueue;
    pthread_t   sendThread_id;
protected:
    static void* sendThread(void* arg);
    void dosendThread();
};
#endif
