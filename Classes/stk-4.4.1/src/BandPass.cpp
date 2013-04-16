/***************************************************/
/*! \class BandPass
 \brief STK BandPass (two-pole, two-zero) filter class.
 
 This class implements a bandpass filter using biquad filter.
 
 by Sang Won Lee
 */
/***************************************************/

#include "BandPass.h"
#include <cmath>

namespace stk {
    
    BandPass :: BandPass() : Filter()
    {
        b_.resize( 3, 0.0 );
        a_.resize( 3, 0.0 );
        b_[0] = 1.0;
        a_[0] = 1.0;
        inputs_.resize( 3, 1, 0.0 );
        outputs_.resize( 3, 1, 0.0 );
        setResonance(440, 0.1);
        Stk::addSampleRateAlert( this );
    }
    
    BandPass :: ~BandPass()
    {
        Stk::removeSampleRateAlert( this );
    }
    
    void BandPass :: sampleRateChanged( StkFloat newRate, StkFloat oldRate )
    {
        if ( !ignoreSampleRateChange_ ) {
            errorString_ << "BandPass::sampleRateChanged: you may need to recompute filter coefficients!";
            handleError( StkError::WARNING );
        }
    }
    
    void BandPass ::setFrequency( StkFloat _frequency){
        setResonance(_frequency, this->q);
    }
    void BandPass ::setQ( StkFloat _radius){
        setResonance(frequency, _radius);
    }
    
    
    void BandPass :: setResonance(StkFloat frequency, StkFloat radius)
    {
        this->frequency = frequency;
        this->q = radius;
        
        // the code below is added.
#ifndef M_LN2
#define M_LN2	   0.69314718055994530942
#endif
        double omega = TWO_PI * frequency/Stk::sampleRate();
        double sn = sin(omega);
        double cs = cos(omega);
        double alpha = sn * sinh(M_LN2 /2 * radius * omega /sn);
        
      //  b_[0] = alpha;
        b_[0] = sn/2;
        b_[1] = 0;
        b_[2] = -sn/2;
       // b_[2] = -alpha;
        a_[0] = 1 + alpha;
        a_[1] = -2 * cs;
        a_[2] = 1 - alpha;
        
        b_[0] /=a_[0];
        b_[1] /=a_[0];
        b_[2] /=a_[0];
        a_[1] /=a_[0];
        a_[2] /=a_[0];
        a_[0] = 1;
        
    }
    
} // stk namespace
