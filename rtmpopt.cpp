#include "rtmpopt.h"

static const AVal av_dquote = AVC("\"");
static const AVal av_escdquote = AVC("\\\"");
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");
static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_Started_publishing_stream = AVC("Started publishing stream");
static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");

static void spawn_dumper(int argc, AVal *av, char *cmd)
{
#ifdef WIN32
  STARTUPINFO si = {0};
  PROCESS_INFORMATION pi = {0};

  si.cb = sizeof(si);
  if (CreateProcess(NULL, (LPWSTR)cmd, NULL, NULL, FALSE, 0, NULL, NULL,
    &si, &pi))
    {
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
    }
#else
  /* reap any dead children */
  while (waitpid(-1, NULL, WNOHANG) > 0);

  if (fork() == 0) {
    char **argv = (char**)malloc((argc+1) * sizeof(char *));
    int i;

    for (i=0; i<argc; i++) {
      argv[i] = av[i].av_val;
      argv[i][av[i].av_len] = '\0';
    }
    argv[i] = NULL;
    if ((i = execvp(argv[0], argv)))
      _exit(i);
  }
#endif
}

static int countAMF(AMFObject *obj, int *argc)
{
  int i, len;

  for (i=0, len=0; i < obj->o_num; i++)
    {
      AMFObjectProperty *p = &obj->o_props[i];
      len += 4;
      (*argc)+= 2;
      if (p->p_name.av_val)
	len += 1;
      len += 2;
      if (p->p_name.av_val)
	len += p->p_name.av_len + 1;
      switch(p->p_type)
	{
	case AMF_BOOLEAN:
	  len += 1;
	  break;
	case AMF_STRING:
	  len += p->p_vu.p_aval.av_len;
	  break;
	case AMF_NUMBER:
	  len += 40;
	  break;
	case AMF_OBJECT:
	  len += 9;
	  len += countAMF(&p->p_vu.p_object, argc);
	  (*argc) += 2;
	  break;
	case AMF_NULL:
	default:
	  break;
	}
    }
  return len;
}

static char * dumpAMF(AMFObject *obj, char *ptr, AVal *argv, int *argc)
{
  int i, ac = *argc;
  const char opt[] = "NBSO Z";

  for (i=0; i < obj->o_num; i++)
    {
      AMFObjectProperty *p = &obj->o_props[i];
      argv[ac].av_val = ptr+1;
      argv[ac++].av_len = 2;
      ptr += sprintf(ptr, " -C ");
      argv[ac].av_val = ptr;
      if (p->p_name.av_val)
	*ptr++ = 'N';
      *ptr++ = opt[p->p_type];
      *ptr++ = ':';
      if (p->p_name.av_val)
	ptr += sprintf(ptr, "%.*s:", p->p_name.av_len, p->p_name.av_val);
      switch(p->p_type)
	{
	case AMF_BOOLEAN:
	  *ptr++ = p->p_vu.p_number != 0 ? '1' : '0';
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_STRING:
	  memcpy(ptr, p->p_vu.p_aval.av_val, p->p_vu.p_aval.av_len);
	  ptr += p->p_vu.p_aval.av_len;
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_NUMBER:
	  ptr += sprintf(ptr, "%f", p->p_vu.p_number);
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_OBJECT:
	  *ptr++ = '1';
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  ac++;
	  *argc = ac;
	  ptr = dumpAMF(&p->p_vu.p_object, ptr, argv, argc);
	  ac = *argc;
	  argv[ac].av_val = ptr+1;
	  argv[ac++].av_len = 2;
	  argv[ac].av_val = ptr+4;
	  argv[ac].av_len = 3;
	  ptr += sprintf(ptr, " -C O:0");
	  break;
	case AMF_NULL:
	default:
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	}
      ac++;
    }
  *argc = ac;
  return ptr;
}

static void AVreplace(AVal *src, const AVal *orig, const AVal *repl)
{
  char *srcbeg = src->av_val;
  char *srcend = src->av_val + src->av_len;
  char *dest, *sptr, *dptr;
  int n = 0;

  /* count occurrences of orig in src */
  sptr = src->av_val;
  while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
    {
      n++;
      sptr += orig->av_len;
    }
  if (!n)
    return;

  dest = (char*)malloc(src->av_len + 1 + (repl->av_len - orig->av_len) * n);

  sptr = src->av_val;
  dptr = dest;
  while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
    {
      n = sptr - srcbeg;
      memcpy(dptr, srcbeg, n);
      dptr += n;
      memcpy(dptr, repl->av_val, repl->av_len);
      dptr += repl->av_len;
      sptr += orig->av_len;
      srcbeg = sptr;
    }
  n = srcend - srcbeg;
  memcpy(dptr, srcbeg, n);
  dptr += n;
  *dptr = '\0';
  src->av_val = dest;
  src->av_len = dptr - dest;
}

void closertmp(RTMP* rtmp)
{
	if (rtmp == NULL)
		return;
	//closesocket(RTMP_Socket(rtmp));
	//RTMP_LogPrintf("Closing connection... ");
	RTMP_Close(rtmp);
	rtmp->Link.playpath.av_val = NULL;
	rtmp->Link.tcUrl.av_val = NULL;
	rtmp->Link.swfUrl.av_val = NULL;
	rtmp->Link.pageUrl.av_val = NULL;
	rtmp->Link.app.av_val = NULL;
	rtmp->Link.flashVer.av_val = NULL;
	if (rtmp->Link.usherToken.av_val)
	{
		free(rtmp->Link.usherToken.av_val);
		rtmp->Link.usherToken.av_val = NULL;
	}
	RTMP_Free(rtmp);
	//RTMP_LogPrintf("done!\n\n");
}

int SendConnectResult(RTMP *r, double txn)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);
  AMFObject obj;
  AMFObjectProperty p, op;
  AVal av;

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "FMS/3,5,1,525");
  enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "status");
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
  STR2AVAL(av, "NetConnection.Connect.Success");
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
  STR2AVAL(av, "Connection succeeded.");
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
#if 0
  STR2AVAL(av, "58656322c972d6cdf2d776167575045f8484ea888e31c086f7b5ffbd0baec55ce442c2fb");
  enc = AMF_EncodeNamedString(enc, pend, &av_secureToken, &av);
#endif
  STR2AVAL(p.p_name, "version");
  STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
  p.p_type = AMF_STRING;
  obj.o_num = 1;
  obj.o_props = &p;
  op.p_type = AMF_OBJECT;
  STR2AVAL(op.p_name, "data");
  op.p_vu.p_object = obj;
  enc = AMFProp_Encode(&op, enc, pend);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return RTMP_SendPacket(r, &packet, FALSE);
}

void HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet)
{
  if (packet->m_nBodySize >= 4)
    {
      r->m_inChunkSize = AMF_DecodeInt32(packet->m_body);
      RTMP_Log(RTMP_LOGDEBUG, "%s, received: chunk size change to %d", __FUNCTION__,
	  r->m_inChunkSize);
    }
}

void HandleCtrl(RTMP *r, const RTMPPacket *packet)
{
  short nType = -1;
  unsigned int tmp;
  if (packet->m_body && packet->m_nBodySize >= 2)
    nType = AMF_DecodeInt16(packet->m_body);
  RTMP_Log(RTMP_LOGDEBUG, "%s, received ctrl. type: %d, len: %d", __FUNCTION__, nType,
      packet->m_nBodySize);
  /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

  if (packet->m_nBodySize >= 6)
    {
      switch (nType)
	{
	case 0:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Begin %d", __FUNCTION__, tmp);
	  break;

	case 1:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream EOF %d", __FUNCTION__, tmp);
	  if (r->m_pausing == 1)
	    r->m_pausing = 2;
	  break;

	case 2:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Dry %d", __FUNCTION__, tmp);
	  break;

	case 4:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream IsRecorded %d", __FUNCTION__, tmp);
	  break;

	case 6:		/* server ping. reply with pong. */
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Ping %d", __FUNCTION__, tmp);
	  RTMP_SendCtrl(r, 0x07, tmp, 0);
	  break;

	/* FMS 3.5 servers send the following two controls to let the client
	 * know when the server has sent a complete buffer. I.e., when the
	 * server has sent an amount of data equal to m_nBufferMS in duration.
	 * The server meters its output so that data arrives at the client
	 * in realtime and no faster.
	 *
	 * The rtmpdump program tries to set m_nBufferMS as large as
	 * possible, to force the server to send data as fast as possible.
	 * In practice, the server appears to cap this at about 1 hour's
	 * worth of data. After the server has sent a complete buffer, and
	 * sends this BufferEmpty message, it will wait until the play
	 * duration of that buffer has passed before sending a new buffer.
	 * The BufferReady message will be sent when the new buffer starts.
	 * (There is no BufferReady message for the very first buffer;
	 * presumably the Stream Begin message is sufficient for that
	 * purpose.)
	 *
	 * If the network speed is much faster than the data bitrate, then
	 * there may be long delays between the end of one buffer and the
	 * start of the next.
	 *
	 * Since usually the network allows data to be sent at
	 * faster than realtime, and rtmpdump wants to download the data
	 * as fast as possible, we use this RTMP_LF_BUFX hack: when we
	 * get the BufferEmpty message, we send a Pause followed by an
	 * Unpause. This causes the server to send the next buffer immediately
	 * instead of waiting for the full duration to elapse. (That's
	 * also the purpose of the ToggleStream function, which rtmpdump
	 * calls if we get a read timeout.)
	 *
	 * Media player apps don't need this hack since they are just
	 * going to play the data in realtime anyway. It also doesn't work
	 * for live streams since they obviously can only be sent in
	 * realtime. And it's all moot if the network speed is actually
	 * slower than the media bitrate.
	 */
	case 31:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
	  if (!(r->Link.lFlags & RTMP_LF_BUFX))
	    break;
	  if (!r->m_pausing)
	    {
	      r->m_pauseStamp = r->m_mediaChannel < r->m_channelsAllocatedIn ?
	                        r->m_channelTimestamp[r->m_mediaChannel] : 0;
	      RTMP_SendPause(r, TRUE, r->m_pauseStamp);
	      r->m_pausing = 1;
	    }
	  else if (r->m_pausing == 2)
	    {
	      RTMP_SendPause(r, FALSE, r->m_pauseStamp);
	      r->m_pausing = 3;
	    }
	  break;

	case 32:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
	  break;

	default:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
	  break;
	}

    }

  if (nType == 0x1A)
    {
      RTMP_Log(RTMP_LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
      if (packet->m_nBodySize > 2 && packet->m_body[2] > 0x01)
	{
	  RTMP_Log(RTMP_LOGERROR,
            "%s: SWFVerification Type %d request not supported! Patches welcome...",
	    __FUNCTION__, packet->m_body[2]);
	}
#ifdef CRYPTO
      /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

      /* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
      else if (r->Link.SWFSize)
	{
	  RTMP_SendCtrl(r, 0x1B, 0, 0);
	}
      else
	{
	  RTMP_Log(RTMP_LOGERROR,
	      "%s: Ignoring SWFVerification request, use --swfVfy!",
	      __FUNCTION__);
	}
#else
      RTMP_Log(RTMP_LOGERROR,
	  "%s: Ignoring SWFVerification request, no CRYPTO support!",
	  __FUNCTION__);
#endif
    }
}

void HandleServerBW(RTMP *r, const RTMPPacket *packet)
{
  r->m_nServerBW = AMF_DecodeInt32(packet->m_body);
  RTMP_Log(RTMP_LOGDEBUG, "%s: server BW = %d", __FUNCTION__, r->m_nServerBW);
}

void HandleClientBW(RTMP *r, const RTMPPacket *packet)
{
  r->m_nClientBW = AMF_DecodeInt32(packet->m_body);
  if (packet->m_nBodySize > 4)
    r->m_nClientBW2 = packet->m_body[4];
  else
    r->m_nClientBW2 = -1;
  RTMP_Log(RTMP_LOGDEBUG, "%s: client BW = %d %d", __FUNCTION__, r->m_nClientBW,
      r->m_nClientBW2);
}

int SendonBWDoneResult(RTMP*r)
{
	RTMPPacket packet;
	char pbuf[512]={0}, *pend = pbuf+sizeof(pbuf);
	AVal av={0};
	char *enc;
	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;//1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 1;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;	
	enc = packet.m_body;
	STR2AVAL(av,"onBWDone");
	enc = AMF_EncodeString(enc, pend, &av);
	enc=AMF_EncodeNumber(enc,pend,0);
	*enc++=AMF_NULL;
	enc=AMF_EncodeNumber(enc,pend,0);
	packet.m_nBodySize =enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}

int SendPlayStart(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[512], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = 0x05;
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}

int SendPublishStart(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[512], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = 0x05;
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Publish_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_publishing_stream);
 /* *enc++ = 0x2e;
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_ASAICiss);*/
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}

int SendPlayStop(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[512], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Stop);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Stopped_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}

int SendResultNumber(RTMP *r, double txn, double ID)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeNumber(enc, pend, ID);

  packet.m_nBodySize = enc - packet.m_body;

  return RTMP_SendPacket(r, &packet, FALSE);
}

int ServeInvoke(STREAMING_SERVER *server, RTMP * r, RTMPPacket *packet, unsigned int offset)
{
  const char *body;
  unsigned int nBodySize;
  int ret = 0, nRes;

  body = packet->m_body + offset;
  nBodySize = packet->m_nBodySize - offset;

  if (body[0] != 0x02)		// make sure it is a string method name we start with
    {
      RTMP_Log(RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
	  __FUNCTION__);
      return 0;
    }

  AMFObject obj;
  nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
  if (nRes < 0)
    {
      RTMP_Log(RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
      return 0;
    }

  AMF_Dump(&obj);
  AVal method;
  AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
  double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
  RTMP_Log(RTMP_LOGDEBUG, "%s, client invoking <%s>", __FUNCTION__, method.av_val);

  if (AVMATCH(&method, &av_connect))
    {
      AMFObject cobj;
      AVal pname, pval;
      int i;

      server->connect = packet->m_body;
      packet->m_body = NULL;

      AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &cobj);
      for (i=0; i<cobj.o_num; i++)
	{
	  pname = cobj.o_props[i].p_name;
	  pval.av_val = NULL;
	  pval.av_len = 0;
	  if (cobj.o_props[i].p_type == AMF_STRING)
	    pval = cobj.o_props[i].p_vu.p_aval;
	  if (AVMATCH(&pname, &av_app))
	    {
	      r->Link.app = pval;
	      pval.av_val = NULL;
	      if (!r->Link.app.av_val)
	        r->Link.app.av_val = "";
	      server->arglen += 6 + pval.av_len;
	      server->argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_flashVer))
	    {
	      r->Link.flashVer = pval;
	      pval.av_val = NULL;
	      server->arglen += 6 + pval.av_len;
	      server->argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_swfUrl))
	    {
	      r->Link.swfUrl = pval;
	      pval.av_val = NULL;
	      server->arglen += 6 + pval.av_len;
	      server->argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_tcUrl))
	    {
	      r->Link.tcUrl = pval;
	      pval.av_val = NULL;
	      server->arglen += 6 + pval.av_len;
	      server->argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_pageUrl))
	    {
	      r->Link.pageUrl = pval;
	      pval.av_val = NULL;
	      server->arglen += 6 + pval.av_len;
	      server->argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_audioCodecs))
	    {
	      r->m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
	    }
	  else if (AVMATCH(&pname, &av_videoCodecs))
	    {
	      r->m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
	    }
	  else if (AVMATCH(&pname, &av_objectEncoding))
	    {
	      r->m_fEncoding = cobj.o_props[i].p_vu.p_number;
	    }
	}
      /* Still have more parameters? Copy them */
      if (obj.o_num > 3)
	{
	  int i = obj.o_num - 3;
	  r->Link.extras.o_num = i;
	  r->Link.extras.o_props = (struct AMFObjectProperty *)malloc(i*sizeof(AMFObjectProperty));
	  memcpy(r->Link.extras.o_props, obj.o_props+3, i*sizeof(AMFObjectProperty));
	  obj.o_num = 3;
	  server->arglen += countAMF(&r->Link.extras, &server->argc);
	}
	  RTMP_SendServerBW(r);
	  RTMP_SendClientBW(r);
      SendConnectResult(r, txn);
	  SendonBWDoneResult(r);
    }
  else if (AVMATCH(&method, &av_createStream))
    {
      SendResultNumber(r, txn, ++server->streamID);
    }
  else if (AVMATCH(&method, &av_getStreamLength))
    {
      SendResultNumber(r, txn, 10.0);
    }
  else if (AVMATCH(&method, &av_NetStream_Authenticate_UsherToken))
    {
      AVal usherToken;
      AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &usherToken);
      AVreplace(&usherToken, &av_dquote, &av_escdquote);
      server->arglen += 6 + usherToken.av_len;
      server->argc += 2;
      r->Link.usherToken = usherToken;
    }
  else if (AVMATCH(&method, &av_play))
    {
	  ret = 1;
	  RTMP_SendCtrl(r, 0, 1, 0);
	  SendPlayStart(r);
    }
  else if (AVMATCH(&method, &av_releaseStream))
	{
	}
  else if (AVMATCH(&method, &av_FCPublish))
	{
	  AVal playpath;;
      AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &playpath);
	  if (playpath.av_len)
	  {
		  r->Link.playpath.av_val = (char*)malloc(playpath.av_len);
		  memcpy(r->Link.playpath.av_val, playpath.av_val, playpath.av_len);
		  r->Link.playpath.av_len = playpath.av_len;
	  }
	  ret = 2;
	  RTMP_SendCtrl(r, 0, 1, 0);
	  SendPublishStart(r);
	}
  else if (AVMATCH(&method, &av_publish))
	{ 
		
	}
  AMF_Reset(&obj);
  return ret;
}

int ServePacket(STREAMING_SERVER *server, RTMP *r, RTMPPacket *packet)
{
  int ret = 0;

  RTMP_Log(RTMP_LOGDEBUG, "%s, received packet type %02X, size %u bytes", __FUNCTION__,
    packet->m_packetType, packet->m_nBodySize);

  switch (packet->m_packetType)
    {
    case RTMP_PACKET_TYPE_CHUNK_SIZE:
      HandleChangeChunkSize(r, packet);
      break;

    case RTMP_PACKET_TYPE_BYTES_READ_REPORT:
      break;

    case RTMP_PACKET_TYPE_CONTROL:
      HandleCtrl(r, packet);
      break;

    case RTMP_PACKET_TYPE_SERVER_BW:
      HandleServerBW(r, packet);
      break;

    case RTMP_PACKET_TYPE_CLIENT_BW:
      HandleClientBW(r, packet);
      break;

    case RTMP_PACKET_TYPE_AUDIO:
	   RTMP_LogPrintf( "received: audio %d bytes\n", packet->m_nBodySize);
      break;

    case RTMP_PACKET_TYPE_VIDEO:
		RTMP_LogPrintf( "received: video %lu bytes\n", packet->m_nBodySize);
      break;

    case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
      break;

    case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
      break;

    case RTMP_PACKET_TYPE_FLEX_MESSAGE:
      {
		RTMP_Log(RTMP_LOGDEBUG, "%s, flex message, size %u bytes, not fully supported",
			__FUNCTION__, packet->m_nBodySize);
		if (ServeInvoke(server, r, packet, 1))
		  RTMP_Close(r);
		break;
      }
    case RTMP_PACKET_TYPE_INFO:
      break;

    case RTMP_PACKET_TYPE_SHARED_OBJECT:
      break;

    case RTMP_PACKET_TYPE_INVOKE:
      RTMP_Log(RTMP_LOGDEBUG, "%s, received: invoke %u bytes", __FUNCTION__,
	  packet->m_nBodySize);
      //RTMP_LogHex(packet.m_body, packet.m_nBodySize);

      ret = ServeInvoke(server, r, packet, 0);
      break;

    case RTMP_PACKET_TYPE_FLASH_VIDEO:
	break;
    default:
      RTMP_Log(RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
	  packet->m_packetType);
    }
  return ret;
}