//=************************* CMem  - the BASESDRAM service class *************************

#ifndef _MEM_H_
#define _MEM_H_

#include	"devsrv.h"
//#include	"dev.h"
//#include	"brd.h"
//#include	"gipcy.h"
//#include	"ctrladc.h"
//#include	"ctrlsdram.h"
//#include	"ctrlstrm.h"

// Usage memory on board
enum
{
	MEM_TEST_OFF = 0,			// 
	MEM_TEST_PSD = 0x100,		// 
	MEM_TEST_CNT = 0x500		// 
};

class CMem : public 	CDeviceSrv
{

	BRD_SdramCfgEx m_cfg;
	//BRD_AdcCfg m_cfg;
	//ADC_PARAM m_param;

protected:

public:

	CMem(int idev, string memsrv);
	CMem(int idev, BRDCHAR* memsrv);
	~CMem();

	static CMem* Create(int idev, string adcsrv);

	inline void getCfg(BRD_SdramCfgEx* pCfg) { *pCfg = m_cfg; };

	int setTestMode(int memTestMode);
	//int setAllParam(int iDev, BRDCHAR* inifile, ADC_PARAM* adcpar, int memflag);
	//inline void getParam(ADC_PARAM* adcpar) { *adcpar = m_param; };

	int setDaqParam(int useMemOnBoard, int streamMemoryType, unsigned long long* samplesOfChannel);
	int setDaqParam(int useMemOnBoard, int streamMemoryType, U32 streamBlockSize, U32* pStreamBlockNum);

	S32 startDaqMem(U32 mstimeout, HANDLE hSyncEvent);
	S32 doDaqMem(U32 mstimeout);

	int prepareStart();
	int enableMem();
	int disableMem();
	int isOverflow();
	int statusFIFO(ULONG* state);
	int waitStreamBlock(U32 msTimeout);
	int waitStreamBuffer(U32 msTimeout);
};

#endif	// _MEM_H_
