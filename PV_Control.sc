PV_Control : PV_ChainUGen
{
	*new { arg buffer0, buffer1, thresh=0.6, mulFactor=0.8, attackReleaseFrames=50, sustainZeroFrames= 100, waitGoFrames=50, tripCount=2,  tripBlockFrames=500, highestBin=400;
		^this.multiNew('control', buffer0, buffer1, thresh, mulFactor, attackReleaseFrames, sustainZeroFrames, waitGoFrames, tripCount, tripBlockFrames, highestBin)
	}
}