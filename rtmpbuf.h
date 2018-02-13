#ifndef __RTMP__BUF_
#define __RTMP__BUF_
#include <queue>
#include "stdlib.h"
#include <pthread.h>
#define free_sharebuf(buf) \
	if(buf)\
	{\
		delete buf;\
		buf = NULL;\
	}\

//////SharePtr////////////////////////////////////////////////////////////////////////////////
template <class T>
class SharePtr
{
public:
	SharePtr<T>(T chunks1, int64_t size1, int64_t width1, int64_t height1, int64_t samples1, int64_t timestamp1):chunks(chunks1), size(size1), width(width1), height(height1), samples (samples1), timestamp(timestamp1), refcount(1)
	{
	}
	~SharePtr<T>()
	{
		if (chunks)
		{
			free(chunks);
			chunks = NULL;
		}
	}
public: 
	int64_t refcount;
	int64_t size;
	int64_t width;
	int64_t	height;
	int64_t samples;
	int64_t timestamp;
	T chunks;
};

//////ShareAVBuffer//////////////////////////////////////////////////////////////////
template <class T>
class ShareAVBuffer
{
public:
	ShareAVBuffer<T>(T chunks1, int64_t size1, int64_t width1, int64_t height1, int64_t samples1, int64_t timestamp1)
	{
		ptr = new SharePtr<char*>(chunks1, size1, width1, height1, samples1, timestamp1);
	};
	~ShareAVBuffer<T>()
	{
		if (ptr && __sync_sub_and_fetch (&ptr->refcount, 1) == 0)
		{
			free_sharebuf(ptr);
		}
		ptr = NULL;
	}
private:
	ShareAVBuffer<T>(){ptr = NULL;};
public:
	SharePtr<T>* ptr;
	//ShareAVBuffer<T>& operator=(const ShareAVBuffer<T>& obj);
	ShareAVBuffer<T>* copy();
};

// template <class T>
// ShareAVBuffer<T>& ShareAVBuffer<T>::operator=(const ShareAVBuffer<T>& obj)
// {
// 	if (ptr && __sync_sub_and_fetch (&ptr->refcount, 1) == 0)
// 	{
// 		free_sharebuf(ptr);
// 	}
// 	ptr = NULL;

// 	if (obj.ptr)
// 	{
// 		ptr = obj.ptr;
// 		__sync_add_and_fetch (&ptr->refcount, 1);
// 	}
// 	return *this;
// }

template <class T>
ShareAVBuffer<T>* ShareAVBuffer<T>::copy()
{
	ShareAVBuffer<T>* newcopy = NULL;
	if (ptr)
	{
		newcopy = new ShareAVBuffer<T>;
		if (newcopy)
		{
			newcopy->ptr = ptr;
			__sync_add_and_fetch (&ptr->refcount, 1);
		}
	}
	return newcopy;
}

///QUEUE//////////////////////////////////////////////////////////////////////////////////////
template <class T>
class ShareAVBufferQueue
{
public:
	ShareAVBufferQueue();
	~ShareAVBufferQueue();
public:
	void push(ShareAVBuffer<T>* sharepkt);
	ShareAVBuffer<T>* pop();
	ShareAVBuffer<T>* front();
	ShareAVBuffer<T>* back();
	int64_t GetBusySize();
	void clear();
private:
	std::queue<ShareAVBuffer<T>*> packetqueue;
    pthread_mutex_t mutex;
};

template <class T>
ShareAVBufferQueue<T>::ShareAVBufferQueue()
{
    pthread_mutex_init(&mutex, NULL);
}

template <class T>
ShareAVBufferQueue<T>::~ShareAVBufferQueue()
{
    pthread_mutex_destroy(&mutex);
}

template <class T>
void ShareAVBufferQueue<T>::clear()
{
    pthread_mutex_lock(&mutex);
	while(!packetqueue.empty())
	{
		ShareAVBuffer<T>* share = packetqueue.front();
		packetqueue.pop();
		if (share)
		{
			free_sharebuf(share);
		}
	}
    pthread_mutex_unlock(&mutex);
}

template <class T>
int64_t ShareAVBufferQueue<T>::GetBusySize()
{
    pthread_mutex_lock(&mutex);
	int64_t nfreesize = packetqueue.size();
    pthread_mutex_unlock(&mutex);
	return nfreesize;
}

template <class T>
void ShareAVBufferQueue<T>::push(ShareAVBuffer<T>* sharepkt)
{	
    pthread_mutex_lock(&mutex);
	packetqueue.push(sharepkt);
    pthread_mutex_unlock(&mutex);
}

template <class T>
ShareAVBuffer<T>* ShareAVBufferQueue<T>::pop()
{
    pthread_mutex_lock(&mutex);
	ShareAVBuffer<T>* share = NULL;
	if (!packetqueue.empty())
	{
		share = packetqueue.front();
		packetqueue.pop();
	}
    pthread_mutex_unlock(&mutex);
	return share;
}

template <class T>
ShareAVBuffer<T>* ShareAVBufferQueue<T>::front()
{
    pthread_mutex_lock(&mutex);
	ShareAVBuffer<T>* share = NULL;
	if (!packetqueue.empty())
	{
		share = packetqueue.front();
	}
    pthread_mutex_unlock(&mutex);
	return share;
}

template <class T>
ShareAVBuffer<T>* ShareAVBufferQueue<T>::back()
{
    pthread_mutex_lock(&mutex);
	ShareAVBuffer<T>* share = NULL;
	if (!packetqueue.empty())
	{
		share = packetqueue.back();
	}
    pthread_mutex_unlock(&mutex);
	return share;
}
#endif
