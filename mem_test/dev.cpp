
#include	"dev.h"

static int g_openDeviceCounter = 0;
static int g_flagOfInit = 0;
static S32 g_numberOfDevices = 0;
static int g_flagAutoInit = 0;

S32 CDevice::Init(int mode, BRDCHAR* inifile)
{
	S32		status;

	//BRD_displayMode(BRDdm_VISIBLE | BRDdm_CONSOLE); // режим вывода информационных сообщений : отображать все уровни на консоле

	if (!g_flagOfInit)
	{
		if (inifile)
			if(mode)
				status = BRD_initEx(BRDinit_AUTOINIT, inifile, NULL, &g_numberOfDevices); // инициализировать библиотеку - автоинициализация
			else
				status = BRD_init(inifile, &g_numberOfDevices); // инициализировать библиотеку
		else
		{
			BRDCHAR ini_file[] = _BRDC("brd.ini");
			if (mode)
				status = BRD_initEx(BRDinit_AUTOINIT, inifile, NULL, &g_numberOfDevices); // инициализировать библиотеку - автоинициализация
			else
			//{
			//	status = BRD_init(inifile, &g_numberOfDevices); // инициализировать библиотеку
			//	status = BRD_init(inifile, &g_numberOfDevices); // инициализировать библиотеку
			//	//status = BRD_initEx(BRDinit_STRING, _BRDC("[LID:31]\n type = bambpex\n pid = 0\n #begin SUBUNIT\n type = mfm816x250mi\n #end\n"), NULL, &g_numberOfDevices);
			//	//status = BRD_initEx(BRDinit_STRING, _BRDC("[LID:31]\n type = bambpex\n pid = 0\n #begin SUBUNIT\n type = mfm816x250mi\n #end\n"), NULL, &g_numberOfDevices);
			//}
				status = BRD_init(inifile, &g_numberOfDevices); // инициализировать библиотеку
		}

		if (BRD_errcmp(status, BRDerr_NONE_DEVICE))
			return 0;

		if (!BRD_errcmp(status, BRDerr_OK))
			return status;

		g_flagOfInit = 1;
		if(mode)
			g_flagAutoInit = 1;
	}

	return g_numberOfDevices;
}

S32 CDevice::Cleanup()
{
	S32		status;
	if (g_openDeviceCounter)
	{
		status = -1;
	}
	else
		if (g_flagOfInit)
		{
			status = BRD_cleanup();
			g_openDeviceCounter = 0;
			g_flagOfInit = 0;
			g_flagAutoInit = 0;
			g_numberOfDevices = 0;
		}
	return status;
}

//=***************************************************************************************
// получить номер устройства по номеру LID (для его открытия - для передачи конструктору класса)
int CDevice::getIndexFromLid(int iLid)
{
	int		status;
	BRD_LidList lidList;

	// получить список LID (каждая запись соответствует устройству)
	lidList.item = g_numberOfDevices;
	lidList.pLID = new U32[g_numberOfDevices];
	//BRD_shell(BRDshl_LID_LIST, &lidList);
	status = BRD_lidList(lidList.pLID, lidList.item, &lidList.itemReal);

	int idx = 0;
	for (idx = 0; idx < (int)lidList.itemReal; idx++)
	{
		if (lidList.pLID[idx] == iLid)
			break;
	}
	if (idx == lidList.itemReal)
		idx = -1;
	return idx;
}

// конструктор
// iDev - (IN) номер устройства для открытия
CDevice::CDevice(int iDev)
{
	S32		status;

	//S32 num_dev = 0;

	m_hDevice = 0;
	m_valid = 0;  // module is NOT opened

	BRD_LidList lidList;

	if (g_flagAutoInit)
	{
		m_hDevice = BRD_open(iDev + 1, BRDopen_SHARED, NULL); // открыть устройство в разделяемом режиме
	}
	else
	{
		// получить список LID (каждая запись соответствует устройству)
		lidList.item = g_numberOfDevices;
		lidList.pLID = new U32[g_numberOfDevices];
		//BRD_shell(BRDshl_LID_LIST, &lidList);
		status = BRD_lidList(lidList.pLID, lidList.item, &lidList.itemReal);


		m_hDevice = BRD_open(lidList.pLID[iDev], BRDopen_SHARED, NULL); // открыть устройство в разделяемом режиме

	}
	if (m_hDevice <= 0)
		return;
	m_valid = 1; // module is opened
	g_openDeviceCounter++;

	// получить информацию об открытом устройстве
	m_devInfo.size = sizeof(m_devInfo);
	if (g_flagAutoInit)
		getInfo(iDev + 1);
	else
	{
		getInfo(lidList.pLID[iDev]);
		delete lidList.pLID;
	}

}

CDevice::~CDevice()
{
	S32		status;
	if (m_hDevice)
	{
		status = BRD_close(m_hDevice); // закрыть устройство 
		g_openDeviceCounter--;
	}
}

S32 CDevice::getValid()
{
	return m_valid;
}

#include <algorithm>
//=***************************************************************************************
bool CDevice::isService(string srvName)
{
	//int flag_found = 0;
	//size_t nsrv = m_servName.size();
	transform(srvName.begin(), srvName.end(), srvName.begin(), ::tolower);
	for (auto &name : m_servName)
		if (name == srvName)
			return true;
	return false;
}

// вспомогательная функция для getInfo
//=***************************************************************************************
void submodName(U32 id, BRDCHAR * str)
//void CDevice::submodName(U32 id, BRDCHAR * str)
{
	switch (id)
	{
	case 0x1010:    BRDC_strcpy(str, _BRDC("FM814x125M")); break;
	case 0x1012:    BRDC_strcpy(str, _BRDC("FM814x250M")); break;
	case 0x1014:    BRDC_strcpy(str, _BRDC("FM814x600M")); break;
	case 0x1020:    BRDC_strcpy(str, _BRDC("FM214x250M")); break;
	case 0x1030:    BRDC_strcpy(str, _BRDC("FM412x500M")); break;
	case 0x1040:    BRDC_strcpy(str, _BRDC("FM212x1G")); break;
	case 0x1050:    BRDC_strcpy(str, _BRDC("FM816x250M")); break;
	case 0x1051:    BRDC_strcpy(str, _BRDC("FM416x250M")); break;
	case 0x1052:    BRDC_strcpy(str, _BRDC("FM216x250MDA")); break;
	case 0x1053:    BRDC_strcpy(str, _BRDC("FM816x250M1")); break;
	case 0x1061:    BRDC_strcpy(str, _BRDC("FM216x100MRF")); break;
	case 0x10B0:    BRDC_strcpy(str, _BRDC("FM416x500MD")); break;
	case 0x10C0:    BRDC_strcpy(str, _BRDC("FM212x4GDA")); break;
	case 0x10C8:    BRDC_strcpy(str, _BRDC("FM214x1GTRF")); break;
	case 0x10D0:    BRDC_strcpy(str, _BRDC("FM112x2G6DA")); break;
	case 0x10D2:    BRDC_strcpy(str, _BRDC("FM214x3GDA")); break;
	case 0x10D4:    BRDC_strcpy(str, _BRDC("FM414x3G")); break;
	default: BRDC_strcpy(str, _BRDC("UNKNOW")); break;
	}
}

//=***************************************************************************************
// получить основную информацию об устройстве (без его открытия - без вызова конструктора класса)
int CDevice::getMainInfo(int iDev, DEV_MAININFO *minfo)
{
//	S32		status;

	if (iDev >= g_numberOfDevices)
		return -1;

	BRD_Info	info;
	info.size = sizeof(info);

	BRD_getInfo(iDev+1, &info); // получить информацию об устройстве
	minfo->devID = info.boardType >> 16;
	minfo->rev = info.boardType & 0xff;
	//BRDC_strcpy(minfo->devName, info.name);
	minfo->pid = info.pid & 0xfffffff;
	minfo->bus = info.bus;
	minfo->dev = info.dev;
	minfo->slot = info.slot + (info.pid >> 28);
	//minfo->subType = info.subunitType[0];

#ifdef _WIN32 //__IPC_WIN__
	char dev_name[MAX_NAME];
	BRDC_bcstombs(dev_name, info.name, MAX_NAME);
	minfo->devName = dev_name;
#else
	minfo->devName = info.name;
#endif

	BRDCHAR subName[MAX_NAME];
	//BRDC_strcpy(m_devInfo.subName, _BRDC("NONE"));
	if (info.subunitType[0] != 0xffff)
	{
		submodName(info.subunitType[0], subName);
#ifdef _WIN32 //__IPC_WIN__
		BRDC_bcstombs(dev_name, subName, MAX_NAME);
		minfo->subName = dev_name;
#else
		minfo->subName = subName;
#endif
		//	BRDC_strcpy(m_devInfo.subName, subName);
	}
	return 0;
}

// получить информацию об устройстве
// iLid - (IN) номер LID c описанием нужного устройства
int CDevice::getInfo(int iLid)
{
	S32		status;

	BRD_Info	info;
	info.size = sizeof(info);

	BRD_getInfo(iLid, &info); // получить информацию об устройстве
	m_devInfo.devID = info.boardType >> 16;
	m_devInfo.rev = info.boardType & 0xff;
	BRDC_strcpy(m_devInfo.devName, info.name);
	m_devInfo.pid = info.pid & 0xfffffff;
	m_devInfo.bus = info.bus;
	m_devInfo.dev = info.dev;
	m_devInfo.slot = info.slot + (info.pid >> 28);
	m_devInfo.subType = info.subunitType[0];

#ifdef _WIN32 //__IPC_WIN__
	char dev_name[MAX_NAME];
	BRDC_bcstombs(dev_name, info.name, MAX_NAME);
	m_devName = dev_name;
#else
	m_devName = info.name;
#endif

	BRDCHAR subName[MAX_NAME];
	BRDC_strcpy(m_devInfo.subName, _BRDC("NONE"));
	if (info.subunitType[0] != 0xffff)
	{
		submodName(info.subunitType[0], subName);
		BRDC_strcpy(m_devInfo.subName, subName);
	}

	BRDC_strcpy(m_devInfo.pldName, _BRDC("ADM PLD"));
	U32 ItemReal;
	BRD_PuList PuList[MAX_PU];
	status = BRD_puList(m_hDevice, PuList, MAX_PU, &ItemReal); // получить список программируемых устройств
	if (ItemReal <= MAX_PU)
	{
		for (U32 j = 0; j < ItemReal; j++)
		{
			U32	PldState;
			status = BRD_puState(m_hDevice, PuList[j].puId, &PldState); // получить состояние ПЛИС
			if (PuList[j].puId == 0x100 && PldState)
			{// если это ПЛИС ADM и она загружена
				BRDC_strcpy(m_devInfo.pldName, PuList[j].puDescription);
				m_devInfo.pldVer = 0xFFFF;
				BRDextn_PLDINFO pld_info;
				pld_info.pldId = 0x100;
				status = BRD_extension(m_hDevice, 0, BRDextn_GET_PLDINFO, &pld_info);
				if (BRD_errcmp(status, BRDerr_OK))
				{
					m_devInfo.pldVer = pld_info.version;
					m_devInfo.pldMod = pld_info.modification;
					m_devInfo.pldBuild = pld_info.build;
				}
			}
			if (PuList[j].puId == 0x03)
			{// если это ICR субмодуля
				if (PldState)
				{ // и оно прописано данными
					char subICR[14];
					status = BRD_puRead(m_hDevice, PuList[j].puId, 0, subICR, 14);
					U16 tagICR = *(U16*)(subICR);
					m_devInfo.subPID = *(U32*)(subICR + 7); // серийный номер субмодуля
					m_devInfo.subType = *(U16*)(subICR + 11);  // тип субмодуля
					m_devInfo.subVer = *(U08*)(subICR + 13);   // версия субмодуля
					submodName(m_devInfo.subType, subName);
					BRDC_strcpy(m_devInfo.subName, subName);
					//submodMonInfo(m_devInfo.subType);
				}
			}
		}
	}

	// получаем состояние FMC-питания (если не FMC-модуль, то ошибка)
	m_devInfo.pwrOn = 0xFF;
	BRDextn_FMCPOWER power;
	power.slot = 0;
	status = BRD_extension(m_hDevice, 0, BRDextn_GET_FMCPOWER, &power);
	if (BRD_errcmp(status, BRDerr_OK))
	{
		m_devInfo.pwrOn = power.onOff;
		m_devInfo.pwrValue = power.value;
	}

	// получить список служб
	BRD_ServList srvList[MAX_SRV];
	status = BRD_serviceList(m_hDevice, 0, srvList, MAX_SRV, &ItemReal);
	if (ItemReal <= MAX_SRV)
	{
		U32 j = 0;
		for (j = 0; j < ItemReal; j++)
		{
			BRDC_strcpy(m_devInfo.srvName[j], srvList[j].name);
#ifdef _WIN32 //__IPC_WIN__
			char srv_name[MAX_NAME];
			BRDC_bcstombs(srv_name, srvList[j].name, MAX_NAME);
			string name = srv_name;
#else
			string name = srvList[j].name;
#endif
			transform(name.begin(), name.end(), name.begin(), ::tolower);
			m_servName.push_back(name);
		}
		BRDC_strcpy(m_devInfo.srvName[j], _BRDC(""));
	}
	else
	{
		//BRDC_printf(_BRDC("BRD_serviceList: Real Items = %d (> 16 - ERROR!!!)\n"), ItemReal);
		return -1; // too much services
	}

	return 0;
}

// modPU = 0 - base module, 1 - submodule 0, 2 - submodule 1
int CDevice::readICR(int modPU, void *pCfgBuffer, U32 size)
{
	S32		status;

	//BRD_PuList PuListTmp[1];
	//U32 ItemReal;
	//BRD_puList(m_hDevice, PuListTmp, 1, &ItemReal);
	//BRD_PuList	*PuList = new BRD_PuList[ItemReal];
	//U32	ItemsCnt = ItemReal;
	//BRD_puList(m_hDevice, PuList, ItemsCnt, &ItemReal);

	U32 puID =  BRDpui_BASEICR;
	if (modPU)
	{
		puID = BRDpui_SUBICR + (modPU - 1);
	}

	status = BRD_puRead(m_hDevice, puID, 0, pCfgBuffer, size);

	return status;
//	delete[] PuList;

}

// modPU = 0 - base module, 1 - submodule 0, 2 - submodule 1
int CDevice::writeICR(int modPU, void *pCfgBuffer, U32 size)
{
	S32		status;

	//BRD_PuList PuListTmp[1];
	//U32 ItemReal;
	//BRD_puList(m_hDevice, PuListTmp, 1, &ItemReal);
	//BRD_PuList	*PuList = new BRD_PuList[ItemReal];
	//U32	ItemsCnt = ItemReal;
	//BRD_puList(m_hDevice, PuList, ItemsCnt, &ItemReal);

	U32 puID = BRDpui_BASEICR;
	if (modPU)
	{
		puID = BRDpui_SUBICR + (modPU - 1);
	}

	BRD_puEnable(m_hDevice, puID);
	status = BRD_puWrite(m_hDevice, puID, 0, pCfgBuffer, size);

	return status;
	//	delete[] PuList;

}
