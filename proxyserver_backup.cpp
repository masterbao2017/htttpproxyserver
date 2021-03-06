//#include "stafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>

#pragma comment(lib,"Ws2_3.lib")

#define MAXSIZE 65507
#define HTTP_PORT 80

struct HttpHeader{
	char method[4];
	char url[1024];
	char host[1024];
	char cookie[1024 * 10];
	HttpHeader(){
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
BOOL InitSocket();
void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

SOCKET proxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

const int ProxyThreadMaxNum = 20;
HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = { 0 };
DWORD ProxyThreadDW[ProxyThreadMaxNum] = { 0 };

struct ProxyParam{
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	if (!InitSocket()){
		printf("innit socket failed\n");
		return -1;
	}
	printf("proxyserver is listening %d \n", ProxyPort);
	ProxyParam *lpProxyParam;//代理服务器所需要的参数类型的指针，类型中包括发出请求的客户套接字，客户所请求的服务器端套接字
	HANDLE hThread;//线程句柄预定义；
	DWORD dwThreadID;
	while (true){//代理服务器监听
		acceptSocket = accept(proxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (NULL == lpProxyParam)
		{
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
BOOL InitSocket()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);//加载套接字动态链接库
	if (err != 0)
	{
		printf("fail to load dll winsokcet,error num: %d",WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("can not find the right winsock version");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);//建议tcp代理套接字
	if (INVALID_SOCKET == proxyServer)
	{
		printf("create socket failed,error num:%d", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;//??????
	if (bind(proxyServer, (SOCKADDR*)ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("bind error \n");
		return FALSE;
	}
	if (listen(proxyServer, SOMAXCONN) == SOCKET_ERROR){//SOMAXCONN 128最大监听队列长度
		printf("listen error\n");
		return FALSE;
	}
	return TRUE;
}

unsigned int __stdcall ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char* CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;//客户端套接字 地址
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);//接受客户端发送的报文
	if (recvSize <= 0)
	{
		goto error;
	}
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	memcpy(CacheBuffer, Buffer, recvSize);//将接受到的报文拷贝到CacheBuffer中
	ParseHttpHead(CacheBuffer, httpHeader);
	delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host))
	{
		goto error;
	}
	printf("connect to server %s \n", httpHeader->host);
	//将客户端发送的HTTP请求发给目标服务器
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer));
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);//接收目标服务器返回的数据
	if (recvSize <= 0)
	{
		goto error;
	}
	ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);//向客户端返回数据,SIZEOF STRLEN
error:
	printf("closesocket\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;

}

void ParseHttpHead(char *buffer, HttpHeader* httpHeader)
{
	char *p;
	char * ptr;
	const char* delim = "\r\n";//换行标志
	p = strtok_s(buffer, delim, &ptr);
	printf("%s\n", p);
	if (p[0] == 'G')
	{
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p)-13);

	}
	else if (p[0] == 'P')
	{
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[4], strlen(p) - 14);
	}
	printf("%s \n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p)
	{
		switch (p[0]){
		case 'H'://host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://cookie
			if (strlen(p) > 8){
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")){
					memcpy(httpHeader->host, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}

}

BOOL ConnectToServer(SOCKET *serverSocket, char *host)
{
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname();
	if (!hostent)
	{
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);//建立目标服务器套接字
	if (*serverSocket == INVALID_SOCKET)
	{
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR* serverAddr), sizeof(serverAddr)==SOCKET_ERROR)//连接目标服务器套接字地址
	{
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;

}