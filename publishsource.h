#ifndef __PUBLISH_SOURCE_H_
#define __PUBLISH_SOURCE_H_
#include "rtmpopt.h"
#include "thread.h"
#include "rtmpbuf.h"
#include <vector>
#include <string>
class playconsumer;
class rtmpserver;
using namespace std;
class publishsource
{
public:
    publishsource(RTMP* rtmp, rtmpserver* server, string url);
    ~publishsource();

public:
    void insertConsumer(playconsumer* consumer);
    void dropConsumer(playconsumer* consumer);
    void release();
private:
    int         nState;
    string      sUrl;
    RTMP*       pRtmp;
    rtmpserver* pServer;
    ShareAVBuffer<char*>* pFirstAVPacket;
    vector<playconsumer*> sConsumers;
    pthread_t   pReceiveThread_id;
protected:
    static void* receiveThread(void* arg);
    void doreceiveThread();
};
#endif
