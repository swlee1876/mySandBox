#include <stdio.h>
#include <logging_api.h>
#include <udt.h>
#include <srt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#define MAX_BUF_LENG 10240
#define READ_TIMEOUT 100
#define READ_SIZE 2048
#define BUFFER_SIZE (1024 * 1024)

char* get_datestr(char* strDate);
char* get_timestr(char* strTime);
void logPrn(const char *format, ... );
int readData(char* filename, char* buffer);
SRTSOCKET connectSRT(char *src_addr, int src_port, char *dest_addr, int dest_port, bool isRendezvous, bool isServer);
int createEPoll(SRTSOCKET sock);
int emulAction(char *src_addr, int src_port, char *dest_addr, int dest_port, char *buffer, int nLength, bool isRendezvous, bool isServer);
int getCurrentTime(void);

int main(int argc , char *argv[]) 
{
    char *src_addr = NULL, *dest_addr, *filename, *pbuffer, buffer[BUFFER_SIZE];
    int src_port, dest_port, nRet = 0, pos = 0, snd_size = 1316;
    SRTSOCKET sock, rfd;
    int rfdlen = 1, cnt = 0, totalSize = 0, argcPos = 1;
    bool isRendezvous = true, isServer = true;

    srt_startup();
    /// set srt log level
    UDT::setloglevel(srt_logging::LogLevel::debug);
    UDT::addlogfa(10);

    if (argc != 7 && argc != 8) {
        fprintf(stderr, "%s [src_ip] src_port dest_ip dest_port recv_file mode role\n" \
               "mode : rendezvous mode (1), normal mode (0)\n" \
               "role : server (1), client (0)\n" , argv[0]);
        return -1;
    }

    /// set argument
    if (argc == 8) {
        argcPos = 2;
        src_addr = argv[1];
    }
    else {
        argcPos = 1;
    }
    src_port = atoi(argv[argcPos]);
    dest_addr = argv[argcPos + 1];
    dest_port = atoi(argv[argcPos + 2]);
    filename = argv[argcPos + 3];
    isRendezvous = (atoi(argv[argcPos + 4]) == 1)?true:false;
    isServer = (atoi(argv[argcPos + 5]) == 1)?true:false;

    if (!isServer) {
        nRet = readData(filename, buffer);
        if (nRet == -1) {
            return -1;
        }
    }

    cnt = 0;
    totalSize = nRet;
    while(cnt < 10) {
        nRet = emulAction(src_addr, src_port, dest_addr, dest_port, buffer, totalSize, isRendezvous, isServer);
        if (nRet == -1) 
            break;
        cnt++;
        sleep(2);
        srt_cleanup(); 
    }

    srt_cleanup(); 
	return 0;
}

char* get_datestr(char* strDate)
{
    time_t*         secs;
    struct timeval  tval;
    struct tm       tmval;

    gettimeofday(&tval, NULL);
    secs = &tval.tv_sec;
    localtime_r(secs, &tmval);

    memset(strDate, 0x00, 9);
    sprintf(strDate, "%04d%02d%02d",
                  tmval.tm_year+1900, tmval.tm_mon+1, tmval.tm_mday);
    return strDate;
}

void logPrn(const char *format, ... )
{
	va_list argList;
	char szBuffer[MAX_BUF_LENG];

	va_start(argList,format);
    vsnprintf(szBuffer, MAX_BUF_LENG - 1, format, argList);
    va_end(argList);   

	char szTimeBuffer[512];
	char szTmpBufferDate[128];
	char szTmpBufferTime[128];
	snprintf(szTimeBuffer, 512, "[%s:%s]",
					get_datestr(szTmpBufferDate),
					get_timestr(szTmpBufferTime));	 

	fprintf(stdout, "%s %s\n" , szTimeBuffer, szBuffer);
}


char* get_timestr(char* strTime)
{
    time_t*         secs;
    struct timeval  tval;
    struct tm       tmval;

    gettimeofday(&tval, NULL);
    secs = &tval.tv_sec;
    localtime_r(secs, &tmval);

    memset(strTime, 0x00, 11);
    sprintf(strTime, "%02d%02d%02d.%03d",
                     tmval.tm_hour, tmval.tm_min, tmval.tm_sec, (int)tval.tv_usec/1000);
    return strTime;
}

SRTSOCKET connectSRT(char *src_addr, int src_port, char *dest_addr, int dest_port, bool isRendezvous, bool isServer)
{
    int file_mode = SRTT_FILE;
    int yes = 1;
    SRTSOCKET sock, clisock;
    int bufferSize = 1024 * 1024;

    fprintf(stderr, "connecting [%s:%s] mode\n" ,
            isRendezvous?"rendezvous":"normal", isServer?"server":"client");

    /// open and set option for srt socket
    sock = srt_socket(AF_INET, SOCK_DGRAM, 0);

    srt_setsockflag(sock, SRTO_TRANSTYPE, &file_mode, sizeof(file_mode));
    //srt_setsockflag(sock, SRTO_MESSAGEAPI, &yes, sizeof(yes));

    if (isRendezvous) 
        srt_setsockopt(sock, 0, SRTO_RENDEZVOUS, &yes, sizeof(yes));

    struct linger zero;
    memset((char *)&zero, 0x00, sizeof(zero));
    srt_setsockopt(sock, 0, SRTO_LINGER, &zero, sizeof(zero));

    srt_setsockopt(sock, 0, SRTO_UDP_SNDBUF, &bufferSize, sizeof(bufferSize));

    /// set source ip and port
	struct sockaddr_in	this_sin;

	this_sin.sin_family			= AF_INET;
	this_sin.sin_port			= htons(src_port);
    if (src_addr != NULL)
	    this_sin.sin_addr.s_addr	= inet_addr(src_addr);
    else
    	this_sin.sin_addr.s_addr	= htonl(INADDR_ANY);

    /// bind srt socket
    int nRet = srt_bind(sock, (struct sockaddr *)&this_sin , sizeof(this_sin));

	if(nRet == SRT_ERROR ) {
        int srt_errno = srt_getlasterror(NULL);
        fprintf(stderr, "Failed : srt_bind [%d][%s]", srt_errno, srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup(); 
        return SRT_ERROR;
    }

    if (!isRendezvous && isServer) {
        nRet = srt_listen(sock , 5);
        if (nRet == SRT_ERROR) {
            int srt_errno = srt_getlasterror(NULL);
            fprintf(stderr, "Failed : srt_listen [%d][%s]", srt_errno, srt_getlasterror_str());
            srt_close(sock);
            srt_cleanup(); 
            return SRT_ERROR;
        }
        memset(&this_sin , 0x00, sizeof(this_sin));
        int nLength;
        clisock = srt_accept(sock , (struct sockaddr *)&this_sin, &nLength); 
        if (clisock == SRT_INVALID_SOCK) {
            int srt_errno = srt_getlasterror(NULL);
            fprintf(stderr, "Failed : srt_accept [%d][%s]", srt_errno, srt_getlasterror_str());
            srt_close(sock);
            srt_cleanup(); 
            return SRT_ERROR;
        }

        srt_close(sock);

        sock = clisock;
    }
    else {
        /// set destination ip and port
        this_sin.sin_family         = AF_INET;
        this_sin.sin_port           = htons(dest_port);
        this_sin.sin_addr.s_addr    = inet_addr(dest_addr);

        int dwTimeOut = 10000;

        /// connect
        if(srt_setsockopt(sock, 0, SRTO_CONNTIMEO, &dwTimeOut, sizeof(dwTimeOut)) == SRT_ERROR)
        {
            int srt_errno = srt_getlasterror(NULL);
            fprintf(stderr, "Failed : srt_setsockopt, timeout (SRT) [%d][%s]", srt_errno, srt_getlasterror_str());
            srt_close(sock);
            srt_cleanup(); 
            return  SRT_ERROR;
        }

        logPrn("try connection");

        nRet = srt_connect(sock, (struct sockaddr*)&this_sin, sizeof(this_sin));
        if(nRet == SRT_ERROR) {
            int srt_errno = srt_getlasterror(NULL);
            fprintf(stderr, "Failed : srt_connect (SRT) [%d][%s]", srt_errno, srt_getlasterror_str());
            srt_close(sock);
            srt_cleanup(); 
            return	SRT_ERROR;
        }
    }

    /*
    int nLength = 10;
    srt_sendmsg(sock, (char *)&nLength , sizeof(nLength) , -1, 1);
    nRet = srt_recvmsg(sock, (char *)&nLength, sizeof(nLength));
    if (nRet == SRT_ERROR) {
        int srt_errno = srt_getlasterror(NULL);
        fprintf(stderr, "Failed : srt_connect (SRT) [%d][%s]", srt_errno, srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup(); 
    }
    logPrn("fisrt negotiation end");
    */

    return sock;

}

int readData(char* filename, char* buffer)
{
    int nRet = 0;
    int fd = open(filename , O_RDONLY , 0);

    if (fd < 0) {
        fprintf(stderr, "can't open %s\n", filename);
        return -1;
    }

    nRet = read(fd, buffer, BUFFER_SIZE);

    if (nRet <= 0) {
        fprintf(stderr, "can't read file(%s)\n" , filename);
        close(fd);
        return -1;
    }

    close(fd);

    return nRet;
}

int createEPoll(SRTSOCKET sock)
{
    /// create srt_epoll
    int pollid;

    pollid = srt_epoll_create();

    if (pollid < 0) {
        fprintf(stderr, "can't create srt epoll : [%d][%s]\n",
                srt_getlasterror(NULL), srt_getlasterror_str());
        return SRT_ERROR;
    }

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;

    if (srt_epoll_add_usock(pollid, sock, &events)) {
        fprintf(stderr, "failed to add srt destination to poll : [%d][%s]\n", 
            srt_getlasterror(NULL), srt_getlasterror_str());
        return SRT_ERROR;
    }

    return pollid;
}

int emulAction(char *src_addr, int src_port, char *dest_addr, int dest_port, 
        char *buffer, int nLength, bool isRendezvous, bool isServer) 
{
    char *pbuffer;
    int nRet = 0, pos = 0, snd_size = 1316;
    SRTSOCKET sock, rfd;
    int rfdlen = 1, cnt = 0, totalSize = 0, argcPos = 1;

    srt_clearlasterror();

    sock = connectSRT(src_addr, src_port, dest_addr, dest_port, isRendezvous, isServer);
    if (sock == SRT_ERROR) {
        return -1;
    }
    logPrn("Sueccess : connect");
    fprintf(stderr, "Connect Status:Error (%d:%d)\n" , srt_getsockstate(sock), srt_getlasterror(NULL));

    int pollid = createEPoll(sock);
    if (pollid == SRT_ERROR) {
        srt_close(sock);
        srt_cleanup();
        return -1;
    }

    if (!isServer) {
        /// send the size
        srt_sendmsg(sock, (char *)&nLength , sizeof(nLength) , -1, 1);
        fprintf(stderr, "Send size : %d\n", nLength);

        /// send the data
#if 0
        while(nRet > 0) {
            if (nRet < 1316)
                snd_size = nRet;
            srt_sendmsg(sock , buffer + pos , snd_size , -1 , 1);
            logPrn("send message (%d)" , snd_size);
            nRet -= snd_size;
            pos += snd_size;
        }
#else
        srt_sendmsg(sock , buffer, nLength, -1 , 1);
#endif

    }

    int startTime = getCurrentTime();
    logPrn("Receiving size : %d\n" , totalSize);
    /// receive the size 
    while (srt_epoll_wait(pollid, &rfd , &rfdlen, 0, 0, 100, 0, 0, 0, 0) < 0) {
        fprintf(stderr, "first step : srt_epoll_wiat error (%d)\n" , srt_getlasterror(NULL));
        /*
        if (isServer) {
            logPrn("Pick slow connection and close socket\n");
            srt_close(sock);
            return -2;
        }
        */
        continue;
    }
    
    logPrn("epoll check done : %d\n" , srt_getlasterror(NULL));

    nRet = srt_recvmsg(sock, (char *)&totalSize , sizeof(totalSize));

    if (nRet == SRT_ERROR) {
        fprintf(stderr, "failed to receive : [%d][%s]\n", srt_getlasterror(NULL), srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup(); 
        return -1;
    }

    int currentTime = getCurrentTime();

    logPrn("[TimeCheck]Receive Size : %d" , currentTime - startTime);

    startTime = currentTime;

    logPrn("Receive size : %d\n" , totalSize);

    pbuffer = (char *)malloc(totalSize);
    memset(pbuffer, 0x00, totalSize);

    /// receive the data
    while((totalSize > 0) && (cnt < 50)) {
        if (srt_epoll_wait(pollid, &rfd , &rfdlen, 0, 0, READ_TIMEOUT, 0, 0, 0, 0) < 0) {
            fprintf(stderr, "srt_epoll_wiat error (%d)\n" , srt_getlasterror(NULL));
            fprintf(stderr, "srt_epoll_wiat timeout (%d)\n" , cnt);
            cnt++;
            continue;
        }

        nRet = srt_recvmsg(sock, buffer + pos , totalSize);

        if (nRet == SRT_ERROR) {
            fprintf(stderr, "failed to receive : [%d][%s]\n", srt_getlasterror(NULL), srt_getlasterror_str());
            srt_epoll_remove_usock(pollid, sock);
            srt_epoll_release(pollid);
            srt_close(sock);
            srt_cleanup(); 
            free(pbuffer);
            return -1;
        }

        logPrn("receive message (%d)" , nRet);

        totalSize -= nRet;
        pos += nRet;
    }

    currentTime = getCurrentTime();

    logPrn("[TimeCheck]Receive Data : %d\n" , currentTime - startTime);

    /// send the size
    totalSize = pos;
    pos = 0;

    if (isServer) {
        srt_sendmsg(sock, (char *)&totalSize , sizeof(totalSize) , -1, 1);
        fprintf(stderr, "Send size : %d\n", totalSize);

        nRet = totalSize;

        /// send the data
#if 0
        while(nRet > 0) {
            if (nRet < 1316)
                snd_size = nRet;
            srt_sendmsg(sock , buffer + pos , snd_size , -1 , 1);
            logPrn("send message (%d)" , snd_size);
            nRet -= snd_size;
            pos += snd_size;
        }
#else
        srt_sendmsg(sock , buffer, totalSize, -1, 1);
#endif

        if (srt_epoll_wait(pollid, &rfd , &rfdlen, 0, 0, 10 * 1000, 0, 0, 0, 0) < 0) {
            fprintf(stderr, "srt_epoll_wiat timeout (%d)\n" , cnt);
            cnt++;
        }
    }

    free(pbuffer);
    srt_epoll_remove_usock(pollid, sock);
    srt_epoll_release(pollid);

    srt_close(sock);

    return 0;
}

int getCurrentTime(void)
{
    struct timeval tp;
    int nRet = gettimeofday(&tp, NULL);
    return (tp.tv_sec * 1000 + tp.tv_usec / 1000);
}
