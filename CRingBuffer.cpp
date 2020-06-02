#include "stdafx.h"
#include "CRingBuffer.h"


CRingBuffer::CRingBuffer()
{
	_iBufferSize = BUFSIZE;
	_iFront = _iRear = _iUsingSize = 0;
	_cBufferArray = (char*)malloc(BUFSIZE);
}

CRingBuffer::~CRingBuffer(void)
{
	free(_cBufferArray);
}

int CRingBuffer::GetUsingSize(void)
{
	return _iUsingSize;
}

int CRingBuffer::GetFreeSize(void)
{
	return _iBufferSize - _iUsingSize;
}

int CRingBuffer::Enqueue(char* data, int size)
{
	// 가득 차있으면 0리턴
	if (GetFreeSize() == 0)
		return 0;
	// 실제 삽입 가능한 크기 확인
	int realsize = GetFreeSize() - size;

	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realsize < 0)
	{
		realsize = GetFreeSize();
	}
	// 남은 공간이 삽입하고자 하는 크기보다 더 크면 삽입하고자 하는 크기만큼 삽입
	else
	{
		realsize = size;
	}

	// Rear의 위치와 배열의 끝까지의 길이 확인
	int rearTomax = _iBufferSize - _iRear;
	char* ptr = _cBufferArray;
	ptr += _iRear;

	// 배열 끝까지 남은 길이가 삽입하고자 하는 크기보다 길거나 같으면
	if (rearTomax > realsize)
	{
		memcpy(ptr, data, realsize);
		_iRear += realsize;
	}
	else if (rearTomax == realsize)
	{
		memcpy(ptr, data, rearTomax);
		_iRear += realsize;
	}
	else
	{
		memcpy(ptr, data, rearTomax);
		data += rearTomax;
		size -= rearTomax;
		memcpy(_cBufferArray, data, size);
		_iRear = 0;
		_iRear += realsize - rearTomax;
	}

	_iUsingSize += realsize;

	if (_iRear == _iBufferSize)
		_iRear = 0;

	return realsize;
}

int CRingBuffer::Dequeue(char* dest, int size)
{
	// 가득 차있으면 0리턴
	if (GetUsingSize() == 0)
		return 0;
	// 실제 삭제 가능한 크기 확인
	int realsize = GetUsingSize() - size;

	// 저장되있는 데이터 공간보다 삭제하고자 하는 크기가 더 크면 남아있는 크기만큼 삭제
	if (realsize < 0)
	{
		realsize = GetUsingSize();
	}
	// 남은 공간이 삭제하고자 하는 크기보다 더 크면 삭제하고자 하는 크기만큼 삭제
	else
	{
		realsize = size;
	}

	// Front의 위치와 배열의 끝까지의 길이 확인
	int frontTomax = _iBufferSize - _iFront;
	char* ptr = _cBufferArray;
	ptr += _iFront;

	if (frontTomax > realsize)
	{
		memcpy(dest, ptr, realsize);
		_iFront += realsize;
	}
	else if (frontTomax == realsize)
	{
		memcpy(dest, ptr, realsize);
		_iFront += realsize;
	}
	else
	{
		memcpy(dest, ptr, frontTomax);
		dest += frontTomax;
		size -= frontTomax;
		memcpy(dest, _cBufferArray, size);
		_iFront = 0;
		_iFront += realsize - frontTomax;
	}

	_iUsingSize -= realsize;

	if (_iFront == _iBufferSize)
		_iFront = 0;

	return realsize;
}

int CRingBuffer::DirectEnqueueSize(void)
{
	return _iBufferSize - _iRear;
}

int CRingBuffer::DirectDequeueSize(void)
{
	return _iBufferSize - _iFront;
}

int CRingBuffer::Peek(char* dest, int size)
{
	// 가득 차있으면 0리턴
	if (GetUsingSize() == 0)
		return 0;
	// 실제 삭제 가능한 크기 확인
	int realsize = GetUsingSize() - size;

	// 남은 공간보다 삭제하고자 하는 크기가 더 크면 남아있는 크기만큼 삭제
	if (realsize < 0)
	{
		realsize = GetUsingSize();
	}
	// 남은 공간이 삭제하고자 하는 크기보다 더 크면 삭제하고자 하는 크기만큼 삭제
	else
	{
		realsize = size;
	}

	// Front의 위치와 배열의 끝까지의 길이 확인
	int frontTomax = _iBufferSize - _iFront;
	char* ptr = _cBufferArray;
	ptr += _iFront;

	if (frontTomax > realsize)
	{
		memcpy(dest, ptr, realsize);
	}
	else if (frontTomax == realsize)
	{
		memcpy(dest, ptr, realsize);
	}
	else
	{
		memcpy(dest, ptr, frontTomax);
		dest += frontTomax;
		size -= frontTomax;
		memcpy(dest, _cBufferArray, size);
	}


	return realsize;
}

void CRingBuffer::ClearBuffer(void)
{
	_iUsingSize = _iRear = _iFront = 0;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	char* ptr = _cBufferArray;
	ptr += _iFront;

	return ptr;
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	char* ptr = _cBufferArray;
	ptr += _iRear;

	return ptr;
}

void CRingBuffer::MoveFront(int size)
{
	// 가득 차있으면 0리턴
	if (GetUsingSize() == 0)
		return ;
	// 실제 삭제 가능한 크기 확인
	int realsize = GetUsingSize() - size;

	// 남은 공간보다 삭제하고자 하는 크기가 더 크면 남아있는 크기만큼 삭제
	if (realsize < 0)
	{
		realsize = GetUsingSize();
	}
	// 남은 공간이 삭제하고자 하는 크기보다 더 크면 삭제하고자 하는 크기만큼 삭제
	else
	{
		realsize = size;
	}
	// Front의 위치와 배열의 끝까지의 길이 확인
	int frontTomax = _iBufferSize - _iFront;

	if (frontTomax > realsize)
	{
		_iFront += size;
	}
	else if (frontTomax == realsize)
	{
		_iFront = 0;
	}
	else
	{
		int temp = size;
		temp = temp - frontTomax;
		_iFront = 0;
		_iFront += temp;
	}

	_iUsingSize -= size;
	//
	//if (_iUsingSize <= 0)
	//{
	//	ClearBuffer();
	//}
}

void CRingBuffer::MoveRear(int size)
{
	// Rear의 위치와 배열의 끝까지의 길이 확인
	int rearTomax = _iBufferSize - _iRear;
	if (rearTomax > size)
	{
		_iRear += size;
	}
	else if (rearTomax == size)
	{
		_iRear = 0;
	}
	else
	{
		int temp = size;
		temp = temp - rearTomax;
		_iRear = 0;
		_iRear += temp;
	}
	
	_iUsingSize += size;

	//if (_iUsingSize >= _iBufferSize)
	//{
	//	_iUsingSize = _iBufferSize;
	//}
}
