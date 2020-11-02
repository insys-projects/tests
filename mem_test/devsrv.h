//=************************* CDeviceSrv  - the Service of Device class *************************

#ifndef _DEVSRV_H_
#define _DEVSRV_H_

#include	"dev.h"
#include	"gipcy.h"
#include	"ctrlsdram.h"
#include	"ctrlstrm.h"

#define		MAX_NAME	32		// считаем, что имя устройства/ПЛИС/службы может быть не более 32 символов 
#define		MAX_CHAN	32		// считаем, что каналов может быть не больше MAX_CHAN

#include <math.h>
#define ROUND(x) ((int)floor((x) + 0.5))

// Usage memory on board
enum
{
	MEM_NOT_USE = 0,			// память на модуле не используется
	MEM_LIKE_MEM = 1,			// сбор в память на модуле
	MEM_LIKE_FIFO = 2			// память на модуле используется в качестве FIFO
};

// Memory type for stream
enum
{
	DLL_USER_MEMORY = 0,		// для стрима выделяется пользовательская память (в библиотеке BARDY)
	SYSTEM_MEMORY = 1,			// для стрима выделяется системная память (на нулевом кольце)
	APP_USER_MEMORY = 2			// для стрима выделяется пользовательская память (в пользовательском приложении)
};

// Start mode for stream
enum
{
	ONE_TIME_STREAM = 0,			// однократный старт стрима
	AUTOINIT_STREAM = 1,			// старт стрима в режиме автоинициализации (непрерывный)
};

//#pragma pack(push,1)
//
//typedef struct _THREAD_PARAM {
//	BRD_Handle hSrv;
//	HANDLE hUserEvent;
//	U32 tout;
//	S32* pevtstat;
//	HANDLE hSyncEvent;
//} THREAD_PARAM, *PTHREAD_PARAM;
//
//#pragma pack(pop)

class CDeviceSrv : public CDevice
{
protected:

	BRD_Handle m_hSrv;
	BRDCHAR m_srvName[MAX_NAME];

	BRDctrl_StreamCBufAlloc m_buf_dscr{};
	unsigned long long m_activeMemSize;
	int m_useMemOnBoard;
	U32 m_extIrqMode;

	S32 allocbuf(unsigned long long* pbytesBufSize, U32 dir, U32 bBlkSize = 0, U32 blkNum = 0);
	S32 freebuf();

	//THREAD_PARAM m_thread_param;
	//HANDLE m_hThread;
	//HANDLE m_hUserEvent;
	S32 m_eventStatus;

public:

	CDeviceSrv(int idev, string srv_name);
	CDeviceSrv(int idev, BRDCHAR* srv_name);
	~CDeviceSrv();
	//static CDeviceSrv* Create(int idev, string adcsrv);

	int getPhysMem(U64* pPhysMemSize);

	inline unsigned long long getActiveMemSize() { return m_activeMemSize; };
	inline unsigned int getStreamBlocks() { return m_buf_dscr.blkNum; };
	inline unsigned int getStreamSize() { return m_buf_dscr.blkSize; };
	inline void* getStreamBuf() { return m_buf_dscr.ppBlk[0]; };
	inline void* getStreamBlock(int num) { return m_buf_dscr.ppBlk[num]; };

	inline int getStreamLastBlock();
	inline unsigned int getStreamIrqNum();
	inline unsigned int getStreamTotalCounter();

	S32 prepareStream(int useMemOnBoard);
	S32 dataXmem(U32 mstimeout);

	//S32 checkThread();
	//void breakThread();
	//S32 endThread();

	S32 startStream(int mode);
	S32 stopStream();

	U32 setExtIrqMode();
};

inline int CDeviceSrv::getStreamLastBlock()
{
	return m_buf_dscr.pStub->lastBlock;
}

inline unsigned int CDeviceSrv::getStreamIrqNum()
{
	if (m_extIrqMode)
		return (unsigned int)m_buf_dscr.pStub->lastBlock;
	else
		return 0xFFFFFFFF;
}

inline unsigned int CDeviceSrv::getStreamTotalCounter()
{
	return m_buf_dscr.pStub->totalCounter;
}

#endif	// _DEVSRV_H_
