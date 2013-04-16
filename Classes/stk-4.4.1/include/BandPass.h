#ifndef STK_BANDPASS_H
#define STK_BANDPASS_H

#include "Filter.h"

namespace stk {
    
    /***************************************************/
    /*! \class BandPass
     \brief STK BandPass (two-pole, two-zero) filter class.
     
     This class implements bandpass filter using biquad filter
     Methods are provided for creating a resonance in the
     frequency response while maintaining a constant filter gain.
     this file has been modified by Sang Won Lee(sangwonlee717@gmail.com)
     The original code is based on band pass filter type implemetned in
     http://www.musicdsp.org/files/biquad.c
     
     */
    /***************************************************/
    
    class BandPass : public Filter
    {
    public:
        
        //! Default constructor creates a second-order pass-through filter.
        BandPass();
        
        //! Class destructor.
        ~BandPass();
        
        //! A function to enable/disable the automatic updating of class data when the STK sample rate changes.
        void ignoreSampleRateChange( bool ignore = true ) { ignoreSampleRateChange_ = ignore; };
        
        //! Sets the filter coefficients for a resonance at \e frequency (in Hz).
        /*!
         This method determines the filter coefficients corresponding to
         two complex-conjugate poles with the given \e frequency (in Hz)
         and \e radius from the z-plane origin.  If \e normalize is true,
         the filter zeros are placed at z = 1, z = -1, and the coefficients
         are then normalized to produce a constant unity peak gain
         (independent of the filter \e gain parameter).  The resulting
         filter frequency response has a resonance at the given \e
         frequency.  The closer the poles are to the unit-circle (\e radius
         close to one), the narrower the resulting resonance width.
         */
        void setResonance( StkFloat frequency, StkFloat radius);
        void setFrequency( StkFloat frequency);
        void setQ( StkFloat radius);
        //! Return the last computed output value.
        StkFloat lastOut( void ) const { return lastFrame_[0]; };
        
        //! Input one sample to the filter and return a reference to one output.
        StkFloat tick( StkFloat input );
        
        //! Take a channel of the StkFrames object as inputs to the filter and replace with corresponding outputs.
        /*!
         The StkFrames argument reference is returned.  The \c channel
         argument must be less than the number of channels in the
         StkFrames argument (the first channel is specified by 0).
         However, range checking is only performed if _STK_DEBUG_ is
         defined during compilation, in which case an out-of-range value
         will trigger an StkError exception.
         */
        StkFrames& tick( StkFrames& frames, unsigned int channel = 0 );
        
        //! Take a channel of the \c iFrames object as inputs to the filter and write outputs to the \c oFrames object.
        /*!
         The \c iFrames object reference is returned.  Each channel
         argument must be less than the number of channels in the
         corresponding StkFrames argument (the first channel is specified
         by 0).  However, range checking is only performed if _STK_DEBUG_
         is defined during compilation, in which case an out-of-range value
         will trigger an StkError exception.
         */
        StkFrames& tick( StkFrames& iFrames, StkFrames &oFrames, unsigned int iChannel = 0, unsigned int oChannel = 0 );
        
    protected:
        double frequency;
        double q;
        
        virtual void sampleRateChanged( StkFloat newRate, StkFloat oldRate );
    };
    
    inline StkFloat BandPass :: tick( StkFloat input )
    {
        inputs_[0] = gain_ * input;
        lastFrame_[0] = b_[0] * inputs_[0] + b_[1] * inputs_[1] + b_[2] * inputs_[2];
        lastFrame_[0] -= a_[2] * outputs_[2] + a_[1] * outputs_[1];
        inputs_[2] = inputs_[1];
        inputs_[1] = inputs_[0];
        outputs_[2] = outputs_[1];
        outputs_[1] = lastFrame_[0];
        
        return lastFrame_[0];
    }
    
    inline StkFrames& BandPass :: tick( StkFrames& frames, unsigned int channel )
    {
#if defined(_STK_DEBUG_)
        if ( channel >= frames.channels() ) {
            errorString_ << "BandPass::tick(): channel and StkFrames arguments are incompatible!";
            handleError( StkError::FUNCTION_ARGUMENT );
        }
#endif
        
        StkFloat *samples = &frames[channel];
        unsigned int hop = frames.channels();
        for ( unsigned int i=0; i<frames.frames(); i++, samples += hop ) {
            inputs_[0] = gain_ * *samples;
            *samples = b_[0] * inputs_[0] + b_[1] * inputs_[1] + b_[2] * inputs_[2];
            *samples -= a_[2] * outputs_[2] + a_[1] * outputs_[1];
            inputs_[2] = inputs_[1];
            inputs_[1] = inputs_[0];
            outputs_[2] = outputs_[1];
            outputs_[1] = *samples;
        }
        
        lastFrame_[0] = outputs_[1];
        return frames;
    }
    
    inline StkFrames& BandPass :: tick( StkFrames& iFrames, StkFrames& oFrames, unsigned int iChannel, unsigned int oChannel )
    {
#if defined(_STK_DEBUG_)
        if ( iChannel >= iFrames.channels() || oChannel >= oFrames.channels() ) {
            errorString_ << "BandPass::tick(): channel and StkFrames arguments are incompatible!";
            handleError( StkError::FUNCTION_ARGUMENT );
        }
#endif
        
        StkFloat *iSamples = &iFrames[iChannel];
        StkFloat *oSamples = &oFrames[oChannel];
        unsigned int iHop = iFrames.channels(), oHop = oFrames.channels();
        for ( unsigned int i=0; i<iFrames.frames(); i++, iSamples += iHop, oSamples += oHop ) {
            inputs_[0] = gain_ * *iSamples;
            *oSamples = b_[0] * inputs_[0] + b_[1] * inputs_[1] + b_[2] * inputs_[2];
            *oSamples -= a_[2] * outputs_[2] + a_[1] * outputs_[1];
            inputs_[2] = inputs_[1];
            inputs_[1] = inputs_[0];
            outputs_[2] = outputs_[1];
            outputs_[1] = *oSamples;
        }
        
        lastFrame_[0] = outputs_[1];
        return iFrames;
    }
    
} // stk namespace

#endif

