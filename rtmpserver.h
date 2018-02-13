#ifndef __RTMP_SERVER_H_
#define __RTMP_SERVER_H_
#include <map>
#include <string>
#include "rtmpopt.h"
#include "thread.h"
class publishsource;
using namespace std;
class rtmpserver
{
public:
    rtmpserver();
    ~rtmpserver();
public: 
    int startStreaming(int port, char* cert, char* key);
    void stopStreaming();
    void dropSource(string url);
private:   
    char        stream_name[1024];
    int         state;
    int         listenSocket; 
    void*       sslCtx;  
    pthread_t   serverThread_id;
    map<string, publishsource*> sources; 

protected:
    static void* serverThread(void* arg);
    void doserverThread();
    void doServe(int sockfd);
    int disposertmp(RTMP* rtmp, int type);
};
#endif
