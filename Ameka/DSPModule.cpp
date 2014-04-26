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
			CTime temp = CTime::GetCurrentTime();
			mDoc->recordFileName = temp.Format("record-%H%M%S.dat");
			if (!mDoc->object.Open(mDoc->recordFileName, CFile::modeCreate | CFile::modeReadWrite))
			{
				AfxMessageBox(L"File cannot be created");
			}
			else
			{
				mDoc->isOpenFile = TRUE;
			}
		}
		else if ((mDoc->isRecord == FALSE) && (mDoc->isOpenFile == TRUE))
		{
			mDoc->object.Close();
			mDoc->isOpenFile = FALSE;
			try
			{
				CFile::Remove(mDoc->recordFileName);
			}
			catch (CFileException* pEx)
			{
				AfxMessageBox(L"File cannot be removed");
				pEx->Delete();
			}
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
				buffer[0] = 0x0;
				buffer[1] = 0x0;
				for (int i=0; i<5; i++)
				{
					RawDataType temp;
					temp = data[i];
					for (int j=0; j<LEAD_NUMBER; j++)
					{
						buffer[j+2] = data[i].value[j];
					}
					buffer[18] = data[i].time >> 16;
					buffer[19] = data[i].time;
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
					LOG(DEBUG) << output[j].time;
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
