
#include	"mem.h"

CMem::CMem(int idev, BRDCHAR* adcsrv) :
	CDeviceSrv(idev, adcsrv)
{
	S32		status;
	if (m_hSrv > 0)
	{
		m_cfg.Size = sizeof(BRD_SdramCfgEx);
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETCFGEX, &m_cfg);
		//	m_hThread = NULL;
		//	m_hUserEvent = NULL;
		m_eventStatus = BRDerr_OK;
		//	m_param.size = sizeof(ADC_PARAM);
	}
}

CMem::CMem(int idev, string adcsrv) :
	CDeviceSrv(idev, adcsrv)
{
	S32		status;
	if (m_hSrv > 0)
	{
		m_cfg.Size = sizeof(BRD_SdramCfgEx);
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETCFGEX, &m_cfg);
		//m_hThread = NULL;
		//m_hUserEvent = NULL;
		m_eventStatus = BRDerr_OK;
		//m_param.size = sizeof(ADC_PARAM);
	}
}

CMem::~CMem()
{

}

CMem* CMem::Create(int idev, string adcsrv)
{
	CMem* pmem = new CMem(idev, adcsrv);
	if (!pmem)
		return nullptr;

	if (pmem->m_valid != 2)
	{
		delete pmem;
		return nullptr;
	}
	return pmem;
}

int CMem::setTestMode(int memTestMode)
{
	ULONG test_mode = memTestMode; // 
	int status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_SETTESTSEQ, &test_mode);
	return status;
}

#define STREAM_BUF_SIZE_BY_MEM 128 * 1024 * 1024 // размер буфера стрима при сборе в память (128Мбайт)

// установить параметры сбора 
// useMemOnBoard - (IN) использование динамической памяти на устройстве:
//	0 - не использовать
//  1 - сбор именно в память на устройстве
//	2 - использовать в качестве FIFO
// isSystemMemory - (IN) тип памяти ПК для стрима
// samplesOfChannel - (IN) число собираемых отсчетов на канал
int CMem::setDaqParam(int useMemOnBoard, int streamMemoryType, unsigned long long* pBytesDaqSize)
{
	S32		status;

	m_useMemOnBoard = useMemOnBoard;

	if (useMemOnBoard)
	{// установить параметры SDRAM
		ULONG fifo_mode = useMemOnBoard - 1; // 1- память используется как FIFO, 0 - память используется как память
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_SETFIFOMODE, &fifo_mode);
	}

	unsigned long long bStreamSize = *pBytesDaqSize;
	m_buf_dscr.isCont = streamMemoryType;

	if (useMemOnBoard == MEM_LIKE_MEM)
	{	// память используется как память
		//unsigned long long m_bMemSize = samplesOfChannel * num_chan * sample_size; // получить размер собираемых данных в байтах
		U32 mem_size = U32(*pBytesDaqSize >> 2);
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_SETMEMSIZE, &mem_size);
		m_activeMemSize = *pBytesDaqSize = (unsigned long long)mem_size << 2; // получить фактический размер активной зоны в байтах
		//BRDC_printf(_BRDC("SDRAM size for DAQ data = %d MBytes\n"), int(m_bMemSize / (1024 * 1024)));
		bStreamSize = STREAM_BUF_SIZE_BY_MEM; // при сборе в память для стрима размещаем буфер на 128Мбайт
		if (bStreamSize > m_activeMemSize) bStreamSize = m_activeMemSize;
		m_buf_dscr.isCont = SYSTEM_MEMORY; // при сборе в память для стрима используем системную память
	}

	m_buf_dscr.blkNum = 1;
	m_buf_dscr.ppBlk = NULL; // указатель на массив указателей на блоки памяти с сигналом
	do {
		if (bStreamSize <= 8 * 1024 * 1024) // 16Мбайт
			break;
		status = allocbuf(&bStreamSize, BRDstrm_DIR_IN);
		if (useMemOnBoard == MEM_NOT_USE)
			break;
		bStreamSize >>= 1;
	} while (status < 0);// пока ошибка выделения памяти

	if (useMemOnBoard == MEM_LIKE_MEM && status < 0)
	{
		bStreamSize = STREAM_BUF_SIZE_BY_MEM; // при сборе в память для стрима размещаем буфер на 128Мбайт
		if (bStreamSize > m_activeMemSize) bStreamSize = m_activeMemSize;
		m_buf_dscr.isCont = DLL_USER_MEMORY; // если системной памяти недостаточно, выделяем пользовательскую
		m_buf_dscr.blkNum = 1;
		m_buf_dscr.ppBlk = NULL; // указатель на массив указателей на блоки памяти с сигналом
		status = allocbuf(&bStreamSize, BRDstrm_DIR_IN);
	}

	//m_streamBlkSize = bStreamSize / m_buf_dscr.blkNum;
	return status;
}

// установить параметры сбора (память на устройстве не используется)
//	memType - тип памяти для данных (IN): 
//		0 - пользовательская память выделяется в драйвере (точнее, в DLL базового модуля)
//		1 - системная память выделяется в драйвере 0-го кольца
//		2 - пользовательская память выделяется в приложении
int CMem::setDaqParam(int useMemOnBoard, int streamMemoryType, U32 streamBlockSize, U32* pStreamBlockNum)
{
	S32		status;

	m_useMemOnBoard = useMemOnBoard;

	if (useMemOnBoard)
	{// установить параметры SDRAM
		ULONG fifo_mode = useMemOnBoard - 1; // 1- память используется как FIFO, 0 - память используется как память
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_SETFIFOMODE, &fifo_mode);
	}

	U32 blknum = *pStreamBlockNum;
	unsigned long long bStreamSize = (U64)streamBlockSize * blknum;
	m_buf_dscr.isCont = streamMemoryType;
	m_buf_dscr.ppBlk = NULL; // указатель на массив указателей на блоки памяти с сигналом
	status = allocbuf(&bStreamSize, BRDstrm_DIR_IN, streamBlockSize, blknum);
	*pStreamBlockNum = m_buf_dscr.blkNum;
	return status;
}

//#include	<process.h> 
//
//// выполнить сбор данных в SDRAM с ПДП-методом передачи в ПК
//// с использованием прерывания по окончанию сбора
//unsigned __stdcall ThreadDaqIntoSdramDMA(void* pParams)
//{
//	S32		status;
//	ULONG AdcStatus = 0;
//	ULONG Status = 0;
//	ULONG isAcqComplete = 0;
//	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
//	PTHREAD_PARAM pThreadParam = (PTHREAD_PARAM)pParams;
//	BRD_Handle hADC = pThreadParam->hSrv;
//	HANDLE hUserEvent = pThreadParam->hUserEvent;
//	U32 msTimeout = pThreadParam->tout;
//	HANDLE hSyncEvent = pThreadParam->hSyncEvent;
//	S32* pevt_status = pThreadParam->pevtstat;
//	*pevt_status = BRDerr_OK;
//
//	// определение скорости сбора данных
//	//LARGE_INTEGER Frequency, StartPerformCount, StopPerformCount;
//	//int bHighRes = QueryPerformanceFrequency(&Frequency);
//
//	ULONG Enable = 1;
//
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_IRQACQCOMPLETE, &Enable); // разрешение прерывания от флага завершения сбора в SDRAM
//	//status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_FIFOSTATUS, &Status);
//
////	status = BRD_ctrl(hADC, 0, BRDctrl_ADC_FIFORESET, NULL); // сброс FIFO АЦП
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_FIFORESET, NULL); // сброс FIFO SDRAM
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_ENABLE, &Enable); // разрешение записи в SDRAM
//
//	//QueryPerformanceCounter(&StartPerformCount);
////	status = BRD_ctrl(hADC, 0, BRDctrl_ADC_ENABLE, &Enable); // разрешение работы АЦП
//
//	//BRDC_printf(_BRDC("ADC is enabled\n"));
//	SetEvent(hSyncEvent); // установить в состояние Signaled
//
//	// дожидаемся окончания сбора
//	BRD_WaitEvent waitEvt;
//	waitEvt.timeout = msTimeout; // ждать окончания сбора данных до msTimeout мсек.
//	waitEvt.hAppEvent = hUserEvent;
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_WAITACQCOMPLETEEX, &waitEvt);
//	//QueryPerformanceCounter(&StopPerformCount);
//	if (BRD_errcmp(status, BRDerr_WAIT_TIMEOUT))
//	{	// если вышли по тайм-ауту
//		*pevt_status = status;
//		//status = BRD_ctrl(hADC, 0, BRDctrl_ADC_FIFOSTATUS, &AdcStatus);
//		status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_FIFOSTATUS, &Status);
//		status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_ISACQCOMPLETE, &isAcqComplete);
//		BRDC_printf(_BRDC("\nBRDctrl_SDRAM_WAITACQCOMPLETE is TIME-OUT(%d sec.)\n    AdcFifoStatus = %08X SdramFifoStatus = %08X\n"),
//																							waitEvt.timeout / 1000, AdcStatus, Status);
//		// получить реальное число собранных данных
//		ULONG acq_size;
//		status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_GETACQSIZE, &acq_size);
//		unsigned __int64 bRealSize = (unsigned __int64)acq_size << 2; // запомнить в байтах
//		//BRDC_printf(_BRDC("    isAcqComplete=%d, DAQ real size = %d kByte\n"), isAcqComplete, (ULONG)(bRealSize / 1024));
//	}
//
//	Enable = 0;
//
//	//status = BRD_ctrl(hADC, 0, BRDctrl_ADC_ENABLE, &Enable); // запрет работы АЦП
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_ENABLE, &Enable); // запрет записи в SDRAM
//
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_IRQACQCOMPLETE, &Enable); // запрет прерывания от флага завершения сбора в SDRAM
//
//	//	CloseHandle(hUserEvent);
//
//	if (!BRD_errcmp(*pevt_status, BRDerr_OK))
//		//	if(BRD_errcmp(evt_status, BRDerr_SIGNALED_APPEVENT))
//		//	if(evt_status == BRDerr_SIGNALED_APPEVENT)
//		return *pevt_status;
//
//	//status = prepareStream(1);
//	// установить, что стрим работает с памятью
//	ULONG tetrad;
//	status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_GETSRCSTREAM, &tetrad);
//	status = BRD_ctrl(hADC, 0, BRDctrl_STREAM_SETSRC, &tetrad);
//
//	// устанавливать флаг для формирования запроса ПДП надо после установки источника (тетрады) для работы стрима
//	//	ULONG flag = BRDstrm_DRQ_ALMOST; // FIFO почти пустое
//	//	ULONG flag = BRDstrm_DRQ_READY;
//	ULONG flag = BRDstrm_DRQ_HALF; // рекомендуется флаг - FIFO наполовину заполнено
//	status = BRD_ctrl(hADC, 0, BRDctrl_STREAM_SETDRQ, &flag);
//
//	BRD_ctrl(hADC, 0, BRDctrl_STREAM_RESETFIFO, NULL);
//
//	*pevt_status = status;
//	//*pevt_status = BRDerr_OK;
//	return status;
//}

// создает событие и запускает тред, 
// собирающий данные в память на устройстве и 
// подготавливающий стрим
S32 CMem::startDaqMem(U32 mstimeout, HANDLE hSyncEvent)
{
	S32		status = BRDerr_OK;
	//unsigned threadID;
	//m_hUserEvent = CreateEvent(
	//						NULL,   // default security attributes
	//						FALSE,  // auto-reset event object
	//						FALSE,  // initial state is nonsignaled
	//						NULL);  // unnamed object

	//m_thread_param.hSrv = m_hSrv;
	//m_thread_param.hUserEvent = m_hUserEvent;
	//m_thread_param.tout = mstimeout;
	//m_thread_param.pevtstat = &m_eventStatus;
	//m_thread_param.hSyncEvent = hSyncEvent;

	//// Create thread
	//m_hThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadDaqIntoSdramDMA, &m_thread_param, 0, &threadID);
	return 1;
}

// выполнить сбор данных в SDRAM с ПДП-методом передачи в ПК
// без использования прерывания по окончанию сбора
S32 CMem::doDaqMem(U32 mstimeout)
{
	//	printf("DAQ into SDRAM\n");
	S32		status;
	ULONG Status = 0;
	//	ULONG isAcqComplete = 0;
	ULONG Enable = 1;

	//status = BRD_ctrl(hADC, 0, BRDctrl_SDRAM_FIFOSTATUS, &Status);

	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFORESET, NULL); // сброс FIFO SDRAM
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ENABLE, &Enable); // разрешение записи в SDRAM

	ULONG cntTimeout = mstimeout / 50;
	m_eventStatus = BRDerr_WAIT_TIMEOUT;
	// дожидаемся окончания сбора
	for (ULONG i = 0; i < cntTimeout; i++)
	{
		//status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, &Status);
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ISACQCOMPLETE, &Status);
		if (Status) { m_eventStatus = BRDerr_OK; break; }
		//if(!(Status & 0x10)) break;
		IPC_delay(50);
	}
	if (BRD_errcmp(m_eventStatus, BRDerr_WAIT_TIMEOUT))
	{
		ULONG SdramStatus = 0;
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, &SdramStatus);
		BRDC_printf(_BRDC("\nBRDctrl_SDRAM_ISACQCOMPLETE is TIME-OUT(%d sec.)\n    SdramFifoStatus = %08X\n"),
			mstimeout / 1000, SdramStatus);
		//return evt_status;
	}

	Enable = 0;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ENABLE, &Enable); // запрет записи в SDRAM

	if (!BRD_errcmp(m_eventStatus, BRDerr_OK))
		return m_eventStatus;

	// установить, что стрим работает с памятью
	ULONG tetrad;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETSRCSTREAM, &tetrad);
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETSRC, &tetrad);

	// устанавливать флаг для формирования запроса ПДП надо после установки источника (тетрады) для работы стрима
//	ULONG flag = BRDstrm_DRQ_ALMOST; // FIFO почти пустое
//	ULONG flag = BRDstrm_DRQ_READY;
	ULONG flag = BRDstrm_DRQ_HALF; // рекомендуется флаг - FIFO наполовину заполнено
	//ULONG flag = g_MemDrqFlag;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETDRQ, &flag);

	BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_RESETFIFO, NULL);

	return status;
}

//=********************************************************
//= подготовить SDRAM и стрим
int CMem::prepareStart()
{
	S32		status = BRDerr_OK;

	// установить источник для работы стрима
	ULONG tetrad;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_GETSRCSTREAM, &tetrad); // стрим будет работать с SDRAM

	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETSRC, &tetrad);

	// устанавливать флаг для формирования запроса ПДП надо после установки источника (тетрады) для работы стрима
	//	ULONG flag = BRDstrm_DRQ_ALMOST; // FIFO почти пустое
	//	ULONG flag = BRDstrm_DRQ_READY;
	ULONG flag = BRDstrm_DRQ_HALF; // рекомендуется флаг - FIFO наполовину заполнено
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_SETDRQ, &flag);

	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFORESET, NULL); // сброс FIFO SDRAM
	ULONG Enable = 1;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ENABLE, &Enable); // разрешение записи в SDRAM

	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_RESETFIFO, NULL);
	return status;
}

//=********************************************************
//= разрешить работу SDRAM
int CMem::enableMem()
{
	S32		status = BRDerr_OK;
	U32 Enable = 1;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ENABLE, &Enable); // разрешение работы АЦП
	return status;
}

//=********************************************************
//= запретить работу SDRAM
int CMem::disableMem()
{
	S32		status = BRDerr_OK;
	U32 Enable = 0;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_ENABLE, &Enable); // запрет записи в SDRAM
	return status;
}

//=********************************************************
//= получить флаги переполнения FIFO АЦП и SDRAM (памяти на модуле)
//= возвращает: 0 - нет переполнения, 1 - переполнение FIFO АЦП
//= 2 - переполнение FIFO SDRAM, 3 - переполнение FIFO АЦП и SDRAM
int CMem::isOverflow()
{
	U32 retval = 0;
	U32 status = 0;
	BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, &status);
	if (status & 0x80)
		retval += 2;
		//BRDC_printf(_BRDC("\nERROR: SDRAM FIFO is overflow (SDRAM FIFO Status = 0x%04X)\n"), sdram_status);
	return retval;
}

//=********************************************************
//= получить регистр статуса FIFO АЦП и SDRAM (памяти на модуле)
//= возвращает: 0 - нет переполнения, 1 - переполнение FIFO АЦП
//= 2 - переполнение FIFO SDRAM, 3 - переполнение FIFO АЦП и SDRAM
int CMem::statusFIFO(ULONG* state)
{
	S32 status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, state);
	*state &= 0xFFFF;
	//BRDC_printf(_BRDC("\nERROR: ADC FIFO is overflow (ADC FIFO Status = 0x%04X)\n"), adc_status);
	//if (m_useMemOnBoard == MEM_LIKE_FIFO)
	//{
	//	BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, &status);
	//	if (status & 0x80)
	//		retval += 2;
	//	//BRDC_printf(_BRDC("\nERROR: SDRAM FIFO is overflow (SDRAM FIFO Status = 0x%04X)\n"), sdram_status);
	//}
	return status;
}

//=********************************************************
//= ожидаем окончание сбора текущего блока
int CMem::waitStreamBlock(U32 msTimeout)
{
	S32		status = BRDerr_OK;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_WAITBLOCK, &msTimeout);
	if (BRD_errcmp(status, BRDerr_WAIT_TIMEOUT))
	{	// если вышли по тайм-ауту, то остановимся
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_STOP, NULL);
		ULONG sdram_status = 0;
		status = BRD_ctrl(m_hSrv, 0, BRDctrl_SDRAM_FIFOSTATUS, &sdram_status);
		//BRDC_printf(_BRDC("BRDctrl_STREAM_CBUF_WAITBUF is TIME-OUT(%d sec.)\n    AdcFifoStatus = %08X SdramFifoStatus = %08X"),
		//	msTimeout / 1000, adc_status, sdram_status);
		//break;
		return -1;
	}
	return 0;
}

//=********************************************************
//= ожидаем окончание сбора текущего блока
int CMem::waitStreamBuffer(U32 msTimeout)
{
	S32		status = BRDerr_OK;
	status = BRD_ctrl(m_hSrv, 0, BRDctrl_STREAM_CBUF_WAITBUF, &msTimeout);
	if (BRD_errcmp(status, BRDerr_OK))
		return 0;	// дождались окончания передачи данных
	return 1;
}
