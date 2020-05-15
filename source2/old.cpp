#include "FFT_UGens.h"
//#include "stdio.h"

InterfaceTable *ft;

struct PV_Control2 : Unit
{
	int m_numbins;
	float *m_mags, *m_mulArray, *m_addArray;
	int *m_waitHoldArray, *m_waitGoArray, *m_tripCountArray, *m_tripWaitArray;
};

extern "C"
{
	void PV_Control2_Ctor(PV_Control2 *unit);
	void PV_Control2_next(PV_Control2 *unit, int inNumSamples);
	void PV_Control2_Dtor(PV_Control2 *unit);
}


void PV_Control2_Ctor(PV_Control2* unit)
{
	SETCALC(PV_Control2_next);
	ZOUT0(0) = ZIN0(0);
	unit->m_mags = 0;
}

void PV_Control2_next(PV_Control2 *unit, int inNumSamples)
{
	PV_GET_BUF2

    SCPolarBuf* p = ToPolarApx(buf1);
    SCPolarBuf* q = ToPolarApx(buf2);

	//the control rate inputs

	float threshold = ZIN0(2); //if a bin accumulates this much magnitude over time, it will be zeroed out
	float mulFactor = ZIN0(3); //mulFactor - the accumulated magnitude is multiplied by this number after each analysis. smaller numbers should mean slower feedback recognition
	int attackReleaseFrames = ZIN0(4); //the attack/release (in bins) of the zeroing envelope of the bin, shorter numbers == faster ducking
	int sustainZeroFrames = ZIN0(5); //the sustain of the ducking envelope, also in bins
	int waitGoFrames = ZIN0(6); //how long the program allow the bin to feed back before any ducking occurs
	int tripCount = ZIN0(7); //if a bin is ducked this many times, it will be ducked for an extended period
	int tripBlockFrames = ZIN0(8); //how long the extended ducking period will be, in frames
	int highestBin = ZIN0(9); //the top bin that the UGen cares about, all bins above this value will be zeroed
	
	if (!unit->m_mags) {
		//alloc the arrays
		unit->m_mags = (float*)RTAlloc(unit->mWorld, numbins * sizeof(float));
		unit->m_mulArray = (float*)RTAlloc(unit->mWorld, numbins * sizeof(float));
		unit->m_addArray = (float*)RTAlloc(unit->mWorld, numbins * sizeof(float));
		unit->m_waitHoldArray = (int*)RTAlloc(unit->mWorld, numbins * sizeof(int));
		unit->m_waitGoArray = (int*)RTAlloc(unit->mWorld, numbins * sizeof(int));
		unit->m_tripCountArray = (int*)RTAlloc(unit->mWorld, numbins * sizeof(int));
		unit->m_tripWaitArray = (int*)RTAlloc(unit->mWorld, numbins * sizeof(int));

		//init the arrays to their starting values

		for (int i=0; i<numbins; ++i) {
			unit->m_mags[i] = 0.f;
			unit->m_mulArray[i] = 1.0f;
			unit->m_addArray[i] = 0.f;
			unit->m_waitHoldArray[i] = 0;
			unit->m_waitGoArray[i] = waitGoFrames;
			unit->m_tripCountArray[i] = 0;
			unit->m_tripWaitArray[i] = 0;
		}

		//unit->counterArray = (int*)RTAlloc(unit->mWorld, numbins * sizeof(int));
		
		unit->m_numbins = numbins;
	} else if (numbins != unit->m_numbins) return;

	float *mags = unit->m_mags; //stores the accumulated magnitude of the bin
	float *mulArray = unit->m_mulArray; //the bins are multiplied by this array. numbers in this array decrease from 1 to 0 when a bin is ducked
	float *addArray = unit->m_addArray; //this stores the value that the mulArray is increased or decreased by each bin
	int *waitHoldArray = unit->m_waitHoldArray; //how long each bin will hold at 0 after it is ducked
	int *waitGoArray = unit->m_waitGoArray; //how long the bin will be allowed to feed back before it is ducked
	int *tripCountArray = unit->m_tripCountArray; //the amount of times the bin has been ducked since the last ducking extension
	int *tripWaitArray = unit->m_tripWaitArray; //how long the extended ducking will be

	//traverse the array and find the summed magnitude
	for (int i=0; i<numbins; ++i) {
		mags[i] = mags[i]*mulFactor + p->bin[i].mag;
	}

	
	for (int i=0; i<highestBin; ++i) {

		if (tripWaitArray[i]==0){
			if (waitHoldArray[i]==0){
				if(addArray[i]==0){
					if(mulArray[i]==0){
						//if addArray, mulArray, and waitHoldArray are 0, then increase the value of mulArray
						addArray[i] = (float)(1/(float)attackReleaseFrames);
					}
					else if(mags[i]>threshold){
						if(waitGoArray[i]<=0) {
							//if we have crossed the threshold and we are not waiting to go, start ducking the bin
							addArray[i] = (float)(-1/(float)attackReleaseFrames);
							tripCountArray[i] ++;
						}
						else waitGoArray[i]--;

					}
				}

				else {
					//if addArray is not 0, then we are in a state where we are changing the mulArray
					mulArray[i] += addArray[i];
					if (mulArray[i]<0)
					{
						//once mulArray passes below 0, the bin is being fully ducked, so we enter the hold state
						mulArray[i] = 0.f;
						addArray[i] = 0.f;
						waitHoldArray[i] = sustainZeroFrames;
						if (tripCountArray[i]>=tripCount){
							tripWaitArray[i] = tripBlockFrames;
							tripCountArray[i] = 0;
						}	
					}
					else if (mulArray[i]>1){
						//when the mulArray goes back above 1, the bin is ready to start over
						mulArray[i] = 1.0f;
						addArray[i] = 0.f;
						waitGoArray[i] = waitGoFrames;

					}
				}

			}
			else {
				waitHoldArray[i]--;
			}
		}
		else {
			tripWaitArray[i]--;
		}	
	}

	for (int i=0; i<numbins; ++i) {
		q->bin[i].phase = 0;
		q->bin[i].mag = mulArray[i];
	}

}

void PV_Control2_Dtor(PV_Control2* unit)
{
	RTFree(unit->mWorld, unit->m_mags);
}

#define DefinePVUnit(name) \
(*ft->fDefineUnit)(#name, sizeof(PV_Unit), (UnitCtorFunc)&name##_Ctor, 0, 0);

PluginLoad(PV_Control2)
{
    // InterfaceTable *inTable implicitly given as argument to the load function
    ft = inTable; // store pointer to InterfaceTable
    //init_SCComplex(inTable);

    DefineDtorUnit(PV_Control2);
}