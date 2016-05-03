
#ifndef __GRINDER_H_
#define __GRINDER_H_

#include "Stk.h"
#include "Generator.h"
#include "Delay.h"

#define NORMDELAY 1000

namespace stk {
    

class Grinder : public Generator
{
public:
	Grinder(StkFloat thresh=0.025);
	~Grinder();
	StkFloat tick(StkFloat sample);
	StkFloat zeroCross() {return zero_cross/zero_delay>1.0?1.0:zero_cross/zero_delay;};
	void setThreshold(StkFloat thres) { grain_threshold = thres; }
	void setZeroFrame(int zf) { zero_delay = zf; zero_hist.setDelay(zf); }
	void setGrainDelay(int gd) { norm_delay = gd*4; }
    StkFloat lastOut() { return lastout; }
    void sampleRateChanged( StkFloat newRate, StkFloat oldRate );
    virtual StkFrames& tick( StkFrames& frames, unsigned int channel = 0 );
private:
  int grain_delay;
  int norm_delay;
  StkFloat grain_threshold;
  Delay zero_hist;
  StkFloat zero_cross;
  int zero_delay;
  StkFloat lastin;
  StkFloat delayed_out;
  StkFloat max_fin;
  StkFloat lastout;
};

    inline StkFrames& Grinder :: tick( StkFrames& frames, unsigned int channel )
    {
#if defined(_STK_DEBUG_)
        if ( channel >= frames.channels() ) {
            errorString_ << "ADSR::tick(): channel and StkFrames arguments are incompatible!";
            handleError( StkError::FUNCTION_ARGUMENT );
        }
#endif
        
        StkFloat *samples = &frames[channel];
        unsigned int hop = frames.channels();
        for ( unsigned int i=0; i<frames.frames(); i++, samples += hop )
            *samples = Grinder::tick(0.0);
        
        return frames;
    }
    
    
} // stk namespace
#endif