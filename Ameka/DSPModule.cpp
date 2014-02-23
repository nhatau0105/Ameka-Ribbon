#include "stdafx.h"
#include "easylogging++.h"
#include "AmekaDoc.h"
#include "dsp_filters.h"
#include "DSPModule.h"


UINT DSP::DSPThread(LPVOID pParam)
{
	CAmekaDoc* mDoc = (CAmekaDoc*)(pParam);
	uint16_t numSamples = 2000;
	Dsp::SmoothedFilterDesign <Dsp::Butterworth::Design::BandPass <4>, LEAD_NUMBER, Dsp::DirectFormII> f (1024);
	Dsp::Params params;
	//params[0] = sampleRate; // sample rate
	//params[1] = 4; // order
	//params[2] = CenterFre; // center frequency
	//params[3] = BandWidth; // band width
	//f.setParams (params);
	//float* audioData[LEAD_NUMBER];
	while (1)
	{
		Sleep(50);
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
		static int count = 0;
		count++;

		float* audioData[LEAD_NUMBER];
		RawDataType* data = mDoc->dataBuffer->popAll();
		int size =  mDoc->dataBuffer->rLen;
		RawDataType* output = new RawDataType[size];
		if (size != 0 && data != NULL)
		{
			for (int i=0; i<LEAD_NUMBER; i++)
			{
				audioData[i] = new float[size];
			}

			for (int i=0; i<LEAD_NUMBER; i++)
				for (int j=0; j<size; j++)
			{
				audioData[i][j] = (float)data[j].value[i];
			}

			f.process (size, audioData);

			//switch (Type_design)
			//{
			//case 0:	//RBJ BandPass1
			//	Dsp::SmoothedFilterDesign <Dsp::RBJ::Design::BandPass1, LEAD_NUMBER> f (1024);
			//	//Dsp::Params params;
			//	params[0] = sampleRate; // sample rate
			//	params[1] = CenterFre; // Center frequency
			//	params[2] = BandWidth; // Band Width
			//	f.setParams (params);
			//	f.process (numSamples, audioData);
			//	break;
			//case 1: //RBJ BandPass2
			//	Dsp::SmoothedFilterDesign <Dsp::RBJ::Design::BandPass2, LEAD_NUMBER> f (1024);
			//	//Dsp::Params params;
			//	params[0] = sampleRate; // sample rate
			//	params[1] = CenterFre; // Center frequency
			//	params[2] = BandWidth; // Band Width
			//	f.setParams (params);
			//	f.process (numSamples, audioData);
			//	break;
			//case 2: //Butterworth BandPass
			//	//Dsp::Filter* f = new Dsp::SmoothedFilterDesign
			//	//<Dsp::Butterworth::Design::BandPass <4>, LEAD_NUMBER, Dsp::DirectFormII> (1024);
			//	Dsp::SmoothedFilterDesign <Dsp::Butterworth::Design::BandPass <4>, LEAD_NUMBER, Dsp::DirectFormII> f (1024);
			//	//Dsp::Params params;
			//	params[0] = sampleRate; // sample rate
			//	params[1] = 4; // order
			//	params[2] = CenterFre; // center frequency
			//	params[3] = BandWidth; // band width
			//	f.setParams (params);
			//	f.process (numSamples, audioData);
			//	break;
			//case 3: //Chebyshevl1 BandPassBase
			//	break;
			//case 4: //Chebyshevl2 BandPassBase
			//	break;
			//case 5: //Eliliptic BandPassBase
			//	break;
			//case 6: //Legendre BandPassBase
			//	break;
			//default:
			//	break;
			//}

			for (int j=0; j<size; j++)
			{
				for (int i=0; i<LEAD_NUMBER; i++)				
				{
					output[j].value[i] = (uint16_t)audioData[i][j];
					output[j].time = data[j].time;				
				}
				//LOG(INFO) << output[j].time;
			}
			//int count = theApp.dataBuffer->pushData(output, size);
			
			for (int i=0; i<size; i++)
			{
				mDoc->PrimaryData->pushData(output[i]);
			}
			
			for (int i=0; i<LEAD_NUMBER; i++)
			{
				delete [] audioData[i];
			}
			//delete [] audioData;
			delete output;
			delete [] data;
		}
	}
	return 0;
}
