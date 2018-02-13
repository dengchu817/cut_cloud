#include "rtmpserver.h"
#include "rtmpopt.h"
#include "publishsource.h"
#include "playconsumer.h"

rtmpserver::rtmpserver()
{
    state = 0;
    sslCtx = NULL;
    serverThread_id = NULL;
}

rtmpserver::~rtmpserver()
{

}

int rtmpserver::startStreaming(int port, char* cert, char* key)
{
    struct sockaddr_in addr;
    int sockfd, tmp;
    char DEFAULT_HTTP_STREAMING_DEVICE[] = "0.0.0.0";
    if (cert && key)
        sslCtx = RTMP_TLS_AllocServerContext(cert, key);
    InitSockets();
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, couldn't create socket", __FUNCTION__);
        return -1;
    }

    tmp = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &tmp, sizeof(tmp) );

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(DEFAULT_HTTP_STREAMING_DEVICE);    //htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) ==-1)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, TCP bind failed for port number: %d", __FUNCTION__, port);
        return -1;
    }

    if (listen(sockfd, 10) == -1)
    {
        RTMP_Log(RTMP_LOGERROR, "%s, listen failed", __FUNCTION__);
        closesocket(sockfd);
        return -1;
    }
    listenSocket = sockfd;
    serverThread_id = ThreadCreate(serverThread, this);
    return 0;
}

void rtmpserver::stopStreaming()
{
    state = 1;
    closesocket(listenSocket);
    void* retval;
    pthread_join(serverThread_id, &retval);
    for (map<string, publishsource*>::iterator itor = sources.begin(); itor != sources.end(); itor++)
    {
        delete itor->second;
    }
    sources.clear();
}

void* rtmpserver::serverThread(void* arg)
{
    rtmpserver* pObj = (rtmpserver*)arg;
    if (pObj)
        pObj->doserverThread();
}

void rtmpserver::doserverThread()//accept
{
    while (!state)
    {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(struct sockaddr_in);
        int sockfd = accept(listenSocket, (struct sockaddr *) &addr, &addrlen);

        if (sockfd > 0)
        {
#ifdef linux
            struct sockaddr_in dest;
            char destch[16];
            socklen_t destlen = sizeof(struct sockaddr_in);
            getsockopt(sockfd, SOL_IP, SO_ORIGINAL_DST, &dest, &destlen);
            strcpy(destch, inet_ntoa(dest.sin_addr));
            RTMP_Log(RTMP_LOGERROR, "%s: accepted connection from %s to %s\n", __FUNCTION__,
            inet_ntoa(addr.sin_addr), destch);
#else
            RTMP_Log(RTMP_LOGERROR, "%s: accepted connection from %s\n", __FUNCTION__,
            inet_ntoa(addr.sin_addr));
#endif
            /* Create a new thread and transfer the control to that */
            doServe(sockfd);
        }
        else
        {
            RTMP_Log(RTMP_LOGERROR, "%s: accept failed", __FUNCTION__);
        }
    }
}

void rtmpserver::doServe(int sockfd)
{
    RTMP *rtmp = RTMP_Alloc();        /* our session with the real client */
    RTMPPacket packet = { 0 };

    // timeout for http requests
    fd_set fds;
    struct timeval tv;

    memset(&tv, 0, sizeof(struct timeval));
    tv.tv_sec = 5;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    if (select(sockfd + 1, &fds, NULL, NULL, &tv) <= 0)
    {
        RTMP_Log(RTMP_LOGERROR, "Request timeout/select failed, ignoring request");
        return;
    }
    else
    {
        RTMP_Init(rtmp);
        rtmp->m_sb.sb_socket = sockfd;
        if (sslCtx && !RTMP_TLS_Accept(rtmp, sslCtx))
        {
            RTMP_Log(RTMP_LOGERROR, "TLS handshake failed");
            closertmp(rtmp);
        }
        if (!RTMP_Serve(rtmp))
        {
            RTMP_Log(RTMP_LOGERROR, "Handshake failed");
            closertmp(rtmp);
        }
    }

    STREAMING_SERVER server;
    server.arglen = 0;
    while (RTMP_IsConnected(rtmp) && RTMP_ReadPacket(rtmp, &packet))
    {
        if (!RTMPPacket_IsReady(&packet))
            continue;
        int type = ServePacket(&server, rtmp, &packet);
        RTMPPacket_Free(&packet);
        if (type == 1 || type == 2)
        {
            disposertmp(rtmp, type);
            break;
        }
    }
    return;
}

int rtmpserver::disposertmp(RTMP* r, int type)
{
    if (!r)
        return -1;
    memcpy(stream_name, r->Link.app.av_val, r->Link.app.av_len);
    stream_name[r->Link.app.av_len] = '/';
    memcpy(stream_name + r->Link.app.av_len + 1, r->Link.playpath.av_val, r->Link.playpath.av_len);
    stream_name[r->Link.app.av_len + r->Link.playpath.av_len+1] = '\0';
    if (type == 1)//play
    {
        map<string, publishsource*>::iterator key = sources.begin();
        if (key != sources.end())
        {
            playconsumer* newconsumer = new playconsumer(r, key->second, stream_name);
            key->second->insertConsumer(newconsumer);
            RTMP_Log(RTMP_LOGERROR, "Play stream %s", stream_name);
        }
        else
        {
            closertmp(r);
            RTMP_Log(RTMP_LOGERROR, "Don't have stream to play： %s", stream_name);
        } 
    }
    else if (type == 2)//publish
    {
        map<string, publishsource*>::iterator key = sources.find(stream_name);
        if (key == sources.end())
        {
            publishsource* newsource = new publishsource(r, this, stream_name);
            sources.insert(make_pair(stream_name, newsource));
            RTMP_Log(RTMP_LOGERROR, "publish stream： %s", stream_name);
        }
        else
        {
            closertmp(r);
            RTMP_Log(RTMP_LOGERROR, "Already have publishing stream： %s", stream_name);
        }
    }
    return -1;
}

void rtmpserver::dropSource(string url)
{
    map<string, publishsource*>::iterator key = sources.find(url);
    if (key != sources.end())
    {
       publishsource* source = key->second;
       source->release();
       delete source;
       sources.erase(key);
    }
}
