#include "playconsumer.h"
#include "rtmpopt.h"
#include "rtmpserver.h"
#include "publishsource.h"
#include "rtmpbuf.h"
#include <arpa/inet.h>

playconsumer::~playconsumer()
{

}

void playconsumer::release()
{
    if (nState != 2)
    {
        nState = 1;
        while(nState != 2)
        {
            usleep(10);
        }
    }
}

playconsumer::playconsumer(RTMP* rtmp, publishsource* source, string url)
{
    nState = 0;
    this->pRtmp = rtmp;
    this->pSource = source;
    this->sUrl = url;
    sendThread_id = ThreadCreate(sendThread, this);
}

bool playconsumer::putSharepacket(ShareAVBuffer<char*>* pkt)
{
    bool ret = false;
    if (pkt)
    {   
        if (dataQueue.GetBusySize() <= 10)
        {
            ShareAVBuffer<char*>* newcopy = pkt->copy();
            dataQueue.push(newcopy);
        }
    }
    return ret;
}

void* playconsumer::sendThread(void* arg)
{
    playconsumer *pObj = (playconsumer*)arg;
    if (pObj)
        pObj->dosendThread();
}

void playconsumer::dosendThread()
{
    RTMPPacket packet;
    RTMPPacket_Alloc(&packet,1024*100);
    packet.m_hasAbsTimestamp    = 0;    
    packet.m_nChannel           = 0x04; 
    packet.m_nInfoField2        = pRtmp->m_stream_id;
    while (!nState)
    {      
        if (pRtmp && RTMP_IsConnected(pRtmp))
        {   
            ShareAVBuffer<char*>* PtrAVPacket = dataQueue.pop();

            if (PtrAVPacket)
            {
                char* ptr = PtrAVPacket->ptr->chunks;
                int shouldtosend = PtrAVPacket->ptr->size;
                while (shouldtosend > 0)
                {
                    if (ptr[0] == 'F' && ptr[1] == 'L' && ptr[2] == 'V')
                    {
                        ptr += 13;
                    }
                    
                    uint32_t type=0;            
                    uint32_t datalength=0;      
                    uint32_t timestamp=0;       
                    uint32_t streamid=0;
                    uint32_t preTagsize = 0;

                    //packettype
                    type = ptr[0];
                    ptr = ptr+1;
                    shouldtosend = shouldtosend-1;
                    if (type != 8 && type != 9 && type != 18)
                    {
                        break;
                    }
                    //length
                    datalength = AMF_DecodeInt24(ptr);
                    ptr = ptr+3;
                    shouldtosend = shouldtosend-3;
                    if (datalength > shouldtosend - 4 - 3 - 4)
                    {
                        break;
                    }

                    //timesample
                    timestamp = AMF_DecodeInt32(ptr);
                    ptr = ptr+4;
                    shouldtosend = shouldtosend-4;
        
                    //streamId
                    streamid = AMF_DecodeInt24(ptr);
                    ptr = ptr+3;
                    shouldtosend = shouldtosend-3;

                    memcpy(packet.m_body, ptr, datalength);
                    ptr = ptr+datalength;
                    shouldtosend = shouldtosend-datalength;

                    //preTagsize
                    preTagsize = AMF_DecodeInt32(ptr);
                    ptr = ptr+4;
                    shouldtosend = shouldtosend-4;
                    if (datalength + 11 != preTagsize)
                    {
                        break;
                    }

                    packet.m_headerType = RTMP_PACKET_SIZE_LARGE; 
                    packet.m_nTimeStamp = timestamp; 
                    packet.m_packetType = type;
                    packet.m_nBodySize  = datalength;
                    RTMP_SendPacket(pRtmp, &packet, 0);
                }
                free_sharebuf(PtrAVPacket);
            }
            else
            {
                usleep(50);
            }
        }
        else
        {
           break;
        }
    }
    closertmp(pRtmp);
    pRtmp = NULL;
    nState = 2;
    if (pSource)
        pSource->dropConsumer(this);
}
