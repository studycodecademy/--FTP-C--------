#include "stdafx.h"
//FTP服务端开发
#include<Winsock2.h>
#include"package.h"
#include<iostream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
//创建线程时传递的数据结构,内含控制连接所需的套接字和客户端地址信息:
struct threadData {
	SOCKET tcps;
	sockaddr_in clientaddr;
};
//全局函数声明
int InitFTP(SOCKET *pListenSock);
int SendRspns(SOCKET tcps,Rspnspacket *prspns);
int RecvCmd(SOCKET tcps,char* pCmd);
int ProcessCmd(SOCKET tcps, CmdPacket *pCmd, SOCKADDR_IN *pClientaddr);
int InitDataSocket(SOCKET *pDatatcps,SOCKADDR_IN *pClientaddr);
int SendFileList(SOCKET datatcps);
int SendFileRecord(SOCKET datatcps,WIN32_FIND_DATA *pfd);
int FileExits(const char *filename);
int SendFile(SOCKET,FILE *);
int RecvFile(SOCKET datatcps,char *filename);
//线程函数,参数包括控制连接的套接字;
DWORD WINAPI Threadproc(LPVOID param) {
	SOCKET tcps;
	sockaddr_in clientaddr;
	tcps = ((struct threadData*)param)->tcps;
	clientaddr = ((struct threadData*)param)->clientaddr;
	cout << "回复的套接字编号为:" << tcps << endl;
	
	//发送回复报文给客户端
	Rspnspacket rspns = { OK,
		"欢迎登陆FTP服务器系统!\n"
	    "你能使用的命令:\n"
	    "LS\t<展示当前目录下的文件(夹),无需参数>\n"
	    "PWD\t<展示当前目录的绝对路径,无需参数>\n"
	    "CD\t<切换到指定目录,参数为路径>\n"
	    "DOWN\t<下载文件,参数为文件名>\n"
	    "UP\t<上传文件,参数为文件名>\n"
	    "QUIT\t<退出系统,无需参数>\n"
	};
	SendRspns(tcps,&rspns);
	//循环获取客户端命令报文并并进行处理
	while (1) {
		CmdPacket cmd;
		if (!RecvCmd(tcps, (char*)&cmd)) {
			cout << "接收报文出错" << endl;
			break;
		}
		if (!ProcessCmd(tcps, &cmd, &clientaddr)) {
			cout << "系统发送文件出错" << endl;
			break;
		}
	}
	return 0;
}
int main(int argc, char* argv[]) {
	SOCKET tcps_listen;                    //FTP服务器控制连接侦听套接字
	struct threadData* pThInfo;            

	if (!InitFTP(&tcps_listen)) {         //FTP初始化
		return 0;
	}
	printf_s("FTP服务器开始监听,端口号为:%d.....",CMD_PORT);
	while(1){
		pThInfo = NULL;
		pThInfo = new threadData;
		if (pThInfo == NULL) {
			cout << "为新线程结构申请空间失败." << endl;
			continue;
		}
		int len = sizeof(sockaddr);
		//等待客户端连接
		pThInfo->tcps = accept(tcps_listen,(sockaddr*)&pThInfo->clientaddr,&len);

		//连接好后,创建线程来处理相应客户端请求;
		DWORD dwThreadid, dwThreadparam = 1;             //线程ID号
		HANDLE hthread;
		hthread = CreateThread(NULL,0,Threadproc,pThInfo,0,&dwThreadid);
		
		//检查返回值是否创建成功
		if (hthread == NULL) {
			cout << "创建线程失败!" << endl;
			closesocket(pThInfo->tcps);
			delete pThInfo;
			exit(0);
		}
	}

	return 0;
}
//ftp初始化,创建一个侦听套接字
int InitFTP(SOCKET *pListenSock) {
   //按照此步骤创建服务器端套接字
	//WSAStartup->socket->bind->listen
	WORD wVersionRequested;
	WSADATA wsadata;
	int err;
	SOCKET tcps_listen;

	wVersionRequested = MAKEWORD(2,2);
	err = WSAStartup(wVersionRequested,&wsadata);        //初始化动态库信息
	if (err !=0) {
		cout << "Winsock初始化动态库错误!" << endl;
		return 0;
	}
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2) {
		WSACleanup();
		cout << "无效Winsock版本号!\n" << endl;
		return 0;
	}
	//创建套接字
	tcps_listen = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (tcps_listen == INVALID_SOCKET) {
		WSACleanup();
		cout << "创建套接字失败!" << endl;
		return 0;
	}
	sockaddr_in tcpaddr;
	tcpaddr.sin_family = AF_INET;
	tcpaddr.sin_port = htons(CMD_PORT);
	tcpaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	err = bind(tcps_listen,(sockaddr*)&tcpaddr,sizeof(sockaddr));
	if (err != 0) {
		err = WSAGetLastError();
		WSACleanup();
		cout << "绑定失败!" << endl;
		return 0;
	}
	err = listen(tcps_listen,3);
	if (err != 0) {
		WSACleanup();
		cout << "监听时发生错误!" << endl;
		return 0;
	}
	*pListenSock = tcps_listen;
	return 1;
}
int SendRspns(SOCKET tcps, Rspnspacket *prspns) {
	if (send(tcps, (char*)prspns, sizeof(Rspnspacket), 0) == SOCKET_ERROR) {
		cout << "与客户端失去联系." << endl;
		return 0;
	}
	return 1;
}
int RecvCmd(SOCKET tcps, char *pCmd) {
	int nRet;
	int left = sizeof(CmdPacket);

	//从控制连接中读取数据
	while(left) {
		nRet = recv(tcps,pCmd,left,0);            //接收消息
		if (nRet == SOCKET_ERROR) {
			cout << "从客户端接收命令时发生未知错误" << endl;
			return 0;
		}
		if (!nRet) {
			cout << "未接收到数据" << endl;
			return 0;
		}
		left -= nRet;
		pCmd += nRet;
	}

	return 1;    //成功获取报文
}
int ProcessCmd(SOCKET tcps, CmdPacket *pCmd, SOCKADDR_IN *pClientaddr) {
	SOCKET datatcps;                           //数据连接套接字
	Rspnspacket rspns;                         //回复报文                      
	FILE *file;

	//根据命令类型分别执行
	switch (pCmd->cmdid) {
	case LS:  //展示当前目录下的文件列表
				  //首先建立数据连接
		if (!InitDataSocket(&datatcps, pClientaddr)) {                    //所需参数数据连接套接字,客户端地址信息
			cout << "建立数据连接失败!" << endl;
			return 0;
		}
		//发送文件列表信息
		if (!SendFileList(datatcps)) {
			cout << "发送文件列表失败!" << endl;
			return 0;
		}
		break;
	case PWD://展示当前目录的绝对路径
		rspns.rspnsid = OK;
		//获取当前目录.并放置回复报文中
		if (!GetCurrentDirectory(RSPNS_TEXT_SIZE, rspns.text)) {
			strcpy_s(rspns.text, "无法获取当前目录!");
		}
		if (!SendRspns(tcps, &rspns))
			return 0;
		break;
	case CD: //设置当前目录,使用win32API接口函数
		if (SetCurrentDirectory(pCmd->param)) {
			rspns.rspnsid = OK;
			strcpy_s(rspns.text, "切换当前目录成功!但是不能获取到当前目录的文件列表!\n");
		}
		else {
			rspns.rspnsid = ERR;
			strcpy_s(rspns.text, "不能更换到所选目录! \n");
		}
		if (!SendRspns(tcps, &rspns)) {  //发送回复报文
			return 0;
		}
		break;
	case DOWN:               //处理下载请求
		errno_t err2;
		err2 = fopen_s(&file, pCmd->param, "rb");
		if (err2 != 0) {
			rspns.rspnsid = ERR;
			strcpy_s(rspns.text, "打开文件失败! \n");
			if (!SendRspns(tcps, &rspns)) {
				return 0;
			}
		}
		else {
			rspns.rspnsid = OK;
			sprintf_s(rspns.text, "下载文件%s\n", pCmd->param);
			if (!SendRspns(tcps, &rspns)) {
				fclose(file);
				return 0;
			}
			else {
				//创建额外数据连接传送数据
				if (!(InitDataSocket(&datatcps, pClientaddr))) {
					fclose(file);
					return 0;
				}
				if (!SendFile(datatcps,file)) {
					return 0;
				}
				fclose(file);
			}
		}
		break;
	case UP:   //处理上传文件请求
	 //先保存所要上传的文件名
		char filename[64];
		strcpy_s(filename, pCmd->param);
		//先检查服务器上是否有这个文件,如果有就告诉客户端不用传输
		if (FileExits(filename)) {

			rspns.rspnsid = ERR;
			sprintf_s(rspns.text, "服务器已经存在此文件! \n");
			if (!SendRspns(tcps, &rspns))
				return 0;
		}
		else {
			rspns.rspnsid = OK;
			if (!SendRspns(tcps, &rspns))
				return 0;

			//另建立一个数据连接来接收数据
			if (!InitDataSocket(&datatcps, pClientaddr))
				return 0;
			if (!RecvFile(datatcps, filename))
				return 0;
			else {
				sprintf_s(rspns.text, "接收成功!");
				if (!SendRspns(tcps, &rspns)) {
					return 0;
				}
			}
		}
		break;
	case QUIT:
		cout << "客户端断开连接!" << endl;
		rspns.rspnsid = OK;
		strcpy_s(rspns.text, "常来啊! \n");
		if (!SendRspns(tcps, &rspns))
			return 0;
		break;
	}
	
	return 1;
}
//建立数据连接
//pDatatcps 建立套接字连接,保存套接字
//在连接过程中需要用到客户端地址信息
int InitDataSocket(SOCKET *pDatatcps, SOCKADDR_IN *pClientaddr) {
	SOCKET datatcps;
	int err;                              //判断连接情况
	//创建socket
	datatcps = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (datatcps == INVALID_SOCKET) {
		cout << "创建套接字数据连接失败" << endl;
		return 0;
	}
	// 所需客户端地址
	SOCKADDR_IN tcpaddr;
	memcpy(&tcpaddr,pClientaddr,sizeof(SOCKADDR_IN));       //将客户端中的地址拷贝到tcpaddr中
	tcpaddr.sin_port = htons(DATA_PORT);                    //更改客户端的端口号

	//请求建立连接
	while (1) {
		err = connect(datatcps, (const sockaddr*)&tcpaddr, sizeof(sockaddr));
		if (err != SOCKET_ERROR)
			break;
	}
	if (err == SOCKET_ERROR) {
		cout << "创建连接请求失败" << endl;
		return 0;
	}
	*pDatatcps = datatcps;                      //将创建的套接字保存到pDatatcps中
	return 1;
}
//发送文件列表信息
//datatcps数据连接套接字
//返回值:0-错误,1-成功
int SendFileList(SOCKET datatcps) {
	HANDLE hff;
	WIN32_FIND_DATA fd;                //保存文件信息的结构体
	int count;
	//搜索文件
	hff = FindFirstFile("E:\\CC++项目实战\\FTP服务器和客户端开发\\*.*",&fd);
	if (hff == INVALID_HANDLE_VALUE) {          //发生错误
		const char* errstr = "不能列出文件! \n";
		cout << "文件列出失败" <<endl;
		count = send(datatcps, errstr, sizeof(errstr), 0);
		if ( count== SOCKET_ERROR) {
			cout << "发送文件列表时未知错误" << endl;
		}
		closesocket(datatcps);
		return 0;
	}
	BOOL fMoreFiles = TRUE;
	while (fMoreFiles) {
	   //发送此项文件信息
		if (!SendFileRecord(datatcps, &fd)) {
			closesocket(datatcps);
			return 0;
		}
		//搜索下一个文件
		fMoreFiles = FindNextFile(hff,&fd);
	}
	closesocket(datatcps);
	return 1;
}

int SendFileRecord(SOCKET datatcps, WIN32_FIND_DATA *fpd) {
	char filerecord[MAX_PATH+32];
	FILETIME ft;
	FileTimeToLocalFileTime(&(fpd->ftLastWriteTime),&ft);
	SYSTEMTIME lastwtime;
	FileTimeToSystemTime(&ft,&lastwtime);
	char *dir = (char*)(fpd->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?"<DIR>":"");
	sprintf_s(filerecord,"%04d-%02d-%02d%02d:%02d %5s %10d %-20s\n",
		lastwtime.wYear,
		lastwtime.wMonth,
		lastwtime.wDay,
		lastwtime.wHour,
		lastwtime.wMinute,
		dir,
		fpd->nFileSizeLow,
		fpd->cFileName);
	int count = send(datatcps, filerecord, strlen(filerecord), 0);
	if ( count== SOCKET_ERROR) {
		printf_s("发送文件列表时发生未知错误! \n");
		return 0;
	}
	return 1;
}
int FileExits(const char* filename) {
	WIN32_FIND_DATA fd;
	if (FindFirstFile(filename, &fd) == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return 1;
}
int RecvFile(SOCKET datatcps, char *filename) {
	char buf[1024];
	FILE *file;
	errno_t err3 = fopen_s(&file,filename,"wb");
	if (err3 != 0) {
		printf_s("写入文件发生未知错误! \n");
		fclose(file);
		closesocket(datatcps);
		return 0;
	}
	printf_s("接收文件数据中.....");
	while (true) {
		int r = recv(datatcps,buf,1024,0);
		if (r == SOCKET_ERROR) {
			printf_s("从客户端接收数据时发生未知错误!");
			fclose(file);
			closesocket(datatcps);
			return 0;
		}
		if (!r) {
			break;
		}
		fwrite(buf,1,r,file);

	}
	fclose(file);
	closesocket(datatcps);
	printf_s("完成传输!");
	return 1;
}

int SendFile(SOCKET datatcps, FILE *file) {
	char buf[1024];
	cout << "发送数据中......." << endl;
	while (true) {
		int r = fread(buf,1,1023,file);
		buf[r] = '\0';
		if (send(datatcps, buf, strlen(buf)+1, 0) == SOCKET_ERROR) {
			cout << "客户端与服务器失去联系" <<endl;
			return 0;
		}
		if (r < 1024) {
			break;
		}
	}
	closesocket(datatcps);
	cout << "完成传输" <<endl;
	return 1;
}