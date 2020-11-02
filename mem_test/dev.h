//=************************* CDevice  - the base class *************************

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include	"brd.h"
#include	"brdpu.h"
//#include	"gipcy.h"
#include	"extn.h"

#include	<string>
#include	<vector>
using namespace std;

#define		MAX_NAME	32		// считаем, что имя устройства/ПЛИС/службы может быть не более 32 символов 
#define		MAX_SRV		16		// считаем, что служб на одном модуле может быть не больше MAX_SRV
//#define		MAX_DEV		12		// считаем, что модулей может быть не больше MAX_DEV
#define		MAX_PU		8		// считаем, что PU-устройств (ПЛИС, ППЗУ) на одном модуле может быть не больше MAX_PU

#pragma pack(push,1)

// информация об устройстве
typedef struct
{
	U32			size;			// sizeof(DEV_INFO)
	U16			devID;			// Device ID
	U16			rev;			// Revision
	BRDCHAR		devName[MAX_NAME];	// Device Name
	U32			pid;			// Board Physical ID
	S32			bus;			// Bus Number
	S32			dev;			// Dev Number
	S32			slot;			// Slot Number
	U08			pwrOn;			// FMC power on/off
	U32			pwrValue;		// FMC power value
	U16			pldVer;			// ADM PLD version
	U16			pldMod;			// ADM PLD modification
	U16			pldBuild;		// ADM PLD build
	BRDCHAR		pldName[MAX_NAME];	// ADM PLD Name
	U16			subType;		// Subunit Type Code
	U16			subVer;			// Subunit version
	U32			subPID;			// Subunit serial number
	BRDCHAR		subName[MAX_NAME];	// Submodule Name
	BRDCHAR		srvName[MAX_NAME][MAX_SRV];	// massive of Service Names
} DEV_INFO, *PDEV_INFO;

// основная информация об устройстве
typedef struct
{
	U16			devID;			// Device ID
	U16			rev;			// Revision
	string		devName;		// Device Name
	U32			pid;			// Board Physical ID
	S32			bus;			// Bus Number
	S32			dev;			// Dev Number
	S32			slot;			// Slot Number
	string		subName;		// Submodule Name
	//BRDCHAR		subName[MAX_NAME];	// Submodule Name
} DEV_MAININFO, *PDEV_MAININFO;

#pragma pack(pop)

class CDevice
{
	string m_devName;

	//void submodName(U32 id, BRDCHAR * str);
	int getInfo(int iLid);

	vector <string> m_servName; // названия служб
	
protected:
	BRD_Handle m_hDevice;
	int m_valid;

public:
	DEV_INFO		m_devInfo;

	static S32 Init(int mode = 1, BRDCHAR* inifile = NULL); // mode = 1 - BRDinit_AUTOINIT 
	static S32 Cleanup();
	static int getMainInfo(int iDev, DEV_MAININFO *minfo);
	static int getIndexFromLid(int iLid);

	CDevice(int iDev);
	~CDevice();

	S32 getValid();
	bool isService(string srvName);
	int readICR(int modPU, void *pCfgBuffer, U32 size);
	int writeICR(int modPU, void *pCfgBuffer, U32 size);

	inline string getDeviceName() { return m_devName; };

};

#endif	// _DEVICE_H_
