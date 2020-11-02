//********************************************************
//
// Тестовое приложение 
//   тестирует память на базовом модуле, 
//		используя псевдослучайную последовательность 
//      или 64-битный счетчик.
//
// (C) InSys, 2020
//
//********************************************************

//#include <iostream>
#include	"mem.h"

string g_MemSrvName("BASESDRAM0"); // служба памяти

int const MAX_DEV = 2;
CMem* g_pMem[MAX_DEV] = { nullptr, nullptr };

unsigned long long g_total_err = 0;
int g_lid = -1;
int g_repeat = -1;
int g_test_mode = MEM_TEST_PSD;
//int g_test_mode = MEM_TEST_CNT;

unsigned long long g_bExpectedSize = 64 * 1024 * 1024;
//U32 const BLOCK_SIZE = 8 * 1024 * 1024; // размер (в байтах) одного блока составного буфера
//U32 numberOfBlocks = 64;

//#ifdef __IPC_LINUX__
#include <signal.h>
static bool g_stop_flag = false;
void sig_handler(int sig)
{
	if (sig == SIGINT)
	{
		printf("\nSIGINT is received\n");
		g_stop_flag = true;
	}
}
//#endif

//*****************************************************************************
#define SAFE_DELETE_PTR(ptr) 		if(ptr != nullptr) { delete ptr; ptr = nullptr; }

#include <tuple> 

//*****************************************************************************
// найти устройство, содержащее указанную службу
std::tuple<int, string, U32, S32, S32> findService(string srvname, int ndevs, int fdev)
//int findService(string srvname, int ndevs, int fdev)
{
	int idev = 0;
	for (idev = fdev; idev < ndevs; idev++)
	{
		CDevice dev(idev);
		bool is = dev.isService(srvname);
		if (is)
		{
			//return idev;
			string name = dev.getDeviceName();
			return std::make_tuple(idev, name, dev.m_devInfo.pid, dev.m_devInfo.bus, dev.m_devInfo.dev); // c++14
			//return{ idev, dev.m_devInfo.devName, dev.m_devInfo.pid, dev.m_devInfo.bus, dev.m_devInfo.dev }; // c++17
		}
	}
	return std::make_tuple(-1, "", 0, 0, 0); // c++14
	//return -1;
}

//*****************************************************************************
std::tuple<int, string, U32, S32, S32> isService(int lid, string srvname)
{
	int idev = -1;
	idev = CMem::getIndexFromLid(lid);
	if (idev != -1)
	{
		CDevice dev(idev);
		bool is = dev.isService(srvname);
		if (is)
		{
			string name = dev.getDeviceName();
			return std::make_tuple(idev, name, dev.m_devInfo.pid, dev.m_devInfo.bus, dev.m_devInfo.dev); // c++14
		}
		else  // нет нужной службы на устройстве с таким LID'ом
			return std::make_tuple(-2, "", 0, 0, 0); // c++14
	}
	else  // нет LID с таким номером
		return std::make_tuple(-1, "", 0, 0, 0); // c++14
}

//*****************************************************************************
int MEM_init(CMem* &pMem)
{
	int idev = -1;
	string devName; U32	pid; S32 bus, dev;	// Device Name, Board Physical ID (serial number), PCI Bus, PCI Device
	std::tie(idev, devName, pid, bus, dev) = isService(g_lid, g_MemSrvName); // c++14
	if (idev >= 0)
	{
		printf("Device%d (%s s/n = %d, Bus = %d, Dev = %d) : %s\n"
			, idev
			, devName.c_str()
			, pid
			, bus
			, dev
			, g_MemSrvName.c_str()
		);
		pMem = CMem::Create(idev, g_MemSrvName);
		BRD_SdramCfgEx mem_cfg;
		pMem->getCfg(&mem_cfg);
		//BRDC_printf(_BRDC("\n"));
		U64 phys_mem_size;
		int ret = pMem->getPhysMem(&phys_mem_size);
		if (ret == -1)
		{
			BRDC_printf(_BRDC("MEM%d: Get SDRAM Config: Error!!!\n"), idev);
			idev = -3;
		}
		else
			if (phys_mem_size)
			{
				if (mem_cfg.MemType == 11)  //DDR3
					BRDC_printf(_BRDC("MEM%d: DDR3 on board width = %d bits\n"), idev, mem_cfg.PrimWidth);
				else
					if (mem_cfg.MemType == 12)  //DDR4
						BRDC_printf(_BRDC("MEM%d: DDR4 on board width = %d bits\n"), idev, mem_cfg.PrimWidth);
					else
						BRDC_printf(_BRDC("MEM%d: SDRAM on board width = %d bits\n"), idev, mem_cfg.PrimWidth);
				BRDC_printf(_BRDC("MEM%d: SDRAM on board size = %llu MBytes\n"), idev, phys_mem_size / (1024 * 1024));
				g_bExpectedSize = phys_mem_size;
				//numberOfBlocks = U32(phys_mem_size / BLOCK_SIZE);
			}
			else
			{
				BRDC_printf(_BRDC("MEM%d: No SDRAM on board!!!\n"), idev);
				idev = -4;
			}
		ret = idev;
	}
	else
	{
		if(idev == -1)
			printf("No device with LID=%d\n", g_lid);
		if (idev == -2)
			printf("Device%d (LID=%d) has NOT the service %s\n", idev, g_lid, g_MemSrvName.c_str());
	}
	return idev;
}

//*****************************************************************************
int MEM_init(int devices)
{
	int idev = -1;
	int num_srv = 0;		// число найденных служб

	printf("\nMEM init:-----------------------------------\n");
	for (int i = 0; i < MAX_DEV; i++)
	{
		string devName; U32	pid; S32 bus, dev;	// Device Name, Board Physical ID (serial number), PCI Bus, PCI Device
		std::tie(idev, devName, pid, bus, dev) = findService(g_MemSrvName, devices, idev + 1); // c++14
		//auto [idev, devName, pid, bus, dev] = findService(g_MemSrvName, devices, idev + 1); // c++17
		if (idev >= 0)
		{
			//BRDC_printf(_BRDC("Device%d has the service %s\n"), idev, g_MemSrvName);
			//printf(" has the service %s\n", g_MemSrvName.c_str());
			printf("Device%d (%s s/n = %d, Bus = %d, Dev = %d) : %s\n"
				, idev
				, devName.c_str()
				, pid
				, bus
				, dev
				, g_MemSrvName.c_str()
			);
			g_pMem[num_srv++] = CMem::Create(idev, g_MemSrvName);
			//if(!g_pMem[num_srv - 1])
			//	printf("Device%d has NOT the service %s\n", idev, g_MemSrvName.c_str());
			BRD_SdramCfgEx mem_cfg;
			g_pMem[num_srv - 1]->getCfg(&mem_cfg);
			//BRDC_printf(_BRDC("\n"));
			U64 phys_mem_size;
			int ret = g_pMem[num_srv - 1]->getPhysMem(&phys_mem_size);
			if (ret == -1)
				BRDC_printf(_BRDC("MEM%d: Get SDRAM Config: Error!!!\n"), num_srv - 1);
			else
				if (phys_mem_size)
				{
					if (mem_cfg.MemType == 11)  //DDR3
						BRDC_printf(_BRDC("MEM%d: DDR3 on board width = %d bits\n"), num_srv - 1, mem_cfg.PrimWidth);
					else
						if (mem_cfg.MemType == 12)  //DDR4
							BRDC_printf(_BRDC("MEM%d: DDR4 on board width = %d bits\n"), num_srv - 1, mem_cfg.PrimWidth);
						else
							BRDC_printf(_BRDC("MEM%d: SDRAM on board width = %d bits\n"), num_srv - 1, mem_cfg.PrimWidth);
					BRDC_printf(_BRDC("MEM%d: SDRAM on board size = %llu MBytes\n"), num_srv - 1, phys_mem_size / (1024 * 1024));
					g_bExpectedSize = phys_mem_size;
					//numberOfBlocks = U32(phys_mem_size / BLOCK_SIZE);
				}
				else
					BRDC_printf(_BRDC("MEM%d: No SDRAM on board!!!\n"), num_srv - 1);
		}
		else
		{
			//printf("The service %s is not found!\n", g_MemSrvName.c_str());
			break;
		}
	}

	return num_srv;
}

//*****************************************************************************
// отобразить информацию о размещенной памяти 
void displayAllocInfo(int status, unsigned long long bExpectedSize, unsigned long long bDaqSize)
{
	switch (status)
	{
	case 0:
		printf("Allocating memory is success");
		break;
	case -1:
		printf("Error allocating memory by the application (type is APP_USER_MEMORY)");
		return;
	case 1:
	{
		printf("The allocated memory size (%lld MBytes) is not equal the expected one (%lld MBytes)"
			, bExpectedSize / 1024 / 1024
			, bDaqSize / 1024 / 1024
		);
		break;
	}
	default:
		printf("Error allocating memory = 0x%X", status);
		return;
	}

	char str1[256];
	unsigned long long  kbsize = bDaqSize / 1024; // в кбайтах
	if (kbsize > 1000)
	{
		unsigned long long  mbsize = kbsize / 1024; // в Мбайтах
		if (mbsize > 1000)
			//BRDC_sprintf(str1, _BRDC("%d MBytes * %d = %lld GBytes\n"), BLOCK_SIZE / 1024 / 1024, numberOfBlocks, mbsize / 1024);
			sprintf(str1, ":     %llu GBytes\n", mbsize / 1024);
		else
			//BRDC_sprintf(str1, _BRDC("%d MBytes * %d = %lld MBytes\n"), BLOCK_SIZE / 1024 / 1024, numberOfBlocks, kbsize / 1024);
			sprintf(str1, ":     %llu MBytes\n", kbsize / 1024);
	}
	else
	{ // в кбайтах
		//BRDC_sprintf(str1, _BRDC("%d MBytes * %d = %lld kBytes\n"), BLOCK_SIZE / 1024 / 1024, numberOfBlocks, kbsize);
		sprintf(str1, ":     %llu kBytes\n", kbsize / 1024);
	}
	printf(str1);
}

//*****************************************************************************
int MEM_prepare(int num_srv)
{
	int status = 0;
	BRDC_printf(_BRDC("\nMEM prepare:--------------------------------\n"));
	for (int i = 0; i < num_srv; i++)
	{
		status = g_pMem[i]->setTestMode(g_test_mode);
		//unsigned long long bDaqSize = g_options.samplesOfChannel * num_chan * adc_param.sampleSize;
		//unsigned long long bExpectedSize = (U64)BLOCK_SIZE * numberOfBlocks;
		unsigned long long bDaqSize = g_bExpectedSize;
		status = g_pMem[i]->setDaqParam(MEM_LIKE_MEM, DLL_USER_MEMORY, &bDaqSize);

		//unsigned long long bExpectedSize = g_options.samplesOfChannel * num_chan * adc_param.sampleSize;
		displayAllocInfo(status, g_bExpectedSize, bDaqSize);
	}
	return status;
}

//=*****************************************************************************
void MEM_delete(int num_srv)
{
	for (int i = 0; i < num_srv; i++)
	{
		//delete g_pMem[i];
		SAFE_DELETE_PTR(g_pMem[i]);
	}
}

//=*****************************************************************************
// проверка псевдослучайной тестовой последовательности
// cnt_err - счетчик ошибок
// iCircle - номер цикла прохода по всей памяти
// iCnt - номер запуска стрима для получения данных из памяти
// iMem - порядковый номер устройства с памятью
// data_wr - ожидаемые данные (константы, которые мы ожидаем принять)
// width - ширина шины данных в 64-битных словах: 1 - 64-битная шина данных, 2 - 128-битная, 4 - 256-битная, 8 - 512-битная
unsigned long long checkPsdSeq(unsigned long long cnt_err, int iCircle, int iCnt, unsigned iMem, unsigned long long* data_wr, int width)
{	// контроль тестовой последовательности

	unsigned int blkSize = g_pMem[iMem]->getStreamSize();
	unsigned int blkNum = g_pMem[iMem]->getStreamBlocks();

	//unsigned long long** pBuffer = (unsigned long long**)pBuf;
	unsigned long long data_rd;
	unsigned long wSize = blkSize / 8; // размер блока в 64-битных словах
	for (ULONG iBlock = 0; iBlock < blkNum; iBlock++)
	{
		unsigned long long* pBuf = (unsigned long long*)g_pMem[iMem]->getStreamBlock(iBlock);
		for (ULONG i = 0; i < wSize; i++)
		{
			for (int j = 0; j < width; j++)
			{
				data_rd = pBuf[i + j];
				if (data_wr[j] != data_rd)
				{
					cnt_err++;
					if (cnt_err < 16)
					{
						unsigned long long mem_adr = (unsigned long long)iCnt * (blkSize * blkNum) + iBlock * blkSize + ( (i+j) << 3 );
						printf("MEM%d(%016llX) ERROR (%d, %d, %d, %d): exp %016llX : rd %016llX : xor %016llX\n", iMem, mem_adr, i + j, iBlock, iCnt, iCircle, data_wr[j], data_rd, data_wr[j] ^ data_rd);
					}
					data_wr[j] = data_rd;
				}
				ULONG data_h = ULONG(data_wr[j] >> 32);
				ULONG f63 = data_h >> 31;
				ULONG f62 = data_h >> 30;
				ULONG f60 = data_h >> 28;
				ULONG f59 = data_h >> 27;
				ULONG f0 = (f63 ^ f62 ^ f60 ^ f59) & 1;

				data_wr[j] <<= 1;
				data_wr[j] &= ~1;
				data_wr[j] |= f0;
			}
			i += width - 1;

		}
	}
	//if (cnt_err)
	//	printf("Test is ERROR!!! Block size = %d kBytes"), blkSize / 1024);
	//else
	//	printf("Test is SUCCESS!!! Block size = %d kBytes"), blkSize / 1024);

	return cnt_err;
}

//=*****************************************************************************
// проверка тестовой последовательности - 64-разрядного счетчика
// cnt_err - счетчик ошибок
// iCircle - номер цикла прохода по всей памяти
// iCnt - номер запуска стрима для получения данных из памяти
// iMem - порядковый номер устройства с памятью
// data_wr - ожидаемые данные
unsigned long long checkCntSeq(unsigned long long cnt_err, int iCircle, int iCnt, unsigned iMem, unsigned long long& data_wr)
{	

	unsigned int blkSize = g_pMem[iMem]->getStreamSize();
	unsigned int blkNum = g_pMem[iMem]->getStreamBlocks();

	//unsigned long long** pBuffer = (unsigned long long**)pBuf;
	unsigned long long data_rd;
	unsigned long wSize = blkSize / 8; // размер блока в 64-битных словах
	for (ULONG iBlock = 0; iBlock < blkNum; iBlock++)
	{
		unsigned long long* pBuf = (unsigned long long*)g_pMem[iMem]->getStreamBlock(iBlock);
		for (ULONG i = 0; i < wSize; i++)
		{
			data_rd = pBuf[i];
			if (data_wr != data_rd)
			{
				cnt_err++;
				if (cnt_err < 16)
				{
					unsigned long long mem_adr = (unsigned long long)iCnt * (blkSize * blkNum) + iBlock * blkSize + (i << 3);
					printf("MEM%d(%016llX) ERROR (%d, %d, %d, %d): exp %016llX : rd %016llX : xor %016llX\n", iMem, mem_adr, i, iBlock, iCnt, iCircle, data_wr, data_rd, data_wr^data_rd);
				}
			}
			data_wr++;
		}
	}
	//if (cnt_err)
	//	printf("Test is ERROR!!! Block size = %d kBytes"), blkSize / 1024);
	//else
	//	printf("Test is SUCCESS!!! Block size = %d kBytes"), blkSize / 1024);

	return cnt_err;
}

// получение данных с контролем тестовой последовательности
//*****************************************************************************
S32	MEM_getData(int iCycle, int num_srv)
{
	S32	status = BRDerr_OK;
	U32 msTimeout = 5000;

	// собрать данные в память на устройстве
	for(int iMem = 0; iMem < num_srv; iMem++)
	{
		status = g_pMem[iMem]->doDaqMem(msTimeout);
		if (!BRD_errcmp(status, BRDerr_OK))
		{
			printf("ERROR by doDaqMem!!!                         \n");
			return status;
		}
	}
	//status = g_pMem[iMem]->prepareStream(1);

	for (int iMem = 0; iMem < num_srv; iMem++)
	{
		void* pBuf = g_pMem[iMem]->getStreamBuf();
		unsigned long long bMemBufSize = g_pMem[iMem]->getActiveMemSize();
		unsigned int blkSize = g_pMem[iMem]->getStreamSize();
		unsigned int bBufSize = g_pMem[iMem]->getStreamBlocks() * blkSize;

		unsigned long long cnt_err = 0;
		// width - ширина шины данных в 64-битных словах: 1 - 64-битная шина данных, 2 - 128-битная, 4 - 256-битная, 8 - 512-битная
		unsigned long long data_wr[] = { 2, 0xAA55, 0xBB66, 0xCC77, 0xDD88, 0xEE99, 0xFFAA, 0x100BB };
		unsigned long long cnt_data_wr = 0;
		//ULONG wSize = bBufSize / 8; // размер блока в 64-битных словах
		int nCnt = int(bMemBufSize / bBufSize);
		for (int iCnt = 0; iCnt < nCnt; iCnt++)
		{
			status = g_pMem[iMem]->dataXmem(msTimeout);
			//if (!BRD_errcmp(status, BRDerr_OK))
			if(status)
			{
				printf("ERROR by dataXmem!!!                         \n");
				return status;
			}
			printf("Cycles %d Blocks %d Errors = %llu    \r", iCycle, iCnt + 1, cnt_err);
			//cnt_err = checkPsdSeq(cnt_err, iCnt, pBuf, blkSize, data_wr, 4);
			if(g_test_mode == MEM_TEST_PSD)
				cnt_err = checkPsdSeq(cnt_err, iCycle, iCnt, iMem, data_wr, 4);
			else
				cnt_err = checkCntSeq(cnt_err, iCycle, iCnt, iMem, cnt_data_wr);

			if(g_stop_flag) break;
			//if (IPC_kbhit()) {
			//	break;
			//}
		}
		g_total_err += cnt_err;
		if (g_stop_flag) break;
		//ULONG ostSize = g_pMem[iMem]->getActiveMemSize() % g_pMem[iMem]->getStreamSize();
		//ULONG ostSize = bMemBufSize % bBufSize;
		////ULONG ostSize = ULONG(bMemBufSize % bBufSize);
		//if (ostSize)
		//{
		//	status = g_pMem[iMem]->dataXmem(msTimeout);
		//	//cnt_err = checkPsdSeq(cnt_err, nCnt, pBuf, ostSize, data_wr, 4);
		//	cnt_err = checkPsdSeq(cnt_err, iMem, data_wr, 4);
		//}

		//printf("\nTest is complete! Total errors = %lld.\n", cnt_err);
	}

	return status;
}

// Вывести подсказку
void usage()
{
	printf("usage: mem_test [<options>]\n");
	//printf("\n");
	printf("Options:\n");
	printf(" -h        - show this help message and exit\n");
	printf(" -b <lid>   - LID of base unit\n");
	printf(" -r <num>   - number of repetitions\n");
	printf(" -cnt	   - testing by 64-bit counter\n");
	printf("\n");
}

// разобрать командную строку
void ParseCommandLine(int argc, char *argv[])
{
	int			ii;
	char		*pLin, *endptr;

	for (ii = 1; ii < argc; ii++)
	{
		if (argv[ii][0] != '-')
		{
			//strcpy(g_szDir, argv[1]);
			//SetCurrentDirectory(g_szDir);
			continue;
		}

		// Вывести подсказку
		if (tolower(argv[ii][1]) == 'h')
		{
			usage();
			//printf("\nPress any key for leaving program...\n");
			//IPC_getch();
			//IPC_cleanupKeyboard();
			exit(1);
		}

		//по-умолчанию: все базовые модули с искомой службой
		if (tolower(argv[ii][1]) == 'b')
		{ // номер LID базового модуля
			pLin = &argv[ii][2];
			if (argv[ii][2] == '\0')
			{
				ii++;
				pLin = argv[ii];
			}
			g_lid = strtoul(pLin, &endptr, 0);
			//printf("Command line: -b%d\n", g_dev);
		}

		//по-умолчанию: бесконечно до нажатия Ctrl+C
		if (tolower(argv[ii][1]) == 'r')
		{ // число повторений (циклов) сбора/чтения памяти
			pLin = &argv[ii][2];
			if (argv[ii][2] == '\0')
			{
				ii++;
				pLin = argv[ii];
			}
			g_repeat = strtoul(pLin, &endptr, 0);
		}

		//по-умолчанию: ПСП-последовательность
		if (!strcmp(argv[ii], "-cnt"))
		{ // 64-битный счетчик
			g_test_mode = MEM_TEST_CNT;
		}

	}
}

//=*****************************************************************************
int main(int argc, char *argv[])
{
	//std::cout << "Hello World!\n";
	//IPC_initKeyboard();

	fflush(stdout);
	setbuf(stdout, NULL);

	ParseCommandLine(argc, argv);

	signal(SIGINT, sig_handler);

	printf("Memory on Board test v1.2: ");
	if (g_test_mode == MEM_TEST_PSD)
		printf("Pseudo random sequence!!!\n");
	if (g_test_mode == MEM_TEST_CNT)
		printf("64-bits Counter!!!\n");

	int devices = CDevice::Init(0); // 0 - инициализация по LID'ам, 1 (по-умолчанию) - автоматическая инициализация
	if (0 >= devices)
	{
		if (0 == devices) 	printf("No devices!!!\n");
		else  				printf("ERROR by initialization of BARDY library!!!\n");
		//printf("\nPress any key for leaving program...\n");
		//IPC_getch();
		//IPC_cleanupKeyboard();
		return 1;
	}

	int nmem = -1;
	if (g_lid == -1)
	{
		nmem = MEM_init(devices);
	}
	else
	{
		int err = MEM_init(g_pMem[0]);
		if (err >= 0)
			nmem = 1;
	}

	if (nmem > 0)
	{
		int ret = MEM_prepare(nmem);
		if (ret >= 0)
		{
			int iCycle = 0;
			printf("\nPress Ctrl+C for breaking test...\n");
			while (!g_stop_flag)
			{
				MEM_getData(iCycle, nmem);
				//if (IPC_kbhit()) {
				//	int ch = IPC_getch();
				//	if (0x1B == ch) {
				//		break;
				//	}
				//}
				iCycle++;
				if (g_repeat > 0 && iCycle >= g_repeat)
					break;
			}
			printf("\nRepetitions  = %d Total errors = %llu                          \n", iCycle, g_total_err);
		}
	}

	//	for(int iMem = 0; iMem < nmem; iMem++)
	//	    S32 status = g_pMem[iMem]->setTestMode(0);

	MEM_delete(nmem);

	CDevice::Cleanup();
//	printf("\nPress any key for leaving program...\n");
//	IPC_getch();
//	IPC_cleanupKeyboard();

	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
