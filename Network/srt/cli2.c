#include <stdio.h>
#include <logging_api.h>
#include <udt.h>
#include <srt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#define MAX_BUF_LENG 10240
#define BUFFER_SIZE (1024 * 1024)
#define READ_TIMEOUT 100
#define READ_SIZE 2048

char* get_datestr(char* strDate);
char* get_timestr(char* strTime);
void logPrn(const char *format, ... );
int readData(char* filename, char* buffer);
SRTSOCKET connectSRT(char *src_addr, int src_port, char *dest_addr, int dest_port, bool rendezvous);
int createEPoll(SRTSOCKET sock);

int main(int argc , char *argv[]) 
{
    char *src_addr = NULL, *dest_addr, *filename, *pbuffer, buffer[BUFFER_SIZE];
    int src_port, dest_port, nRet = 0, pos = 0, snd_size = 1316;
	SRTSOCKET sock, rfd;
	int rfdlen = 1, cnt = 0, totalSize = 0;


    srt_startup();
    UDT::setloglevel(srt_logging::LogLevel::debug);
    UDT::addlogfa(10);

    if (argc != 5 && argc != 6) {
        printf("%s [src_ip] src_port dest_ip dest_port trans_file\n" , argv[0]);
        return -1;
    }

    /// assign argument	
    if (argc == 6) {
        src_addr = argv[1];
        src_port = atoi(argv[2]);
        dest_addr = argv[3];
        dest_port = atoi(argv[4]);
        filename = argv[5];
    }
    else {
        src_addr = NULL;
        src_port = atoi(argv[1]);
        dest_addr = argv[2];
        dest_port = atoi(argv[3]);
        filename = argv[4];
    }

    /// open the sending data
    nRet = readData(filename, buffer);

    if (nRet == -1) {
        return -1;
    }


    sock = connectSRT(src_addr, src_port, dest_addr, dest_port, true);
    if (sock == SRT_ERROR) {
        return -1;
    }

    logPrn("Sueccess : connect");

    /// send the size
    srt_sendmsg(sock, (char *)&nRet , sizeof(nRet) , -1, 1);
    printf("Send size : %d\n", nRet);

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
    srt_sendmsg(sock , buffer, nRet, -1 , 1);
#endif

    int pollid = createEPoll(sock);

    if (pollid == SRT_ERROR) {
        srt_close(sock);
        srt_cleanup();
        return -1;
    }

	logPrn("Receiving size : %d\n" , totalSize);
	/// receive the size
	while (srt_epoll_wait(pollid, &rfd , &rfdlen, 0, 0, 10 * 1000, 0, 0, 0, 0) < 0) {
		printf("first step : srt_epoll_wiat timeout (%d)\n" , cnt);
		continue;
	}	

	nRet = srt_recvmsg(sock, (char *)&totalSize , sizeof(totalSize));

	if (nRet == SRT_ERROR) {
        printf("failed to receive : [%d][%s]\n", srt_getlasterror(NULL), srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup();
        return -1;
	}

	logPrn("Receive size : %d\n" , totalSize);

	pbuffer = (char *)malloc(totalSize);
	memset(pbuffer, 0x00, totalSize);
	pos = 0;

	/// receive the data
    while((totalSize > 0) && (cnt < 50)) {
        if (srt_epoll_wait(pollid, &rfd , &rfdlen, 0, 0, READ_TIMEOUT, 0, 0, 0, 0) < 0) {
            printf("srt_epoll_wiat timeout (%d)\n" , cnt);
            cnt++;
            continue;
        }

        nRet = srt_recvmsg(sock, pbuffer + pos , totalSize);

        if (nRet == SRT_ERROR) {
            printf("failed to receive : [%d][%s]\n", srt_getlasterror(NULL), srt_getlasterror_str());
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


    /// cleanup srt epoll
    srt_epoll_remove_usock(pollid, sock);
    srt_epoll_release(pollid);

    free(pbuffer);

    srt_close(sock);

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

	printf("%s %s\n" , szTimeBuffer, szBuffer);
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
int readData(char* filename, char* buffer)
{
    int nRet = 0;
    int fd = open(filename , O_RDONLY , 0);

    if (fd < 0) {
        printf("can't open %s\n", filename);
        return -1;
    }

    nRet = read(fd, buffer, BUFFER_SIZE);

    if (nRet <= 0) {
        printf("can't read file(%s)\n" , filename);
        close(fd);
        return -1;
    }

    close(fd);

    return nRet;
}

SRTSOCKET connectSRT(char *src_addr, int src_port, char *dest_addr, int dest_port, bool rendezvous)
{
    int file_mode = SRTT_FILE;
    int yes = 1;

    /// open and set option for srt socket
    SRTSOCKET sock = srt_socket(AF_INET, SOCK_DGRAM, 0);

    srt_setsockflag(sock, SRTO_TRANSTYPE, &file_mode, sizeof(file_mode));
    //srt_setsockflag(sock, SRTO_MESSAGEAPI, &yes, sizeof(yes));

    srt_setsockopt(sock, 0, SRTO_RENDEZVOUS, &yes, sizeof(yes));

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
        printf("Failed : srt_bind [%d][%s]", srt_errno, srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup();
        return SRT_ERROR;
    }

    /// set destination ip and port
    memset(&this_sin , 0x00, sizeof(this_sin));
    this_sin.sin_family         = AF_INET;
    this_sin.sin_port           = htons(dest_port);
    this_sin.sin_addr.s_addr    = inet_addr(dest_addr);

	int dwTimeOut = 10000;

    /// connect 
	if(srt_setsockopt(sock, 0, SRTO_CONNTIMEO, &dwTimeOut, sizeof(dwTimeOut)) == SRT_ERROR)
    {
        int srt_errno = srt_getlasterror(NULL);
        printf("Failed : srt_setsockopt, timeout (SRT) [%d][%s]", srt_errno, srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup();
        return SRT_ERROR;
    }

    logPrn("try connection");

    nRet = srt_connect(sock, (struct sockaddr*)&this_sin, sizeof(this_sin));
    if(nRet == SRT_ERROR) {
        int srt_errno = srt_getlasterror(NULL);
        printf("Failed : srt_connect (SRT) [%d][%s]", srt_errno, srt_getlasterror_str());
        srt_close(sock);
        srt_cleanup();
	    return	SRT_ERROR;
    }
     
    return sock;
}

int createEPoll(SRTSOCKET sock)
{
	/// create srt epoll
	int pollid;

	pollid = srt_epoll_create();

	if (pollid < 0) {
		printf("can't create srt epoll : [%d][%s]\n", 
                srt_getlasterror(NULL), srt_getlasterror_str());
		return SRT_ERROR;
	}


	int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;

	if (srt_epoll_add_usock(pollid, sock, &events)) {
		printf("failed to add srt destination to poll : [%d][%s]\n",
		  srt_getlasterror(NULL), srt_getlasterror_str());
		return SRT_ERROR;
	}

    return pollid;
}
