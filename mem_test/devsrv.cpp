
#include	"devsrv.h"

CDeviceSrv::CDeviceSrv(int idev, BRDCHAR* srv_name) :
	CDevice(idev)
{
	S32		status;
	U32 mode = BRDcapt_EXCLUSIVE;
	m_hSrv = BRD_capture(m_hDevice, 0, &mode, srv_name, 10000);
	if (m_hSrv > 0)
	{
		if (mode != BRDcapt_EXCLUSIVE)
		{
			status = BRD_release(m_hSrv, 0);	// освободить службу АЦП
			m_hSrv = 0;
			return;
		}
		BRDC_strcpy(m_srvName, srv_name);
		m_valid = 2; // service is OK
	}
}

CDeviceSrv::CDeviceSrv(int idev, string srvname) :
	CDevice(idev)
{
	S32		status;

	const char* tmp_name = srvname.c_str();
	BRDCHAR srv_name[MAX_NAME];
#ifdef _WIN32 //__IPC_WIN__
	BRDC_mbstobcs(srv_name, tmp_name, MAX_NAME);
#else
	BRDC_strcpy(srv_name, tmp_name);
#endif

	U32 mode = BRDcapt_EXCLUSIVE;
	m_hSrv = BRD_capture(m_hDevice, 0, &mode, srv_name, 10000);
	if (m_hSrv > 0)
	{
		if (mode != BRDcapt_EXCLUSIVE)
		{
			status = BRD_release(m_hSrv, 0);	// освободить службу
			m_hSrv = 0;
			return;
		}
		BRDC_strcpy(m_srvName, srv_name);
		m_valid = 2; // service is OK
	}
}

CDeviceSrv::~CDeviceSrv()
{
	S32		status;
	status = freebuf();
	status = BRD_release(m_hSrv, 0);	// освободить службу
	m_hSrv = 0;
}

//*****************************************************************************
//CDeviceSrv* CDeviceSrv::Create(int idev, string adcsrv)
//{
//	CDeviceSrv* psrv = new CDeviceSrv(idev, adcsrv);
//	if (!psrv)
//		return nullptr;
//
//	if (psrv->m_valid != 2)
//	{
//		delete psrv;
//		return nullptr;
//	}
//	return psrv;
//}

int CDeviceSrv::getPhysMem(U64* pPhysMemSize)
{
	S32		status;

	// проверяем наличие динамической памяти
	BRD_SdramCfgEx SdramConfig;
	SdramConfig.Size = sizeof(BRD_SdramCfgEx);
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETCFGEX, &SdramConfig);
	if (status < 0)
	{
		//BRDC_printf(_BRDC("Get SDRAM Config: Error!!!\n"));
		return -1;
	}
	else
	{
		if (SdramConfig.MemType == 11 || //DDR3
			SdramConfig.MemType == 12) //DDR4
			*pPhysMemSize = 
			(((__int64)SdramConfig.CapacityMbits * 1024 * 1024) >> 3) *
				(__int64)SdramConfig.PrimWidth / SdramConfig.ChipWidth * SdramConfig.ModuleBanks *
				SdramConfig.ModuleCnt; // в байтах
		else
			*pPhysMemSize = (1 << SdramConfig.RowAddrBits) *
			(1 << SdramConfig.ColAddrBits) *
			SdramConfig.ModuleBanks *
			SdramConfig.ChipBanks *
			(__int64)SdramConfig.ModuleCnt * 4; // в байтах
	}
	if (!*pPhysMemSize)
	{
		// освободить службу SDRAM (она могла быть захвачена командой BRDctrl_SDRAM_GETCFG, если та отработала без ошибки)
		ULONG mem_size = 0;
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_SETMEMSIZE, &mem_size);
		//BRDC_printf(_BRDC("No SDRAM on board!!!\n"));
		//status = -1;
	}


	return 0;
}

#ifdef _WIN32
//#define MAX_BLOCK_SIZE 1073741824		// максимальный размер блока = 1 Гбайт 
//#define MAX_BLOCK_SIZE 536870912		// максимальный размер блока = 512 Мбайт 
//#define MAX_BLOCK_SIZE 268435456		// максимальный размер блока = 256 Мбайт 
//#define MAX_BLOCK_SIZE 134217728		// максимальный размер блока = 128 Мбайт 
#define MAX_BLOCK_SIZE 4194304			// максимальный размер блока = 4 Mбайтa 
#else  // LINUX
#define MAX_BLOCK_SIZE 4194304			// максимальный размер блока = 4 Mбайтa 
#endif

//	pSig - указатель на массив указателей (IN), каждый элемент массива является указателем на блок (OUT)
//	pBlkNum - число блоков составного буфера (IN/OUT)
//	memType - тип памяти для данных (IN): 
//		0 - пользовательская память выделяется в драйвере (точнее, в DLL базового модуля)
//		1 - системная память выделяется драйвере 0-го кольца
//		2 - пользовательская память выделяется в приложении
//S32 ADC_allocbuf(BRD_Handle hADC, PVOID* &pSig, unsigned long long* pbytesBufSize, int* pBlkNum, int memType)
//S32 ADC_allocbuf(BRD_Handle hADC, PVOID* &pSig, U64* pbytesBufSize)

// размещение составного буфера стрима для получения/отправки данных с АЦП/на ЦАП
// buf_dscr -  описание буфера стрима (IN/OUT)
//	pbytesBufSize - общий размер данных (всех блоков составного буфера), которые должны быть выделены (IN/OUT - может меняться внутри функции)
// dir - направление передачи данных: 1 (BRDstrm_DIR_IN) - to HOST, 2 (BRDstrm_DIR_OUT) - from HOST (IN)
S32 CDeviceSrv::allocbuf(unsigned long long* pbytesBufSize, U32 dir, U32 bBlkSize, U32 blkNum)
{
	S32		status;

	unsigned long long bBufSize = *pbytesBufSize;
	//ULONG bBlkSize;
	//ULONG blkNum = 1;
	if (bBlkSize == 0 && blkNum == 0)
	{
		blkNum = 1;
		// определяем число блоков составного буфера
		if (bBufSize > MAX_BLOCK_SIZE)
		{
			do {
				blkNum <<= 1;
				bBufSize >>= 1;
			} while (bBufSize > MAX_BLOCK_SIZE);
		}
		bBlkSize = (ULONG)bBufSize;

	}
	void** pBuffer = NULL;
	if (2 == m_buf_dscr.isCont)
	{
		pBuffer = new PVOID[blkNum];
		for (ULONG i = 0; i < blkNum; i++)
		{
			pBuffer[i] = IPC_virtAlloc(bBlkSize);
			if (!pBuffer[i])
				return -1; // error
		}
	}
	m_buf_dscr.dir = dir; // BRDstrm_DIR_IN;
	//buf_dscr->isCont = memType;
	m_buf_dscr.blkNum = blkNum;
	m_buf_dscr.blkSize = bBlkSize;//*pbytesBufSize;
	m_buf_dscr.ppBlk = new PVOID[m_buf_dscr.blkNum];
	if (m_buf_dscr.isCont == 2)
	{
		for (ULONG i = 0; i < blkNum; i++)
			m_buf_dscr.ppBlk[i] = pBuffer[i];
		delete[] pBuffer;
	}
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_ALLOC, &m_buf_dscr);
	if (BRD_errcmp(status, BRDerr_PARAMETER_CHANGED))
	{ // может быть выделено меньшее количество памяти
		//BRDC_printf(_BRDC("Warning!!! BRDctrl_STREAM_CBUF_ALLOC: BRDerr_PARAMETER_CHANGED\n"));
		status = 1;
	}
	else
	{
		if (BRD_errcmp(status, BRDerr_OK))
		{
			//BRDC_printf(_BRDC("BRDctrl_STREAM_CBUF_ALLOC SUCCESS: status = 0x%08X\n"), status);
			status = 0;
		}
		else
		{ // при выделении памяти произошла ошибка
			if (2 == m_buf_dscr.isCont)
			{
				for (ULONG i = 0; i < blkNum; i++)
					IPC_virtFree(m_buf_dscr.ppBlk[i]);
			}
			delete[] m_buf_dscr.ppBlk;
			return status;
		}
	}
	//pSig = new PVOID[blkNum];
	//for (ULONG i = 0; i < blkNum; i++)
	//{
	//	pSig[i] = buf_dscr->ppBlk[i];
	//}
	*pbytesBufSize = (unsigned long long)m_buf_dscr.blkSize * blkNum;
	//*pBlkNum = blkNum;
	return status;
}

// освобождение буфера стрима
//	hADC - дескриптор службы АЦП (IN)
// buf_dscr -  описание буфера стрима (IN/OUT)
S32 CDeviceSrv::freebuf()
{
	S32		status = BRDerr_OK;
	if (!m_buf_dscr.blkNum)
		return status;

	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_FREE, NULL);
	if (m_buf_dscr.isCont == 2)
	{
		for (ULONG i = 0; i < m_buf_dscr.blkNum; i++)
			IPC_virtFree(m_buf_dscr.ppBlk[i]);
	}
	delete[] m_buf_dscr.ppBlk;
	return status;
}

S32 CDeviceSrv::prepareStream(int useMemOnBoard)
{
	S32		status = BRDerr_OK;
	if(useMemOnBoard)
	{	// установить, что стрим работает с памятью на модуле
		ULONG tetrad;
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETSRCSTREAM, &tetrad);
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETSRC, &tetrad);
	}

	// устанавливать флаг для формирования запроса ПДП надо после установки источника (тетрады) для работы стрима
	//	ULONG flag = BRDstrm_DRQ_ALMOST; // FIFO почти пустое
	//	ULONG flag = BRDstrm_DRQ_READY;
	ULONG flag = BRDstrm_DRQ_HALF; // рекомендуется флаг - FIFO наполовину заполнено
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETDRQ, &flag);

	BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_RESETFIFO, NULL);

	return status;
}

// получить/передать данные из/в память
S32 CDeviceSrv::dataXmem(U32 mstimeout)
{
	S32		status;
	BRDctrl_StreamCBufStart start_pars;
	start_pars.isCycle = 0; // без зацикливания 
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_START, &start_pars); // стартуем передачу данных из памяти устройства в ПК
	if (!BRD_errcmp(status, BRDerr_OK))
		return -2; // start error
	else
	{
		ULONG msTimeout = mstimeout; // ждать окончания передачи данных до mstimeout мсек.
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_WAITBUF, &msTimeout);
		if (BRD_errcmp(status, BRDerr_WAIT_TIMEOUT))
		{	// если вышли по тайм-ауту, то остановимся
			status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_STOP, NULL);
			return -1; // timeout error
		}
	}
	return 0; // success
}

//// проверяет завершение треда
//S32 CDeviceSrv::checkThread()
//{
//	// check for terminate thread
//	ULONG ret = WaitForSingleObject(m_hThread, 0);
//	if (ret == WAIT_TIMEOUT)
//		return 0;
//	return 1;
//}
//
//// прерывает исполнение треда, находящегося в ожидании завершения сбора данных в SDRAM или передачи их по ПДП
//void CDeviceSrv::breakThread()
//{
//	SetEvent(m_hUserEvent); // установить в состояние Signaled
//	WaitForSingleObject(m_hThread, INFINITE); // Wait until thread terminates
//											  //	CloseHandle(g_hUserEvent);
//											  //	CloseHandle(g_hThread);
//}
//
//// Эта функция должна вызываться ТОЛЬКО когда тред уже закончил исполняться 
//S32 CDeviceSrv::endThread()
//{
//	CloseHandle(m_hUserEvent);
//	CloseHandle(m_hThread);
//	m_hUserEvent = NULL;
//	m_hThread = NULL;
//	//	return m_eventStatus;
//	if (!BRD_errcmp(m_eventStatus, BRDerr_OK))
//		return -1; // timeout error
//	return 0;
//}

//= запускаем стрим
S32 CDeviceSrv::startStream(int mode)
{
	S32		status;
	BRDctrl_StreamCBufStart start_pars;
	start_pars.isCycle = mode; // 1 - с зацикливанием
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_START, &start_pars); // старт ПДП
	return status;
}

//= останавливаем стрим
S32 CDeviceSrv::stopStream()
{
	S32	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_STOP, NULL);
	return status;
}

//= устанавливаем расширенный режим прерывания
U32 CDeviceSrv::setExtIrqMode()
{
	U32 *pBuf0 = (U32*)m_buf_dscr.ppBlk[0];
	U32 pPhysStub = (U32)(pBuf0[3]);  // физический адрес блочка (используется как адрес для счетчика прерываний)
									  // этот адрес возвращает BRDctrl_STREAM_CBUF_ALLOC в нулевом блоке

	U32 irqadr_table[64]; // таблица для расширенного контроллера прерывания
	irqadr_table[0] = pPhysStub;	// адрес для записи счетчика в формате 0xB600<cnt16>, где cnt16 - счетчик прерываний
									// попадает на поле lastBlock

	irqadr_table[1] = pPhysStub + 8;	// адрес для записи константы
										// попадает на поле offset, которое не используется

	irqadr_table[2] = 0xAAAA;	// константа, может быть любая
	irqadr_table[3] = 0;		// резерв

	// команда на перевод в расширенный режим
	BRDctrl_StreamSetIrqMode irq_mode;
	irq_mode.mode = 1;		// 1 - включаем режим прерываний в C6678
	irq_mode.irqNum = 4;	// 4 - означает, по существу, один элемент таблицы
	irq_mode.pIrqTable = irqadr_table;

	S32 err = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETIRQMODE, &irq_mode);
	m_extIrqMode = irq_mode.mode; // возвращает 0, если нет расширенного контроллера прерываний
	return m_extIrqMode;
}
