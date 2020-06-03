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
	int RoomID;
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
// 네트워크 초기화
bool InitialNetwork();

// 네트워크 부분
void NetWorkPart();

// 클라이언트의 connect() 요청을 accpet()하는 함수
void Accept();

// 64개의 세션으로 끊어서 select를 실행하는 함수
void SelectSocket(DWORD* dwTableNO, SOCKET* pTableSocket, FD_SET* pReadSet, FD_SET* pWriteSet);

// 해당 소켓에 recv하는 함수
void NetWork_Recv(DWORD UserID);

// 해당 소켓이 send하는 함수
void NetWork_Send(DWORD UserID);

// 접속 종료 절차
void Disconnect(DWORD UserID);

// 패킷 생성과 처리 절차
int CompletePacket(SESSION* session);

// 실질적인 패킷 처리 함수
bool PacketProc(SESSION* session, WORD dwType, CMessage* message);

// 패킷 대응 함수
// 클라이언트의 요청
bool NetWork_ReqLogin(SESSION* session, CMessage* message);
bool NetWork_ReqRoomList(SESSION* session, CMessage* message);
bool NetWork_ReqRoomCreate(SESSION* session, CMessage* message);
bool NetWork_ReqRoomEnter(SESSION* session, CMessage* message);
bool NetWork_ReqChatting(SESSION* session, CMessage* message);

// 서버의 응답
void NetWork_ResLogin(SESSION* session, BYTE byResult);
void NetWork_ResRoomList(SESSION* session);
void NetWork_ResRoomCreate(SESSION* session, BYTE result, ROOM* room = nullptr);
void NetWork_ResRoomEnter(SESSION* session, BYTE result, ROOM* room = nullptr);
void NetWork_ResRoomOtherClientEnter(SESSION* session, BYTE result, SESSION* enteredSession);
void NetWork_ResRoomChatting(SESSION* session, CMessage* message);

// 패킷 생성 함수
void MakePacket_ResLogin(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, DWORD ID);
void MakePacket_ResRoomList(st_PACKET_HEADER* pHeader, CMessage* message);
void MakePacket_ResRoomCreate(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, ROOM* pRoom);
void MakePacket_ResRoomEnter(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, ROOM* pRoom);
void MakePacket_ResRoomOtherClientEnter(st_PACKET_HEADER* pHeader, CMessage* message, SESSION* session);
void MakePacket_ResChatting(st_PACKET_HEADER* pHeader, CMessage* message, SESSION* session, CMessage* text);

// 중복 닉네임 검사
bool CheckNickName(WCHAR* nick);

// 중복 방제목 검사
bool CheckTitle(WCHAR* title);

// 체크섬 생성
BYTE MakeCheckSum(CMessage* message, WORD dwType);

// 유니캐스트 Send
void SendPacket_Unicast(st_SESSION*, st_PACKET_HEADER* ,CMessage*);
// 특정 세션을 제외한 브로드 캐스트
void SendPacket_Broadcast(st_PACKET_HEADER*, CMessage*);

SESSION* FindSession(DWORD UserID);

ROOM* FindRoom(DWORD RoomID);

// 로그 파일 초기화 함수
void InitialLogFile();
// 로그파일에 에러입력하는 함수
void PrintError(const WCHAR* error);
// 세션 초기화 함수
void CloseAllSession();

int main()
{
	setlocale(LC_ALL, "");
	InitialLogFile();

	if (!InitialNetwork())
	{
		wprintf(L"네트워크 초기화 실패\n");
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
	// 윈속 초기화
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

	// 넌블록 소켓으로 전환
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		wprintf(L"ioctlsocket() eror\n");
		return false;
	}

	// 네이글 알고리즘 끄기
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

	// 리슨 소켓 넣기
	FD_SET(listen_sock, &readSet);
	UserTable_NO[iSessionCount] = 0;
	UserTable_Socket[iSessionCount] = listen_sock;
	iSessionCount++;
	std::map<int, SESSION*>::iterator itor;
	for (itor = g_Session_List.begin(); itor != g_Session_List.end();)
	{
		pSession = (itor)->second;
		itor++;

		// Read Set과 Write Set에 등록
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
	wprintf(L"\n[Select 서버] 클라이언트 접속 : IP 주소 %s, 포트 번호 = %d\n", szParam, ntohs(clientaddr.sin_port));
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
				// listen_sock이면 접속 처리
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

	// 서버를 꺼야하는 것 아닐까??
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


	if (iResult > 0)
	{
		while (true)
		{
			iResult = CompletePacket(pSession);
			if (iResult == 1)
				break;
			// 패킷 처리 오류
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

	wprintf(L"\n[Select 서버] 클라이언트 종료 : IP 주소 %s, 포트 번호 = %d\n", szParam, ntohs(pSession->clientaddr.sin_port));
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

	// 패킷 검사
	session->RecvQ.Peek((char*)&PacketHeader, sizeof(st_PACKET_HEADER));

	// 패킷 코드 검사
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

//////////////////////////////////////////////////////
// 패킷 처리 함수
//////////////////////////////////////////////////////
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
		return NetWork_ReqRoomCreate(session, message);
		break;
	case df_REQ_ROOM_ENTER:
		return NetWork_ReqRoomEnter(session, message);
		break;
	case df_REQ_CHAT:
		return NetWork_ReqChatting(session, message);
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

	// 2. 중복닉네임 검사
	if (CheckNickName(temp))
	{
		NetWork_ResLogin(session, df_RESULT_LOGIN_DNICK);
		return true;
	}
	// 3. 수용인원 초과
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

bool NetWork_ReqRoomCreate(SESSION* session, CMessage* message)
{
	WCHAR szRoomTitle[256] = { 0 };
	WORD wTitleSize = 0;

	ROOM* pRoom = nullptr;

	(*message) >> wTitleSize;
	message->GetData((char*)szRoomTitle, wTitleSize);

	if (wTitleSize > 256 || wTitleSize == 0)
	{
		NetWork_ResRoomCreate(session, df_RESULT_ROOM_CREATE_ETC);
		return true;
	}

	if (CheckTitle(szRoomTitle))
	{
		NetWork_ResRoomCreate(session, df_RESULT_ROOM_CREATE_DNICK);
		return true;
	}

	pRoom = new ROOM;

	pRoom->ID = g_iRoomID++;
	wcscpy_s(pRoom->Title, szRoomTitle);
	
	g_Room_List.insert(std::make_pair(pRoom->ID, pRoom));

	// 방생성 로그
	wprintf(L" 방 생성. UserID : %d, RoomTitle : %s, RoomID : %d, RoomCount : %d\n", session->ID, pRoom->Title, pRoom->ID, g_Room_List.size());

	NetWork_ResRoomCreate(session, df_RESULT_ROOM_CREATE_OK, pRoom);
	return true;
}

bool NetWork_ReqRoomEnter(SESSION* session, CMessage* message)
{
	ROOM* pRoom = nullptr;
	DWORD RoomID = 0;
	(*message) >> RoomID;

	SESSION* pClient = FindSession(session->ID);
	if (pClient == nullptr)
		Disconnect(session->ID);
	else
	{
		pRoom = FindRoom(RoomID);
		if (pRoom == nullptr)
		{
			// 방 ID오류
			NetWork_ResRoomEnter(session, df_RESULT_ROOM_ENTER_NOT);
			return true;
		}
	}
	// 입장한 클라이언트에게 결과를 전송하기 전에 기존에 방에 있던 클라이언트들에게 새로운 클라이언트의 접속을 알린다.
	for (std::map<int, SESSION*>::iterator itor = pRoom->userList.begin(); itor != pRoom->userList.end(); itor++)
	{
		NetWork_ResRoomOtherClientEnter(itor->second, df_RES_USER_ENTER, pClient);
	}
	// 방입장 로그
	wprintf(L" 방 입장. UserID : %d, RoomTitle : %s, RoomID : %d, UserCount : %d\n", session->ID, pRoom->Title, pRoom->ID, pRoom->userList.size());
	NetWork_ResRoomEnter(session, df_RESULT_ROOM_ENTER_OK, pRoom);
	pRoom->userList.insert(std::make_pair(session->ID, session));
	pClient->RoomID = RoomID;
	NetWork_ResRoomOtherClientEnter(session, df_RES_USER_ENTER, session);
	return true;
}

bool NetWork_ReqChatting(SESSION* session, CMessage* message)
{
	ROOM* pRoom = FindRoom(session->RoomID);
	// 입장한 클라이언트에게 결과를 전송하기 전에 기존에 방에 있던 클라이언트들에게 새로운 클라이언트의 접속을 알린다.
	for (std::map<int, SESSION*>::iterator itor = pRoom->userList.begin(); itor != pRoom->userList.end(); itor++)
	{
		if (itor->first == session->ID)
			continue;
		else
			NetWork_ResRoomChatting(itor->second, message);
	}
	return true;
}

void NetWork_ResRoomList(SESSION* session)
{
	CMessage packet;
	st_PACKET_HEADER header;

	MakePacket_ResRoomList(&header, &packet);

	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResRoomCreate(SESSION* session, BYTE result, ROOM* room)
{
	CMessage packet;
	st_PACKET_HEADER header;

	MakePacket_ResRoomCreate(&header, &packet, result, room);

	if (result == df_RESULT_ROOM_CREATE_OK)
	{
		SendPacket_Broadcast(&header, &packet);
	}
	else
	{
		SendPacket_Unicast(session, &header, &packet);
	}
}

void NetWork_ResRoomEnter(SESSION* session, BYTE result, ROOM* room)
{
	st_PACKET_HEADER header;
	CMessage packet;

	MakePacket_ResRoomEnter(&header, &packet, result, room);
	
	if (result == df_RESULT_ROOM_ENTER_OK)
	{
		SendPacket_Unicast(session, &header, &packet);
	}
}

void NetWork_ResRoomOtherClientEnter(SESSION* session, BYTE result, SESSION* enteredSession)
{
	CMessage packet;
	st_PACKET_HEADER header;
	MakePacket_ResRoomOtherClientEnter(&header, &packet, enteredSession);

	SendPacket_Unicast(session, &header, &packet);
}

void NetWork_ResRoomChatting(SESSION* session, CMessage* message)
{
	CMessage packet;
	st_PACKET_HEADER header;
	WORD textSize = 0;

	MakePacket_ResChatting(&header, &packet, session, message);
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

void MakePacket_ResRoomCreate(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, ROOM* pRoom)
{
	WORD wTitleSize;

	message->Clear();

	if(result == df_RESULT_ROOM_CREATE_OK)
	{
		(*message) << result;
		(*message) << pRoom->ID;
		wTitleSize = wcslen(pRoom->Title) * sizeof(WCHAR);
		(*message) << wTitleSize;
		message->PutData((char*)pRoom->Title, wTitleSize);
	}
	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_ROOM_CREATE);
	pHeader->wMsgType = df_RES_ROOM_CREATE;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResRoomEnter(st_PACKET_HEADER* pHeader, CMessage* message, BYTE result, ROOM* pRoom)
{
	WORD wTitleSize;
	message->Clear();
	(*message) << result;

	if (result == df_RESULT_ROOM_ENTER_OK)
	{
		(*message) << pRoom->ID;
		wTitleSize = wcslen(pRoom->Title) * sizeof(WCHAR);
		(*message) << wTitleSize;
		message->PutData((char*)pRoom->Title, wTitleSize);
		(*message) << (BYTE)pRoom->userList.size();
		for (std::map<int, SESSION*>::iterator itor = pRoom->userList.begin(); itor != pRoom->userList.end(); itor++)
		{
			message->PutData((char*)itor->second->NickName, sizeof(WCHAR) * 15);
			(*message) << itor->second->ID;
		}
	}

	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_ROOM_ENTER);
	pHeader->wMsgType = df_RES_ROOM_ENTER;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResRoomOtherClientEnter(st_PACKET_HEADER* pHeader, CMessage* message, SESSION* session)
{
	message->PutData((char*)session->NickName, sizeof(WCHAR) * 15);
	(*message) << session->ID;

	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_USER_ENTER);
	pHeader->wMsgType = df_RES_USER_ENTER;
	pHeader->wPayloadSize = message->GetDataSize();
}

void MakePacket_ResChatting(st_PACKET_HEADER* pHeader, CMessage* message, SESSION* session, CMessage* text)
{
	WORD szTextSize = 0;
	(*text) >> szTextSize;
	
	
	(*message) << session->ID;

	(*message) << szTextSize;

	message->PutData((char*)(text->GetFront()), szTextSize);

	pHeader->byCode = dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(message, df_RES_CHAT);
	pHeader->wMsgType = df_RES_CHAT;
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

bool CheckTitle(WCHAR* title)
{
	for (std::map<int, ROOM*>::iterator itor = g_Room_List.begin(); itor != g_Room_List.end(); itor++)
	{
		if (wcscmp(title, itor->second->Title) == 0)
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

void SendPacket_Broadcast(st_PACKET_HEADER* pHeader, CMessage* message)
{
	SESSION* temp = nullptr;
	for (std::map<int, SESSION*>::iterator itor = g_Session_List.begin(); itor != g_Session_List.end(); itor++)
	{
		temp = itor->second;
		SendPacket_Unicast(temp, pHeader, message);
	}
}

SESSION* FindSession(DWORD UserID)
{
	SESSION* pSession = nullptr;
	pSession = g_Session_List.find(UserID)->second;
	return pSession;
}

ROOM* FindRoom(DWORD RoomID)
{
	ROOM* pRoom = nullptr;
	pRoom = g_Room_List.find(RoomID)->second;
	return pRoom;
}

