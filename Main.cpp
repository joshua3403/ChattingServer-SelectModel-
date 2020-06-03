#include "stdafx.h"
#include "Protocol.h"
#include "CMessage.h"
#include "CRingBuffer.h"

#define MAX_CLIENT_COUNT 100

typedef struct st_SESSION
{
	SOCKET socket;
	SOCKADDR_IN clientaddr;
	int ID;
	WCHAR NickName[dfNICK_MAX_LEN];
	CRingBuffer RecvQ;
	CRingBuffer SendQ;
} SESSION;

typedef struct st_CHATTINGROOM
{
	int ID;
	WCHAR Title[256];
	std::map<int, SESSION*> userList;
}ROOM;

std::map<int,SESSION*> g_Session_List;
std::map<int, ROOM*> g_Room_List;
int g_iUserID = 1;
int g_iRoomID = 1;
SOCKET listen_sock = INVALID_SOCKET;
timeval selecttime = { 0,0 };
FILE* g_File;
WCHAR g_TextTitle[64] = { 0 };
int g_iCurrentClientCount = 0;
// ��Ʈ��ũ �ʱ�ȭ
bool InitialNetwork();

// ��Ʈ��ũ �κ�
void NetWorkPart();

// Ŭ���̾�Ʈ�� connect() ��û�� accpet()�ϴ� �Լ�
void Accept();

// 64���� �������� ��� select�� �����ϴ� �Լ�
void SelectSocket(DWORD* dwTableNO, SOCKET* pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet);

// �ش� ���Ͽ� recv�ϴ� �Լ�
void NetWork_Recv(DWORD UserID);

// �ش� ������ send�ϴ� �Լ�
void NetWork_Send(DWORD UserID);

// ���� ���� ����
void Disconnect(DWORD UserID);

// ��Ŷ ������ ó�� ����
int CompletePacket(SESSION* session);

// �������� ��Ŷ ó�� �Լ�
bool PacketProc(SESSION* session, WORD dwType, CMessage* message);

// ��Ŷ ���� �Լ�
// Ŭ���̾�Ʈ�� ��û
bool NetWork_ReqLogin(SESSION* session, CMessage* message);
bool NetWork_ReqRoomList(SESSION* session, CMessage* message);

// ������ ����
void NetWork_ResLogin(SESSION* session, BYTE byResult);
void NetWork_ResRoomList(SESSION* session);

// ��Ŷ ���� �Լ�
void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, DWORD ID);
void MakePacket_ResRoomList(st_PACKET_HEADER* pHeader, CMessage* message);

// �ߺ� �г��� �˻�
bool CheckNickName(WCHAR* nick);

// üũ�� ����
BYTE MakeCheckSum(CMessage* message, WORD dwType);

// ����ĳ��Ʈ Send
void SendPacket_Unicast(st_SESSION*, st_PACKET_HEADER* ,CMessage*);
// Ư�� ������ ������ ��ε� ĳ��Ʈ
void SendPacket_Broadcast(st_SESSION*, CMessage*);

SESSION* FindSession(DWORD UserID);

// �α� ���� �ʱ�ȭ �Լ�
void InitialLogFile();
// �α����Ͽ� �����Է��ϴ� �Լ�
void PrintError(const WCHAR* error);
// ���� �ʱ�ȭ �Լ�
void CloseAllSession();

int main()
{
	setlocale(LC_ALL, "");
	InitialLogFile();

	if (!InitialNetwork())
	{
		wprintf(L"��Ʈ��ũ �ʱ�ȭ ����\n");
		return 0;
	}

	while (true)
	{
		NetWorkPart();
	}

	CloseAllSession();
	return 0;
}

bool InitialNetwork()
{
	int retval;
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		wprintf(L"WSAStartup() eror\n");
		return false;
	}

	// socket()
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		wprintf(L"socket() eror\n");
		return false;
	}

	// �ͺ�� �������� ��ȯ
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"ioctlsocket() eror\n");
		return false;
	}

	// ���̱� �˰��� ����
	BOOL optval = TRUE;
	setsockopt(listen_sock, IPPROTO_IP, TCP_NODELAY, (char*)&optval, sizeof(optval));

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(dfNETWORK_PORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"bind() error\n");
		return false;
	}

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"listen() error\n");
		return false;
	}

	return true;
}

void InitialLogFile()
{
	time_t now = time(NULL);
	struct tm date;
	setlocale(LC_ALL, "Korean");
	localtime_s(&date, &now);

	wcsftime(g_TextTitle, 64, L"WSAAsyncSelect(Server)_%Y%m%d_%H%M%S.txt", &date);
	g_File = _wfopen(g_TextTitle, L"wb");

	fclose(g_File);
}

void PrintError(const WCHAR* error)
{
	time_t now = time(NULL);
	struct tm date;
	setlocale(LC_ALL, "Korean");
	localtime_s(&date, &now);
	WCHAR errortext[128] = { 0 };
	WCHAR time[64] = { 0 };

	wcscat(errortext, error);
	wcsftime(time, 64, L"_%Y%m%d_%H%M%S.txt\n", &date);

	wcscat(errortext, time);

	g_File = _wfopen(g_TextTitle, L"ab");

	fwprintf_s(g_File, L"%s", errortext);

	fclose(g_File);

}

void CloseAllSession()
{
	for (std::map<int, ROOM*>::iterator itor = g_Room_List.begin(); itor != g_Room_List.end(); itor++)
	{
		delete itor->second;
	}

	for (std::map<int, SESSION*>::iterator itor = g_Session_List.begin(); itor != g_Session_List.end(); itor++)
	{
		delete itor->second;
	}
	g_Room_List.clear();
	g_Session_List.clear();
}

void NetWorkPart()
{
	SESSION* pSession = nullptr;
	FD_SET readSet, writeSet;
	DWORD UserTable_NO[FD_SETSIZE];
	SOCKET UserTable_Socket[FD_SETSIZE];
	int addrlen, retval;
	int iSessionCount = 0;

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	memset(UserTable_NO, -1, sizeof(DWORD) * FD_SETSIZE);
	memset(UserTable_Socket, INVALID_SOCKET, sizeof(SOCKET) * FD_SETSIZE);

	// ���� ���� �ֱ�
	FD_SET(listen_sock, &readSet);
	UserTable_NO[iSessionCount] = 0;
	UserTable_Socket[iSessionCount] = listen_sock;
	iSessionCount++;
	std::map<int, SESSION*>::iterator itor;
	for (itor = g_Session_List.begin(); itor != g_Session_List.end();)
	{
		pSession = (itor)->second;
		itor++;

		// Read Set�� Write Set�� ���
		UserTable_NO[iSessionCount] = pSession->ID;
		UserTable_Socket[iSessionCount] = pSession->socket;

		FD_SET(pSession->socket, &readSet);

		if (pSession->SendQ.GetUsingSize() > 0)
		{
			FD_SET(pSession->socket, &writeSet);
		}

		iSessionCount++;

		if (iSessionCount >= FD_SETSIZE)
		{
			SelectSocket(UserTable_NO, UserTable_Socket, &readSet, &writeSet);

			FD_ZERO(&readSet);
			FD_ZERO(&writeSet);
			memset(UserTable_NO, -1, sizeof(DWORD) * FD_SETSIZE);
			memset(UserTable_Socket, INVALID_SOCKET, sizeof(DWORD) * FD_SETSIZE);
			iSessionCount = 0;
		}
	}

	if (iSessionCount > 0)
	{
		SelectSocket(UserTable_NO, UserTable_Socket, &readSet, &writeSet);
	}

}

void Accept()
{
	SOCKADDR_IN clientaddr;
	SOCKET client_socket;
	int retval, addrlen;
	WCHAR szParam[16] = { 0 };
	SESSION* clientSessionPtr;

	addrlen = sizeof(clientaddr);
	client_socket = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_socket == INVALID_SOCKET)
	{
		PrintError(L"accept()");
		return;
	}

	clientSessionPtr = new SESSION;
	clientSessionPtr->socket = client_socket;
	clientSessionPtr->clientaddr = clientaddr;
	clientSessionPtr->ID = g_iUserID;
	InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
	wprintf(L"\n[Select ����] Ŭ���̾�Ʈ ���� : IP �ּ� %s, ��Ʈ ��ȣ = %d\n", szParam, ntohs(clientaddr.sin_port));
	g_iUserID++;
	g_Session_List.insert(std::make_pair(clientSessionPtr->ID, clientSessionPtr));

}

void SelectSocket(DWORD* dwTableNO, SOCKET* pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet)
{
	SESSION* pSession = nullptr;
	int iResult = 0;
	int iCnt = 0;

	iResult = select(0, pReadSet, pWriteSet, 0, &selecttime);

	if (iResult > 0)
	{
		for (int i = 0; i < FD_SETSIZE; i++)
		{
			if (pTableSocket[i] == INVALID_SOCKET)
				continue;

			if (FD_ISSET(pTableSocket[i], pReadSet))
			{
				// listen_sock�̸� ���� ó��
				if (dwTableNO[i] == 0)
					Accept();
				else
					NetWork_Recv(dwTableNO[i]);
			}

			if (FD_ISSET(pTableSocket[i], pWriteSet))
			{
				NetWork_Send(dwTableNO[i]);
			}
		}
	}
}

void NetWork_Recv(DWORD UserID)
{
	int iResult = 0;
	SESSION* pSession = nullptr;
	char buffer[BUFSIZE] = { 0 };
	int iRecvBufferFreeSize = 0;
	int iEnqueueSize = 0;

	pSession = FindSession(UserID);

	// ������ �����ϴ� �� �ƴұ�??
	if (pSession == nullptr)
		closesocket(listen_sock);

	iRecvBufferFreeSize = pSession->RecvQ.GetFreeSize();
	char* temp = pSession->RecvQ.GetRearBufferPtr();
	iResult = recv(pSession->socket, temp, iRecvBufferFreeSize, 0);

	pSession->RecvQ.MoveRear(iResult);
	

	if (iResult == SOCKET_ERROR || iResult == 0)
	{
		Disconnect(UserID);
		return;
	}

	//iEnqueueSize = pSession->RecvQ.Enqueue(buffer, iResult);

	if (iResult > 0)
	{
		//if (iResult != iEnqueueSize)
		//{
		//	PrintError(L"FatalError EnQueue");
		//	exit(1);
		//}

		while (true)
		{
			iResult = CompletePacket(pSession);
			if (iResult == 1)
				break;
			// ��Ŷ ó�� ����
			if (iResult == -1)
			{
				WCHAR error[32] = { 0 };
				swprintf_s(error, 32, L"Packet Error UserID : %d\n", pSession->ID);
				PrintError(error);
				return;
			}
		}
	}

}

void NetWork_Send(DWORD UserID)
{
	int iResult = 0;
	SESSION* pSession = nullptr;
	char buffer[BUFSIZE] = { 0 };
	int iSendBufferUsingSize = 0;
	int iDequeueSize = 0;

	pSession = FindSession(UserID);
	if (pSession == nullptr)
		closesocket(listen_sock);

	iSendBufferUsingSize = pSession->SendQ.GetUsingSize();

	if (iSendBufferUsingSize <= 0)
		return;

	iResult = send(pSession->socket, pSession->SendQ.GetFrontBufferPtr(), iSendBufferUsingSize, 0);
	pSession->SendQ.MoveFront(iResult);

	if (iResult == SOCKET_ERROR || iResult == 0)
	{
		DWORD error = WSAGetLastError();
		if (WSAEWOULDBLOCK == error)
		{
			wprintf(L"Socket WOULDBLOCK - UerID : %d\n", UserID);
			return;
		}
		wprintf(L"Socket Error - Error : %d, UserID : %d\n", error,UserID);
		Disconnect(UserID);
		return;
	}

	return;
}

void Disconnect(DWORD UserID)
{
	WCHAR szParam[16] = { 0 };
	SESSION* pSession = FindSession(UserID);

	InetNtop(AF_INET, &(pSession->clientaddr.sin_addr), szParam, 16);

	wprintf(L"\n[Select ����] Ŭ���̾�Ʈ ���� : IP �ּ� %s, ��Ʈ ��ȣ = %d\n", szParam, ntohs(pSession->clientaddr.sin_port));
	closesocket(pSession->socket);
	delete pSession;
	g_Session_List.erase(UserID);
}

int CompletePacket(SESSION* session)
{
	st_PACKET_HEADER PacketHeader;
	int iRecvQSize = session->RecvQ.GetUsingSize();

	if (iRecvQSize < sizeof(PacketHeader))
		return 1;

	// ��Ŷ �˻�
	session->RecvQ.Peek((char*)&PacketHeader, sizeof(st_PACKET_HEADER));

	// ��Ŷ �ڵ� �˻�
	if (PacketHeader.byCode != dfPACKET_CODE)
		return -1;

	if (PacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER) > iRecvQSize)
		return 1;

	session->RecvQ.MoveFront(sizeof(st_PACKET_HEADER));

	CMessage Packet;
	if (PacketHeader.wPayloadSize != session->RecvQ.Dequeue(Packet.GetBufferPtr(), PacketHeader.wPayloadSize))
		return -1;

	Packet.MoveWritePos(PacketHeader.wPayloadSize);
	BYTE byCheckSum = MakeCheckSum(&Packet, PacketHeader.wMsgType);

	if (byCheckSum != PacketHeader.byCheckSum)
	{
		wprintf(L"CheckSum Error UserID : %d\n", session->ID);
		return -1;
	}

	if (!PacketProc(session, PacketHeader.wMsgType, &Packet))
	{
		wprintf(L"PacketProc Error!\n");
		return -1;
	}
	return 0;
}

bool PacketProc(SESSION* session, WORD dwType, CMessage* message)
{
	wprintf(L"Packet Info UserID : %d, Message Type : %d\n", session->ID, dwType);
	switch (dwType)
	{
	case df_REQ_LOGIN:
		return NetWork_ReqLogin(session, message);
			break;

	case df_REQ_ROOM_LIST:
		return NetWork_ReqRoomList(session, message);
		break;
	case df_REQ_ROOM_CREATE:
		return;
		break;
	default:
		break;
	}
	return false;
}

bool NetWork_ReqLogin(SESSION* session, CMessage* message)
{
	WCHAR temp[15] = { '\0' };
	memcpy(temp, message->GetBufferPtr(), sizeof(temp));

	// 2. �ߺ��г��� �˻�
	if (CheckNickName(temp))
	{
		NetWork_ResLogin(session, df_RESULT_LOGIN_DNICK);
		return true;
	}
	// 3. �����ο� �ʰ�
	if (g_iCurrentClientCount > 100)
	{
		NetWork_ResLogin(session, df_RESULT_LOGIN_MAX);
		return true;
	}

	wcscpy(session->NickName, temp);
	NetWork_ResLogin(session,df_RESULT_LOGIN_OK);
	g_iCurrentClientCount++;
	return true;
}

void NetWork_ResLogin(SESSION* session, BYTE byResult)
{
	CMessage packet;
	st_PACKET_HEADER header;

	MakePacket_ResLogin(&header, &packet, byResult, session->ID);

	SendPacket_Unicast(session, &header, &packet);
}

bool NetWork_ReqRoomList(SESSION* session, CMessage* message)
{
	NetWork_ResRoomList(session);
	return true;
}

void NetWork_ResRoomList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;

	MakePacket_ResRoomList(&header, &packet);

	SendPacket_Unicast(session, &header, &packet);
}



void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, DWORD ID)
{
	(*message) << result;
	(*message) << ID;
	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_LOGIN);
	pHeader->wMsgType = df_RES_LOGIN;
	pHeader->wPayloadSize = message->GetDataSize();

}

void MakePacket_ResRoomList(st_PACKET_HEADER* pHeader, CMessage* message)
{
	WORD RoomCount = g_Room_List.size();
	WORD titleSize = 0;
	message->Clear();
	(*message) << RoomCount;
	for (std::map<int, ROOM*>::iterator itor = g_Room_List.begin(); itor != g_Room_List.end(); itor++)
	{
		(*message) << itor->second->ID;
		titleSize = wcslen(itor->second->Title) * sizeof(WCHAR);
		(*message) << titleSize;
		message->PutData((char*)itor->second->Title, titleSize);
		BYTE userCount = itor->second->userList.size();
		(*message) << userCount;
		for (std::map<int, SESSION*>::iterator itor2 = itor->second->userList.begin(); itor2 != itor->second->userList.end(); itor2++)
		{
			message->PutData((char*)itor2->second->NickName, 30);
		}
	}

	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_ROOM_LIST);
	pHeader->wMsgType = df_RES_ROOM_LIST;
	pHeader->wPayloadSize = message->GetDataSize();
}

bool CheckNickName(WCHAR* nick)
{
	for (std::map<int, SESSION*>::iterator itor = g_Session_List.begin(); itor != g_Session_List.end(); itor++)
	{
		if (wcscmp(nick, (itor)->second->NickName) == 0)
			return true;
	}
	return false;
}

BYTE MakeCheckSum(CMessage* message, WORD dwType)
{
	int iSize = message->GetDataSize();
	BYTE* pPtr = (BYTE*)message->GetBufferPtr();
	int iCheckSum = dwType;
	for (int i = 0; i < iSize; i++)
	{
		iCheckSum += *pPtr;
		pPtr++;
	}
	return(BYTE)(iCheckSum % 256);
	return 0;
}

void SendPacket_Unicast(st_SESSION* session, st_PACKET_HEADER* header, CMessage* message)
{
	if (session == nullptr)
	{
		wprintf(L"SendPacket_Unicast Error!\n");
		return;
	}

	session->SendQ.Enqueue((char*)header, sizeof(st_PACKET_HEADER));
	session->SendQ.Enqueue((char*)message->GetBufferPtr(), message->GetDataSize());
}

SESSION* FindSession(DWORD UserID)
{
	SESSION* pSession = nullptr;
	pSession = g_Session_List.find(UserID)->second;
	return pSession;
}

