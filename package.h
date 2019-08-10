#pragma once
#ifndef _PACKAGE_H_
#define _PACKAGE_H_
//����������������������Ķ˿ں�
#define CMD_PORT 5858
//�ͻ�������������������Ķ˿�
#define DATA_PORT 5000
//����Ĳ����Ļ����С
#define CMD_PARAM_SIZE 256
//�ظ�������Ϣ����Ĵ�С
#define RSPNS_TEXT_SIZE 256
#define BACKLOG 10
#define DATA_BUFSIZE 4096

//��������
typedef enum {
        LS,PWD,CD,DOWN,UP,QUIT
}CmdID;
//�����,�ӿͻ��˷���������
typedef struct _CmdPacket {
	CmdID cmdid;
	char param[CMD_PARAM_SIZE];
}CmdPacket;
//�ظ����ĵ�����
typedef enum {
	  OK,ERR
}RspnsID;
//�ظ�����,�ӷ����������ͻ���
typedef struct _RspnsPacket {
	RspnsID rspnsid;
	char text[RSPNS_TEXT_SIZE];
}Rspnspacket;
#endif
