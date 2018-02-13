//#include <QCoreApplication>
#include "rtmpserver.h"
int main(int argc, char *argv[])
{
    rtmpserver* server = new rtmpserver();
    server->startStreaming(1935, NULL, NULL);
    char s;
    while (1)
    {
        s = getchar();
        //printf("%s\n", s);
        if (s == 'q')
            break;
        usleep(1000);
    }
    return 0;
}
