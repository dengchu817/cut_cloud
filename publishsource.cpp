#include "publishsource.h"
#include "rtmpopt.h"
#include "rtmpserver.h"
#include "rtmpbuf.h"
#include "playconsumer.h"

publishsource::~publishsource()
{

}

void publishsource::release()
{
    if (nState != 2)
    {
         nState = 1;
         while(nState != 2)
         {
             usleep(10);
         }
    }
    for (vector<playconsumer*>::iterator itor = sConsumers.begin(); itor != sConsumers.end(); itor++)
    {
        playconsumer* consumer = *itor;
        consumer->release();
        delete consumer;
    }
    sConsumers.clear();
}

publishsource::publishsource(RTMP* rtmp, rtmpserver* server, string url)
{
    nState = 0;
    pFirstAVPacket = NULL;
    this->pRtmp = rtmp;
    this->pServer = server;
    this->sUrl = url;
    pReceiveThread_id = ThreadCreate(receiveThread, this);
}

void* publishsource::receiveThread(void* arg)
{
    publishsource *pObj = (publishsource*)arg;
    if (pObj)
        pObj->doreceiveThread();
}

void publishsource::insertConsumer(playconsumer* consumer)
{
    if (pFirstAVPacket)
    {
        consumer->putSharepacket(pFirstAVPacket);
        sConsumers.push_back(consumer);
    }
}

void publishsource::dropConsumer(playconsumer* consumer)
{
    if (nState == 2)
        return;
    for (vector<playconsumer*>::iterator itor = sConsumers.begin(); itor!= sConsumers.end(); )
    {
        if (consumer == *itor)
        {
            consumer->release();
            delete consumer;
            itor = sConsumers.erase(itor);
            break;
        }
        else
        {
            itor++;
        }
    }
}

void publishsource::doreceiveThread()
{
    int buf_size = 1024*1024;
    char buf[1024*1024];
    RTMP_Log(RTMP_LOGERROR, "doreceiveThread");
    while (!nState)
    {
        int nRead  = 0;
        if (RTMP_IsConnected(pRtmp) && !RTMP_IsTimedout(pRtmp))
        {   
            nRead = RTMP_Read(pRtmp, (char*)buf,buf_size);
            if (nRead)
            {
                char* source = (char*)malloc(nRead);
                memcpy(source, buf, nRead);

                ShareAVBuffer<char*> sharebuf(source, nRead, 0, 0, 0, 0);;
                if (pFirstAVPacket == NULL)
                {
                    pFirstAVPacket = sharebuf.copy();
                }
                for (vector<playconsumer*>::iterator itor = sConsumers.begin(); itor != sConsumers.end(); itor++)
                {
                    playconsumer* consumer = *itor;
                    if (consumer)
                    {
                        consumer->putSharepacket(&sharebuf);
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
    closertmp(pRtmp);
    pRtmp = NULL;
    nState = 2;//have be exited
    pServer->dropSource(sUrl);
}
