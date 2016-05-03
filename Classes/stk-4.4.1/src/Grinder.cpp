
#include "Grinder.h"
#include <iostream.h>

#undef IMMEDIATE_ONSET
#define DELAYEDOUT

namespace stk {
    
Grinder :: Grinder(StkFloat grainthres)
{
  grain_threshold = grainthres;
  grain_delay = 0;
  norm_delay = NORMDELAY;
  lastin = 0.0;
  zero_cross = 0.0;
  max_fin = 0.0;
  zero_hist.setDelay(NORMDELAY);
  zero_delay = NORMDELAY;
  zero_hist.clear();
}

Grinder :: ~Grinder()
{
}


StkFloat Grinder :: tick(StkFloat input)
{
  StkFloat absinput =input >=0.0?input:-input;
  if(grain_delay > 0)
  {
	  grain_delay--;
#ifdef DELAYEDOUT
	  if(grain_delay == 0)
	  {
          lastout = delayed_out;
		  return delayed_out;
	  }
#endif
  }

  if((lastin == 0.0 && input != 0.0) || (lastin > 0.0 && input < 0.0) || (lastin < 0.0 && input > 0.0))
  {
    zero_cross += 1;
	zero_hist.tick(1);
  }
  else
    zero_hist.tick(0);

  zero_cross -= zero_hist.lastOut();
  if(zero_cross < 0)
	  cerr << "YIKES!";
  lastin = absinput;

#ifdef IMMEDIATE_ONSET
  if(grain_delay == 0 && absinput>grain_threshold)
  {
//	  cerr << "!"<< absinput << ":" << zero_cross;
	grain_delay = norm_delay;
	max_fin=0.0;
#ifdef DELAYEDOUT
	delayed_out = absinput;
//    lastout = 0.0;
	return 0.0;
#else
    lastout = absinput;
	return absinput;
#endif
  }
#else
  if(grain_delay == 0 && absinput>grain_threshold)
  {
	if(max_fin>0.0)
	{
		if(absinput > max_fin)
		{
//			cerr << max_fin;
			max_fin = absinput;
		}
		else
		{
			absinput = max_fin;
			max_fin = 0.0;
//		    cerr << "!"<< absinput << ":" << zero_cross;
			grain_delay = norm_delay;
#ifdef DELAYEDOUT
			delayed_out = absinput;
//            lastout = 0.0;
			return 0.0;
#else
            lastout = absinput;
			return absinput;
#endif
		}
	}
	else
		max_fin = absinput;
//	return absinput;
  }
#endif
    lastout = 0.0;
  return 0.0;

}

void Grinder :: sampleRateChanged( StkFloat newRate, StkFloat oldRate )
{
}
    

    
} // stk namespace
