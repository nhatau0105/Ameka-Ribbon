#include "stdafx.h"
#include "easylogging++.h"
#include "AmekaDoc.h"
#include "dsp_filters.h"
#include "DSPModule.h"
#include "kiss_fft.h"
#include "AmekaUserConfig.h"

static void convert_to_freq(kiss_fft_cpx *cout, int n) {
	const float NC = n/2.0 + 1;
	while (n-- > 0) {
		cout->r /= NC;
		cout->i /= NC;
		cout++;
	}
}

static void complex_abs(kiss_fft_cpx *cout, int n) {
	while (n-- > 0) {
		cout->r = sqrt(cout->r * cout->r + cout->i * cout->i);
		cout->i = 0;
		cout++;
	}
}

static int get_peak_pos(const kiss_fft_cpx *cout, int nfft, int start_pos) {
	int pos = 0;
	float maxdata = 0;
	int i;
	for (i = start_pos; i < nfft/2; i++) {
		if ((cout[i].r - maxdata) > 0.0001) {
			maxdata = cout[i].r;
			pos = i;
		}
	}
	return pos;
}

static float get_peak_frequence(const kiss_fft_cpx *cout, int nfft, float start_hz, float sample_hz) {
	int start_pos = start_hz * nfft / sample_hz;
	return get_peak_pos(cout, nfft, start_pos) * sample_hz / nfft;
}

void photic_processing(float fre_step, float HighFre, amekaData<PrimaryDataType>* PrimaryData, amekaData<SecondaryDataType>* SecondaryData)
{
	int nfft;
	nfft = (float)SAMPLE_RATE/fre_step;
	float NC = (float)nfft/2.0 + 1.0;

	PrimaryDataType* output = PrimaryData->checkPopData(nfft);
	uint16_t size =  PrimaryData->rLen;
		
	if (size > 0 && output != NULL)
	{
		int dataLen = PrimaryData->dataLen;
		PrimaryData->LRPos = (PrimaryData->LRPos + dataLen - size + 100) % dataLen;
		int isinverse = 0;
		kiss_fft_cfg st;
		kiss_fft_cpx * buf[MONTAGE_NUM];
		kiss_fft_cpx * bufout[MONTAGE_NUM];

		for (int i=0; i<MONTAGE_NUM; i++)
		{
			buf[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
			bufout[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
		}
		st = kiss_fft_alloc( nfft ,isinverse ,0,0);
			
		// Add value for input buf, then convert to frequency
		for (int j=0; j<MONTAGE_NUM; j++)
		{
			for (int i=0; i<nfft; i++)
			{
				buf[j][i].r = output[i].value[j];
				buf[j][i].i = 0;
			}
			kiss_fft( st , buf[j] ,bufout[j]);
			convert_to_freq(bufout[j], nfft);
			complex_abs(bufout[j], NC);
		}

		// Print output to file
		for (int i=0; i<(int)NC; i++)
		{
			SecondaryDataType temp;
			float fre = i * fre_step;
			temp.fre = fre;
			for (int j=0; j<MONTAGE_NUM; j++)	
			{
				if (fre < HighFre)
				{
					temp.value[j] = 0;
				}
				else
				{
					temp.value[j] = bufout[j][i].r;
				}
			SecondaryData->pushData(temp);
			}
		}
		free(st);
		for (int i=0; i<MONTAGE_NUM; i++)
		{
			free(buf[i]);
			free(bufout[i]);
		}
		delete [] output;
	}
}
UINT DSP::DSPThread(LPVOID pParam)
{
	CAmekaDoc* mDoc = (CAmekaDoc*)(pParam);
	uint16_t numSamples = 2000;
	time_t oldtime = 0;
	CTime t = CTime::GetCurrentTime();
	oldtime = t.GetTime();
	int count = 0;
	Dsp::SmoothedFilterDesign <Dsp::Butterworth::Design::BandPass <4>, MONTAGE_NUM, Dsp::DirectFormII> f (50);
	Dsp::Params params;
	
	while (1)
	{
		Sleep(5);
		if ((mDoc->isRecord == TRUE) && (mDoc->isOpenFile == FALSE))
		{
			// Try to remove old record file
			try
			{
				CFile::Remove(mDoc->recordFileName);
			}
			catch (CFileException* pEx)
			{
				//AfxMessageBox(L"File cannot be removed");
				pEx->Delete();
			}

			// Create new record file
			CTime temp = CTime::GetCurrentTime();
			mDoc->recordFileName = temp.Format("record-%H%M%S.dat");
			if (!mDoc->object.Open(mDoc->recordFileName, CFile::modeCreate | CFile::modeReadWrite))
			{
				AfxMessageBox(L"File cannot be created");
			}
			else
			{
				mDoc->isOpenFile = TRUE;
				mDoc->counter = 0;
				uint16_t temp[100];
				for (int i=0; i<100; i++)
				{
					temp[i] = 0xFF;
				}
				mDoc->object.Write(temp, sizeof(temp));
			}
		}
		else if ((mDoc->isRecord == FALSE) && (mDoc->isOpenFile == TRUE))
		{
			uint16_t buffer[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
			mDoc->object.Write(buffer, sizeof(buffer));
			mDoc->object.SeekToBegin();
			uint16_t temp[8];
			temp[0] = (uint16_t)(mDoc->mDSP.HPFFre * 10);
			temp[1] = (uint16_t)(mDoc->mDSP.LPFFre * 10);
			temp[2] = (uint16_t)(mDoc->mDSP.epocLength * 10);
			temp[3] = mDoc->mDSP.SampleRate;
			temp[4] = (uint16_t)(mDoc->counter);
			temp[5] = (uint16_t)(mDoc->counter >> 16);
			temp[6] = (uint16_t)(mDoc->counter >> 32);
			temp[7] = (uint16_t)(mDoc->counter >> 48);
			mDoc->object.Write(temp, sizeof(temp));

			int temp_mon[65];
			int monNum =  mDoc->mMon->mList.GetCount();
			temp_mon[64] = monNum;
			POSITION pos;
			pos = mDoc->mMon->mList.GetHeadPosition();
			if (monNum > 32)
				monNum = 32;
			for (int i=0; i<monNum; i++)
			{
				LPAlead temp;
				temp = mDoc->mMon->mList.GetNext(pos);
				int fID = temp->lFirstID;
				int sID = temp->lSecondID;
				temp_mon[i*2] = fID;
				temp_mon[i*2 + 1] = sID;
			}
			mDoc->object.Write(temp_mon, sizeof(temp_mon));

			int nLen = mDoc->mMon->mName.GetLength()*sizeof(TCHAR);
			mDoc->object.Write(mDoc->mMon->mName.GetBuffer(), nLen);
			mDoc->object.Close();
			mDoc->isOpenFile = FALSE;
			mDoc->counter = 0;
			//mDoc->CloseFileEvent = CreateEvent( NULL, FALSE, TRUE, NULL );
			/*if (mDoc->isSave != TRUE)
			{
				try
				{
				   CFile::Remove(mDoc->recordFileName);
				}
				catch (CFileException* pEx)
				{
				   AfxMessageBox(L"File cannot be removed");
				   pEx->Delete();
				}
			}
			else
			{
				try
				{
					CFile::Rename(mDoc->recordFileName, mDoc->saveFileName);
				}
				catch(CFileException* pEx )
				{
					AfxMessageBox(L"File cannot be saved");
					pEx->Delete();
				}				
				mDoc->isSave = FALSE;
			}*/
		}

		float HighFre = mDoc->mDSP.HPFFre;
		float LowFre = mDoc->mDSP.LPFFre;
		float sampleRate = mDoc->mDSP.SampleRate;
		float CenterFre = sqrt(HighFre*LowFre);
		float BandWidth = LowFre - HighFre;
		params[0] = sampleRate; // sample rate
		params[1] = 4; // order
		params[2] = CenterFre; // center frequency
		params[3] = BandWidth; // band width
		f.setParams (params);

		RawDataType* data = mDoc->dataBuffer->checkPopData(5);
		int size =  mDoc->dataBuffer->rLen;
		
		if (size > 0 && data != NULL)
		{
			if ((mDoc->isRecord == TRUE) && (mDoc->isOpenFile == TRUE))
			{
				uint16_t buffer[20];
				buffer[18] = 0x0;
				buffer[19] = 0x0;
				for (int i=0; i<5; i++)
				{
					RawDataType temp;
					temp = data[i];
					for (int j=0; j<LEAD_NUMBER; j++)
					{
						buffer[j] = data[i].value[j];
					}
					buffer[16] = data[i].time;
					buffer[17] = data[i].time >> 16;
					mDoc->counter++;
					mDoc->object.Write(buffer, sizeof(buffer));
				}
			}
			float* audioData[MONTAGE_NUM];
			PrimaryDataType* output = new PrimaryDataType[size];
			
			for (int i=0; i<MONTAGE_NUM; i++)
			{
				audioData[i] = new float[size];
			}

			int monNum =  mDoc->mMon->mList.GetCount();
			POSITION pos;
			pos = mDoc->mMon->mList.GetHeadPosition();
			for (int i=0; i<monNum; i++)
			{
				LPAlead temp;
				temp = mDoc->mMon->mList.GetNext(pos);
				int fID = temp->lFirstID;
				int sID = temp->lSecondID;

				for (int j=0; j<size; j++)
				{
					//audioData[i][j] = (float)data[j].value[i];
					float fData;
					float sData;
					if (fID <= 2)
						fData = BASELINE;
					else
						fData = (float)data[j].value[fID - 3];
					if (sID <= 2)
						sData = BASELINE;
					else
						sData = (float)data[j].value[sID - 3];
					audioData[i][j] = (sData - fData) + BASELINE;
				}
			}
			if (monNum < MONTAGE_NUM)
			{
				for (int i=monNum; i<MONTAGE_NUM; i++)
					for (int j=0; j<size; j++)
					{
						audioData[i][j] = 0;
					}
			}
			f.process (size, audioData);

			for (int j=0; j<size; j++)
			{
				count++;
				//output[j].time = data[j].time;
				if (count >= SAMPLE_RATE)
				{
					oldtime++;
					output[j].time = oldtime;
					output[j].isDraw = TRUE;
					count = 0;
					//LOG(DEBUG) << output[j].time;
					//oldtime = data[j].time;
				}
				else
				{
					output[j].time = 0;
					output[j].isDraw = FALSE;
				}
				for (int i=0; i<MONTAGE_NUM; i++)				
				{
					output[j].value[i] = (uint16_t)audioData[i][j];					
				}
			}
			
			for (int i=0; i<size; i++)
			{
				/*if (mDoc->PrimaryData->pushData(output[i]) != 0)
				{
					LOG(DEBUG) << "Primary Data ring buffer is full";	
				}
				if (mDoc->TemporaryData->pushData(output[i]) != 0)
				{
					LOG(DEBUG) << "Temporary Data ring buffer is full";	
				}*/
				mDoc->PrimaryData->pushData(output[i]);
				mDoc->TemporaryData->pushData(output[i]);
			}
			
			for (int i=0; i<MONTAGE_NUM; i++)
			{
				delete [] audioData[i];
			}
			delete [] data;
			delete [] output;
		}		

		float epocLength = mDoc->mDSP.epocLength;
		float fre_step = FRE_STEP;
		int nfft;
		nfft = (float)SAMPLE_RATE/fre_step;
		float NC = (float)nfft/2.0 + 1.0;

		PrimaryDataType* output = mDoc->TemporaryData->checkPopData(nfft);
		size =  mDoc->TemporaryData->rLen;
		
		if (size > 0 && output != NULL)
		{
			int dataLen = mDoc->TemporaryData->dataLen;
			mDoc->TemporaryData->LRPos = (mDoc->TemporaryData->LRPos + dataLen - size + 100) % dataLen;
			int isinverse = 0;
			kiss_fft_cfg st;
			kiss_fft_cpx * buf[MONTAGE_NUM];
			kiss_fft_cpx * bufout[MONTAGE_NUM];

			for (int i=0; i<MONTAGE_NUM; i++)
			{
				buf[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
				bufout[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
			}
			st = kiss_fft_alloc( nfft ,isinverse ,0,0);
			
			// Add value for input buf, then convert to frequency
			for (int j=0; j<MONTAGE_NUM; j++)
			{
				for (int i=0; i<nfft; i++)
				{
					buf[j][i].r = output[i].value[j];
					buf[j][i].i = 0;
				}
				kiss_fft( st , buf[j] ,bufout[j]);
				convert_to_freq(bufout[j], nfft);
				complex_abs(bufout[j], NC);
			}

			// Convert output to frequency
			/*float NC = nfft/2.0 + 1;
			for (int i=0; i<LEAD_NUMBER; i++)
			{
				convert_to_freq(bufout[i], nfft);
				complex_abs(bufout[i], nfft);
			}*/

			// Print output to file
			for (int i=0; i<(int)NC; i++)
			{
				SecondaryDataType temp;
				float fre = i * fre_step;
				temp.fre = fre;
				for (int j=0; j<MONTAGE_NUM; j++)	
				{
					if (fre < HighFre)
					{
						temp.value[j] = 0;
					}
					else
					{
						temp.value[j] = bufout[j][i].r;
					}
					//float fre = i * (float)(SAMPLE_RATE / nfft);
					/*LOG(INFO) << "------------";
					LOG(INFO) << j;
					LOG(INFO) << fre;
					LOG(INFO) << bufout[j][i].r;*/
				}				
				/*if (mDoc->SecondaryData->pushData(temp) != 0)
				{
					LOG(DEBUG) << "Secondary Data Ring buffer is full";
				};*/
				mDoc->SecondaryData->pushData(temp);
			}
			free(st);
			for (int i=0; i<MONTAGE_NUM; i++)
			{
				free(buf[i]);
				free(bufout[i]);
			}
			delete [] output;			
		}
	}
	return 0;
}

UINT DSP::ProcessRecordDataThread(LPVOID pParam)
{
	uint16_t stdCfrmData[2] = {0x0, 0x0};
 	CAmekaDoc* mDoc = (CAmekaDoc*)(pParam);
	CString fileName;
	fileName = mDoc->saveFileName;

	uint16_t temp[8];
	
	if (!mDoc->object.Open(fileName, CFile::modeRead))
	{
		AfxMessageBox(L"File cannot be opened");
	}

	mDoc->object.Read(temp, sizeof(temp));
	mDoc->mDSP.HPFFre = (float)(temp[0]) / 10.0;
	mDoc->mDSP.LPFFre = (float)(temp[1]) / 10.0;
	mDoc->mDSP.epocLength = (float)(temp[2]) / 10.0;
	mDoc->mDSP.SampleRate = temp[3];
	uint64_t counter = 0;
	counter = (uint64_t)(temp[4]) | (uint64_t)(temp[5] << 16) | (uint64_t)(temp[6] << 32) | (uint64_t)(temp[7] << 48);
	mDoc->counter = counter;
	int temp_mon[65];
	mDoc->object.Read(temp_mon, sizeof(temp_mon));
	int monNum = temp_mon[64];
	POSITION pos;
	mDoc->mMon->mList.RemoveAll();
	pos = mDoc->mMon->mList.GetHeadPosition();
	if (monNum > 32)
		monNum = 32;
	for (int i=0; i<monNum; i++)
	{
		Alead temp;
		temp.lFirstID = temp_mon[i*2];
		temp.lSecondID = temp_mon[i*2 + 1];
		mDoc->mMon->mList.AddTail(&temp);		
	}

	mDoc->object.Read(mDoc->mMon->mName.GetBuffer(), 44);
	mDoc->mMon->mName.ReleaseBuffer();
	// Khoi tao vung nho cho PrimaryData va SecondaryData
	//mDoc->dataBuffer = new amekaData<RawDataType>(counter);
	if (mDoc->PrimaryData)
		delete mDoc->PrimaryData;
	mDoc->PrimaryData = new amekaData<PrimaryDataType>(counter);
	//mDoc->SecondaryData = new amekaData<SecondaryDataType>(counter);
	
	// Xu ly du lieu Primary Data
	// Thiet lap tham so cho dsp
	time_t oldtime = 0;
	mDoc->dataBuffer->LRPos = 0; //Dam bao con tro doc o dau mang
	oldtime = mDoc->dataBuffer->popData()->time;
	mDoc->dataBuffer->LRPos = 0; //Tra con tro doc ve dau mang
	uint16_t count = 0;
	Dsp::SmoothedFilterDesign <Dsp::Butterworth::Design::BandPass <4>, MONTAGE_NUM, Dsp::DirectFormII> f (50);
	Dsp::Params params;
	float HighFre = mDoc->mDSP.HPFFre;
	float LowFre = mDoc->mDSP.LPFFre;
	float sampleRate = mDoc->mDSP.SampleRate;
	float CenterFre = sqrt(HighFre*LowFre);
	float BandWidth = LowFre - HighFre;
	params[0] = sampleRate; // sample rate
	params[1] = 4; // order
	params[2] = CenterFre; // center frequency
	params[3] = BandWidth; // band width
	f.setParams (params);

	float* audioData[MONTAGE_NUM];
		
	for (int i=0; i<MONTAGE_NUM; i++)
	{
		audioData[i] = new float[counter];
	}

	// Doc du lieu tu file va ghi vao rawdata buffer
	RawDataType* m_rawData;
	amekaData<uint16_t>* m_recvBuffer;
	m_recvBuffer = new amekaData<uint16_t>(20);
	m_rawData = new RawDataType[1024];

	uint16_t raw_cnt = 0;
	uint16_t buffer[1];
	while (mDoc->object.Read(buffer, sizeof(buffer)) != NULL)
	{
		m_recvBuffer->pushData(buffer[0]);
		
		if ((m_recvBuffer->get(19) == stdCfrmData[1]) && (m_recvBuffer->get(18) == stdCfrmData[0]))
		{
			/*for (int i=0; i<20; i++)
			getval[i] = m_recvBuffer->get(i);*/
			RawDataType temp;
			for (int i=0; i<LEAD_NUMBER; i++)
			{
				temp.value[i] = m_recvBuffer->get(i);
				temp.time = (time_t)(m_recvBuffer->get(16)) | (time_t)(m_recvBuffer->get(17) << 16);
			}
			//mDoc->dataBuffer->pushData(temp);
			m_rawData[raw_cnt] = temp;
			raw_cnt++;
			if (raw_cnt >= 1024)
			{
				raw_cnt = 0;
				pos = mDoc->mMon->mList.GetHeadPosition();
				for (int i=0; i<monNum; i++)
				{
					LPAlead temp;
					temp = mDoc->mMon->mList.GetNext(pos);
					int fID = temp->lFirstID;
					int sID = temp->lSecondID;

					for (int j=0; j<1024; j++)
					{
						float fData;
						float sData;
						if (fID <= 2)
							fData = BASELINE;
						else
							fData = (float)m_rawData[j].value[fID - 3];
						if (sID <= 2)
							sData = BASELINE;
						else
							sData = (float)m_rawData[j].value[sID - 3];
						audioData[i][j] = (sData - fData) + BASELINE;
					}
				}
				if (monNum < MONTAGE_NUM)
				{
					for (int i=monNum; i<MONTAGE_NUM; i++)
						for (int j=0; j<1024; j++)
						{
							audioData[i][j] = 0;
						}
				}
				f.process (1024, audioData);

				for (int j=0; j<1024; j++)
				{
					count++;
					PrimaryDataType temp;
					if (count >= SAMPLE_RATE)
					{
						oldtime++;			
						temp.time = oldtime;
						temp.isDraw = TRUE;
						count = 0;
					}
					else
					{
						temp.time = 0;
						temp.isDraw = FALSE;
					}
					for (int i=0; i<MONTAGE_NUM; i++)				
					{
						temp.value[i] = (uint16_t)audioData[i][j];					
					}
					mDoc->PrimaryData->pushData(temp);
				}
			}
		}
	}
	delete m_recvBuffer;	
	
	if (raw_cnt > 0)
	{
		//m_rawData = new RawDataType[raw_cnt];
		pos = mDoc->mMon->mList.GetHeadPosition();
		for (int i=0; i<monNum; i++)
		{
			LPAlead temp;
			temp = mDoc->mMon->mList.GetNext(pos);
			int fID = temp->lFirstID;
			int sID = temp->lSecondID;

			for (int j=0; j<raw_cnt; j++)
			{
				float fData;
				float sData;
				if (fID <= 2)
					fData = BASELINE;
				else
					fData = (float)m_rawData[j].value[fID - 3];
				if (sID <= 2)
					sData = BASELINE;
				else
					sData = (float)m_rawData[j].value[sID - 3];
				audioData[i][j] = (sData - fData) + BASELINE;
			}
		}
		if (monNum < MONTAGE_NUM)
		{
			for (int i=monNum; i<MONTAGE_NUM; i++)
				for (int j=0; j<raw_cnt; j++)
				{
					audioData[i][j] = 0;
				}
		}
		f.process (raw_cnt, audioData);

		for (int j=0; j<raw_cnt; j++)
		{
			count++;
			PrimaryDataType temp;
			if (count >= SAMPLE_RATE)
			{
				oldtime++;			
				temp.time = oldtime;
				temp.isDraw = TRUE;
				count = 0;
			}
			else
			{
				temp.time = 0;
				temp.isDraw = FALSE;
			}
			for (int i=0; i<MONTAGE_NUM; i++)				
			{
				temp.value[i] = (uint16_t)audioData[i][j];					
			}
			mDoc->PrimaryData->pushData(temp);
		}		
		raw_cnt = 0;
		//delete m_rawData;
	}	
	delete m_rawData;
	for (int i=0; i<MONTAGE_NUM; i++)
	{
		delete [] audioData[i];
	}

	//Xu ly du lieu Secondary Data	
	float epocLength = mDoc->mDSP.epocLength;
	float fre_step = FRE_STEP;
	int nfft;
	nfft = (float)SAMPLE_RATE/fre_step;
	float NC = (float)nfft/2.0 + 1.0;

	// Dam bao file record co so mau lon hon so mau dau vao cua pho
	if (mDoc->PrimaryData->crtWPos < nfft)
		return 0;
		//mDoc->PrimaryData->LRPos = mDoc->PrimaryData->crtWPos - nfft;
	photic_processing(fre_step, HighFre, mDoc->PrimaryData, mDoc->SecondaryData);

	//PrimaryDataType* output = mDoc->PrimaryData->checkPopData(nfft);
	//uint16_t size =  mDoc->PrimaryData->rLen;
	//	
	//if (size > 0 && output != NULL)
	//{
	//	int dataLen = mDoc->PrimaryData->dataLen;
	//	mDoc->PrimaryData->LRPos = (mDoc->PrimaryData->LRPos + dataLen - size + 100) % dataLen;
	//	int isinverse = 0;
	//	kiss_fft_cfg st;
	//	kiss_fft_cpx * buf[MONTAGE_NUM];
	//	kiss_fft_cpx * bufout[MONTAGE_NUM];

	//	for (int i=0; i<MONTAGE_NUM; i++)
	//	{
	//		buf[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
	//		bufout[i] = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft );
	//	}
	//	st = kiss_fft_alloc( nfft ,isinverse ,0,0);
	//		
	//	// Add value for input buf, then convert to frequency
	//	for (int j=0; j<MONTAGE_NUM; j++)
	//	{
	//		for (int i=0; i<nfft; i++)
	//		{
	//			buf[j][i].r = output[i].value[j];
	//			buf[j][i].i = 0;
	//		}
	//		kiss_fft( st , buf[j] ,bufout[j]);
	//		convert_to_freq(bufout[j], nfft);
	//		complex_abs(bufout[j], NC);
	//	}

	//	// Print output to file
	//	for (int i=0; i<(int)NC; i++)
	//	{
	//		SecondaryDataType temp;
	//		float fre = i * fre_step;
	//		temp.fre = fre;
	//		for (int j=0; j<MONTAGE_NUM; j++)
	//		{
	//			if (fre < HighFre)
	//			{
	//				temp.value[j] = 0;
	//			}
	//			else
	//			{
	//				temp.value[j] = bufout[j][i].r;
	//			}
	//		mDoc->SecondaryData->pushData(temp);
	//		}
	//	}
	//	free(st);
	//	for (int i=0; i<MONTAGE_NUM; i++)
	//	{
	//		free(buf[i]);
	//		free(bufout[i]);
	//	}
	//	delete [] output;
	//}
	mDoc->PrimaryData->LRPos = 0; //Tra ve con tro doc o dau mang

	mDoc->object.Close();

	return 0;
}
