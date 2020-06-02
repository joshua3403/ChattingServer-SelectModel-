#pragma once
#include "stdafx.h"


#define BUFSIZE 1000


class CRingBuffer
{
private:
	int _iBufferSize;
	int _iFront;
	int _iRear;
	int _iUsingSize;
	char* _cBufferArray;

public:
	CRingBuffer();
	
	~CRingBuffer(void);

	// 사용중인 용량 반환
	int GetUsingSize(void);

	// 남은 용량 반환
	int GetFreeSize(void);

	// 버퍼에 데이터 입력
	int Enqueue(char* data, int size);

	// 버퍼에 데이터 삭제
	int Dequeue(char* dest, int size);

	/////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
	// (끊기지 않은 길이)
	//
	// 원형 큐의 구조상 버퍼의 끝단에 있는 데이터는 끝 -> 처음으로 돌아가서
	// 2번에 데이터를 얻거나 넣을 수 있음. 이 부분에서 끊어지지 않은 길이를 의미
	int DirectEnqueueSize(void);

	int DirectDequeueSize(void);
	
	// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
	int Peek(char* chpDest, int iSize);

	// 버퍼의 모든 데이타 삭제.
	void ClearBuffer(void);

	// 버퍼의 Front 포인터 얻음.
	char* GetFrontBufferPtr(void);

	// 버퍼의 RearPos 포인터 얻음.
	char* GetRearBufferPtr(void);


	// Front를 이동
	void MoveFront(int size);

	// Rear를 이동
	void MoveRear(int size);
};