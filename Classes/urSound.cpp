/*
 *  urSound.c
 *  urMus
 *
 *  Created by gessl on 9/13/09.
 *  Copyright 2009 Georg Essl. All rights reserved. See LICENSE.txt for license conditions.
 *
 */

#include "urSound.h"
#include "RIOAudioUnitLayer.h"
#include "urSTK.h"
#include "urSoundAtoms.h"
#include "FileWrite.h"
#include "FileRead.h"

//#undef LOAD_STK_OBJECTS
#define LOAD_STK_OBJECTS

// Parameter conversions
// From SpeedDial
// -1:1->FreqSpace: 55.0*pow(2.0,96*nrparam/12.0);

#define URS_SINKLISTSTARTSIZE 30

ursSinkList::ursSinkList() 
{
	sinks = new urSoundOut*[URS_SINKLISTSTARTSIZE]; 
	length = 0; 
	allocsize = URS_SINKLISTSTARTSIZE;
}


ursSinkList::~ursSinkList()
{
	for(int i=0; i<length; i++)
		sinks[i] = NULL;
	delete sinks;
}

void ursSinkList::AddSink(urSoundOut* sink)
{
	if(length < allocsize)
	{
		sinks[length++] = sink;
	}
	else
	{
		/* NYI */
        int a = 0;
        assert(0);
	}
}

void ursSinkList::RemoveSink(urSoundOut* sink)
{
	for(int i=0; i<length; i++)
	{
		if(sink == sinks[i])
		{
			for(; i<length; i++)
			{
				sinks[i] = sinks[i+1];
			} 
			length--; 
			sinks[length] = NULL;
		}
	}
}

void ursSinkList::RemoveObject(ursObject* obj)
{
    for(int i=0; i<length; i++)
    {
        if(obj == sinks[i]->object)
            RemoveSink(sinks[i]); // Inefficient
    }
}

//ursSinkList urActiveDacTickSinkList;
//ursSinkList urActiveDacArraySinkList;

//ursSinkList urActiveVisTickSinkList;
//ursSinkList urActiveDrainTickSinkList;
//ursSinkList urActiveDrainArraySinkList;
//ursSinkList urActiveNetTickSinkList;

//ursSinkList urActiveFlashTickSinkList;

//ursSinkList urActiveAudioFrameSinkList;

extern int freePatches[];
extern int currentPatch;

int FreeAllFlowboxes(int patch);

/*
void urs_PullActiveDacSinks(SInt16 *buff, UInt32 len)
{
	ursObject *self;
	double out = 0.0;
    
	for(int i=0; i < urActiveDacArraySinkList.length; i++)
	{
		self = urActiveDacArraySinkList.sinks[i]->object;
		urActiveDacArraySinkList.sinks[i]->outFuncFillBuffer(self,buff,len);
	}
	for(int i=0; i<urActiveDacTickSinkList.length; i++)
	{
		self = urActiveDacTickSinkList.sinks[i]->object;
		for(int j=0; j<len; j++)
		{
			out = urActiveDacTickSinkList.sinks[i]->outFuncTick(self);// /65536;
			if(i==0)
				buff[j] = (SInt16)32767*out;
			else
				buff[j] += (SInt16)32767*out;
		}
	}
//	if(urActiveDacTickSinkList.length==0 && urActiveDacArraySinkList.length==0)
//	{
//		memset(buff,0,sizeof(SInt16)*len);
//	}
}
*/
 
void urs_PullActiveTickSinks(ursSinkList& urActiveTickSinkList, SInt16 *buff, UInt32 len)
{
	ursObject *self;
	double out;

	for(int i=0; i<urActiveTickSinkList.length; i++)
	{
		self = urActiveTickSinkList.sinks[i]->object;
		for(int j=0; j<len; j++)
		{
			out = urActiveTickSinkList.sinks[i]->outFuncTick(self);
			buff[j] = (SInt16)32767*out;
		}
	}
}

extern pthread_mutex_t fb_mutex;

double urs_PullActiveSingleTickSinks(ursSinkList& urActiveTickSinkList)
{
	ursObject *self;
	double out=0.0;

    pthread_mutex_lock( &fb_mutex );
    
    

    
	for(int i=0; i<urActiveTickSinkList.length; i++)
	{
		self = urActiveTickSinkList.sinks[i]->object;
        out = out + urActiveTickSinkList.sinks[i]->outFuncTick(self);
	}
    pthread_mutex_unlock( &fb_mutex );

	return out;
}

double dacindata = 0.0;

#ifdef FLAGBASEDCAFB
extern bool fb_clearing;
#endif

double urs_PullActiveDacSingleTickSinks()
{
    double res = 0.0;

    
    
#ifndef FLAGBASEDCAFB
    pthread_mutex_lock( &fb_mutex );
    res = dacindata + dacobject->CallAllPullIns();
    pthread_mutex_unlock( &fb_mutex );
#else
    if(!fb_clearing)
    {
        pthread_mutex_unlock( &fb_mutex );
        res = dacindata + dacobject->CallAllPullIns();
    }
    else
    {
        FreeAllFlowboxes(currentPatch);
        fb_clearing = false;
        pthread_mutex_unlock( &fb_mutex );
    }
#endif
	//res = dacindata + urs_PullActiveSingleTickSinks(urActiveDacTickSinkList);
	dacindata = 0.0;
	return res;
}

void urs_PullActiveArraySinks(ursSinkList& urActiveArraySinkList, SInt16 *buff, UInt32 len)
{
	ursObject *self;
	
	for(int i=0; i < urActiveArraySinkList.length; i++)
	{
		self = urActiveArraySinkList.sinks[i]->object;
		urActiveArraySinkList.sinks[i]->outFuncFillBuffer(self,buff,len);
	}
}

/*
#define DRAINBUFFER_MAXSIZE 512
SInt16 drainbuffer[DRAINBUFFER_MAXSIZE];
UInt32 drainbufferlen = DRAINBUFFER_MAXSIZE;

void urs_PullActiveDrainSinks(UInt32 len)
{
	urs_PullActiveTickSinks(urActiveDrainTickSinkList, drainbuffer, len);
}

void urs_PullActiveDrainFrameSinks(UInt32 len)
{
	urs_PullActiveArraySinks(urActiveDrainArraySinkList, drainbuffer, len);
}

void urs_PullActiveAudioFrameSinks()
{
	urs_PullActiveSingleTickSinks(urActiveAudioFrameSinkList);
}
*/
double visindata = 0.0;
double visoutdata = 0.0;

double urs_PullActiveVisSinks()
{
    double res = 0.0;
    
    pthread_mutex_lock( &fb_mutex );
    res = visindata + visobject->CallAllPullIns();
    pthread_mutex_unlock( &fb_mutex );
    
//	double res = visindata + urs_PullActiveSingleTickSinks(urActiveVisTickSinkList);
//	visindata = 0.0;
	return res;
}
/*
double urs_PullActiveFlashSinks()
{
	double res = urs_PullActiveSingleTickSinks(urActiveVisTickSinkList);
	return res;
}
*/
void urs_PullVis()
{
	visoutdata = urs_PullActiveVisSinks();
}

double netindata = 0.0;
double netoutdata = 0.0;

double urs_PullActiveNetSinks()
{
    double res = 0.0;
    
    pthread_mutex_lock( &fb_mutex );
    res = netindata + netobject->CallAllPullIns();
    pthread_mutex_unlock( &fb_mutex );
//	double res = netindata + urs_PullActiveSingleTickSinks(urActiveNetTickSinkList);
	//	visindata = 0.0;
	return res;
}

void urs_PullNet()
{
	netoutdata = urs_PullActiveNetSinks();
}

/*
double pullindata = 0.0;

double urs_PullActivePullSinks()
{
	double res = pullindata + urs_PullActiveSingleTickSinks(urActiveVisTickSinkList);
	pullindata = 0.0;
	return res;
}
*/
#define URS_SOURCELISTSTARTSIZE 10

double lastcambright = 0.0;
double lastcamblue = 0.0;
double lastcamgreen = 0.0;
double lastcamred = 0.0;
double lastcamedge = 0.0;

void callAllCameraSources(double brightness, double blueTotal, double greenTotal, double redTotal, double edginess)
{
	if(cameraObject)
	{
        pthread_mutex_lock( &fb_mutex );
        lastcambright = brightness;
        lastcamblue = blueTotal;
        lastcamgreen = greenTotal;
        lastcamred = redTotal;
        lastcamedge = edginess;
		cameraObject->CallAllPushOuts(brightness,0);
		cameraObject->CallAllPushOuts(blueTotal,1);
		cameraObject->CallAllPushOuts(greenTotal,2);
		cameraObject->CallAllPushOuts(redTotal,3);
		cameraObject->CallAllPushOuts(edginess,4);	
        pthread_mutex_unlock( &fb_mutex );
	}
}


float lastaccelx = 0.0;
float lastaccely = 0.0;
float lastaccelz = 0.0;

void callAllAccelerateSources(double tilt_x, double tilt_y, double tilt_z)
{
	double tilt;

    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<3; i++) // for all 3 dimensions
	{
		switch(i)
		{
			case 0 : tilt = lastaccelx = tilt_x; break;
			case 1 : tilt = lastaccely = tilt_y; break;
			case 2 : tilt = lastaccelz = tilt_z; break;
		}
		
		accelobject->CallAllPushOuts(tilt, i);
	}
    pthread_mutex_unlock( &fb_mutex );

}

double lastrotratex = 0.0;
double lastrotratey = 0.0;
double lastrotratez = 0.0;

void callAllGyroSources(double rate_x, double rate_y, double rate_z)
{
	double rate;
	
    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<3; i++) // for all 3 dimensions
	{
		switch(i)
		{
			case 0 : rate = lastrotratex = rate_x; break;
			case 1 : rate = lastrotratey = rate_y; break;
			case 2 : rate = lastrotratez = rate_z; break;
		}
		
		gyroobject->CallAllPushOuts(rate, i);
	}
    pthread_mutex_unlock( &fb_mutex );
}

double lastcompassx = 0.0;
double lastcompassy = 0.0;
double lastcompassz = 0.0;
double lastcompassnorth = 0.0;

void callAllCompassSources(double heading_x, double heading_y, double heading_z, double heading_north)
{
	double heading;
	
    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<4; i++) // for all 3 dimensions
	{
		switch(i)
		{
			case 0 : heading = lastcompassx = heading_x; break;
			case 1 : heading = lastcompassy = heading_y; break;
			case 2 : heading = lastcompassz = heading_z; break;
			case 3 : heading = lastcompassnorth = heading_north; break;
		}
		
		compassobject->CallAllPushOuts(heading, i);
	}
    pthread_mutex_unlock( &fb_mutex );
}

double lastlocationlat = 0.0;
double lastlocationlong = 0.0;

void callAllLocationSources(double latitude, double longitude)
{
	double coord;
	
    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<2; i++) // for all 2 dimensions
	{
		switch(i)
		{
			case 0 : coord = lastlocationlat = latitude; break;
			case 1 : coord = lastlocationlong = longitude; break;
		}
		
		locationobject->CallAllPushOuts(coord, i);
	}
    pthread_mutex_unlock( &fb_mutex );
}

double lasttouchx[11];
double lasttouchy[11];

void callAllTouchSources(double touch_x, double touch_y, int idx)
{
	double touch;
	
    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<2; i++) // for all 2 dimensions
	{
		switch(i)
		{
			case 0 : touch = lasttouchx[idx] = touch_x; break;
			case 1 : touch = lasttouchy[idx] = touch_y; break;
		}
		
		touchobject->CallAllPushOuts(touch, i+2*idx);
	}
    pthread_mutex_unlock( &fb_mutex );
}

double lastmic;

void callAllMicSources(SInt16* buff, UInt32 len)
{

    pthread_mutex_lock( &fb_mutex );
	for(int i=0; i<len; i++)
	{
		micobject->CallAllPushOuts(buff[i]/32768.0);
	}
    lastmic = buff[len-1];
    pthread_mutex_unlock( &fb_mutex );
}

void callAllMicSingleTickSourcesF(double data)
{
    pthread_mutex_lock( &fb_mutex );
    lastmic = data;
    micobject->CallAllPushOuts(data);
    pthread_mutex_unlock( &fb_mutex );
}

void callAllMicSingleTickSources(SInt16 data)
{
	
    pthread_mutex_lock( &fb_mutex );
    lastmic = data/32768.0;
	micobject->CallAllPushOuts(data/32768.0);
    pthread_mutex_unlock( &fb_mutex );
}

void callAllNetSingleTickSources(SInt16 data)
{
    if(netinobject != NULL)
    {
        pthread_mutex_lock( &fb_mutex );
        netinobject->lastindata[0] = (float)data/128.0;//32768.0;
        netinobject->CallAllPushOuts(data/128.0);//32768.0);
        pthread_mutex_unlock( &fb_mutex );
    }
}

double NetIn_Tick(ursObject* gself)
{
	double res;
	res = gself->lastindata[0];
	
//	res += gself->CallAllPullIns();
	return res;
}

double NetIn_Out(ursObject* gself)
{
	return gself->lastindata[0];//+gself->CallAllPullIns();
}



ursObject::ursObject(const char* objname, void* (*objconst)(), void (*objdest)(ursObject*),int nrins, int nrouts, bool dontinstance, bool coupled, ursObjectArray* instancearray, char* objnote)
{
	nr_ins = nrins;
	nr_outs = nrouts;
#ifdef OLDINOUTS
	ins = new urSoundIn[nrins];
	outs = new urSoundOut[nrouts];
	firstpullin = new urSoundPullIn*[nrins];
	firstpushout = new urSoundPushOut*[nrouts];
	for(int i=0; i< nrins ; i++)
    {
        ins[i].object = this;
		firstpullin[i] = NULL;
    }
	for(int i=0; i< nrouts; i++)
    {
        outs[i].object = this;
		firstpushout[i] = NULL;
    }
#endif
	lastin = 0;
	lastout = 0;
	lastindata[0] = 0.0;
	indatapos = 0;
	outdatapos = 0;
	filled = false;
	noninstantiable = dontinstance;
	if(!noninstantiable && instancearray == NULL)
	{
		instancelist = new ursObjectArray(6);
		instancenumber = 0;
		instancelist->Append(this);
	}
	else if(!noninstantiable)
	{
		instancelist = instancearray;
		instancenumber = instancelist->Last();
		instancelist->Append(this);
	}
	else
    {
        instancelist = NULL;
		instancenumber = 0;
    }
	name = objname;
	
	couple_in = -1;
	couple_out = -1;
	iscoupled = coupled;
	
	DataConstructor = objconst;
	DataDestructor = objdest;
	if(DataConstructor != NULL)
		objectdata = DataConstructor();
	fed = false;
    
    note = objnote;
}

ursObject::~ursObject()
{
#ifdef OLDINOUTS
    if(instancelist)
        instancelist->Remove(this);
	delete ins;
	delete outs;
    delete firstpullin;
    delete firstpushout;
#endif
}

ursObject* ursObject::Clone()
{
	if(noninstantiable)
		return this;

	ursObject* clone = new ursObject(name, DataConstructor, DataDestructor, nr_ins, nr_outs, this->noninstantiable, this->iscoupled, this->instancelist);
	
	for(int i=0; i<lastin; i++)
		clone->AddIn(ins[i].name,ins[i].semantics,ins[i].inFuncTick);
	
	for(int i=0; i<lastout; i++)
		clone->AddOut(outs[i].name,outs[i].semantics,outs[i].outFuncTick,outs[i].outFuncValue,outs[i].outFuncFillBuffer);

	return clone;
}


void ursObject::AddOut(const char* outname, const char* outsemantics, double (*func)(ursObject *), double (*func3)(ursObject *), void (*func2)(ursObject*, SInt16*, UInt32))
{
#ifdef OLDINOUTS

	if(lastout >= nr_outs)
	{
		int a = 0;
		/* NYI gotta grow here */
        assert(0);
	}
/*	char* str = (char*)malloc(strlen(outname)+1);
	strcpy(str, outname);
	outs[lastout].name = str;
	str = (char *)malloc(strlen(outsemantics)+1);
	strcpy(str, outsemantics);
	outs[lastout].semantics = str;*/
    outs[lastout].name = outname;
	outs[lastout].semantics = outsemantics;
	outs[lastout].outFuncTick = func;
	outs[lastout].outFuncFillBuffer = func2;
	outs[lastout].outFuncValue = func3;
	outs[lastout].object = this;
	outs[lastout].data = this->objectdata;
	lastout++;
#else
    urSoundOut newout;
    newout.name = outname;
	newout.semantics = outsemantics;
	newout.outFuncTick = func;
	newout.outFuncFillBuffer = func2;
	newout.outFuncValue = func3;
	newout.object = this;
	newout.data = this->objectdata;
    
    outs.push_back(newout);
    firstpushout.push_back((urSoundPushOut*)NULL);
#endif
}

void ursObject::AddIn(const char* inname, const char* insemantics, void (*func)(ursObject *, double))
{
#ifdef OLDINOUTS
	if(lastin >= nr_ins)
	{
		int a = 0;
		// NYI gotta grow here
        assert(0);
	}
	ins[lastin].name = inname;
	ins[lastin].semantics = insemantics;
	ins[lastin].inFuncTick = func;
	ins[lastin].object = this;
	ins[lastin].data = this->objectdata;
	lastin++;
#else
    urSoundIn newin;
	newin.name = inname;
	newin.semantics = insemantics;
	newin.inFuncTick = func;
	newin.object = this;
	newin.data = this->objectdata;
    
    ins.push_back(newin);
    firstpullin.push_back((urSoundPullIn*)NULL);
#endif

}

void ursObject::CallAllPushOuts(double indata, int idx)
{
#ifdef OLDINOUTS
	if(this == NULL || this->firstpushout == NULL) return;
#else
    if(this == NULL || this->firstpushout.size() == 0) return;
#endif
    
#ifdef RELOCATE_FAFB
    if(this->noninstantiable && freePatches[currentPatch] != 0) return;
#endif
    
	if(this->firstpushout[idx]!=NULL)
	{
		ursObject* inobject;
		urSoundPushOut* pushto = this->firstpushout[idx];
		for(;pushto!=NULL; pushto = pushto->next)
		{	
#ifdef RELOCATE_FAFB
            if(freePatches[currentPatch] == 0)
            {
#endif
               urSoundIn* in = pushto->in;
                inobject = in->object;

#ifdef RELOCATE_FAFB
                if(this->noninstantiable && freePatches[currentPatch] != 0) return;
#endif
                in->inFuncTick(inobject, indata);
#ifdef RELOCATE_FAFB
            }
#endif
		}
	}
}

void ursObject::FeedAllPullIns(int minidx)
{
	if(fed != true)
	{
		fed = true;
		
		for(int i=minidx; i< nr_ins; i++)
		{
			if(firstpullin[i]!=NULL)
			{
				ins[i].inFuncTick(this, CallAllPullIns(i));
			}
		}
		fed = false;
	}
}

double ursObject::CallAllPullIns(int idx)
{
	double res = 0.0;
	if(this->firstpullin[idx]!=NULL)
	{
		ursObject* outobject;
		urSoundPullIn* pullfrom = this->firstpullin[idx];
		for(; pullfrom != NULL; pullfrom = pullfrom->next)
		{	
			urSoundOut* out = pullfrom->out;
			outobject = out->object;
			res = res + out->outFuncTick(outobject);
		}
	}
	return res;
}

void ursObject::AddPushOut(int idx, urSoundIn* in)
{
	urSoundPushOut* self = new urSoundPushOut;
	self->in = in;
	in->isplaced = true;
	self->next = NULL;
	if(firstpushout[idx] == NULL)
	{
		firstpushout[idx] = self;
	}
	else
	{
		urSoundPushOut* finder = firstpushout[idx];
		for(;finder->next != NULL; finder = finder->next)
		{
		}
		finder->next = self;
	}
}

void ursObject::AddPullIn(int idx, urSoundOut* out)
{
	urSoundPullIn* self = new urSoundPullIn;
	self->out = out;
	out->isplaced = true;
	self->next = NULL;
	if(firstpullin[idx] == NULL)
	{
		firstpullin[idx] = self;
	}
	else
	{
		urSoundPullIn* finder = firstpullin[idx];
		for(;finder->next != NULL; finder = finder->next)
		{
		}
		finder->next = self;
	}
    
//    if(!strcmp(this->name,dacobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveDacTickSinkList.AddSink(out);
	
//	if(!strcmp(this->name,visobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveVisTickSinkList.AddSink(out);
    
//	if(!strcmp(this->name,netobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveNetTickSinkList.AddSink(out);

}

bool ursObject::IsPushedOut(int idx, urSoundIn* in)
{
	if(firstpushout[idx] == NULL )
		return false;
	
	urSoundPushOut* finder = firstpushout[idx];
	for(;finder != NULL && finder->in != in ; finder = finder->next)
	{
	}
	if(finder != NULL && finder->in == in)
	{
		return true;
	}
	return false;
}

bool ursObject::IsPulledIn(int idx, urSoundOut* out)
{
	if(firstpullin[idx] == NULL)
		return false;
	
	urSoundPullIn* finder = firstpullin[idx];
	for(;finder != NULL && finder->out != out; finder = finder->next)
	{
	}
	if(finder != NULL && finder->out == out)
	{
		return true;
	}
	return false;
}

void ursObject::RemovePushOut(int idx, urSoundIn* in)
{
	in->isplaced = false;
	if(firstpushout[idx] == NULL)
		return;
	
	urSoundPushOut* finder = firstpushout[idx];
	urSoundPushOut* previous = NULL;
	for(;finder != NULL && finder->in != in ; finder = finder->next)
	{
		previous = finder;
	}
	if(finder != NULL && finder->in == in)
	{
		if(previous != NULL)
			previous->next = finder->next;
		
		if (firstpushout[idx] == finder)
			firstpushout[idx] = NULL;
		delete finder;
	}
}

void ursObject::RemoveFromSink(urSoundOut* out)
{
    dacobject->RemoveAllPullIns();
    dacobject->RemoveAllPushOuts();

    visobject->RemoveAllPullIns();
    visobject->RemoveAllPushOuts();

    netobject->RemoveAllPullIns();
    netobject->RemoveAllPushOuts();

//    if(!strcmp(this->name,dacobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveDacTickSinkList.RemoveSink(out);
	
//	if(!strcmp(this->name,visobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveVisTickSinkList.RemoveSink(out);
    
//	if(!strcmp(this->name,netobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
//		urActiveNetTickSinkList.RemoveSink(out);
   
}

void ursObject::RemoveFromSinks()
{
    
    dacobject->RemoveAllPullIns();
    dacobject->RemoveAllPushOuts();

    visobject->RemoveAllPullIns();
    visobject->RemoveAllPushOuts();

    netobject->RemoveAllPullIns();
    netobject->RemoveAllPushOuts();

//    urActiveDacTickSinkList.RemoveObject(this);
//    urActiveVisTickSinkList.RemoveObject(this);
//    urActiveNetTickSinkList.RemoveObject(this);
}

void ursObject::RemovePushOutsByObject(ursObject* src)
{
	for(int idx=0; idx<src->nr_outs; idx++)
	{
		urSoundPushOut* finder = src->firstpushout[idx];
        urSoundPushOut* findernext;
        if(finder)
            findernext = finder->next;
        while(finder != NULL)
		{
            //			finder = NULL;
            if(finder->in->object==this)
            {
                src->RemovePushOut(idx,finder->in);
            }
            finder = findernext;
            if(finder)
                findernext = finder->next;
		}
        src->firstpushout[idx] = NULL;
	}
    
}

void ursObject::RemoveFromSources()
{
    for(int i=0;i<ursourceobjectlist.Last();i++)
    {
        RemovePushOutsByObject(ursourceobjectlist[i]);
    }
}

void ursObject::RemoveAllPushOuts()
{
	for(int idx=0; idx<nr_outs; idx++)
	{
		urSoundPushOut* finder = firstpushout[idx];
        urSoundPushOut* findernext;
        if(finder)
            findernext = finder->next;
        while(finder != NULL)
		{
//			finder = NULL;
            delete finder;
            finder = findernext;
            if(finder)
                findernext = finder->next;
		}
        firstpushout[idx] = NULL;
	}
}

void ursObject::RemovePullIn(int idx, urSoundOut* out)
{
	out->isplaced = false;
	if(firstpullin[idx] == NULL)
		return;
	
	urSoundPullIn* finder = firstpullin[idx];
	urSoundPullIn* previous = NULL;
	for(;finder != NULL && finder->out != out; finder = finder->next)
	{
		previous = finder;
	}
	if(finder != NULL && finder->out == out)
	{
		if(previous != NULL)
			previous->next = finder->next;
		if (firstpullin[idx] == finder)
			firstpullin[idx] = NULL;
		delete finder;
	}

    RemoveFromSink(out);
}

void ursObject::RemoveAllPullIns()
{
	for(int idx=0; idx<nr_ins; idx++)
	{
		urSoundPullIn* finder = firstpullin[idx];
        urSoundPullIn* findernext;
        if(finder)
            findernext = finder->next;
        while(finder != NULL)
		{
            delete finder;
            finder = findernext;
            if(finder)
                findernext = finder->next;
		}
        firstpullin[idx] = NULL;

	}
}



void ursObject::SetCouple(int inidx, int outidx)
{
	couple_in = inidx;
	couple_out = outidx;
	iscoupled = true;
}

// An object array to help us keep objects by type and by instance.

ursObjectArray::ursObjectArray(int initmax)
{
	objectlist = new ursObject*[initmax];
	max = initmax;
	current = 0;
	for(int i=0; i< initmax; i++)
		objectlist[i] = NULL;
}

ursObjectArray::~ursObjectArray()
{
	for(int i=0; i<current; i++)
	{
		if(objectlist[i] != NULL)
			delete objectlist[i];
	}
	delete objectlist;
}

void ursObjectArray::TestObjects()
{
    for(int i=0; i<current; i++)
    {
        assert(objectlist[i]->nr_ins == objectlist[i]->lastin);
        assert(objectlist[i]->nr_outs == objectlist[i]->lastout);
    }
}

void ursObjectArray::Append(ursObject* object)
{
	if(current >= max)
	{
		max = max*2; // Yes we don't grow exponentially because we want to be kind to memory... and we definitely don't grow enough to make a huge dent in the asymptotic difference here.
		ursObject** newlist = new ursObject*[max];
        assert(newlist != NULL);
		for(int i=0; i<current; i++)
		{
			newlist[i] = objectlist[i];
			objectlist[i] = NULL;
		}
		delete objectlist;
		objectlist = newlist;
	}
	
	objectlist[current] = object;
	current++;
}

void ursObjectArray::Remove(ursObject* object)
{
    int i;
    for(i=0; i<current && objectlist[i] != object; i++)
    {
    }
    
    assert(i<current);
    
    for(;i< current-1; i++)
        objectlist[i] = objectlist[i+1];
    objectlist[i+1]=NULL;
    current--;
}

ursObject* ursObjectArray::Get(int idx)
{
	if(idx >=0 && idx < current)
		return objectlist[idx];
	else
		return NULL;
}

ursObject* ursObjectArray::operator[](int idx)
{
	if(idx >=0 && idx < current)
		return objectlist[idx];
	else
		return NULL;
}

//#define MAX_URMANIPULATOROBJECTS 40
//int lastmanipulatorobj = 0;
//ursObject* urmanipulatorobjectlist[MAX_URMANIPULATOROBJECTS];
ursObjectArray urmanipulatorobjectlist;

int urs_NumUrManipulatorObjects()
{
	return urmanipulatorobjectlist.Last(); //lastmanipulatorobj;
}

const char* urs_GetManipulatorObjectName(int pos)
{
	if(pos >= urmanipulatorobjectlist.Last()) return NULL;
	
	return urmanipulatorobjectlist[pos]->name;
}

int urs_NumUrManipulatorIns(int pos)
{
	return urmanipulatorobjectlist[pos]->nr_ins;
}

int urs_NumUrManipulatorOuts(int pos)
{
	return urmanipulatorobjectlist[pos]->nr_outs;
}

//#define MAX_URSOURCEOBJECTS 10
//int lastsourceobj = 0;
//ursObject* ursourceobjectlist[MAX_URSOURCEOBJECTS];
ursObjectArray ursourceobjectlist;

int urs_NumUrSourceObjects()
{
	return ursourceobjectlist.Last();
}

const char* urs_GetSourceObjectName(int pos)
{
	if(pos >= ursourceobjectlist.Last()) return NULL;
	
	return ursourceobjectlist[pos]->name;
}

int urs_NumUrSourceIns(int pos)
{
	return ursourceobjectlist[pos]->nr_ins;
}

int urs_NumUrSourceOuts(int pos)
{
	return ursourceobjectlist[pos]->nr_outs;
}


//#define MAX_URSINKOBJECTS 10
//int lastsinkobj = 0;
//ursObject* ursinkobjectlist[MAX_URSINKOBJECTS];
ursObjectArray ursinkobjectlist;

int urs_NumUrSinkObjects()
{
	return ursinkobjectlist.Last();
}

const char* urs_GetSinkObjectName(int pos)
{
	if(pos >= ursinkobjectlist.Last()) return NULL;
	
	return ursinkobjectlist[pos]->name;
}

int urs_NumUrSinkIns(int pos)
{
	return ursinkobjectlist[pos]->nr_ins;
}

int urs_NumUrSinkOuts(int pos)
{
	return ursinkobjectlist[pos]->nr_outs;
}

const char* urs_GetManipulatorIn(int pos, int in)
{
	return urmanipulatorobjectlist[pos]->ins[in].name;
}

const char* urs_GetSinkIn(int pos, int in)
{
	return ursinkobjectlist[pos]->ins[in].name;
}

const char* urs_GetSourceOut(int pos, int out)
{
	return ursourceobjectlist[pos]->outs[out].name;
}

const char* urs_GetManipulatorOut(int pos, int out)
{
	return urmanipulatorobjectlist[pos]->outs[out].name;
}

Loop::Loop(long len, long maxlen)
{
	loop = new double[maxlen];
	now = 0;
	looplength = len;
	startpos = -1;
	maxlength = maxlen;
}

Loop::~Loop()
{
	delete loop;
}

double Loop::Tick()
{
	now = now + 1 % looplength;
	return loop[now];
}

void Loop::SetNow(double indata)
{
	loop[now] = indata;
}

void Loop::SetAt(double indata, int pos)
{
	pos = pos % looplength;
	loop[pos] = indata;
}

void Loop::SetBoundary()
{
	if(startpos == -1)
	{
		startpos = now;
	}
	else
	{
		looplength = now-startpos;
		if(looplength > maxlength) looplength = maxlength;
		startpos = now;
	}
}


// DPS Objects (aka Unit Generators) below


double Accel_Y_Out(ursObject* gself)
{
    return lastaccely;
}

double Accel_X_Out(ursObject* gself)
{
    return lastaccelx;
}

double Accel_Z_Out(ursObject* gself)
{
    return lastaccelz;
}

double Cam_Bright_Out(ursObject* gself)
{
    return lastcambright;
}

double Cam_Blue_Out(ursObject* gself)
{
    return lastcamblue;
}

double Cam_Green_Out(ursObject* gself)
{
    return lastcamgreen;
}

double Cam_Red_Out(ursObject* gself)
{
    return lastcamred;
}

double Cam_Edge_Out(ursObject* gself)
{
    return lastcamedge;
}

double Compass_X_Out(ursObject* gself)
{
    return lastcompassx;
}

double Compass_Y_Out(ursObject* gself)
{
    return lastcompassy;
}

double Compass_Z_Out(ursObject* gself)
{
    return lastcompassz;
}

double Compass_North_Out(ursObject* gself)
{
    return lastcompassnorth;
}

double Location_Lat_Out(ursObject* gself)
{
    return lastlocationlat;
}

double Location_Long_Out(ursObject* gself)
{
    return lastlocationlong;
}

double Mic_Out(ursObject* gself)
{
    return lastmic;
}

double Touch_X1_Out(ursObject* gself)
{
    return lasttouchx[0];
}

double Touch_Y1_Out(ursObject* gself)
{
    return lasttouchy[0];
}

double Touch_X2_Out(ursObject* gself)
{
    return lasttouchx[1];
}

double Touch_Y2_Out(ursObject* gself)
{
    return lasttouchy[1];
}

double Touch_X3_Out(ursObject* gself)
{
    return lasttouchx[2];
}

double Touch_Y3_Out(ursObject* gself)
{
    return lasttouchy[2];
}

double Touch_X4_Out(ursObject* gself)
{
    return lasttouchx[3];
}

double Touch_Y4_Out(ursObject* gself)
{
    return lasttouchy[3];
}

double Touch_X5_Out(ursObject* gself)
{
    return lasttouchx[4];
}

double Touch_Y5_Out(ursObject* gself)
{
    return lasttouchy[4];
}

double Touch_X6_Out(ursObject* gself)
{
    return lasttouchx[5];
}

double Touch_Y6_Out(ursObject* gself)
{
    return lasttouchy[5];
}

double Touch_X7_Out(ursObject* gself)
{
    return lasttouchx[6];
}

double Touch_Y7_Out(ursObject* gself)
{
    return lasttouchy[6];
}

double Touch_X8_Out(ursObject* gself)
{
    return lasttouchx[7];
}

double Touch_Y8_Out(ursObject* gself)
{
    return lasttouchy[7];
}

double Touch_X9_Out(ursObject* gself)
{
    return lasttouchx[8];
}

double Touch_Y9_Out(ursObject* gself)
{
    return lasttouchy[8];
}

double Touch_X10_Out(ursObject* gself)
{
    return lasttouchx[9];
}

double Touch_Y10_Out(ursObject* gself)
{
    return lasttouchy[9];
}

double Touch_X11_Out(ursObject* gself)
{
    return lasttouchx[10];
}

double Touch_Y11_Out(ursObject* gself)
{
    return lasttouchy[10];
}

double RotRate_X_Out(ursObject* gself)
{
    return lastrotratex;
}

double RotRate_Y_Out(ursObject* gself)
{
    return lastrotratey;
}

double RotRate_Z_Out(ursObject* gself)
{
    return lastrotratez;
}

double lastpushout = 0.0;

double Push_Out(ursObject* gself)
{
    return lastpushout;
}

void Dac_In(ursObject* gself, double in)
{
	dacindata = in;
}

void Vis_In(ursObject* gself, double in)
{
	visindata = in;
}

void Net_Send(float data);

void Net_In(ursObject* gself, double in)
{
	netindata = in;
	Net_Send(netindata);
}

/*void Drain_In(ursObject* gself, double in)
{
}
*/

void Pull_In(ursObject* gself, double in)
{
    gself->lastindata[0] = in;
//	pullindata = in;
}

void* Oct_Constructor()
{
	Oct_Data* self = new Oct_Data;
	self->freq = 0.0;
	return (void*)self;
}

void Oct_Destructor(ursObject* gself)
{
	Oct_Data* self = (Oct_Data*)gself->objectdata;
	delete (Oct_Data*)self;
}

double Oct_Tick(ursObject* gself)
{
	Oct_Data* self = (Oct_Data*)gself->objectdata;
	double res;
	res = gself->lastindata[0];
	
	res += gself->CallAllPullIns();
	return ((1.0+res)/2.0*0.125*2+self->freq);
}

double Oct_Out(ursObject* gself)
{
	Oct_Data* self = (Oct_Data*)gself->objectdata;
	return ((1.0+gself->CallAllPullIns())/2.0*0.125*2+self->freq);
	//	return gself->lastindata[0];
}


void Oct_In(ursObject* gself, double in)
{
	Oct_Data* self = (Oct_Data*)gself->objectdata;
	gself->CallAllPushOuts(((1.0+in)/2.0*0.125*2+self->freq));
}

void Oct_Freq(ursObject* gself, double in)
{
	Oct_Data* self = (Oct_Data*)gself->objectdata;
    self->freq = in;
    
}

// Range

void* Range_Constructor()
{
	Range_Data* self = new Range_Data;
	self->bottom = -0.75;
    self->top = +0.75;
    self->k = (self->top - self->bottom)/2.0;
    self->d = (self->top + self->bottom)/2.0;
	return (void*)self;
}

void Range_Destructor(ursObject* gself)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	delete (Range_Data*)self;
}

double Range_Tick(ursObject* gself)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	double res;
	res = gself->lastindata[0];
	
	res += gself->CallAllPullIns();
	return (self->k*res+self->d);
}

double Range_Out(ursObject* gself)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	return (gself->CallAllPullIns()*self->k+self->d);
	//	return gself->lastindata[0];
}


void Range_In(ursObject* gself, double in)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	gself->CallAllPushOuts(self->k*in+self->d);
}

void Range_Bottom(ursObject* gself, double in)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	self->bottom = in;
    self->k = (self->top - self->bottom)/2.0;
    self->d = (self->top + self->bottom)/2.0;
}

void Range_Top(ursObject* gself, double in)
{
	Range_Data* self = (Range_Data*)gself->objectdata;
	self->top = in;
    self->k = (self->top - self->bottom)/2.0;
    self->d = (self->top + self->bottom)/2.0;
}

// Quantization to semi-tones

void* Quant_Constructor()
{
	return NULL;
}

void Quant_Destructor(ursObject* gself)
{
}

double Quant_Tick(ursObject* gself)
{
	double res;
	res = gself->lastindata[0];
	
	res += gself->CallAllPullIns();
	float quantstep = 0.125/12.0;
	return (floor((res/quantstep)+0.5)*quantstep);
}

double Quant_Out(ursObject* gself)
{
	float quantstep = 0.125/12.0;
	return (floor((gself->CallAllPullIns()/quantstep)+0.5)*quantstep);
	//	return gself->lastindata[0];
}

                
void Quant_In(ursObject* gself, double in)
{
	float quantstep = 0.125/12.0;
	gself->CallAllPushOuts(floor((in/quantstep)+0.5)*quantstep);
}

void* Gain_Constructor()
{
	Gain_Data* self = new Gain_Data;
	self->amp = 0.95;
	return (void*)self;
}

void Gain_Destructor(ursObject* gself)
{
	Gain_Data* self = (Gain_Data*)gself->objectdata;
	delete (Gain_Data*)self;
}

double Gain_Tick(ursObject* gself)
{
	Gain_Data* self = (Gain_Data*)gself->objectdata;
	double res;
	res = 0; //gself->lastindata[0];

	gself->FeedAllPullIns(1); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	
	res += gself->CallAllPullIns();
	res = res * self->amp;
	return (res);
}

double Gain_Out(ursObject* gself)
{
	Gain_Data* self = (Gain_Data*)gself->objectdata;

    gself->FeedAllPullIns(1); // This is decoupled so no forwarding, just pulling to propagate our natural rate

    return self->amp*gself->CallAllPullIns();
}

void Gain_In(ursObject* gself, double in)
{
	Gain_Data* self = (Gain_Data*)gself->objectdata;
	gself->lastindata[0] = in;
	gself->CallAllPushOuts(in*self->amp);
}

void Gain_Amp(ursObject* gself, double in)
{
	Gain_Data* self = (Gain_Data*)gself->objectdata;
	self->amp = in;
}

void* SinOsc_Constructor()
{
	SinOsc_Data* self = new SinOsc_Data;
	self->freq = 440;
	self->srate = URSOUND_DEFAULTSRATE;
	self->time = 0;
	self->phase = 0;
	self->amp = 1.0;//2147483647;//32767;
	return (void*)self;
}

void SinOsc_Destructor(ursObject* gself)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	delete (SinOsc_Data*)self;
}

double SinOsc_Tick(ursObject* gself)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	self->phase = self->phase + 2.0*PI*self->freq/self->srate;
	double out = self->amp*sin(self->phase /*+ PI*self->phase*/);
//	self->time = self->time + 1;
	self->lastout = out;
	return out;
}

double SinOsc_Out(ursObject* gself)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	return self->lastout;
}

void SinOsc_FillBuffer(ursObject* gself, SInt16* buffer, UInt32 len)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	for(int i=0; i<len; i++)
	{
		self->phase = self->phase + 2.0*PI*self->freq/self->srate;
		buffer[i] =  32767*self->amp*sin(self->phase /*+ PI*self->phase*/);
//		self->time = self->time + 1;
	}
	self->lastout = buffer[len-1];
}

void SinOsc_SetFreq(ursObject* gself, double infreq)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;

	self->freq = norm2Freq(infreq);
}

void SinOsc_SetAmp(ursObject* gself, double inamp)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	self->amp = capNorm(inamp); //*(2147483647/256);
}


void SinOsc_SetRate(ursObject* gself, double inrate)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	self->srate = (inrate+1.0)*96000;//(inrate+256)/256.0*96000;
}

void SinOsc_SetPhase(ursObject* gself, double inphase)
{
	SinOsc_Data* self = (SinOsc_Data*)gself->objectdata;
	self->phase = inphase;
}

// OWF

void* OWF_Constructor()
{
	OWF_Data* self = new OWF_Data;
	self->freq = 440;
	self->srate = URSOUND_DEFAULTSRATE;
	self->time = 0;
	self->phase = 0;
	self->amp = 1.0;//2147483647;//32767;
	return (void*)self;
}

void OWF_Destructor(ursObject* gself)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	delete (OWF_Data*)self;
}

float saw(float t)
{
	return fmod(t,2.0*PI)*2/2*PI-1;
}

float rect(float t)
{
	if (fmod(t,2*PI)<PI)
		return -1;
	else
		return 1;
}

double OWF_Tick(ursObject* gself)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	self->phase = self->phase + 2.0*PI*self->freq/self->srate;
	double out = 0.25*self->amp*(rect(2*self->phase)*(rect(self->phase)+saw(self->phase)+saw(self->phase/2))*saw(self->phase/2))/6.0;
	//	self->time = self->time + 1;
	self->lastout = out;
	return out;
}

double OWF_Out(ursObject* gself)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	return self->lastout;
}

void OWF_FillBuffer(ursObject* gself, SInt16* buffer, UInt32 len)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	for(int i=0; i<len; i++)
	{
		self->phase = self->phase + 2.0*PI*self->freq/self->srate;
		buffer[i] =  32767*self->amp*sin(self->phase /*+ PI*self->phase*/);
		//		self->time = self->time + 1;
	}
	self->lastout = buffer[len-1];
}

void OWF_SetFreq(ursObject* gself, double infreq)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	
	self->freq = norm2Freq(infreq);
}

void OWF_SetAmp(ursObject* gself, double inamp)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	self->amp = capNorm(inamp); //*(2147483647/256);
}


void OWF_SetRate(ursObject* gself, double inrate)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	self->srate = (inrate+1.0)*96000;//(inrate+256)/256.0*96000;
}

void OWF_SetPhase(ursObject* gself, double inphase)
{
	OWF_Data* self = (OWF_Data*)gself->objectdata;
	self->phase = inphase;
}



// Sample

void* Sample_Constructor()
{
//	UInt32 frate;
	Sample_Data* self = new Sample_Data;
	self->numsamples = 0;
	self->activesample = 0;
	self->amp = 1.0; //64*65535;//2147483647;//32767;
	return (void*)self;
}

void Sample_AddFile(ursObject* gself, const char* filename)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
//	UInt32 frate;
    
	self->numsamples = self->numsamples+1;
	if(self->numsamples == 1)
	{
		self->samplebuffer = (SInt16**)malloc(sizeof(SInt16*));
		self->len = (UInt32*)malloc(sizeof(UInt32));
	}
	else
	{
		self->samplebuffer = (SInt16**)realloc(self->samplebuffer, sizeof(SInt16*)*self->numsamples);
		self->len = (UInt32*)realloc(self->len,sizeof(UInt32)*self->numsamples);
	}
/*    self->samplebuffer[self->numsamples-1] = (SInt16*)malloc(1000);
    self->len[self->numsamples-1] = 250;
    memset(self->samplebuffer[self->numsamples-1], 0, 1000);*/

    const char* filestr = multiPath(filename);

	FileRead fr(filestr);

    UInt32 len = fr.fileSize();
    StkFrames frames(len,1);
    fr.read(frames);
    self->samplebuffer[self->numsamples-1] = (SInt16*)malloc(frames.size()*sizeof(SInt16));
    len = frames.size();
    for(int i=0; i<len; i++)
    {
        self->samplebuffer[self->numsamples-1][i]=frames[i]*32767;
    }
    self->len[self->numsamples-1] = len;
    
    fr.close();
    
    
    
//	self->samplebuffer[self->numsamples-1] = (SInt16*)LoadAudioFileData2(filename, &self->len[self->numsamples-1], &frate);
//	self->len[self->numsamples-1] = self->len[self->numsamples-1]-1;
//	self->rate = 48000.0/frate;
	self->rate = 1.0;
}

void Sample_Destructor(ursObject* gself)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	delete (Sample_Data*)self;
}

double Sample_Tick(ursObject* gself)
{	
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	double out = 0;
	if(self->playing && self->numsamples > 0)
	{
		if(self->samplebuffer[self->activesample]!=NULL)
			out = self->amp*self->samplebuffer[self->activesample][self->position]/32767.0;
		else
			out = 0;

		self->realposition = self->realposition + self->rate;
		self->position = (SInt32)self->realposition;
		if(self->loop)
		{
			self->position = self->position % self->len[self->activesample];
			while(self->position < 0)
				self->position += self->len[self->activesample];
		}
		else
		{
			if(self->position >= self->len[self->activesample] || self->position < 0)
				self->playing = false;
		}
	}
	self->lastout = out;
	return out;
}

double Sample_Out(ursObject* gself)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	return self->lastout;
}

void Sample_SetAmp(ursObject* gself, double inamp)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	self->amp = inamp;// *(128*65535/256);
}


void Sample_SetRate(ursObject* gself, double inrate)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	self->rate = 4.0*inrate;// /128.0;
}

void Sample_SetPos(ursObject* gself, double inpos)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	if(inpos < 0)
		inpos = 1.0 + inpos;
	
	self->position = self->len[self->activesample]*inpos;
	self->realposition  = self->len[self->activesample]*inpos;
	if(self->position >= self->len[self->activesample] || self->position < 0)
		self->playing = false;
	else
		self->playing = true;
}

void Sample_Trigger(ursObject* gself, double amp)
{
    if(amp > 0.0)
    {
        Sample_SetAmp(gself, amp);
        Sample_SetPos(gself,0.0);
    }
}

void Sample_SetSample(ursObject* gself, double insample)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;

	self->activesample = (int)(insample*(self->numsamples-0.5));
	if(self->activesample > self->numsamples - 1) self->activesample = self->numsamples - 1;
	if(self->activesample < 0) self->activesample = 0;

	self->position = self->position % self->len[self->activesample];
	while(self->position < 0)
		self->position += self->len[self->activesample];
}

void Sample_SetLoop(ursObject*gself, double instate)
{
	Sample_Data* self = (Sample_Data*)gself->objectdata;
	if(instate > 0.0)
		self->loop = true;
	else
		self->loop = false;
}

// Sleigh

void* Sleigh_Constructor()
{
	UInt32 frate;
	Sleigh_Data* self = new Sleigh_Data;
	self->Sleighbuffer[0] = (SInt16* )LoadAudioFileData("sleighbells.wav", &self->len[0], &frate);
	self->activeSleigh = 0;
	self->len[0] = self->len[0]-1;
	self->rate = 48000.0/frate;
	self->amp = 1.0; //64*65535;//2147483647;//32767;
	return (void*)self;
}

void Sleigh_Destructor(ursObject* gself)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	delete (Sleigh_Data*)self;
}

double Sleigh_Tick(ursObject* gself)
{	
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	double out = 0;
	if(self->playing==true)
	{
		out = self->amp*self->Sleighbuffer[self->activeSleigh][self->position]/32767.0;
		self->realposition = self->realposition + self->rate;
		self->position = (SInt32)self->realposition;
		if(self->loop)
		{
			self->position = self->position % self->len[self->activeSleigh];
			while(self->position < 0)
				self->position += self->len[self->activeSleigh];
		}
		else
		{
			if(self->position >= self->len[self->activeSleigh] || self->position < 0)
				self->playing = false;
		}
	}
	self->lastout = out;
	return out;
}

double Sleigh_Out(ursObject* gself)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	return self->lastout;
}

void Sleigh_SetAmp(ursObject* gself, double inamp)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	self->amp = inamp;// *(128*65535/256);
}


void Sleigh_SetRate(ursObject* gself, double inrate)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	self->rate = 4.0*inrate;// /128.0;
}

void Sleigh_SetPos(ursObject* gself, double inpos)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	if(inpos < 0)
		inpos = 1.0 + inpos;
	
	self->position = self->len[self->activeSleigh]*inpos;
	self->realposition  = self->len[self->activeSleigh]*inpos;
}

void Sleigh_Play(ursObject* gself, double inplay)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;

	if(self->playing && inplay > 0.5)
	{
		self->position = 0;
		self->realposition = 0.0;
	}
	else if(!self->playing && inplay > 0.5)
	{
		self->position = 0;
		self->realposition = 0.0;
		self->playing = true;
	}		
}

void Sleigh_Loop(ursObject* gself, double inloop)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;

	if(inloop >= 0.5)
		self->loop = true;
	else
		self->loop = false;
	
}

void Sleigh_SetSleigh(ursObject* gself, double inSleigh)
{
	Sleigh_Data* self = (Sleigh_Data*)gself->objectdata;
	
	self->activeSleigh = (int)(inSleigh*7.0-0.00001);
	if(self->activeSleigh < 0) self->activeSleigh = 0;
	
	self->position = self->position % self->len[self->activeSleigh];
	while(self->position < 0)
		self->position += self->len[self->activeSleigh];
}

// Slow

#define MAX_SLOW_DEFAULT 32

void* Slow_Constructor()
{
    //	UInt32 frate;
/*	Slow_Data* self = new Slow_Data;
	self->samplebuffer = new SInt16[MAX_SLOW_DEFAULT];
	self->slow = 0;
	self->amp = 1.0; //64*65535;//2147483647;//32767;
	self->recpos = 0;
	self->playpos = 0;
	self->reclen = 0;
	self->recording = false;
	self->playing = false;
	self->loop = true;
	return (void*)self;*/
    return NULL;
}

void Slow_Destructor(ursObject* gself)
{
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	delete (Slow_Data*)self;*/
}

double Slow_Tick(ursObject* gself)
{	
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	gself->FeedAllPullIns(1); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	double out = 0;
	if(self->playing)
	{
		out = self->amp*self->samplebuffer[self->position]/32767.0;
		self->realposition = self->realposition + self->rate;
		self->position = (SInt32)self->realposition;
		if(self->loop)
		{
			self->position = self->position % self->len;
			while(self->position < 0)
				self->position += self->len;
		}
		else
		{
			if(self->position >= self->len || self->position < 0)
				self->playing = false;
		}
	}
	self->lastout = out;
	return out;*/
    return 0.0;
}

double Slow_Out(ursObject* gself)
{
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	return self->lastout;*/
    return 0.0;
}

void Slow_SetAmp(ursObject* gself, double inamp)
{
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	self->amp = inamp;// *(128*65535/256);*/
}


void Slow_SetRate(ursObject* gself, double inrate)
{
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	self->rate = 4.0*inrate;// /128.0;*/
}

void Slow_In(ursObject* gself, double indata)
{
/*	Slow_Data* self = (Slow_Data*)gself->objectdata;
	if(self->recording)
	{
		self->samplebuffer[self->recpos++] = indata*32767.0;
		if(self->recpos >= MAX_LOOPER_DEFAULT || self->recpos >= self->reclen)
			self->recording = false;
	}*/
}


// Looper

// Space for 10 second loop is default
#define MAX_LOOPER_DEFAULT 48000*10


void* Looper_Constructor()
{
//	UInt32 frate;
	Looper_Data* self = new Looper_Data;
	self->samplebuffer = new SInt16[MAX_LOOPER_DEFAULT];
	self->len = 0;
	self->amp = 1.0; //64*65535;//2147483647;//32767;
	self->recpos = 0;
	self->playpos = 0;
	self->reclen = 0;
	self->recording = false;
	self->playing = false;
	self->loop = true;
	return (void*)self;
}

void Looper_Destructor(ursObject* gself)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	delete (Looper_Data*)self;
}

double Looper_Tick(ursObject* gself)
{	
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	gself->FeedAllPullIns(1); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	double out = 0;
	if(self->playing && self->reclen > 0)
	{
		out = self->amp*self->samplebuffer[self->position]/32767.0;
		self->realposition = self->realposition + self->rate;
		self->position = (SInt32)self->realposition;
		if(self->loop)
		{
			self->position = self->position % self->len;
			while(self->position < 0)
				self->position += self->len;
		}
		else
		{
			if(self->position >= self->len || self->position < 0)
				self->playing = false;
		}
	}
	self->lastout = out;
	return out;
}

double Looper_Out(ursObject* gself)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	return self->lastout;
}

void Looper_SetAmp(ursObject* gself, double inamp)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	self->amp = inamp;// *(128*65535/256);
}


void Looper_SetRate(ursObject* gself, double inrate)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	self->rate = 4.0*inrate;// /128.0;
}

void Looper_In(ursObject* gself, double indata)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	if(self->recording)
	{
		self->samplebuffer[self->recpos++] = indata*32767.0;
		if(self->recpos >= MAX_LOOPER_DEFAULT || self->recpos >= self->reclen)
			self->recording = false;
	}
}

// Placing virtual zero at below 24 bit resolution. This should be fine for virtually all applications.
#define VIRTUAL_ZERO 1.0/(65536.0*64.0)

void Looper_Record(ursObject* gself, double indata)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	if(indata < VIRTUAL_ZERO)
	{
		self->recording = false;
		self->reclen = self->recpos;
		self->len = self->recpos;
	}
	else
	{
		self->reclen = MAX_LOOPER_DEFAULT*indata;
		self->recpos = 0;
		self->recording = true;
	}
}

void Looper_Play(ursObject* gself, double indata)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	if(indata < VIRTUAL_ZERO)
	{
		self->playing = false;
	}
	else
	{
		self->len = self->reclen*indata;
		self->position = 0;
		self->realposition = 0.0;
		self->playing = true;
	}
}

void Looper_Pos(ursObject* gself, double indata)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	self->realposition = self->reclen*indata;
}

void Looper_ReadFile(ursObject* gself, const char* filename)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
	UInt32 frate;
    SInt16*	tempbuffer;
	tempbuffer = (SInt16*)LoadAudioFileData(filename, &self->len, &frate);
    //	self->rate = 48000.0/frate;
	self->rate = 1.0;
    
    if(tempbuffer != NULL)
    {
        for(int i=0; i<self->len; i++)
        {
            self->samplebuffer[i] = tempbuffer[i]*32767.0;
        }
        self->reclen = self->len;
		self->position = 0;
		self->realposition = 0.0;
    }
}

void Looper_WriteFile(ursObject* gself, const char* filename)
{
	Looper_Data* self = (Looper_Data*)gself->objectdata;
//	UInt32 frate;

    StkFrames frames(self->len,1);
    for(int i=0; i<self->len; i++)
    {
        frames[i]=(double)self->amp*self->samplebuffer[i]/32767.0;
    }
	FileWrite fw(filename);

    fw.write(frames);
    fw.close();
}



// LoopRhythm

void* LoopRhythm_Constructor()
{
	LoopRhythm_Data* self = new LoopRhythm_Data;
	self->loop = new Loop(16);
	self->btime = 0.0;
	self->bstep = 1/90.0; // Default HPM = 90
	self->sampletime = 1/48000.0; // Default sample time = 1/SR
	self->lastout = 0.0;
	return (void*)self;
}

void LoopRhythm_Destructor(ursObject* gself)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	delete self->loop;
	delete (LoopRhythm_Data*)self;
}

double LoopRhythm_Tick(ursObject* gself)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	double res = 0.0;
	self->btime = self->btime + self->sampletime;
	if(self->btime > self->bstep) // Keeping this double means that it will re-align. It's accurate on average but will be quantized by SR.
	{
		self->btime = self->btime - self->bstep;
		res = self->loop->Tick();
		self->lastout = res;
	}
	return res;
} 

double LoopRhythm_Out(ursObject* gself)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	return self->lastout;
}

void LoopRhythm_SetSampleRate(ursObject* gself, double indata)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	self->sampletime = 1.0/indata;
}


// Hits per minute (beat usually means a number of "hits". Smallest beat granularity really.
void LoopRhythm_SetHMP(ursObject* gself, double indata)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	self->bstep = 1.0/indata;
}

void LoopRhythm_SetBeatNow(ursObject* gself, double indata)
{
	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
	self->loop->SetNow(norm2PositiveLinear(indata)); // No point in allowing negative beats.
}

void LoopRhythm_Pos(ursObject* gself, double indata)
{
//	LoopRhythm_Data* self = (LoopRhythm_Data*)gself->objectdata;
    // NYI
}

// CircleMap

void* CircleMap_Constructor()
{
	CircleMap_Data* self = new CircleMap_Data;
	self->freq = 440;
	self->srate = URSOUND_DEFAULTSRATE;
	self->time = 0;
	self->phase = 0;
	self->amp = 1.0;//2147483647;//32767;
	self->nonl = 0.0;
	return (void*)self;
}

void CircleMap_Destructor(ursObject* gself)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	delete (CircleMap_Data*)self;
}

double CircleMap_Tick(ursObject* gself)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	gself->FeedAllPullIns(); // This is decoupled so no forwarding, just pulling to propagate our natural rate
	self->phase = self->phase + 2.0*PI*self->freq/self->srate;
	double out = self->amp*sin(self->phase + self->nonl*sin(2.0*PI*self->lastout));
	self->lastout = out;
	return out;
}

double CircleMap_Out(ursObject* gself)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	return self->lastout;
}

void CircleMap_SetFreq(ursObject* gself, double infreq)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	
	self->freq = norm2Freq(infreq);
}

void CircleMap_SetAmp(ursObject* gself, double inamp)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	self->amp = capNorm(inamp); //*(2147483647/256);
}


void CircleMap_SetRate(ursObject* gself, double inrate)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	self->srate = (inrate+1.0)*96000;//(inrate+256)/256.0*96000;
}

void CircleMap_SetPhase(ursObject* gself, double inphase)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	self->phase = inphase;
}

void CircleMap_SetNonL(ursObject* gself, double indata)
{
	CircleMap_Data* self = (CircleMap_Data*)gself->objectdata;
	self->nonl = 10*indata/PI;
}


// Avg

void* Avg_Constructor()
{
	Avg_Data* self = new Avg_Data;
	self->bufferlen = 256;//32;
	self->buffer = new double[256];
	for (int i=0; i < self->bufferlen; i++)
		self->buffer[i] = 0.0;
	self->inpos = 0;
	self->outpos = 0;
	self->avg = 0.0;
	return (void*)self;
}

void Avg_Destructor(ursObject* gself)
{
	Avg_Data* self = (Avg_Data*)gself->objectdata;
	delete (Avg_Data*)self;
}

double Avg_Tick(ursObject* gself)
{
	Avg_Data* self = (Avg_Data*)gself->objectdata;
	return self->avg/self->bufferlen;
}

double Avg_Out(ursObject* gself)
{
	Avg_Data* self = (Avg_Data*)gself->objectdata;
	return self->avg/self->bufferlen;
}

void Avg_In(ursObject* gself, double indata)
{
	Avg_Data* self = (Avg_Data*)gself->objectdata;
	double outvalue = self->buffer[self->inpos];
	double invalue = fabs(indata);
	self->buffer[self->inpos++] = invalue;
	self->avg += invalue - outvalue;
	if(self->inpos >= self->bufferlen) self->inpos = 0;
	double out = self->avg/self->bufferlen > 0.005 ? self->avg/self->bufferlen : 0.0;
	gself->CallAllPushOuts(out);
}

void Avg_Len(ursObject* gself, double indata)
{
	Avg_Data* self = (Avg_Data*)gself->objectdata;
	self->bufferlen = 1+255*norm2PositiveLinear(indata);
}

// FFT

// fft function
// v[] is composed of imaginary and real parts of the input:
// [ imag[0], real[0], imag[1], real[1], .... ]
// 

static void FFT(float v[], int n) {
	int        ip, k, length;
	float      theta, pi = 3.1415926535897932384f;
	float      wr, wi, ur, ui, tr, ti, tmp;
	
	if((n&(n-1))!=0) {
		n |= n >> 1; n |= n >> 2; n |= n >> 4; n |= n >> 8; n |= n >> 16;
		n=(n+1)>>1;
	}
	
	for (int i=0, j=0; i < n-1; i++,j+=k) {
		if (i<j) {
			tmp = v[i*2]; v[i*2] = v[j*2]; v[j*2] = tmp;
			tmp = v[i*2+1]; v[i*2+1] = v[j*2+1]; v[j*2+1] = tmp;
		}
		for(k=n/2;k<=j;k>>=1) j -= k;
	}
	for (int li=1; li<n; li*=2 ){
		length = 2*li;
		theta  = pi/li;
		ur   = 1.0f;
		ui   = 0.0f;
		if ( li == 1 ) {
			wr = -1.0f;
			wi =  0.0f;
		} else if ( li == 2 ) {
			wr =  0.0f;
			wi =  1.0f;
		} else {
			wr = cos(theta);
			wi = sin(theta);
		}
		for (int j = 0; j < li; j++ ) {
			for (int i = j; i < n; i += length ) {
				ip=i+li;
				tr=v[ip*2]*ur-v[ip*2+1]*ui;	ti=v[ip*2]*ui+v[ip*2+1]*ur;
				v[ip*2]=v[i*2]-tr;			v[ip*2+1]=v[i*2+1]-ti;
				v[i*2]+=tr;					v[i*2+1]+=ti;
			}
			ur = ur*wr - ui*wi;
			ui = ui*wr + ur*wi;
		}
	}
}

void* Tuner_Constructor()
{
	Tuner_Data* self = new Tuner_Data;
	self->bufferlen = 8192;//32;
	self->buffer = new float[self->bufferlen*2];
	for (int i=0; i < self->bufferlen*2; i++)
		self->buffer[i] = 0.0;
	self->inpos = 0;
	return (void*)self;
}

void Tuner_Destructor(ursObject* gself)
{
	Tuner_Data* self = (Tuner_Data*)gself->objectdata;
	delete [] self->buffer;
	delete (Tuner_Data*)self;
}

double Tuner_Tick(ursObject* gself)
{
	Tuner_Data* self = (Tuner_Data*)gself->objectdata;
	return self->lastout;
}

double Tuner_Out(ursObject* gself)
{
	Tuner_Data* self = (Tuner_Data*)gself->objectdata;
	return self->lastout;
}

void Tuner_In(ursObject* gself, double indata)
{
	Tuner_Data* self = (Tuner_Data*)gself->objectdata;
	self->buffer[self->inpos++] = 0;
	self->buffer[self->inpos++] = indata;
	int maxfreq=-1;
	float maxval=0;
	float *buf=self->buffer;
	float prevamp, amp=buf[0]*buf[0]+buf[1]*buf[1], nextamp=buf[2]*buf[2]+buf[3]*buf[3];
	float maxprev, maxamp, maxnext;
	if(self->inpos >= self->bufferlen*2) {
		self->inpos = 0;
		FFT(buf,self->bufferlen);
		for(int i=2;i<self->bufferlen/2-1;i+=2) {
			float *p=&buf[i];
			prevamp=amp;
			amp=nextamp;
			nextamp=p[2]*p[2]+p[3]*p[3];
			
			if(prevamp+amp+nextamp > maxval) {
				maxprev=prevamp;
				maxamp=amp;
				maxnext=nextamp;
				maxval=prevamp+amp+nextamp;
				maxfreq=i/2;
			}
		}
		if(maxval<1.0e3) {
			self->lastout=(0.0);			
			return;
		}
		double out = (maxprev*(maxfreq-1)+maxamp*maxfreq+maxnext*(maxfreq+1))/maxval;
		double lg = log(out)/log(2.0)-0.358;
		double mantissa = lg-(int)lg;
		double moved=(mantissa+0.75);
		moved=moved-(int)moved;
		//gself->CallAllPushOuts(mantissa);
		self->lastout=(0.29225+moved*12*0.01041);
        gself->CallAllPushOuts(self->lastout);
	}
}

// Drain
void* Drain_Constructor()
{
	State_Data* self = new State_Data;
	return (void*)self;
}

void Drain_Destructor(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	delete (State_Data*)self;
}

double Drain_Out(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = res;
    }
	return self->lastout;
}

void Drain_In(ursObject* gself, double indata)
{
	State_Data* self = (State_Data*)gself->objectdata;
    self->lastout = indata;
    gself->CallAllPushOuts(indata);
}

double Drain_Time(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = res;
    }
    gself->CallAllPushOuts(self->lastout);

    return 0.0; // Not propagating to pumps at the moment, this may change
}

// Pump

void* Pump_Constructor()
{
	State_Data* self = new State_Data;
	return (void*)self;
}

void Pump_Destructor(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	delete (State_Data*)self;
}

double Pump_Out(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = res;
    }
	return self->lastout;
}

void Pump_In(ursObject* gself, double indata)
{
    
	State_Data* self = (State_Data*)gself->objectdata;
    self->lastout = indata;
    gself->CallAllPushOuts(indata);
}

void Pump_Time(ursObject* gself, double indata)
{
    // Currently ignoring the pump's indata. This likely will never change.
	State_Data* self = (State_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = res;
    }
    gself->CallAllPushOuts(self->lastout);
}


// Sniff
void* Sniff_Constructor()
{
	State_Data* self = new State_Data;
	return (void*)self;
}

void Sniff_Destructor(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	delete (State_Data*)self;
}

double Sniff_Out(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = res;
    }
	return self->lastout;
}

void Sniff_In(ursObject* gself, double indata)
{
	State_Data* self = (State_Data*)gself->objectdata;
    self->lastout = indata;
    gself->CallAllPushOuts(indata);
}

double Sniff_Sniff(ursObject* gself)
{
	State_Data* self = (State_Data*)gself->objectdata;
    return self->lastout; // Cheapest form of interpolation
}


// SniffL (linear interpolating)
void* SniffL_Constructor()
{
	TwoState_Data* self = new TwoState_Data;
	return (void*)self;
}

void SniffL_Destructor(ursObject* gself)
{
	TwoState_Data* self = (TwoState_Data*)gself->objectdata;
	delete (TwoState_Data*)self;
}

double SniffL_Out(ursObject* gself)
{
	TwoState_Data* self = (TwoState_Data*)gself->objectdata;
	if(gself->firstpullin[0]!=NULL)
    {
        float res = gself->CallAllPullIns();
        self->lastout = (self->previous + res)/2.0;
        self->previous = res;
    }
	return self->lastout;
}

void SniffL_In(ursObject* gself, double indata)
{
	TwoState_Data* self = (TwoState_Data*)gself->objectdata;
    self->lastout = (self->previous + indata)/2.0;
    self->previous = indata;
    gself->CallAllPushOuts(indata);
}

double SniffL_Sniff(ursObject* gself)
{
	TwoState_Data* self = (TwoState_Data*)gself->objectdata;
    return self->lastout; // Cheapest form of interpolation
}



// k-means (k = 3)

void* ThreeDist_Constructor()
{
	ThreeDist_Data* self = new ThreeDist_Data;
	self->mean1 = -1;
	self->mean2 = 0;
	self->mean3 = 1;
	
	self->train = false;
	return (void*)self;
}

void ThreeDist_Destructor(ursObject* gself)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	delete self;
}

double ThreeDist_Tick(ursObject* gself)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	double dist1 = self->mean1-self->in1;
	double dist2 = self->mean2-self->in2;
	double dist3 = self->mean3-self->in3;
	double out = sqrt(dist1*dist1+dist2*dist2+dist3*dist3)/3.0;
	self->lastout = out;
	return self->lastout;
}

double ThreeDist_Out(ursObject* gself)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	return self->lastout;
}

void ThreeDist_In1(ursObject* gself, double indata)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	if(self->train == false)
	{
		self->in1 = indata;
		double dist1 = self->mean1-self->in1;
		double dist2 = self->mean2-self->in2;
		double dist3 = self->mean3-self->in3;
		double out = sqrt(dist1*dist1+dist2*dist2+dist3*dist3);
		gself->CallAllPushOuts(out);
	}
	else
		self->mean1 = indata;
}

void ThreeDist_In2(ursObject* gself, double indata)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	if(self->train == false)
	{
		self->in2 = indata;
		double dist1 = self->mean1-self->in1;
		double dist2 = self->mean2-self->in2;
		double dist3 = self->mean3-self->in3;
		double out = sqrt(dist1*dist1+dist2*dist2+dist3*dist3);
		gself->CallAllPushOuts(out);
	}
	else
		self->mean2 = indata;
}

void ThreeDist_In3(ursObject* gself, double indata)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	if(self->train == false)
	{
		self->in3 = indata;
		double dist1 = self->mean1-self->in1;
		double dist2 = self->mean2-self->in2;
		double dist3 = self->mean3-self->in3;
		double out = sqrt(dist1*dist1+dist2*dist2+dist3*dist3);
		gself->CallAllPushOuts(out);
	}
	else
		self->mean3 = indata;
}

void ThreeDist_Train(ursObject* gself, double indata)
{
	ThreeDist_Data* self = (ThreeDist_Data*)gself->objectdata;
	if(indata >0.0)
		self->train = true;
	else
		self->train = false;
}

// -- Binary comparators

void* Add_Constructor()
{
	Comp_Data* self = new Comp_Data;
	self->lastin1 = 0.0;
	self->lastin2 = 0.0;
	return (void*)self;
}

void Add_Destructor(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	delete self;
}

double Add_Tick(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	self->lastout = self->lastin1 + self->lastin2;
	return self->lastout;
}

double Add_Out(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	return self->lastout;
}

void Add_In1(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin1 = indata;
	double out = self->lastin1 + self->lastin2;
	gself->CallAllPushOuts(out);
}

void Add_In2(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin2 = indata;
	double out = self->lastin1 + self->lastin2;
	gself->CallAllPushOuts(out);
}
// -- Binary comparators

void* Min_Constructor()
{
	Comp_Data* self = new Comp_Data;
	self->lastin1 = 0.0;
	self->lastin2 = 0.0;
	return (void*)self;
}

void Min_Destructor(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	delete self;
}

double Min_Tick(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	self->lastout = fmin(self->lastin1, self->lastin2);
	return self->lastout;
}

double Min_Out(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	return self->lastout;
}

void Min_In1(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin1 = indata;
	double out = fmin(self->lastin1, self->lastin2);
	gself->CallAllPushOuts(out);
}

void Min_In2(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin2 = indata;
	double out = fmin(self->lastin1, self->lastin2);
	gself->CallAllPushOuts(out);
}


void* Max_Constructor()
{
	Comp_Data* self = new Comp_Data;
	self->lastin1 = 0.0;
	self->lastin2 = 0.0;
	return (void*)self;
}

void Max_Destructor(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	delete self;
}

double Max_Tick(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	self->lastout = fmax(self->lastin1, self->lastin2);
	return self->lastout;
}

double Max_Out(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	return self->lastout;
}

void Max_In1(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin1 = indata;
	double out = fmax(self->lastin1, self->lastin2);
	gself->CallAllPushOuts(out);
}

void Max_In2(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin2 = indata;
	double out = fmax(self->lastin1, self->lastin2);
	gself->CallAllPushOuts(out);
}


void* MinS_Constructor()
{
	Comp_Data* self = new Comp_Data;
	self->lastin1 = 0.0;
	self->lastin2 = 0.0;
	return (void*)self;
}

void MinS_Destructor(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	delete self;
}

double MinS_Tick(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	float res = fmin(self->lastin1, self->lastin2);
	
	if(res == self->lastin1)
		self->lastout = -1.0;
	else
		self->lastout = 1.0;
	
	return self->lastout;
}

double MinS_Out(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	return self->lastout;
}

void MinS_In1(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin1 = indata;
	float res = fmin(self->lastin1, self->lastin2);
	float out;
	
	if(res == self->lastin1)
		out = -1.0;
	else
		out = 1.0;
	
	gself->CallAllPushOuts(out);
}

void MinS_In2(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin2 = indata;
	float res = fmin(self->lastin1, self->lastin2);
	float out;
	
	if(res == self->lastin1)
		out = -1.0;
	else
		out = 1.0;
	
	gself->CallAllPushOuts(out);
}


void* MaxS_Constructor()
{
	Comp_Data* self = new Comp_Data;
	self->lastin1 = 0.0;
	self->lastin2 = 0.0;
	return (void*)self;
}

void MaxS_Destructor(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	delete self;
}

double MaxS_Tick(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	float res = fmax(self->lastin1, self->lastin2);
	
	if(res == self->lastin1)
		self->lastout = -1.0;
	else
		self->lastout = 1.0;
	
	return self->lastout;
}

double MaxS_Out(ursObject* gself)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	return self->lastout;
}

void MaxS_In1(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin1 = indata;
	float res = fmax(self->lastin1, self->lastin2);
	float out;
	
	if(res == self->lastin1)
		out = -1.0;
	else
		out = 1.0;
	
	gself->CallAllPushOuts(out);
}

void MaxS_In2(ursObject* gself, double indata)
{
	Comp_Data* self = (Comp_Data*)gself->objectdata;
	self->lastin2 = indata;
	float res = fmax(self->lastin1, self->lastin2);
	float out;
	
	if(res == self->lastin1)
		out = -1.0;
	else
		out = 1.0;
	
	gself->CallAllPushOuts(out);
}

// --


void* Demux316_Constructor()
{
	Demux_Data* self = new Demux_Data;
	return (void*)self;
}

void Demux316_Destructor(ursObject* gself)
{
	Demux_Data* self = (Demux_Data*)gself->objectdata;
	delete self;
}

/*
double Demux316_Tick(ursObject* gself)
{
	Demux_Data* self = (Demux_Data*)gself->objectdata;
	gself->FeedAllPullIns();
	float res = fmax(self->lastin1, self->lastin2);
	
	if(res == self->lastin1)
		self->lastout = -1.0;
	else
		self->lastout = 1.0;
	
	return self->lastout;
}
 */

double Demux316_Out(ursObject* gself, int idx)
{
	Demux_Data* self = (Demux_Data*)gself->objectdata;
    
    
    if(self->current_mutex == idx/3)
    {
        if(gself->firstpullin[idx%3]!=NULL)
        {
            float res = gself->CallAllPullIns(idx%3);
            self->lastoutput[idx] = res;
            return res;
        }
    }
    
    return self->lastoutput[idx];
}

void Demux316_In(ursObject* gself, double indata, int idx)
{
	Demux_Data* self = (Demux_Data*)gself->objectdata;
	gself->CallAllPushOuts(indata,idx+self->current_mutex*3);
}

void Demux316_Next(ursObject* gself, double indata)
{
    Demux_Data* self = (Demux_Data*)gself->objectdata;
    if(indata != 0.0) // Revisit. Only nexting when nonzero.
    {
        self->current_mutex ++;
        self->current_mutex = self->current_mutex % 16;
    }
    // This does not couple to anything!
}

//

void Demux316_In1(ursObject* gself, double indata) { Demux316_In(gself, indata, 0); }
void Demux316_In2(ursObject* gself, double indata) { Demux316_In(gself, indata, 1); }
void Demux316_In3(ursObject* gself, double indata) { Demux316_In(gself, indata, 2); }

double Demux316_Out1_1(ursObject* gself) { return Demux316_Out(gself, 0); }
double Demux316_Out1_2(ursObject* gself) { return Demux316_Out(gself, 1); }
double Demux316_Out1_3(ursObject* gself) { return Demux316_Out(gself, 2); }
double Demux316_Out2_1(ursObject* gself) { return Demux316_Out(gself, 3); }
double Demux316_Out2_2(ursObject* gself) { return Demux316_Out(gself, 4); }
double Demux316_Out2_3(ursObject* gself) { return Demux316_Out(gself, 5); }
double Demux316_Out3_1(ursObject* gself) { return Demux316_Out(gself, 6); }
double Demux316_Out3_2(ursObject* gself) { return Demux316_Out(gself, 7); }
double Demux316_Out3_3(ursObject* gself) { return Demux316_Out(gself, 8); }
double Demux316_Out4_1(ursObject* gself) { return Demux316_Out(gself, 9); }
double Demux316_Out4_2(ursObject* gself) { return Demux316_Out(gself, 10); }
double Demux316_Out4_3(ursObject* gself) { return Demux316_Out(gself, 11); }
double Demux316_Out5_1(ursObject* gself) { return Demux316_Out(gself, 12); }
double Demux316_Out5_2(ursObject* gself) { return Demux316_Out(gself, 13); }
double Demux316_Out5_3(ursObject* gself) { return Demux316_Out(gself, 14); }
double Demux316_Out6_1(ursObject* gself) { return Demux316_Out(gself, 15); }
double Demux316_Out6_2(ursObject* gself) { return Demux316_Out(gself, 16); }
double Demux316_Out6_3(ursObject* gself) { return Demux316_Out(gself, 17); }
double Demux316_Out7_1(ursObject* gself) { return Demux316_Out(gself, 18); }
double Demux316_Out7_2(ursObject* gself) { return Demux316_Out(gself, 19); }
double Demux316_Out7_3(ursObject* gself) { return Demux316_Out(gself, 20); }
double Demux316_Out8_1(ursObject* gself) { return Demux316_Out(gself, 21); }
double Demux316_Out8_2(ursObject* gself) { return Demux316_Out(gself, 22); }
double Demux316_Out8_3(ursObject* gself) { return Demux316_Out(gself, 23); }
double Demux316_Out9_1(ursObject* gself) { return Demux316_Out(gself, 24); }
double Demux316_Out9_2(ursObject* gself) { return Demux316_Out(gself, 25); }
double Demux316_Out9_3(ursObject* gself) { return Demux316_Out(gself, 26); }
double Demux316_Out10_1(ursObject* gself) { return Demux316_Out(gself, 27); }
double Demux316_Out10_2(ursObject* gself) { return Demux316_Out(gself, 28); }
double Demux316_Out10_3(ursObject* gself) { return Demux316_Out(gself, 29); }
double Demux316_Out11_1(ursObject* gself) { return Demux316_Out(gself, 30); }
double Demux316_Out11_2(ursObject* gself) { return Demux316_Out(gself, 31); }
double Demux316_Out11_3(ursObject* gself) { return Demux316_Out(gself, 32); }
double Demux316_Out12_1(ursObject* gself) { return Demux316_Out(gself, 33); }
double Demux316_Out12_2(ursObject* gself) { return Demux316_Out(gself, 34); }
double Demux316_Out12_3(ursObject* gself) { return Demux316_Out(gself, 35); }
double Demux316_Out13_1(ursObject* gself) { return Demux316_Out(gself, 36); }
double Demux316_Out13_2(ursObject* gself) { return Demux316_Out(gself, 37); }
double Demux316_Out13_3(ursObject* gself) { return Demux316_Out(gself, 38); }
double Demux316_Out14_1(ursObject* gself) { return Demux316_Out(gself, 39); }
double Demux316_Out14_2(ursObject* gself) { return Demux316_Out(gself, 40); }
double Demux316_Out14_3(ursObject* gself) { return Demux316_Out(gself, 41); }
double Demux316_Out15_1(ursObject* gself) { return Demux316_Out(gself, 42); }
double Demux316_Out15_2(ursObject* gself) { return Demux316_Out(gself, 43); }
double Demux316_Out15_3(ursObject* gself) { return Demux316_Out(gself, 44); }
double Demux316_Out16_1(ursObject* gself) { return Demux316_Out(gself, 45); }
double Demux316_Out16_2(ursObject* gself) { return Demux316_Out(gself, 46); }
double Demux316_Out16_3(ursObject* gself) { return Demux316_Out(gself, 47); }

// --

ursObject* sinobject;
ursObject* nopeobject;
ursObject* sampleobject;
ursObject* looprhythmobject;

ursObject* cameraObject;
ursObject* accelobject;
ursObject* gyroobject;
ursObject* compassobject;
ursObject* locationobject;
ursObject* touchobject;
ursObject* micobject;
ursObject* netinobject;
ursObject* pushobject;
ursObject* fileobject;

ursObject* dacobject;
ursObject* visobject;
ursObject* netobject;
ursObject* drainobject;
ursObject* pullobject;

ursObject* object;

void urs_SetupObjects()
{
	
	accelobject = new ursObject("Accel", NULL, NULL, 0, 3, true);
	accelobject->AddOut("X", "TimeSeries", Accel_X_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	accelobject->AddOut("Y", "TimeSeries", Accel_Y_Out, NULL, NULL); // No more poetic justice: Pushers can now be pulled
	accelobject->AddOut("Z", "TimeSeries", Accel_Z_Out, NULL, NULL);
	ursourceobjectlist.Append(accelobject);
	cameraObject = new ursObject("Cam",NULL,NULL,0,5,true);
	cameraObject->AddOut("Bright","TimeSeries",Cam_Bright_Out,NULL,NULL);
	cameraObject->AddOut("Blue","TimeSeries",Cam_Blue_Out,NULL,NULL);
	cameraObject->AddOut("Green","TimeSeries",Cam_Green_Out,NULL,NULL);
	cameraObject->AddOut("Red","TimeSeries",Cam_Red_Out,NULL,NULL);
	cameraObject->AddOut("Edge","TimeSeries",Cam_Edge_Out,NULL,NULL);
	ursourceobjectlist.Append(cameraObject);
	compassobject = new ursObject("Compass", NULL, NULL, 0, 4, true);
	compassobject->AddOut("X", "TimeSeries", Compass_X_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	compassobject->AddOut("Y", "TimeSeries", Compass_Y_Out, NULL, NULL);
	compassobject->AddOut("Z", "TimeSeries", Compass_Z_Out, NULL, NULL);
	compassobject->AddOut("North", "TimeSeries", Compass_North_Out, NULL, NULL);
	ursourceobjectlist.Append(compassobject);
	locationobject = new ursObject("Location", NULL, NULL, 0, 2, true);
	locationobject->AddOut("Lat", "TimeSeries", Location_Lat_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	locationobject->AddOut("Long", "TimeSeries", Location_Long_Out, NULL, NULL);
	ursourceobjectlist.Append(locationobject);
	micobject = new ursObject("Mic", NULL, NULL, 0, 1, true);
	micobject->AddOut("Out", "TimeSeries", Mic_Out, NULL, NULL);
	ursourceobjectlist.Append(micobject);
	netinobject = new ursObject("NetIn", NULL, NULL, 0, 1, true);
	netinobject->AddOut("Out", "Event", NetIn_Tick, NetIn_Out, NULL);
	ursourceobjectlist.Append(netinobject);
	pushobject = new ursObject("Push", NULL, NULL, 0, 1); // An event based source ("bang" in PD parlance)
	pushobject->AddOut("Out", "Event", Push_Out, NULL, NULL);
	ursourceobjectlist.Append(pushobject);
	gyroobject = new ursObject("RotRate", NULL, NULL, 0, 3, true);
	gyroobject->AddOut("X", "TimeSeries", RotRate_X_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	gyroobject->AddOut("Y", "TimeSeries", RotRate_Y_Out, NULL, NULL);
	gyroobject->AddOut("Z", "TimeSeries", RotRate_Z_Out, NULL, NULL);
	ursourceobjectlist.Append(gyroobject);
	touchobject = new ursObject("Touch", NULL, NULL, 0, 22, true);
	touchobject->AddOut("X1", "TimeSeries", Touch_X1_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	touchobject->AddOut("Y1", "TimeSeries", Touch_Y1_Out, NULL, NULL);
	touchobject->AddOut("X2", "TimeSeries", Touch_X2_Out, NULL, NULL);
	touchobject->AddOut("Y2", "TimeSeries", Touch_Y2_Out, NULL, NULL);
	touchobject->AddOut("X3", "TimeSeries", Touch_X3_Out, NULL, NULL);
	touchobject->AddOut("Y3", "TimeSeries", Touch_Y3_Out, NULL, NULL);
	touchobject->AddOut("X4", "TimeSeries", Touch_X4_Out, NULL, NULL);
	touchobject->AddOut("Y4", "TimeSeries", Touch_Y4_Out, NULL, NULL);
	touchobject->AddOut("X5", "TimeSeries", Touch_X5_Out, NULL, NULL);
	touchobject->AddOut("Y5", "TimeSeries", Touch_Y5_Out, NULL, NULL);
	touchobject->AddOut("X6", "TimeSeries", Touch_X6_Out, NULL, NULL); // Pushers cannot be ticked (oh the poetic justice)
	touchobject->AddOut("Y6", "TimeSeries", Touch_Y6_Out, NULL, NULL);
	touchobject->AddOut("X7", "TimeSeries", Touch_X7_Out, NULL, NULL);
	touchobject->AddOut("Y7", "TimeSeries", Touch_Y7_Out, NULL, NULL);
	touchobject->AddOut("X8", "TimeSeries", Touch_X8_Out, NULL, NULL);
	touchobject->AddOut("Y8", "TimeSeries", Touch_Y8_Out, NULL, NULL);
	touchobject->AddOut("X9", "TimeSeries", Touch_X9_Out, NULL, NULL);
	touchobject->AddOut("Y9", "TimeSeries", Touch_Y9_Out, NULL, NULL);
	touchobject->AddOut("X10", "TimeSeries", Touch_X10_Out, NULL, NULL);
	touchobject->AddOut("Y10", "TimeSeries", Touch_Y10_Out, NULL, NULL);
	touchobject->AddOut("X11", "TimeSeries", Touch_X11_Out, NULL, NULL);
	touchobject->AddOut("Y11", "TimeSeries", Touch_Y11_Out, NULL, NULL);
	ursourceobjectlist.Append(touchobject);
    //	fileobject = new ursObject("File", NULL, NULL, 0, 1); // An file based source
    //	fileobject->AddOut("Out", "Event", NULL, NULL, NULL);
    //	ursourceobjectlist.Append(fileobject);
	
    
    /*    ratemasterobhject = new ursObject("RateMaster", RateMaster_Constructor, RateMaster_Destructor,1,2);
     ratemasterobhject->AddOut("Control", "TimeSeries", RateMaster_ControlTick, RateMaster_ControlOut, NULL);
     ratemasterobhject->AddOut("Read", "TimeSeries", RateMaster_ReadTick, RateMaster_ReadOut, NULL);
     ratemasterobject->AddIn("In", "Generic", RateMaster_In);
     urmanipulatorobjectlist.Append(ratemasterobject);
     */
	sinobject = new ursObject("SinOsc", SinOsc_Constructor, SinOsc_Destructor,4,1);
	sinobject->AddOut("Out", "TimeSeries", SinOsc_Tick, SinOsc_Out, NULL);
    //	sinobject->AddOut("Out", "TimeSeries", NULL, SinOsc_FillBuffer);
	sinobject->AddIn("Freq", "Frequency", SinOsc_SetFreq);
	sinobject->AddIn("Amp", "Amplitude", SinOsc_SetAmp);
	sinobject->AddIn("SRate", "Rate", SinOsc_SetRate);
	sinobject->AddIn("Time", "Time", SinOsc_SetPhase);
	urmanipulatorobjectlist.Append(sinobject);
    //	urmanipulatorobjectlist[lastmanipulatorobj++] = sinobject;
	
	object = new ursObject("Avg", Avg_Constructor, Avg_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", Avg_Tick, Avg_Out, NULL);
	object->AddIn("In", "TimeSeries", Avg_In);
	object->AddIn("Len", "Length", Avg_Len);
	object->SetCouple(0,0);
	urmanipulatorobjectlist.Append(object);
    
	
    
     object = new ursObject("Tuner", Tuner_Constructor, Tuner_Destructor,1,1);
     object->AddOut("Out", "TimeSeries", Tuner_Tick, Tuner_Out, NULL);
     object->AddIn("In", "TimeSeries", Tuner_In);
     object->SetCouple(0,0);
     urmanipulatorobjectlist.Append(object);

     object = new ursObject("Pump", Pump_Constructor, Pump_Destructor,2,1);
     object->AddOut("Out", "TimeSeries", Pump_Out, NULL, NULL);
     object->AddIn("In", "TimeSeries", Pump_In);
     object->AddIn("Time", "Timing", Pump_Time);
     object->SetCouple(0,0);
     urmanipulatorobjectlist.Append(object);
     
    
     object = new ursObject("Drain", Drain_Constructor, Drain_Destructor,1,2);
     object->AddOut("Out", "TimeSeries", Drain_Out, NULL, NULL);
     object->AddOut("Time", "Timing", Drain_Time, NULL, NULL);
     object->AddIn("In", "TimeSeries", Drain_In);
     object->SetCouple(0,0);
     urmanipulatorobjectlist.Append(object);
  
     object = new ursObject("Sniff", Sniff_Constructor, Sniff_Destructor,1,2);
     object->AddOut("Out", "TimeSeries", Sniff_Out, NULL, NULL);
     object->AddOut("Sniff", "TimeSeries", Sniff_Sniff, NULL, NULL);
     object->AddIn("In", "TimeSeries", Sniff_In);
     object->SetCouple(0,0);
     urmanipulatorobjectlist.Append(object);
     
     object = new ursObject("SniffL", SniffL_Constructor, SniffL_Destructor,1,2);
     object->AddOut("Out", "TimeSeries", SniffL_Out, NULL, NULL);
     object->AddOut("Sniff", "TimeSeries", SniffL_Sniff, NULL, NULL);
     object->AddIn("In", "TimeSeries", SniffL_In);
     object->SetCouple(0,0);
     urmanipulatorobjectlist.Append(object);

    object = new ursObject("Dist3", ThreeDist_Constructor, ThreeDist_Destructor,4,1);
	object->AddOut("Out", "TimeSeries", ThreeDist_Tick, ThreeDist_Out, NULL);
	object->AddIn("In1", "TimeSeries", ThreeDist_In1);
	object->AddIn("In2", "TimeSeries", ThreeDist_In2);
	object->AddIn("In3", "TimeSeries", ThreeDist_In3);
	object->AddIn("Train", "TimeSeries", ThreeDist_Train);
	urmanipulatorobjectlist.Append(object);

    object = new ursObject("Add", Add_Constructor, Add_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", Add_Tick, Add_Out, NULL);
	object->AddIn("In1", "TimeSeries", Add_In1);
	object->AddIn("In2", "TimeSeries", Add_In2);
	urmanipulatorobjectlist.Append(object);
    
	object = new ursObject("Min", Min_Constructor, Min_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", Min_Tick, Min_Out, NULL);
	object->AddIn("In1", "TimeSeries", Min_In1);
	object->AddIn("In2", "TimeSeries", Min_In2);
	urmanipulatorobjectlist.Append(object);
    
	object = new ursObject("Max", Max_Constructor, Max_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", Max_Tick, Max_Out, NULL);
	object->AddIn("In1", "TimeSeries", Max_In1);
	object->AddIn("In2", "TimeSeries", Max_In2);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("MinS", MinS_Constructor, MinS_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", MinS_Tick, MinS_Out, NULL);
	object->AddIn("In1", "TimeSeries", MinS_In1);
	object->AddIn("In2", "TimeSeries", MinS_In2);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("MaxS", MaxS_Constructor, MaxS_Destructor,2,1);
	object->AddOut("Out", "TimeSeries", MaxS_Tick, MaxS_Out, NULL);
	object->AddIn("In1", "TimeSeries", MaxS_In1);
	object->AddIn("In2", "TimeSeries", MaxS_In2);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("Demux", Demux316_Constructor, Demux316_Destructor,4,48);
	object->AddOut("Out1_1", "TimeSeries", Demux316_Out1_1, NULL, NULL);
	object->AddOut("Out1_2", "TimeSeries", Demux316_Out1_2, NULL, NULL);
	object->AddOut("Out1_3", "TimeSeries", Demux316_Out1_3, NULL, NULL);
	object->AddOut("Out2_1", "TimeSeries", Demux316_Out2_1, NULL, NULL);
	object->AddOut("Out2_2", "TimeSeries", Demux316_Out2_2, NULL, NULL);
	object->AddOut("Out2_3", "TimeSeries", Demux316_Out2_3, NULL, NULL);
	object->AddOut("Out3_1", "TimeSeries", Demux316_Out3_1, NULL, NULL);
	object->AddOut("Out3_2", "TimeSeries", Demux316_Out3_2, NULL, NULL);
	object->AddOut("Out3_3", "TimeSeries", Demux316_Out3_3, NULL, NULL);
	object->AddOut("Out4_1", "TimeSeries", Demux316_Out4_1, NULL, NULL);
	object->AddOut("Out4_2", "TimeSeries", Demux316_Out4_2, NULL, NULL);
	object->AddOut("Out4_3", "TimeSeries", Demux316_Out4_3, NULL, NULL);
	object->AddOut("Out5_1", "TimeSeries", Demux316_Out5_1, NULL, NULL);
	object->AddOut("Out5_2", "TimeSeries", Demux316_Out5_2, NULL, NULL);
	object->AddOut("Out5_3", "TimeSeries", Demux316_Out5_3, NULL, NULL);
	object->AddOut("Out6_1", "TimeSeries", Demux316_Out6_1, NULL, NULL);
	object->AddOut("Out6_2", "TimeSeries", Demux316_Out6_2, NULL, NULL);
	object->AddOut("Out6_3", "TimeSeries", Demux316_Out6_3, NULL, NULL);
	object->AddOut("Out7_1", "TimeSeries", Demux316_Out7_1, NULL, NULL);
	object->AddOut("Out7_2", "TimeSeries", Demux316_Out7_2, NULL, NULL);
	object->AddOut("Out7_3", "TimeSeries", Demux316_Out7_3, NULL, NULL);
	object->AddOut("Out8_1", "TimeSeries", Demux316_Out8_1, NULL, NULL);
	object->AddOut("Out8_2", "TimeSeries", Demux316_Out8_2, NULL, NULL);
	object->AddOut("Out8_3", "TimeSeries", Demux316_Out8_3, NULL, NULL);
	object->AddOut("Out9_1", "TimeSeries", Demux316_Out9_1, NULL, NULL);
	object->AddOut("Out9_2", "TimeSeries", Demux316_Out9_2, NULL, NULL);
	object->AddOut("Out9_3", "TimeSeries", Demux316_Out9_3, NULL, NULL);
	object->AddOut("Out10_1", "TimeSeries", Demux316_Out10_1, NULL, NULL);
	object->AddOut("Out10_2", "TimeSeries", Demux316_Out10_2, NULL, NULL);
	object->AddOut("Out10_3", "TimeSeries", Demux316_Out10_3, NULL, NULL);
	object->AddOut("Out11_1", "TimeSeries", Demux316_Out11_1, NULL, NULL);
	object->AddOut("Out11_2", "TimeSeries", Demux316_Out11_2, NULL, NULL);
	object->AddOut("Out11_3", "TimeSeries", Demux316_Out11_3, NULL, NULL);
	object->AddOut("Out12_1", "TimeSeries", Demux316_Out12_1, NULL, NULL);
	object->AddOut("Out12_2", "TimeSeries", Demux316_Out12_2, NULL, NULL);
	object->AddOut("Out12_3", "TimeSeries", Demux316_Out12_3, NULL, NULL);
	object->AddOut("Out13_1", "TimeSeries", Demux316_Out13_1, NULL, NULL);
	object->AddOut("Out13_2", "TimeSeries", Demux316_Out13_2, NULL, NULL);
	object->AddOut("Out13_3", "TimeSeries", Demux316_Out13_3, NULL, NULL);
	object->AddOut("Out14_1", "TimeSeries", Demux316_Out14_1, NULL, NULL);
	object->AddOut("Out14_2", "TimeSeries", Demux316_Out14_2, NULL, NULL);
	object->AddOut("Out14_3", "TimeSeries", Demux316_Out14_3, NULL, NULL);
	object->AddOut("Out15_1", "TimeSeries", Demux316_Out15_1, NULL, NULL);
	object->AddOut("Out15_2", "TimeSeries", Demux316_Out15_2, NULL, NULL);
	object->AddOut("Out15_3", "TimeSeries", Demux316_Out15_3, NULL, NULL);
	object->AddOut("Out16_1", "TimeSeries", Demux316_Out16_1, NULL, NULL);
	object->AddOut("Out16_2", "TimeSeries", Demux316_Out16_2, NULL, NULL);
	object->AddOut("Out16_3", "TimeSeries", Demux316_Out16_3, NULL, NULL);
	object->AddIn("In1", "TimeSeries", Demux316_In1);
	object->AddIn("In2", "TimeSeries", Demux316_In2);
	object->AddIn("In3", "TimeSeries", Demux316_In3);
    object->AddIn("Next", "Trigger", Demux316_Next);
	urmanipulatorobjectlist.Append(object);
    
	urSoundAtoms_Setup();
	
	object = new ursObject("Oct", Oct_Constructor, Oct_Destructor,2,1);
	object->AddOut("Out", "Generic", Oct_Out, Oct_Tick, NULL);
	object->AddIn("In", "Generic", Oct_In);
    object->AddIn("Freq", "Frequency", Oct_Freq);
	object->SetCouple(0,0);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("Range", Range_Constructor, Range_Destructor,3,1);
	object->AddOut("Out", "Generic", Range_Out, Range_Tick, NULL);
	object->AddIn("In", "Generic", Range_In);
    object->AddIn("Bottom", "Generic", Range_Bottom);
    object->AddIn("Top", "Generic", Range_Top);
	object->SetCouple(0,0);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("Quant", Quant_Constructor, Quant_Destructor,1,1);
	object->AddOut("Out", "Generic", Quant_Out, Quant_Tick, NULL);
	object->AddIn("In", "Generic", Quant_In);
	//	object->AddIn("Base", "Frequency", Quant_Oct);
	object->SetCouple(0,0);
	urmanipulatorobjectlist.Append(object);
	
	object = new ursObject("Gain", Gain_Constructor, Gain_Destructor,2,1);
	object->AddOut("Out", "Generic", Gain_Out, Gain_Tick, NULL);
	object->AddIn("In", "Generic", Gain_In);
	object->AddIn("Amp", "Amplitude", Gain_Amp);
	object->SetCouple(0,0);
	urmanipulatorobjectlist.Append(object);
    
    /*
     object = new ursObject("Slow", Slow_Constructor, Slow_Destructor,2,1);
     object->AddOut("Out", "TimeSeries", Slow_Tick, Slow_Out, NULL);
     object->AddIn("In", "TimeSeries", Slow_In);
     object->AddIn("Rate", "Rate", Slow_SetRate);
     urmanipulatorobjectlist.Append(object);
     */
	
	sampleobject = new ursObject("Sample", Sample_Constructor, Sample_Destructor,6,1);
	sampleobject->AddOut("Out", "TimeSeries", Sample_Tick, Sample_Out, NULL);
	sampleobject->AddIn("Amp", "Amplitude", Sample_SetAmp);
	sampleobject->AddIn("Rate", "Rate", Sample_SetRate);
	sampleobject->AddIn("Pos", "Position", Sample_SetPos);
	sampleobject->AddIn("Sample", "Sample", Sample_SetSample);
	sampleobject->AddIn("Loop", "State", Sample_SetLoop);
    sampleobject->AddIn("Trigger", "Trigger", Sample_Trigger);
    //	urmanipulatorobjectlist[lastmanipulatorobj++] = sampleobject;
	urmanipulatorobjectlist.Append(sampleobject);
    
#ifdef OFFER_SLEIGH
	object = new ursObject("Sleigh", Sleigh_Constructor, Sleigh_Destructor,6,1);
	object->AddOut("Out", "TimeSeries", Sleigh_Tick, Sleigh_Out, NULL);
	object->AddIn("Amp", "Amplitude", Sleigh_SetAmp);
	object->AddIn("Rate", "Rate", Sleigh_SetRate);
	object->AddIn("Pos", "Position", Sleigh_SetPos);
	object->AddIn("Sleigh", "Sleigh", Sleigh_SetSleigh);
	object->AddIn("Play", "Play", Sleigh_Play);
	object->AddIn("Loop", "Loop", Sleigh_Loop);
	//	urmanipulatorobjectlist[lastmanipulatorobj++] = Sleighobject;
	urmanipulatorobjectlist.Append(object);
#endif
	
	object = new ursObject("Looper", Looper_Constructor, Looper_Destructor,6,1);
	object->AddOut("Out", "TimeSeries", Looper_Tick, Looper_Out, NULL);
	object->AddIn("In", "TimeSeries", Looper_In);
	object->AddIn("Amp", "Amplitude", Looper_SetAmp);
	object->AddIn("Rate", "Rate", Looper_SetRate);
	object->AddIn("Record", "Trigger", Looper_Record);
	object->AddIn("Play", "Trigger", Looper_Play);
	object->AddIn("Pos", "Time", Looper_Pos);
	urmanipulatorobjectlist.Append(object);
    /*
     looprhythmobject = new ursObject("LoopRhythm", LoopRhythm_Constructor, LoopRhythm_Destructor,3,1);
     looprhythmobject->AddOut("Beats", "TimeSeries", LoopRhythm_Tick, LoopRhythm_Out, NULL);
     looprhythmobject->AddIn("BMP", "Rate", LoopRhythm_SetHMP);
     looprhythmobject->AddIn("Now", "Event", LoopRhythm_SetBeatNow);
     looprhythmobject->AddIn("Pos", "Position", LoopRhythm_Pos);
     //	urmanipulatorobjectlist[lastmanipulatorobj++] = looprhythmobject;
     urmanipulatorobjectlist.Append(looprhythmobject);
     */
	object = new ursObject("CMap", CircleMap_Constructor, CircleMap_Destructor,5,1);
	object->AddOut("Out", "TimeSeries", CircleMap_Tick, CircleMap_Out, NULL);
	object->AddIn("Freq", "Frequency", CircleMap_SetFreq);
	object->AddIn("NonL", "Generic", CircleMap_SetNonL);
	object->AddIn("Amp", "Amplitude", CircleMap_SetAmp);
	object->AddIn("SRate", "Rate", CircleMap_SetRate);
	object->AddIn("Time", "Time", CircleMap_SetPhase);
	urmanipulatorobjectlist.Append(object);
    
    /*
     object = new ursObject("OWF", OWF_Constructor, OWF_Destructor,4,1);
     object->AddOut("Out", "TimeSeries", OWF_Tick, OWF_Out, NULL);
     object->AddIn("Freq", "Frequency", OWF_SetFreq);
     object->AddIn("Amp", "Amplitude", OWF_SetAmp);
     object->AddIn("SRate", "Rate", OWF_SetRate);
     object->AddIn("Time", "Time", OWF_SetPhase);
     urmanipulatorobjectlist.Append(object);
     */
	
	dacobject = new ursObject("Dac", NULL, NULL, 1, 0, true);
	dacobject->AddIn("In", "TimeSeries", Dac_In);
	ursinkobjectlist.Append(dacobject);
    
	visobject = new ursObject("Vis", NULL, NULL, 1, 0, true);
	visobject->AddIn("In", "TimeSeries", Vis_In);
	ursinkobjectlist.Append(visobject);

	netobject = new ursObject("Net", NULL, NULL, 1, 0, true);
	netobject->AddIn("In", "Event", Net_In);
	ursinkobjectlist.Append(netobject);
	
	//	drainobject = new ursObject("Drain", NULL, NULL, 1, 0);
    //	drainobject->AddIn("In", "TimeSeries", Drain_In); // A rate based drain
    //	ursinkobjectlist.Append(drainobject);
	
	pullobject = new ursObject("Pull", NULL, NULL, 1, 0);
	pullobject->AddIn("In", "Event", Pull_In); // A event based drain ("bang" drain in PD parlance)
	ursinkobjectlist.Append(pullobject);
    
//#undef LOAD_STK_OBJECTS
#ifdef LOAD_STK_OBJECTS
	urSTK_Setup();
#endif
	
    urmanipulatorobjectlist.TestObjects();
    ursinkobjectlist.TestObjects();
    ursourceobjectlist.TestObjects();
    
}
