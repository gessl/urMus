
/*
 *  urAPI.c
 *  urMus
 *
 *  Created by Georg Essl on 6/20/09.
 *  Copyright 2009-11 Georg Essl. All rights reserved. See LICENSE.txt for license details.
 *
 */
#define FULL3
#ifdef FULL3

#include "urAPI.h"
#import "EAGLView.h"
#import "MachTimer.h"
#include "urSound.h"
#include "httpServer.h"


//------------------------------------------------------------------------------
// Recursive Mutex for ThreadSafety
//------------------------------------------------------------------------------

#ifdef THREADSAFETY
extern RMutex luamutex;
#endif

//------------------------------------------------------------------------------
// Mutex for Event Safety
//------------------------------------------------------------------------------
pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER; // Flowbox, safeguards freeing flowboxes
pthread_mutex_t r_mutex = PTHREAD_MUTEX_INITIALIZER; // Region, safeguards freeing regions
pthread_mutex_t n_mutex = PTHREAD_MUTEX_INITIALIZER; // Network threat safety

//------------------------------------------------------------------------------
// MUMO Audio Callbacks
//------------------------------------------------------------------------------

#ifdef USEMUMOAUDIO
// Implement audio callback here
void audioCallback( Float32 * buffer, UInt32 framesize, void* userData)
{
	// NSLog(@"inside audioCB");
	
#ifdef ENABLE_URMICEVENTS
	currentMicBuffer = buffer;
	callAllOnMicrophone(currentMicBuffer, framesize);
#endif
    
#ifdef ENABLE_URSOUNDBUFFER		
    ur_GetSoundBuffer(buffer, j+1, inNumberFrames);
#endif
	for (int i=0; i<framesize; i++)	{
		callAllMicSingleTickSourcesF(buffer[2*i]);
		buffer[2*i] = buffer[2*i+1] = urs_PullActiveDacSingleTickSinks();    // amplitude*sin(2.0*M_PI*phase);
	}
}
#endif

//------------------------------------------------------------------------------
// Curly
//------------------------------------------------------------------------------

#define CURLY
#ifdef CURLY
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "curl.h"

//------------------------------------------------------------------------------
// Curly FileIO Callbacks
//------------------------------------------------------------------------------

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t retcode;
    
    /* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */ 
    retcode = fread(ptr, size, nmemb, (FILE *)stream);
    
//    fprintf(stderr, "*** We read %d bytes from file\n", retcode);
    
    return retcode;
}
#endif

//------------------------------------------------------------------------------
// Extern exchange with Display
//------------------------------------------------------------------------------

// Make EAGLview global so lua interface can grab it without breaking a leg over IMP
extern EAGLView* g_glView;
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern int TOUCH_SCREEN_WIDTH;
extern int TOUCH_SCREEN_HEIGHT;
extern bool portrait;

// This is to transport error and print messages to EAGLview
extern std::string errorstr;
extern bool newerror;
enum eventIDs currenterrorevent;

//------------------------------------------------------------------------------
// Sharing global lua state
//------------------------------------------------------------------------------

// Global lua state
lua_State *lua= NULL;

// Region based API below, this is inspired by WoW's frame API with many modifications and expansions.
// Our engine supports paging, region horizontal and vertical scrolling, full multi-touch and more.

//------------------------------------------------------------------------------
// Paging
//------------------------------------------------------------------------------

// Hardcoded for now... lazy me
#define MAX_PAGES 40
#define MAX_PATCHES 30

int currentPage = 0;
int currentExternalPage = 0;
bool linkExternal = true;

urAPI_Region_t* firstRegion[MAX_PAGES] = {nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil};
urAPI_Region_t* lastRegion[MAX_PAGES] = {nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil};
int numRegions[MAX_PAGES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//------------------------------------------------------------------------------
// UIParent region
//------------------------------------------------------------------------------

urAPI_Region_t* UIParent = nil;

//------------------------------------------------------------------------------
// Flowbox patches (NYI)
//------------------------------------------------------------------------------

//ursAPI_FlowBox_t* FBNope = nil;

int currentPatch = 0;
ursAPI_FlowBox_t** firstFlowbox[MAX_PATCHES] = {nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil};
int numFlowBoxes[MAX_PATCHES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int freePatches[MAX_PATCHES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

MachTimer* systimer;

const char DEFAULT_RPOINT[] = "BOTTOMLEFT";

#define STRATA_PARENT 0
#define STRATA_BACKGROUND 1
#define STRATA_LOW 2
#define STRATA_MEDIUM 3
#define STRATA_HIGH 4
#define STRATA_DIALOG 5
#define STRATA_FULLSCREEN 6
#define STRATA_FULLSCREEN_DIALOG 7
#define STRATA_TOOLTIP 8

#define LAYER_BACKGROUND 1
#define LAYER_BORDER 2
#define LAYER_ARTWORK 3
#define LAYER_OVERLAY 4
#define LAYER_HIGHLIGHT 5

//------------------------------------------------------------------------------
// Event strings
//------------------------------------------------------------------------------

// IMPORTANT: Make sure to keep urEventNames and the enum eventIDs (in urAPI.h) synchronized, else bad things happen.

// Events Strings, index has to match ID.
const char* urEventNames[] = { "OnDragStart", "OnDragStop", "OnHide", "OnShow", "OnTouchDown", "OnTouchUp", "OnDoubleTap", "OnSizeChanged", "OnEnter", "OnLeave", "OnUpdate", "OnNetIn", "OnNetConnect", "OnNetDisconnect", "OnOSCMessage",
#ifdef SANDWICH_SUPPORT
    "OnPressure",
#endif
#ifdef SOAR_SUPPORT
    "OnSoarOutput",
#endif
    "OnAccelerate", "OnAttitude", "OnRotation", "OnHeading", "OnLocation", "OnMicrophone", "OnHorizontalScroll", "OnVerticalScroll", "OnMove", "OnPageEntered", "OnPageLeft", "OnDragging", "OnKeyboard", "OnKeyboardBackspace",
};


//------------------------------------------------------------------------------
// Page Camera Usage
//------------------------------------------------------------------------------

// Camera chain keeps track of which regions use the camera

int page_camerause=0;

//------------------------------------------------------------------------------
// Event Chains
//------------------------------------------------------------------------------

// Event chains prevent that resorting or changing region order interfers with event processing. Events are processed by chain order not by strata order.

Region_Chain_Iterator_t EventChain[EventsCount];

//------------------------------------------------------------------------------
// Event Chain handling
//------------------------------------------------------------------------------

void RemoveRegionFromChain(Region_Chain_Iterator_t &chain, urAPI_Region* region)
{
    if (region->page == currentPage)
    {
        Region_Chain_t* p = NULL;
        for( Region_Chain_t* c=chain.first; c!= NULL; c=c->next)
        {
            if(c->region == region)
            {
                if(p == NULL)
                {
                    p = c->next;
                    free(c);
                    chain.first = p;
                    return;
                }
                p->next = c->next;
                free(c);
                return;
            }
            p = c;
        }
    }
}

void AddRegionToChain(Region_Chain_Iterator_t &chain, urAPI_Region* region)
{
    if (region->page == currentPage)
    {
        Region_Chain_t* n = (Region_Chain_t*)malloc(sizeof(Region_Chain_t));
        n->region = region;
        n->next = chain.first;
        chain.first = n;
    }
}

void FreeChain(Region_Chain_Iterator_t &chain)
{
    Region_Chain_t* p;
    for( Region_Chain_t* c=chain.first; c!= NULL; c=p)
    {
        p = c->next;
        free(c);
    }
    chain.first = NULL;
}

void FreeAllChains()
{
    for(int i=0; i<MAX_EVENTS; i++)
    {
        FreeChain(EventChain[i]);
    }
}

void PopulateAllChains(urAPI_Region_t* first)
{
    for(urAPI_Region_t* region=first; region != nil; region=region->next)
	{
        for(int i=0; i< MAX_EVENTS; i++)
        {
            if(region->OnEvents[i] != 0)
            {
                AddRegionToChain(EventChain[i], region);
            }
        }
        // Populate Camera Chain for performance reasons here.
        if(region->texture != NULL && region->texture->usecamera != 0)
        {
            page_camerause++;
//            AddRegionToChain(CameraChain, region);
            incCameraUse();
        }
    }
}

void RemoveEventRegistry(lua_State* lua, enum eventIDs event, urAPI_Region_t* region)
{
    luaL_unref(lua, LUA_REGISTRYINDEX, region->OnEvents[event]);
    // Update iterator if needed
    if(EventChain[event].current!=NULL && region==EventChain[event].current->region)
    {
        EventChain[event].current = EventChain[event].next; 
        if(EventChain[event].next != NULL)
            EventChain[event].next = EventChain[event].next->next;
    }
    if(EventChain[event].next != NULL && region==EventChain[event].next->region)
    {
        EventChain[event].next = EventChain[event].next->next;
    }
    RemoveRegionFromChain(EventChain[event], region);
    region->OnEvents[event] = 0;
}

//------------------------------------------------------------------------------
// Event Service functions (single region argument calls)
//------------------------------------------------------------------------------

void lock_events()
{
    pthread_mutex_lock( &n_mutex );
}

void unlock_events()
{
    pthread_mutex_unlock( &n_mutex );
}

bool callScriptWithOscArgs(enum eventIDs event, int func_ref, urAPI_Region_t* region, osc::ReceivedMessageArgumentStream & s, osc::ReceivedMessage::const_iterator & arg)
{
	if(func_ref == 0) return false;
    
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	int len = 0;
    
	while(!s.Eos())
	{
		float num;
		s >> num;
		lua_pushnumber(lua,num);
		len = len+1;
	}
	if(lua_pcall(lua,len+1,0,0) != 0)
	{
		// Error!!
        
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
        pthread_mutex_unlock( &n_mutex );
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith5Args(enum eventIDs event, int func_ref, urAPI_Region_t* region, float a, float b, float c, float d, float e)
{
	if(func_ref == 0) return false;
	
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushnumber(lua,a);
	lua_pushnumber(lua,b);
	lua_pushnumber(lua,c);
	lua_pushnumber(lua,d);
	lua_pushnumber(lua,e);
	if(lua_pcall(lua,6,0,0) != 0)
	{
		// Error!!
		const char* error = lua_tostring(lua, -1);
		errorstr = error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith4Args(enum eventIDs event, int func_ref, urAPI_Region_t* region, float a, float b, float c, float d)
{
	if(func_ref == 0) return false;
    
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushnumber(lua,a);
	lua_pushnumber(lua,b);
	lua_pushnumber(lua,c);
	lua_pushnumber(lua,d);
	if(lua_pcall(lua,5,0,0) != 0)
	{
		// Error!!
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		errorstr = error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith3Args(enum eventIDs event, int func_ref, urAPI_Region_t* region, float a, float b, float c)
{
	if(func_ref == 0) return false;
	
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushnumber(lua,a);
	lua_pushnumber(lua,b);
	lua_pushnumber(lua,c);
	if(lua_pcall(lua,4,0,0) != 0)
	{
		// Error!!
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith2Args(enum eventIDs event, int func_ref, urAPI_Region_t* region, float a, float b)
{
	if(func_ref == 0) return false;
	
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushnumber(lua,a);
	lua_pushnumber(lua,b);
	if(lua_pcall(lua,3,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
        if(error)
            errorstr = eventstr+": "+error; // DPrinting errors for now
        else
            errorstr = eventstr+": Unknown error";
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith1Args(enum eventIDs event, int func_ref, urAPI_Region_t* region, float a)
{
	if(func_ref == 0) return false;
	
	//		int func_ref = region->OnDragging;
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushnumber(lua,a);
	if(lua_pcall(lua,2,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
    
	// OK!
	return true;
}

bool callScriptWith1Char(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char *a)
{
//    char st[2];
//    st[0]=a;
//    st[1]=0;
    if(func_ref == 0) return false;
	
	//		int func_ref = region->OnDragging;
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
//	lua_pushstring(lua,st);
	lua_pushstring(lua,a);
	if(lua_pcall(lua,2,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
    
	// OK!
	return true;
}

bool callScriptWith1Global(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* globaldata)
{
	if(func_ref == 0) return false;
	
	//		int func_ref = region->OnDragging;
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_getglobal(lua, globaldata);
	if(lua_pcall(lua,2,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith1String(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* name)
{
	if(func_ref == 0) return false;
	
	//		int func_ref = region->OnDragging;
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushstring(lua, name);
    int perror = lua_pcall(lua,2,0,0);
	if( perror != 0)
	{
		//<return Error>
        if(perror == LUA_ERRRUN){
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
        if(error != NULL)
            errorstr = eventstr+": "+error; // DPrinting errors for now
        else
            errorstr = eventstr+": "+"Null Error";
		newerror = true;
        ur_Log(errorstr.c_str());
        }
        else if (perror == LUA_ERRMEM)
        {
            std::string eventstr(urEventNames[event]);
            errorstr = eventstr+": "+"LUA_ERRMEM"; // DPrinting errors for now
            newerror = true;
            ur_Log(errorstr.c_str());
            
        }
        else if (perror == LUA_ERRERR)
        {
            std::string eventstr(urEventNames[event]);
            errorstr = eventstr+": "+"LUA_ERRERR"; // DPrinting errors for now
            newerror = true;
            ur_Log(errorstr.c_str());
            
        }
        else
        {
            std::string eventstr(urEventNames[event]);
            errorstr = eventstr+": "+"LUA_UNKNOWNERROR"; // DPrinting errors for now
            newerror = true;
            ur_Log(errorstr.c_str());
            
        }
		return false;
	}
	
	// OK!
	return true;
}

bool callScriptWith2String(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* name, const char* btype)
{
	if(func_ref == 0) return false;
	
	//		int func_ref = region->OnDragging;
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	lua_pushstring(lua, name);
	lua_pushstring(lua, btype);
	if(lua_pcall(lua,3,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}

bool callScript(enum eventIDs event, int func_ref, urAPI_Region_t* region)
{
	if(func_ref == 0) return false;
	
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	if(lua_pcall(lua,1,0,0) != 0) // find table of udata here!!
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
        std::string eventstr(urEventNames[event]);
		errorstr = eventstr+": "+error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
    
	// OK!
	return true;
}

//------------------------------------------------------------------------------
// Event Service functions (All region argument calls)
//------------------------------------------------------------------------------

bool callAllEvents(enum eventIDs event)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
    
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScript(OnKeyboardBackspace,t->OnEvents[event], t);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
}

bool callAllOn1Args(enum eventIDs event, float data)
{
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith1Args(event,t->OnEvents[event], t, data);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
	return true;
    
}

bool callAllOn1Char(enum eventIDs event, const char *data)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith1Char(event,t->OnEvents[event], t, data);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
    
}

bool callAllOn1Global(enum eventIDs event, const char* data)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith1Global(event,t->OnEvents[event], t, data);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
    
}

bool callAllOn2Args(enum eventIDs event, float data, float data2)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith2Args(event,t->OnEvents[event], t, data, data2);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
    
}

bool callAllOn3Args(enum eventIDs event, float data, float data2, float data3)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith3Args(event,t->OnEvents[event], t, data, data2, data3);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
    
}

bool callAllOn4Args(enum eventIDs event, float data, float data2, float data3, float data4)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith4Args(event,t->OnEvents[event], t, data, data2, data3, data4);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
    
}

bool callAllOn1String(enum eventIDs event, const char* data)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith1String(event,t->OnEvents[event],t,data);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
}

bool callAllOn2Strings(enum eventIDs event, const char* data, const char* data2)
{
    lock_events();
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWith2String(event,t->OnEvents[event],t,data,data2);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
	return true;
}

bool callAllOnOSCArgs(enum eventIDs event, osc::ReceivedMessageArgumentStream & argument_stream, osc::ReceivedMessage::const_iterator & arg)
{
    lock_events();
    
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the EventChain[event] should a region unhook itself
        EventChain[event].next = EventChain[event].first;
        
        while(EventChain[event].next != NULL)
        {
            urAPI_Region_t* t = EventChain[event].next->region;
            callScriptWithOscArgs(event,t->OnEvents[event],t,argument_stream, arg);
            EventChain[event].next = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
    unlock_events();
    
	return true;
    
}

//------------------------------------------------------------------------------
// Event specific interface calls for all regions
//------------------------------------------------------------------------------

bool callAllOnUpdate(float time)
{
    lock_events();
    callAllOn1Args(OnUpdate, time);
    unlock_events();
	return true;
}

#ifdef SOAR_SUPPORT
bool callAllOnSoarOutput()
{
    lock_events();
    enum eventIDs event = OnSoarOutput;
    
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next;
        EventChain[event].prev = NULL;
        EventChain[event].current = EventChain[event].first;
        
        while(EventChain[event].current != NULL)
        {
            urAPI_Region_t* t = EventChain[event].current->region;
            if (t->OnEvents[event] != 0)
            {
                int counter=0;
                
                while ( !t->soarAgent->Commands() && (counter<SOAR_ASYNCH_PER_UPDATE) )
                {
                    t->soarAgent->RunSelfTilOutput();
                    counter++;
                }
                
                if (t->soarAgent->Commands())
                {
                    int callback_id = t->OnEvents[event];
                    t->OnEvents[event]=0;
                    if(EventChain[event].current == EventChain[event].first)
                        EventChain[event].first = EventChain[event].next;
                    else if( EventChain[event].prev != NULL)
                        EventChain[event].prev->next = EventChain[event].next;
                    
                    free(EventChain[event].current);
                    
                    callScript(event,callback_id, t);
                }
            }
            EventChain[event].current = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
            if(EventChain[event].prev == NULL)
                EventChain[event].prev = EventChain[event].first;
            else
                EventChain[event].prev = EventChain[event].prev->next;
        }
    }
    unlock_events();
    return true;
}
#endif

bool callAllOnPageEntered(float page)
{
    callAllOn1Args(OnPageEntered, page);

	return true;
}

bool callAllOnPageLeft(float page)
{
    callAllOn1Args(OnPageLeft, page);

	return true;
}

#ifdef SOAR_SUPPORT
bool callScriptWith2ActionTableArgs(enum eventIDs event, int func_ref, urAPI_Region_t* region)
{
	if(func_ref == 0) return false;
	
	// Call lua function by stored Reference
	lua_rawgeti(lua,LUA_REGISTRYINDEX, func_ref);
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	//constuctSoarArgs();
	//lua_pushnumber(lua,a);
	//lua_pushnumber(lua,b);
	if(lua_pcall(lua,3,0,0) != 0)
	{
		//<return Error>
		const char* error = lua_tostring(lua, -1);
		errorstr = error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
		return false;
	}
	
	// OK!
	return true;
}
#endif

bool callAllOnLocation(float latitude, float longitude)
{
    callAllOn2Args(OnLocation, latitude, longitude);

	return true;
}

bool callAllOnHeading(float x, float y, float z, float north)
{
    callAllOn4Args(OnHeading,x,y,z,north);

    return true;
}

bool callAllOnAttitude(float x, float y, float z, float w)
{
    callAllOn4Args(OnAttitude, x,y,z,w);

    return true;
}

bool callAllOnRotRate(float x, float y, float z)
{
    callAllOn3Args(OnRotation, x,y,z);

    return true;
}

bool callAllOnAccelerate(float x, float y, float z)
{
    callAllOn3Args(OnAccelerate, x,y,z);

    return true;
}

bool callAllOnNetIn(float a)
{
    lock_events();
    callAllOn1Args(OnNetIn, a);
    unlock_events();

    return true;
}

bool callAllOnNetConnect(const char* name, const char* btype)
{
    callAllOn2Strings(OnNetConnect, name, btype);

    return true;
}

bool callAllOnNetDisconnect(const char* name, const char* btype)
{
    callAllOn2Strings(OnNetDisconnect, name, btype);

    return true;
}

// Shared event for different argument types.
bool callAllOnOSCMessage(osc::ReceivedMessageArgumentStream & argument_stream, osc::ReceivedMessage::const_iterator & arg)
{
    callAllOnOSCArgs(OnOSCMessage, argument_stream, arg);

    return true;
}

// Shared event for different argument types.
bool callAllOnOSCString(const char* str)
{
    callAllOn1String(OnOSCMessage, str);

    return true;
}

#ifdef SANDWICH_SUPPORT
bool callAllOnPressure(float p)
{
    lock_events();
    callAllOn1Args(OnPressure, p);
    unlock_events();

    return true;
}
#endif

bool callAllOnKeyboard(const char *c)
{
    callAllOn1Char(OnKeyboard, c);

    return true;
}

bool callAllOnKeyboardBackspace()
{
    callAllEvents(OnKeyboardBackspace);

    return true;
}

bool callAllOnMicrophone(SInt32* mic_buffer, UInt32 bufferlen)
{
    lock_events();
	lua_getglobal(lua, "urMicData");
	if(lua_isnil(lua, -1) || !lua_istable(lua,-1)) // Channel doesn't exist or is falsely set up
	{
		lua_pop(lua,1);
		return false;
	}
	
	for(UInt32 i=0;i<bufferlen; i++)
	{
		lua_pushnumber(lua, mic_buffer[i]);
		lua_rawseti(lua, -2, i+1);
	}	
	lua_setglobal(lua, "urMicData");
    
    unlock_events();
    callAllOn1Global(OnMicrophone, "urMicData");
	return true;
}

//------------------------------------------------------------------------------
// Events around Region Collision (hit, OnEnter, OnLeave)
//------------------------------------------------------------------------------

urAPI_Region_t* findRegionHit(float x, float y)
{
	for(urAPI_Region_t* t=lastRegion[currentPage]; t != nil /* && t != firstRegion[currentPage] */; t=t->prev)
	{
		if(x >= t->left && x <= t->left+t->width &&
		   y >= t->bottom && y <= t->bottom+t->height && t->isTouchEnabled)
			if(t->isClipping==false || (x >= t->clipleft && x <= t->clipleft+t->clipwidth &&
										y >= t->clipbottom && y <= t->clipbottom+t->clipheight))
			{
//				t->lastinputx = x - t->left; GESSL: Check reason why removed (Android->iOS new pinching code)
//				t->lastinputy = y - t->bottom;
				return t;
			}
	}
	return nil;
}

void callAllOnLeaveRegions(int nr, float* x, float* y, float* ox, float* oy)
{
    enum eventIDs event = OnLeave;
    if(EventChain[event].first != NULL)
    {
        EventChain[event].next = EventChain[event].first->next; // Helps save-guard the chain should a region unhook itself
        EventChain[event].current = EventChain[event].first;
        
        while(EventChain[event].current != NULL)
        {
            urAPI_Region_t* t = EventChain[event].current->region;
            for(int i=0; i<nr; i++)
            {
                if(!(x[i] >= t->left && x[i] <= t->left+t->width &&
                     y[i] >= t->bottom && y[i] <= t->bottom+t->height) && 
                   ox[i] >= t->left && ox[i] <= t->left+t->width &&
                   oy[i] >= t->bottom && oy[i] <= t->bottom+t->height			   
                   && t->OnEvents[event] != 0)
                {
                    t->entered = false;
                    callScriptWith2Args(event,t->OnEvents[event], t,x[i]-t->left,y[i]-t->bottom);
                }
            }
            EventChain[event].current = EventChain[event].next;
            if(EventChain[event].next != NULL)
                EventChain[event].next = EventChain[event].next->next;
        }
    }
}

void callAllOnEnterLeaveRegions(int nr, float* x, float* y, float* ox, float* oy)
{
    enum eventIDs event = OnLeave;
    for(int i=0; i<nr; i++)
    {
        event = OnLeave;
        if(EventChain[event].first != NULL)
        {
            EventChain[event].next = EventChain[event].first->next; // Helps save-guard the chain should a region unhook itself
            EventChain[event].current = EventChain[event].first;
            
            while(EventChain[event].current != NULL)
            {
                urAPI_Region_t* t = EventChain[event].current->region;
                if(!(x[i] >= t->left && x[i] <= t->left+t->width &&
                     y[i] >= t->bottom && y[i] <= t->bottom+t->height) && 
                   ox[i] >= t->left && ox[i] <= t->left+t->width &&
                   oy[i] >= t->bottom && oy[i] <= t->bottom+t->height			   
                   && t->OnEvents[event] != 0)
                {
                    t->entered = false;
                    callScriptWith2Args(event,t->OnEvents[event], t,x[i]-t->left,y[i]-t->bottom);
                }
                EventChain[event].current = EventChain[event].next;
                if(EventChain[event].next != NULL)
                    EventChain[event].next = EventChain[event].next->next;
            }
            EventChain[event].current = NULL;
            EventChain[event].next = NULL;
        }

        event = OnEnter;
        
        if(EventChain[event].first != NULL)
        {
            EventChain[event].next = EventChain[event].first->next; // Helps save-guard the chain should a region unhook itself
            EventChain[event].current = EventChain[event].first;
            while(EventChain[event].current != NULL)
            {
                urAPI_Region_t* t = EventChain[event].current->region;
                if(x[i] >= t->left && x[i] <= t->left+t->width &&
                   y[i] >= t->bottom && y[i] <= t->bottom+t->height &&
                   (!(ox[i] >= t->left && ox[i] <= t->left+t->width &&
                      oy[i] >= t->bottom && oy[i] <= t->bottom+t->height) || !t->entered)			   
                   && t->OnEvents[event] != 0)
                {
                    t->entered = true;
                    callScriptWith2Args(event,t->OnEvents[event], t, x[i]-t->left, y[i]-t->bottom);
                }
                EventChain[event].current = EventChain[event].next;
                if(EventChain[event].next != NULL)
                    EventChain[event].next = EventChain[event].next->next;
            }
            EventChain[event].current = NULL;
            EventChain[event].next = NULL;
        }
    }
}

//------------------------------------------------------------------------------
// Region Dragging
//------------------------------------------------------------------------------

urAPI_Region_t* findRegionDraggable(float x, float y)
{
    // NYI Move to chain processing
    for(urAPI_Region_t* t=lastRegion[currentPage]; t != nil ; t=t->prev)
	{
		if(x >= t->left && x <= t->left+t->width &&
		   y >= t->bottom && y <= t->bottom+t->height && t->isMovable && t->isTouchEnabled)
			if(t->isClipping==false || (x >= t->clipleft && x <= t->clipleft+t->clipwidth &&
								  y >= t->clipbottom && y <= t->clipbottom+t->clipheight))
				return t;
	}
	return nil;
}

//------------------------------------------------------------------------------
// Region Scrolling/Moving
//------------------------------------------------------------------------------

urAPI_Region_t* findRegionXScrolled(float x, float y, float dx)
{
	if(fabs(dx) > 0.9)
	{
        // NYI Move to chain processing
		for(urAPI_Region_t* t=lastRegion[currentPage]; t != nil ; t=t->prev)
		{
			if(x >= t->left && x <= t->left+t->width &&
			   y >= t->bottom && y <= t->bottom+t->height && t->isScrollXEnabled && t->isTouchEnabled)
				if(t->isClipping==false || (x >= t->clipleft && x <= t->clipleft+t->clipwidth &&
									  y >= t->clipbottom && y <= t->clipbottom+t->clipheight))
				return t;
		}
	}
	return nil;
}

urAPI_Region_t* findRegionYScrolled(float x, float y, float dy)
{
	if(fabs(dy) > 0.9*3)
	{
        // NYI Move to chain processing
 		for(urAPI_Region_t* t=lastRegion[currentPage]; t != nil ; t=t->prev)
		{
			if(x >= t->left && x <= t->left+t->width &&
			   y >= t->bottom && y <= t->bottom+t->height && t->isScrollYEnabled && t->isTouchEnabled)
				if(t->isClipping==false || (x >= t->clipleft && x <= t->clipleft+t->clipwidth &&
									  y >= t->clipbottom && y <= t->clipbottom+t->clipheight))
				return t;
		}
	}
	return nil;
}

urAPI_Region_t* findRegionMoved(float x, float y, float dx, float dy)
{
    for(Region_Chain_t* c=EventChain[OnMove].first; c != nil; c=c->next)
    {
        urAPI_Region_t* t = c->region;
		if(x >= t->left && x <= t->left+t->width &&
		   y >= t->bottom && y <= t->bottom+t->height && t->isTouchEnabled && t->OnEvents[OnMove] != 0)
			if(t->isClipping==false || (x >= t->clipleft && x <= t->clipleft+t->clipwidth &&
										y >= t->clipbottom && y <= t->clipbottom+t->clipheight))
				return t;
	}
	return nil;
}

//------------------------------------------------------------------------------
// Region Layouting
//------------------------------------------------------------------------------

void layoutchildren(urAPI_Region_t* region)
{
	urAPI_Region_t* child = region->firstchild;
	while(child!=NULL)
	{
		child->update = true;
		layout(child);
		child = child->nextchild;
	}
}

bool visibleparent(urAPI_Region_t* region)
{
	if(region == UIParent)
		return true;

	urAPI_Region_t* parent = region->parent;
	
	while(parent != UIParent && parent->isVisible == true)
	{
		parent = parent->parent;
	}
	
	if(parent == UIParent)
		return true;
	else
		return false;
}

void showchildren(urAPI_Region_t* region)
{
	urAPI_Region_t* child = region->firstchild;
	while(child!=NULL)
	{
		if(child->isShown)
		{
			child->isVisible = true;
			if(region->OnEvents[OnShow] != 0)
				callScript(OnShow,region->OnEvents[OnShow], region);
			showchildren(child);
		}
		child = child->nextchild;
	}
}

void hidechildren(urAPI_Region_t* region)
{
	urAPI_Region_t* child = region->firstchild;
	while(child!=NULL)
	{
		if(child->isVisible)
		{
			child->isVisible = false;
			if(region->OnEvents[OnHide] != 0)
				callScript(OnHide,region->OnEvents[OnHide], region);
			hidechildren(child);
		}
		child = child->nextchild;
	}
}

// This function is heavily informed by Jerry's base.lua in wowsim function, which is covered by a BSD-style (open) license.
// (EDIT) Fixed it up. Was buggy as is and didn't properly align for most anchor sides.

bool layout(urAPI_Region_t* region)
{
	if(region == nil) return false;
	
	bool update = region->update;

	if(!update)
	{
		if(region->relativeRegion)
			update = layout(region->relativeRegion);
		else
			update = layout(region->parent);
	}
		
	if(!update) return false;

/*	if(region->textlabel!=NULL)
		region->textlabel->updatestring = true; */

	float left, right, top, bottom, width, height, cx, cy,x,y;

	left = right = top = bottom = width = height = cx = cy = x = y = -1000000;
	
	
	const char* point = region->point;
	if(point == nil)
		point = DEFAULT_RPOINT;
	
	urAPI_Region_t* relativeRegion = region->relativeRegion;
	if(relativeRegion == nil)
		relativeRegion = region->parent;
	if(relativeRegion == nil)
		relativeRegion = UIParent; // This should be another layer but we don't care for now

	const char* relativePoint = region->relativePoint;
	if(relativePoint == nil)
		relativePoint = DEFAULT_RPOINT;

	if(!strcmp(relativePoint, "ALL"))
	{
		left = relativeRegion->left;
		bottom = relativeRegion->bottom;
		width = relativeRegion->width;
		height = relativeRegion->height;
	}
	else if(!strcmp(relativePoint,"TOPLEFT"))
	{
		x = relativeRegion->left;
		y = relativeRegion->top;
	}
	else if(!strcmp(relativePoint,"TOPRIGHT"))
	{
		x = relativeRegion->right;
		y = relativeRegion->top;
	}
	else if(!strcmp(relativePoint,"TOP"))
	{
		x = relativeRegion->cx;
		y = relativeRegion->top;
	}
	else if(!strcmp(relativePoint,"LEFT"))
	{
		x = relativeRegion->left;
		y = relativeRegion->cy;
	}
	else if(!strcmp(relativePoint,"RIGHT"))
	{
		x = relativeRegion->right;
		y = relativeRegion->cy;
	}
	else if(!strcmp(relativePoint,"CENTER"))
	{
		x = relativeRegion->cx;
		y = relativeRegion->cy;
	}
	else if(!strcmp(relativePoint,"BOTTOMLEFT"))
	{
		x = relativeRegion->left;
		y = relativeRegion->bottom;
	}
	else if(!strcmp(relativePoint,"BOTTOMRIGHT"))
	{
		x = relativeRegion->right;
		y = relativeRegion->bottom;
	}
	else if(!strcmp(relativePoint,"BOTTOM"))
	{
		x = relativeRegion->cx;
		y = relativeRegion->bottom;
	}
	else
	{
		// Error!!
		luaL_error(lua, "Unknown relativePoint when layouting regions.");
		return false;
	}
	
	x = x+region->ofsx;
	y = y+region->ofsy;

	if(!strcmp(point,"TOPLEFT"))
	{
		left = x;
		top = y;
	}
	else if(!strcmp(point,"TOPRIGHT"))
	{
		right = x;
		top = y;
	}
	else if(!strcmp(point,"TOP"))
	{
		cx = x;
		top = y;
	}
	else if(!strcmp(point,"LEFT"))
	{
		left = x;
		cy = y; // Another typo here
	}
	else if(!strcmp(point,"RIGHT"))
	{
		right = x;
		cy = y;
	}
	else if(!strcmp(point,"CENTER"))
	{
		cx = x;
		cy = y;
	}
	else if(!strcmp(point,"BOTTOMLEFT"))
	{
		left = x;
		bottom = y;
	}
	else if(!strcmp(point,"BOTTOMRIGHT"))
	{
		right = x;
		bottom = y;
	}
	else if(!strcmp(point,"BOTTOM"))
	{
		cx = x;
		bottom = y;
	}
	else
	{
		// Error!!
		luaL_error(lua, "Unknown relativePoint when layouting regions.");
		return false;
	}
	
	if(left > 0 && right > 0)
	{
		width = right - left;
	}
	if(top > 0 && bottom > 0)
	{
		height = top - bottom;
	}
	
	if(width == -1000000 && region->width > 0) width = region->width;
	if(height == -1000000 && region->height > 0) height = region->height;
	
	if(left == -1000000 && width > 0)
	{
		if(right>0) left = right - width;
		else if(cx>0)
		{
			left = cx - width/2; // This was buggy. Fixing it up.
			right = cx + width/2;
		} 
	}
	if(bottom == -1000000 && height > 0)
	{
		if(top>0) bottom = top - height;
		if(cy>0) 
		{
			bottom = cy - height/2; // This was buggy. Fixing it up.
			top = cy + height/2;
		}
	}
	
	update = false;
	
	if(left != region->left || bottom != region->bottom || width != region->width || height != region->height)
		update = true;

	if(region->textlabel!=NULL && (width != region->width || height != region->height))
        region->textlabel->updatestring = true;

    
	region->left = left;
	region->bottom = bottom;
	region->width = width;
	region->height = height;
	region->cx = left + width/2;
	region->cy = bottom + height/2;
	top = bottom + height; // All this was missing with bad effects
	region->top = top;
	right = left + width;
	region->right = right;
	
	region->update = false;
	
	if(update)
	{
		layoutchildren(region);
		// callScript("OnSizeChanged", width, height)
	}
	return update;
	
}

// Compute the relative anchor coordinate from left/bottom screen coordinates. Allows to reanchor regions to other regions or input positions

void reanchor(urAPI_Region_t* region)
{
	if (region == nil)
		return;
    
	/*	if(region->textlabel!=NULL)
	 region->textlabel->updatestring = true; */
    
	float left, right, top, bottom, width, height, cx, cy, x, y;
    
	left = right = top = bottom = width = height = cx = cy = x = y = -1000000;
    
	const char* point = region->point;
	if (point == nil)
		point = DEFAULT_RPOINT;
    
	urAPI_Region_t* relativeRegion = region->relativeRegion;
	if (relativeRegion == nil)
		relativeRegion = region->parent;
	if (relativeRegion == nil)
		relativeRegion = UIParent; // This should be another layer but we don't care for now
    
	const char* relativePoint = region->relativePoint;
	if (relativePoint == nil)
		relativePoint = DEFAULT_RPOINT;
    
	if (!strcmp(relativePoint, "ALL")) {
		left = relativeRegion->left;
		bottom = relativeRegion->bottom;
		width = relativeRegion->width;
		height = relativeRegion->height;
	} else if (!strcmp(relativePoint, "TOPLEFT")) {
		x = relativeRegion->left;
		y = relativeRegion->top;
	} else if (!strcmp(relativePoint, "TOPRIGHT")) {
		x = relativeRegion->right;
		y = relativeRegion->top;
	} else if (!strcmp(relativePoint, "TOP")) {
		x = relativeRegion->cx;
		y = relativeRegion->top;
	} else if (!strcmp(relativePoint, "LEFT")) {
		x = relativeRegion->left;
		y = relativeRegion->cy;
	} else if (!strcmp(relativePoint, "RIGHT")) {
		x = relativeRegion->right;
		y = relativeRegion->cy;
	} else if (!strcmp(relativePoint, "CENTER")) {
		x = relativeRegion->cx;
		y = relativeRegion->cy;
	} else if (!strcmp(relativePoint, "BOTTOMLEFT")) {
		x = relativeRegion->left;
		y = relativeRegion->bottom;
	} else if (!strcmp(relativePoint, "BOTTOMRIGHT")) {
		x = relativeRegion->right;
		y = relativeRegion->bottom;
	} else if (!strcmp(relativePoint, "BOTTOM")) {
		x = relativeRegion->cx;
		y = relativeRegion->bottom;
	} else {
		// Error!!
		luaL_error(lua, "Unknown relativePoint when layouting regions.");
		return;
	}
    
	float ofsx = 0.0;
	float ofsy = 0.0;
    //	x = x + region->ofsx;
    //	y = y + region->ofsy;
    
	if (!strcmp(point, "TOPLEFT")) {
		ofsx = region->left - x;
		ofsy = region->top - y;
	} else if (!strcmp(point, "TOPRIGHT")) {
		ofsx = region->right - x;
		ofsy = region->top - y;
	} else if (!strcmp(point, "TOP")) {
		ofsx = region->cx - x;
		ofsy = region->top - y;
	} else if (!strcmp(point, "LEFT")) {
		ofsx = region->left - x;
		ofsy = region->cy - y; // Another typo here
	} else if (!strcmp(point, "RIGHT")) {
		ofsx = region->right - x;
		ofsy = region->cy - y;
	} else if (!strcmp(point, "CENTER")) {
		ofsx = region->cx - x;
		ofsy = region->cy - y;
	} else if (!strcmp(point, "BOTTOMLEFT")) {
		ofsx = region->left - x;
		ofsy = region->bottom - y;
	} else if (!strcmp(point, "BOTTOMRIGHT")) {
		ofsx = region->right - x;
		ofsy = region->bottom - y;
	} else if (!strcmp(point, "BOTTOM")) {
		ofsx = region->cx - x;
		ofsy = region->bottom - y;
	} else {
		// Error!!
		luaL_error(lua, "Unknown relativePoint when layouting regions.");
		return;
	}
    
	region->ofsx = ofsx;
	region->ofsy = ofsy;
}



//------------------------------------------------------------------------------
// Our custom lua API
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Service function to check userdata type integrity
//------------------------------------------------------------------------------

static urAPI_Region_t *checkregion(lua_State *lua, int nr)
{
//	void *region = luaL_checkudata(lua, nr, "URAPI.region");
	luaL_checktype(lua, nr, LUA_TTABLE);
	lua_rawgeti(lua, nr, 0);
	void *region = lua_touserdata(lua, -1);
	lua_pop(lua,1);
	luaL_argcheck(lua, region!= NULL && ((urAPI_Region_t*)region)->ud_id == UR_REGION, nr, "'region' expected");
	return (urAPI_Region_t*)region;
}

static urAPI_Texture_t *checktexture(lua_State *lua, int nr)
{
	void *texture = luaL_checkudata(lua, nr, "URAPI.texture");
	luaL_argcheck(lua, texture!= NULL, nr, "'texture' expected");
	return (urAPI_Texture_t*)texture;
}

static urAPI_TextLabel_t *checktextlabel(lua_State *lua, int nr)
{
	void *textlabel = luaL_checkudata(lua, nr, "URAPI.textlabel");
	luaL_argcheck(lua, textlabel!= NULL, nr, "'textlabel' expected");
	return (urAPI_TextLabel_t*)textlabel;
}

static ursAPI_FlowBox_t *checkflowbox(lua_State *lua, int nr)
{
	luaL_checktype(lua, nr, LUA_TTABLE);
	lua_rawgeti(lua, nr, 0);
	void *flowbox = lua_touserdata(lua, -1);
	lua_pop(lua,1);
	luaL_argcheck(lua, flowbox!= NULL && ((ursAPI_FlowBox_t*)flowbox)->ud_id == UR_FLOWBOX, nr, "'flowbox' expected");
	return (ursAPI_FlowBox_t*)flowbox;
}

static ursAPI_FlowBox_Port_t *checkflowboxport(lua_State *lua, int nr)
{
	luaL_checktype(lua, nr, LUA_TTABLE);
	lua_rawgeti(lua, nr, 0);
	void *flowbox = lua_touserdata(lua, -1);
	lua_pop(lua,1);
	luaL_argcheck(lua, flowbox!= NULL && ((ursAPI_FlowBox_Port_t*)flowbox)->ud_id == UR_FLOWBOXPORT, nr, "'flowboxport' expected");
	return (ursAPI_FlowBox_Port_t*)flowbox;
}

//------------------------------------------------------------------------------
// Region Member urMus lua API function implementations
//------------------------------------------------------------------------------

int region_Handle(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	//get parameter
	const char* handler = luaL_checkstring(lua, 2);
	
	if(lua_isnil(lua,3))
	{
        bool found = false;
        for(int i=0; i<MAX_EVENTS; i++)
        {
            if(!strcmp(handler, urEventNames[i]))
            {
                RemoveEventRegistry(lua, (eventIDs)i, region);
                found = true;
            }
        }
            
		if(!found)
        {
			luaL_error(lua, "Trying to set a script for an unknown event: %s",handler);
            newerror = true;
            ur_Log(errorstr.c_str());
			return 0; // Error, unknown event
        }
		return 1;
	}
	else
	{
        luaL_argcheck(lua, lua_isfunction(lua,3), 3, "'function' expected");
		if(lua_isfunction(lua,3))
		{
			// Store funtion reference
			lua_pushvalue(lua, 3);
			int func_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
			
            bool found = false;
            for(int i=0; i<MAX_EVENTS; i++)
            {
                if(!strcmp(handler, urEventNames[i]))
                {
                       AddRegionToChain(EventChain[i], region);
                       region->OnEvents[i] = func_ref;
                       found = true;
                }
            }
                   
            if(!found)
            {
                luaL_unref(lua, LUA_REGISTRYINDEX, func_ref);
                luaL_error(lua, "Trying to set a script for an unknown event: %s",handler);
                newerror = true;
                ur_Log(errorstr.c_str());
                return 0; // Error, unknown event
            }

            // OK! 
			return 1;
		}
		return 0;
	}
}

int region_ReadHandle(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	//get parameter
	const char* handler = luaL_checkstring(lua, 2);
    
    for(int i=0; i<MAX_EVENTS; i++)
    {
        if(!strcmp(handler, urEventNames[i]))
        {
            lua_rawgeti(lua,LUA_REGISTRYINDEX, region->OnEvents[i]);
            return 1;
        }
    }
    return 0;
}

void ClampRegion(urAPI_Region_t* region);

void changeLayout(urAPI_Region_t* region)
{
	region->update = true;

	if(region->isClamped)
		ClampRegion(region);
    
	if(!layout(region)) // Change may not have had a layouting effect on parent, but still could affect children that are anchored to Y
		layoutchildren(region);
}

int region_SetHeight(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_Number height = luaL_checknumber(lua,2);
	region->height=height;
    changeLayout(region);
	return 0;
}

int region_SetWidth(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_Number width = luaL_checknumber(lua,2);
	region->width=width;
    changeLayout(region);
	return 0;
}

int region_EnableInput(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool enableinput = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	region->isTouchEnabled = enableinput;
	return 0;
}

int region_EnableHorizontalScroll(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool enablescrollx = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	region->isScrollXEnabled = enablescrollx;
	return 0;
}

int region_EnableVerticalScroll(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool enablescrolly = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	region->isScrollYEnabled = enablescrolly;
	return 0;
}

int region_EnableClipping(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	bool enableclipping = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	region->isClipping = enableclipping;
	return 0;
}

int region_SetClipRegion(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
	t->clipleft = luaL_checknumber(lua, 2);
	t->clipbottom = luaL_checknumber(lua, 3);
	t->clipwidth = luaL_checknumber(lua, 4);
	t->clipheight = luaL_checknumber(lua, 5);
	return 0;
}

int region_ClipRegion(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
	lua_pushnumber(lua, t->clipleft);
	lua_pushnumber(lua, t->clipbottom);
	lua_pushnumber(lua, t->clipwidth);
	lua_pushnumber(lua, t->clipheight);
	return 4;
}

int region_EnableMoving(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool setmovable = lua_toboolean(lua,2);//!lua_isnil(lua,2);
	region->isMovable = setmovable;
	return 0;
}

int region_EnableResizing(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool setresizable = lua_toboolean(lua,2);//!lua_isnil(lua,2);
	region->isResizable = setresizable;
	return 0;
}

int region_SetAnchor(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	if(region==UIParent) return 0;
	lua_Number ofsx=0;
	lua_Number ofsy=0;
	urAPI_Region_t* relativeRegion = UIParent; 
	
	const char* point = luaL_checkstring(lua, 2);
	const char* relativePoint = DEFAULT_RPOINT;
	if(lua_isnil(lua,3)) // SetAnchor(point);
	{
		
	}
	else
	{
		if(lua_isnumber(lua, 3) && lua_isnumber(lua, 4)) // SetAnchor(point, x,y);
		{
			ofsx = luaL_checknumber(lua, 3);
			ofsy = luaL_checknumber(lua, 4);
		}
		else
		{
			if(lua_isstring(lua, 3)) // SetAnchor(point, "relativeRegion")
			{
				// find parent here
			}
			else // SetAnchor(point, relativeRegion)
				relativeRegion = checkregion(lua, 3);
			
			if(lua_isstring(lua, 4))
				relativePoint = luaL_checkstring(lua, 4);
			
			if(lua_isnumber(lua, 5) && lua_isnumber(lua, 6)) // SetAnchor(point, x,y);
			{
				ofsx = luaL_checknumber(lua, 5);
				ofsy = luaL_checknumber(lua, 6);
			}
		}
			
	}
	
	if(relativeRegion == region)
	{
		luaL_error(lua, "Cannot anchor a region to itself.");
		return 0;
	}	

	if(region->point != NULL)
		free(region->point);
	region->point = (char*)malloc(strlen(point)+1);
	strcpy(region->point, point);
	region->relativeRegion = relativeRegion;

	
	if(relativeRegion != region->parent)
	{
		removeChild(region->parent, region);
 		region->parent = relativeRegion;
		addChild(relativeRegion, region);
	}

	if(region->relativePoint != NULL)
		free(region->relativePoint);
	region->relativePoint = (char*)malloc(strlen(relativePoint)+1);
	strcpy(region->relativePoint, relativePoint);

	region->ofsx = ofsx;
	region->ofsy = ofsy;
	region->update = true;
	if(region->isClamped)
		ClampRegion(region);
	layout(region);
	return true;
}

int region_Show(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	region->isShown = true;
	if(visibleparent(region)) // Check visibility change for children
	{
		region->isVisible = true;
		if(region->OnEvents[OnShow] != 0)
			callScript(OnShow,region->OnEvents[OnShow], region);
		showchildren(region);
	}
	return 0;
}

int region_Hide(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	region->isVisible = false;
	region->isShown = false;
	if(region->OnEvents[OnHide] != 0)
		callScript(OnHide,region->OnEvents[OnHide], region);
	hidechildren(region); // parent got hidden so hide children too.
	return 0;
}

const char STRATASTRING_PARENT[] = "PARENT";
const char STRATASTRING_BACKGROUND[] = "BACKGROUND";
const char STRATASTRING_LOW[] = "LOW";
const char STRATASTRING_MEDIUM[] = "MEDIUM";
const char STRATASTRING_HIGH[] = "HIGH";
const char STRATASTRING_DIALOG[] = "DIALOG";
const char STRATASTRING_FULLSCREEN[] = "FULLSCREEN";
const char STRATASTRING_FULLSCREEN_DIALOG[] = "FULLSCREEN_DIALOG";
const char STRATASTRING_TOOLTIP[] = "TOOLTIP";

const char* region_strataindex2str(int strataidx)
{
	switch(strataidx)
	{
		case STRATA_PARENT:
			return STRATASTRING_PARENT;
		case STRATA_BACKGROUND:
			return STRATASTRING_BACKGROUND;
		case STRATA_LOW:
			return STRATASTRING_LOW;
		case STRATA_MEDIUM:
			return STRATASTRING_MEDIUM;
		case STRATA_HIGH:
			return STRATASTRING_HIGH;
		case STRATA_FULLSCREEN:
			return STRATASTRING_FULLSCREEN;
		case STRATA_FULLSCREEN_DIALOG:
			return STRATASTRING_FULLSCREEN_DIALOG;
		case STRATA_TOOLTIP:
			return STRATASTRING_TOOLTIP;
		default:
			return nil;
	}
}

int region_strata2index(const char* strata)
{

	if(!strcmp(strata, "PARENT"))
		return STRATA_PARENT;
	else if(!strcmp(strata, "BACKGROUND"))
		return STRATA_BACKGROUND;
	else if(!strcmp(strata, "LOW"))
		return STRATA_LOW;
	else if(!strcmp(strata, "MEDIUM"))
		return STRATA_MEDIUM;
	else if(!strcmp(strata, "HIGH"))
		return STRATA_HIGH;
	else if(!strcmp(strata, "DIALOG"))
		return STRATA_DIALOG;
	else if(!strcmp(strata, "FULLSCREEN"))
		return STRATA_FULLSCREEN;
	else if(!strcmp(strata, "FULLSCREEN_DIALOG"))
		return STRATA_FULLSCREEN_DIALOG;
	else if(!strcmp(strata, "TOOLTIP"))
		return STRATA_TOOLTIP;
	else
	{
		return -1; // unknown strata
	}
	
}

const char LAYERSTRING_BACKGROUND[] = "BACKGROUND";
const char LAYERSTRING_BORDER[] = "BORDER";
const char LAYERSTRING_ARTWORK[] = "ARTWORK";
const char LAYERSTRING_OVERLAY[] = "OVERLAY";
const char LAYERSTRING_HIGHLIGHT[] = "HIGHLIGHT";

const char* region_layerindex2str(int layeridx)
{
	switch(layeridx)
	{
		case LAYER_BACKGROUND:
			return LAYERSTRING_BACKGROUND;
		case LAYER_BORDER:
			return LAYERSTRING_BORDER;
		case LAYER_ARTWORK:
			return LAYERSTRING_ARTWORK;
		case LAYER_OVERLAY:
			return LAYERSTRING_OVERLAY;
		case LAYER_HIGHLIGHT:
			return LAYERSTRING_HIGHLIGHT;
		default:
			return nil;
	}
}

int region_layer2index(const char* layer)
{
	
	if(!strcmp(layer, "BACKGROUND"))
		return LAYER_BACKGROUND;
	else if(!strcmp(layer, "BORDER"))
		return LAYER_BORDER;
	else if(!strcmp(layer, "ARTWORK"))
		return LAYER_ARTWORK;
	else if(!strcmp(layer, "OVERLAY"))
		return LAYER_OVERLAY;
	else if(!strcmp(layer, "HIGHLIGHT"))
		return LAYER_HIGHLIGHT;
	else
	{
		return -1; // unknown layer
	}
	
}

const char WRAPSTRING_WORD[] = "WORD";
const char WRAPSTRING_CHAR[] = "CHAR";
const char WRAPSTRING_CLIP[] = "CLIP";

const char* textlabel_wrapindex2str(int wrapidx)
{
	switch(wrapidx)
	{
		case WRAP_WORD:
			return WRAPSTRING_WORD;
		case WRAP_CHAR:
			return WRAPSTRING_CHAR;
		case WRAP_CLIP:
			return WRAPSTRING_CLIP;
		default:
			return nil;
	}
}

int textlabel_wrap2index(const char* wrap)
{
	if(!strcmp(wrap, "WORD"))
		return WRAP_WORD;
	else if(!strcmp(wrap, "CHAR"))
		return WRAP_CHAR;
	else if(!strcmp(wrap, "CLIP"))
		return WRAP_CLIP;
	else
	{
		return -1; // unknown wrap
	}
}

void l_SortStrata(urAPI_Region_t* region, int strata)
{
	if(region->prev == nil && firstRegion[currentPage] == region) // first region!
	{
		firstRegion[currentPage] = region->next; // unlink!
		firstRegion[currentPage]->prev = nil;
	}
	else if(region->next == nil && lastRegion[currentPage] == region) // last region!
	{
		lastRegion[currentPage] = region->prev; // unlink!
		lastRegion[currentPage]->next = nil;
	}
	else if(region->prev != NULL && region->next !=NULL)
	{
		region->prev->next = region->next; // unlink!
		region->next->prev = region->prev;
	}
	for(urAPI_Region_t* t=firstRegion[currentPage]; t!=NULL; t=t->next)
	{
		if(t->strata!=STRATA_PARENT) // ignoring PARENT strata regions.
		{
			if(t->strata > strata) // insert here!
			{
				if(t == firstRegion[currentPage])
					firstRegion[currentPage] = region;
				region->prev = t->prev;
				if(t->prev != NULL) // Again, may be the first.
					t->prev->next = region;
				region->next = t; // Link in

				t->prev = region; // fix links
				region->strata = strata;
//				region->prev->next = region;
				return; // Done.
			}
		}
	}
	
	if(region!=lastRegion[currentPage])
	{
		region->prev = lastRegion[currentPage];
		region->next = nil;
		lastRegion[currentPage]->next = region;
		lastRegion[currentPage] = region;
	}
	else
	{
		lastRegion[currentPage] = nil;
	}
}

void l_setstrataindex(urAPI_Region_t* region , int strataindex)
{
	if(strataindex == STRATA_PARENT)
	{
		region->strata = strataindex;
		urAPI_Region_t* p = region->parent;
		int newstrataindex = 1;
		do
		{
			if (p->strata != STRATA_PARENT) newstrataindex = p->strata;
			p = p->parent;
		}
		while(p!=NULL && p->strata == 0);
		
		l_SortStrata(region, newstrataindex);
	}
	if (strataindex > 0 && strataindex != region->strata)
	{
		region->strata = strataindex;
		l_SortStrata(region, strataindex);
	}
}

int region_SetLayer(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	const char* strata = luaL_checkstring(lua,2);
	if(strata)
	{
		int strataindex = region_strata2index(strata);
		if( region == firstRegion[currentPage] && region == lastRegion[currentPage])
		{
			// This is a sole region, no need to stratify
		}
		else
			l_setstrataindex(region , strataindex);
		region->strata = strataindex;
	}
	return 0;
}

int region_Parent(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	if(region != nil)
	{
		region = region->parent;
		lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);

		return 1;
	}
	else
		return 0;
}

int region_Children(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	urAPI_Region_t* child = region->firstchild;
	
	int childcount = 0;
	while(child!=NULL)
	{
		childcount++;
		lua_rawgeti(lua,LUA_REGISTRYINDEX, child->tableref);
		child = child->nextchild;
	}
	return childcount;
}

int region_Alpha(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->alpha);
	return 1;
}

int region_SetAlpha(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	lua_Number alpha = luaL_checknumber(lua,2);
	if(alpha > 1.0) alpha = 1.0;
	else if(alpha < 0.0) alpha = 0.0;
	region->alpha=alpha;
	return 0;
}

int region_Name(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushstring(lua, region->name);
	return 1;
}

int region_Bottom(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->bottom);
	return 1;
}

int region_Center(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->cx);
	lua_pushnumber(lua, region->cy);
	return 2;
}

int region_Height(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->height);
	return 1;
}

int region_Left(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->left);
	return 1;
}

int region_Right(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->right);
	return 1;
}

int region_Top(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->top);
	return 1;
}

int region_Width(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, region->width);
	return 1;
}

int region_NumAnchors(lua_State* lua)
{
//	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushnumber(lua, 1); // NYI always 1 point for now
	return 1;
}

int region_Anchor(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);

	lua_pushstring(lua, region->point);
	if(region->relativeRegion)
	{
		lua_rawgeti(lua, LUA_REGISTRYINDEX, region->relativeRegion->tableref);
	}
	else
		lua_pushnil(lua);
	lua_pushstring(lua, region->relativePoint);
	lua_pushnumber(lua, region->ofsx);
	lua_pushnumber(lua, region->ofsy);
	
	return 5;
}

int region_IsShown(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushboolean(lua, region->isVisible);
	return 1;
}

int region_IsVisible(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool visible = false;
	if(region->parent!=NULL)
		visible = region->isVisible && region->parent->isVisible;
	else
		visible = region->isVisible;
	lua_pushboolean(lua, visible );
	return 1;
}

void setParent(urAPI_Region_t* region, urAPI_Region_t* parent)
{
	if(region!= NULL && parent!= NULL && region != parent)
	{
		region->relativeRegion = parent;
		
		removeChild(region->parent, region);
		if(parent == UIParent)
			region->parent = UIParent;
		else
		{
			region->parent = parent;
			addChild(parent, region);
		}
	}
}

int region_SetParent(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	urAPI_Region_t* parent = checkregion(lua, 2);

	setParent(region, parent);
	region->update = true;
	layout(region);
	return 0;
}

int region_Layer(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	lua_pushstring(lua, region_strataindex2str(region->strata));
	return 1;
}

int region_Lower(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	if(region->prev != nil)
	{
		urAPI_Region_t* temp = region->prev;
        if(firstRegion[currentPage] == temp)
            firstRegion[currentPage] = region;
        if(lastRegion[currentPage] == region)
            lastRegion[currentPage] = temp;
		region->prev = temp->prev;
		temp->next = region->next;
        if(temp->prev)
            temp->prev->next = region;
		temp->prev = region;
        if(region->next)
            region->next->prev = temp;
		region->next = temp;
	}
	return 0;
}

int region_Raise(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	if(region->next != nil)
	{
		urAPI_Region_t* temp = region->next;
        if(firstRegion[currentPage] == region)
            firstRegion[currentPage] = temp;
        if(lastRegion[currentPage] == temp)
            lastRegion[currentPage] = region;
		region->next = temp->next;
		temp->prev = region->prev;
        if(temp->next)
            temp->next->prev = region;
		temp->next = region;
        if(region->prev)
            region->prev->next = temp;
		region->prev = temp;
	}
	return 0;
}

void removeRegion(urAPI_Region_t* region)
{

    for(int i=0; i<MAX_EVENTS; i++)
    {
        RemoveEventRegistry(lua, (eventIDs)i, region);
    }

	int currentPage = region->page;
	
	if(firstRegion[currentPage] == region)
		firstRegion[currentPage] = region->next;

	if(region->prev != NULL)
		region->prev->next = region->next;

	if(region->next != NULL)
		region->next->prev = region->prev;

	if(lastRegion[currentPage] == region)
		lastRegion[currentPage] = region->prev;
    
    assert(!region->parent);
    assert(!region->firstchild);
	
	numRegions[currentPage]--;
}

void freeTexture(urAPI_Texture_t* texture)
{
    if(texture->usecamera != 0)
        decCameraUse();
    
#ifdef GPUIMAGE
    if(texture->filterHandler)
    {
        //                    [t->filterInput removeAllTargets];
        //                    [t->filterInput release];
        //                    t->filterInput = NULL;
        [g_glView->videoCamera removeTarget:texture->inputFilter];
        [texture->inputFilter removeTarget:texture->filterOutput];
        [texture->inputFilter release];
        texture->inputFilter = NULL;
        [texture->filterOutput release];
        texture->filterOutput = NULL;
        [texture->filterHandler release];
        texture->filterHandler = NULL;
    }
#endif
    
	if(texture->backgroundTex!= NULL)
//		delete texture->backgroundTex;
        free(texture->backgroundTex);
//	free(texture); // GC should take care of this ... maybe
}

void freeTextLabel(urAPI_TextLabel_t* textlabel)
{
	if(textlabel->textlabelTex != NULL)
		free(textlabel->textlabelTex);
//	delete textlabel; // GC should take care of this ... maybe
}

void loseChildren(urAPI_Region_t* region)
{
    urAPI_Region_t *findlast = region->firstchild;
    urAPI_Region_t *findlast2;

    while(findlast)
    {
        findlast->parent = NULL;
        findlast2 = findlast->nextchild;
        findlast->nextchild = NULL;
        findlast = findlast2;
    }
    region->firstchild = NULL;
}

void freeRegion(urAPI_Region_t* region)
{
	removeChild(region->parent, region);
    loseChildren(region);
	removeRegion(region);
	if(region->texture != NULL)
		freeTexture(region->texture);
	if(region->textlabel != NULL)
		freeTextLabel(region->textlabel);
	delete region;
}

int region_Free(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);

	freeRegion(region);
    return 0;
}

int region_IsToplevel(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);

	bool istop = false;
	
	if(region == lastRegion[currentPage])
	{
		istop = true;
	}

	lua_pushboolean(lua, istop);
	return 1;
}

int region_MoveToTop(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	if(region != lastRegion[currentPage]) 
	{
		if(region->prev != nil) // Could be first region!
			region->prev->next = region->next; // unlink!
		else 
			firstRegion[currentPage] = region->next;

//        if(region->next != nil)
            region->next->prev = region->prev;
		// and make last
		lastRegion[currentPage]->next = region;
		region->prev = lastRegion[currentPage];
		region->next = nil;
		lastRegion[currentPage] = region;
	}
	return 0;
}

void instantiateTexture(urAPI_Region_t* t);
void instantiateAllTextures(urAPI_Region_t* t);

char TEXTURE_SOLID[] = "Solid Texture";

#define GRADIENT_ORIENTATION_VERTICAL 0
#define GRADIENT_ORIENTATION_HORIZONTAL 1
#define GRADIENT_ORIENTATION_DOWNWARD 2
#define GRADIENT_ORIENTATION_UPWARD 3

void textureColorCopyToGradient(urAPI_Texture_t* mytexture)
{
	mytexture->gradientUL[0] = mytexture->texturesolidcolor[0]; // R
	mytexture->gradientUL[1] = mytexture->texturesolidcolor[1]; // G
	mytexture->gradientUL[2] = mytexture->texturesolidcolor[2]; // B
	mytexture->gradientUL[3] = mytexture->texturesolidcolor[3]; // A
	mytexture->gradientUR[0] = mytexture->texturesolidcolor[0]; // R
	mytexture->gradientUR[1] = mytexture->texturesolidcolor[1]; // G
	mytexture->gradientUR[2] = mytexture->texturesolidcolor[2]; // B
	mytexture->gradientUR[3] = mytexture->texturesolidcolor[3]; // A
	mytexture->gradientBL[0] = mytexture->texturesolidcolor[0]; // R
	mytexture->gradientBL[1] = mytexture->texturesolidcolor[1]; // G
	mytexture->gradientBL[2] = mytexture->texturesolidcolor[2]; // B
	mytexture->gradientBL[3] = mytexture->texturesolidcolor[3]; // A
	mytexture->gradientBR[0] = mytexture->texturesolidcolor[0]; // R
	mytexture->gradientBR[1] = mytexture->texturesolidcolor[1]; // G
	mytexture->gradientBR[2] = mytexture->texturesolidcolor[2]; // B
	mytexture->gradientBR[3] = mytexture->texturesolidcolor[3]; // A
}

int region_Texture(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	const char* texturename = NULL;
	const char* texturelayer;
	int texturelayerindex=1;
	
	float r,g,b,a;
	r = 255;
	g = 255;
	b = 255;
	a = 255;
	
	if(lua_gettop(lua)<2 || lua_isnil(lua,2)) // this can legitimately be nil.
		texturename = nil;
	else
	{
		if(lua_isstring(lua,2) && lua_gettop(lua) <4)
		{
			texturename = luaL_checkstring(lua,2);
			if(lua_gettop(lua)==3 && !lua_isnil(lua,3)) // this should be set.
			{
				texturelayer = luaL_checkstring(lua,3);
				texturelayerindex = region_layer2index(texturelayer);
			}
			// NYI arg3.. are inheritsFrom regions
		}
		else if(lua_isnumber(lua,2)) {
			texturename = nil;
			r = luaL_checknumber(lua, 2);
			if(lua_gettop(lua)>2)
			{
				g = luaL_checknumber(lua,3);
				if(lua_gettop(lua)>3)
				{
					b = luaL_checknumber(lua,4);
					a = 255;
					if(lua_gettop(lua)>4)
						a = luaL_checknumber(lua,5);
				}
				else
				{
					g = r;
					b = r;
					a = g;
				}
			}
			else {
				g = r;
				b = r;
				a = 255;
			}

		}

	}
	urAPI_Texture_t* mytexture = (urAPI_Texture_t*)lua_newuserdata(lua, sizeof(urAPI_Texture_t));
    
	mytexture->blendmode = BLEND_DISABLED;
	mytexture->texcoords[0] = 0.0;
	mytexture->texcoords[1] = 1.0;
	mytexture->texcoords[2] = 1.0;
	mytexture->texcoords[3] = 1.0;
	mytexture->texcoords[4] = 0.0;
	mytexture->texcoords[5] = 0.0;
	mytexture->texcoords[6] = 1.0;
	mytexture->texcoords[7] = 0.0;
	if(texturename == NULL)
		mytexture->texturepath = TEXTURE_SOLID;
	else
	{
		mytexture->texturepath = (char*)malloc(strlen(texturename)+1);
		strcpy(mytexture->texturepath, texturename);
//		mytexture->texturepath = texturename;
	}
	mytexture->modifyRect = false;
	mytexture->isDesaturated = false;
	mytexture->isTiled = false;
	mytexture->fill = false;
//	mytexture->gradientOrientation = GRADIENT_ORIENTATION_VERTICAL; OBSOLETE
	mytexture->texturesolidcolor[0] = r; // R for solid
	mytexture->texturesolidcolor[1] = g; // G
	mytexture->texturesolidcolor[2] = b; // B
	mytexture->texturesolidcolor[3] = a; // A
	textureColorCopyToGradient(mytexture);
	
	mytexture->backgroundTex = NULL;
#ifdef GPUIMAGE
    mytexture->movieTex = NULL;
    mytexture->regionMovie = NULL;
    mytexture->textureOutput = NULL;
    mytexture->textureInput = NULL;
    mytexture->filterInput = NULL;
    mytexture->filterOutput = NULL;
    mytexture->inputFilter = NULL;
    mytexture->filterValue = 0;
    mytexture->filterHandler = NULL;
    mytexture->_filterTexture = 0;
#endif
   
	mytexture->usecamera = 0;
	
	region->texture = mytexture; // HACK
	mytexture->region = region;
	
	luaL_getmetatable(lua, "URAPI.texture");
	lua_setmetatable(lua, -2);

//	instantiateAllTextures(mytexture->region);
	
	if(mytexture->backgroundTex == nil && mytexture->texturepath != TEXTURE_SOLID)
	{
		instantiateTexture(mytexture->region);
	}
	return 1;
}

char textlabel_empty[] = "";
#ifdef UISTRINGS
const char textlabel_defaultfont[] = "Helvetica";
#else
const char textlabel_defaultfont[] = "DroidSansMono.ttf";
#endif
int region_TextLabel(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	const char* texturename;
	const char* texturelayer;
	int texturelayerindex=1;
	
	if(lua_gettop(lua)<2 || lua_isnil(lua,2)) // this can legitimately be nil.
		texturename = nil;
	else
		if(!lua_isnil(lua,3)) // this should be set.
		{
			texturelayer = luaL_checkstring(lua,3);
			texturelayerindex = region_layer2index(texturelayer);
		}
	// NYI arg3.. are inheritsFrom regions
	
	urAPI_TextLabel_t* mytextlabel = (urAPI_TextLabel_t*)lua_newuserdata(lua, sizeof(urAPI_TextLabel_t));
	
	region->textlabel = mytextlabel; // HACK
	
	mytextlabel->text = textlabel_empty;
	mytextlabel->updatestring = true;
	mytextlabel->font = textlabel_defaultfont;
	mytextlabel->justifyh = JUSTIFYH_CENTER;
	mytextlabel->justifyv = JUSTIFYV_MIDDLE;
	mytextlabel->shadowcolor[0] = 0.0;
	mytextlabel->shadowcolor[1] = 0.0;
	mytextlabel->shadowcolor[2] = 0.0;
	mytextlabel->shadowcolor[3] = 128.0;
	mytextlabel->shadowoffset[0] = 0.0;
	mytextlabel->shadowoffset[1] = 0.0;
	mytextlabel->shadowblur = 0.0;
	mytextlabel->drawshadow = false;
	mytextlabel->linespacing = 2;
	mytextlabel->textcolor[0] = 255.0;
	mytextlabel->textcolor[1] = 255.0;
	mytextlabel->textcolor[2] = 255.0;
	mytextlabel->textcolor[3] = 255.0;
	mytextlabel->textheight = 12;
	mytextlabel->wrap = WRAP_WORD;
	mytextlabel->rotation = 0.0;
    mytextlabel->outlinemode = 0; // Default is no outline
    mytextlabel->outlinethickness = 0; // Default outline thickness is 1
	
	mytextlabel->textlabelTex = nil;
    
    mytextlabel->region = region;
	
	luaL_getmetatable(lua, "URAPI.textlabel");
	lua_setmetatable(lua, -2);

	return 1;
}



//------------------------------------------------------------------------------
// Texture Member urMus lua API function implementations
//------------------------------------------------------------------------------

int texture_SetTexture(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	if(lua_isnumber(lua,2) && lua_isnumber(lua,3) && lua_isnumber(lua,4))
	{
		t->texturepath = TEXTURE_SOLID;
		t->texturesolidcolor[0] = luaL_checknumber(lua, 2); 
		t->texturesolidcolor[1] = luaL_checknumber(lua, 3); 
		t->texturesolidcolor[2] = luaL_checknumber(lua, 4); 
		if(lua_isnumber(lua, 5))
			t->texturesolidcolor[3] = luaL_checknumber(lua, 5);
		else
			t->texturesolidcolor[3] = 255;
		textureColorCopyToGradient(t);
		if(t->backgroundTex != NULL) [t->backgroundTex release]; // Antileak
		t->backgroundTex = nil;
	}
	else
	{
		const char* texturename = luaL_checkstring(lua,2);
		if(t->texturepath != TEXTURE_SOLID && t->texturepath != NULL)
			free(t->texturepath);
		t->texturepath = (char*)malloc(strlen(texturename)+1);
		strcpy(t->texturepath, texturename);
		if(t->backgroundTex != NULL) [t->backgroundTex release]; // Antileak
		t->backgroundTex = nil;
		instantiateTexture(t->region);
	}
    
    if(t->usecamera>0)
    {
//        RemoveRegionFromChain(CameraChain,t->region);
        page_camerause--;
        decCameraUse();
    }
//        [g_glView DecCameraUse];
    t->usecamera = 0;
	
	return 0;
}

int texture_SaveMovie(lua_State *lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	const char* filename = luaL_checkstring(lua, 2);
#ifdef GPUIMAGE
    NSString* filename2 = [NSString stringWithUTF8String:filename];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if ([paths count] > 0) {
		NSString *filePath = [paths objectAtIndex:0];
		NSString *resultPath = [NSString stringWithFormat:@"%@/%@", filePath, filename2];
        
        GLuint textureID;
        if(t->usecamera)
            textureID = t->textureOutput.texture;
        else
            textureID = [t->backgroundTex getTextureID];
                
//        [g_glView writeMovie:resultPath ofSize:CGSizeMake(t->region->width, t->region->height) fromTexture:textureID];
        [g_glView writeMovie:resultPath ofSize:CGSizeMake(t->region->width, t->region->height) fromTexture:textureID];
    }
#endif
    return 0;
}

int texture_FinishMovie(lua_State *lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
#ifdef GPUIMAGE
    [g_glView finishMovie];
#endif
    return 0;
}

int texture_SetGradientColor(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	const char* orientation = luaL_checkstring(lua, 2);
	float minR = luaL_checknumber(lua, 3);
	float minG = luaL_checknumber(lua, 4);
	float minB = luaL_checknumber(lua, 5);
	float minA = luaL_checknumber(lua, 6);
	float maxR = luaL_checknumber(lua, 7);
	float maxG = luaL_checknumber(lua, 8);
	float maxB = luaL_checknumber(lua, 9);
	float maxA = luaL_checknumber(lua, 10);
	
	if(!strcmp(orientation, "HORIZONTAL"))
	{
		t->gradientUL[0] = minR;
		t->gradientUL[1] = minG;
		t->gradientUL[2] = minB;
		t->gradientUL[3] = minA;
		t->gradientBL[0] = minR;
		t->gradientBL[1] = minG;
		t->gradientBL[2] = minB;
		t->gradientBL[3] = minA;
		t->gradientUR[0] = maxR;
		t->gradientUR[1] = maxG;
		t->gradientUR[2] = maxB;
		t->gradientUR[3] = maxA;
		t->gradientBR[0] = maxR;
		t->gradientBR[1] = maxG;
		t->gradientBR[2] = maxB;
		t->gradientBR[3] = maxA;
	}
	else if(!strcmp(orientation, "VERTICAL"))
	{
		t->gradientUL[0] = minR;
		t->gradientUL[1] = minG;
		t->gradientUL[2] = minB;
		t->gradientUL[3] = minA;
		t->gradientUR[0] = minR;
		t->gradientUR[1] = minG;
		t->gradientUR[2] = minB;
		t->gradientUR[3] = minA;
		t->gradientBL[0] = maxR;
		t->gradientBL[1] = maxG;
		t->gradientBL[2] = maxB;
		t->gradientBL[3] = maxA;
		t->gradientBR[0] = maxR;
		t->gradientBR[1] = maxG;
		t->gradientBR[2] = maxB;
		t->gradientBR[3] = maxA;
		
	} 
	else if(!strcmp(orientation, "TOP")) // UR! Allows to set the full gradient in 2 calls.
	{
		t->gradientUL[0] = minR;
		t->gradientUL[1] = minG;
		t->gradientUL[2] = minB;
		t->gradientUL[3] = minA;
		t->gradientUR[0] = maxR;
		t->gradientUR[1] = maxG;
		t->gradientUR[2] = maxB;
		t->gradientUR[3] = maxA;
		
	} 
	else if(!strcmp(orientation, "BOTTOM")) // UR!
	{
		t->gradientBL[0] = minR;
		t->gradientBL[1] = minG;
		t->gradientBL[2] = minB;
		t->gradientBL[3] = minA;
		t->gradientBR[0] = maxR;
		t->gradientBR[1] = maxG;
		t->gradientBR[2] = maxB;
		t->gradientBR[3] = maxA;
		
	}	
	
	return 0;	
}

int texture_Texture(lua_State* lua)
{
//	urAPI_Texture_t* t = checktexture(lua, 1);
	// NYI still don't know how to return user values
	return 0;
}

int texture_SetSolidColor(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float vertR = luaL_checknumber(lua, 2);
	float vertG = luaL_checknumber(lua, 3);
	float vertB = luaL_checknumber(lua, 4);
	float vertA = 255;
	if(lua_gettop(lua)==5)
		vertA = luaL_checknumber(lua, 5);
	t->texturesolidcolor[0] = vertR;
	t->texturesolidcolor[1] = vertG;
	t->texturesolidcolor[2] = vertB;
	t->texturesolidcolor[3] = vertA;
	textureColorCopyToGradient(t);

	return 0;
}

int texture_SolidColor(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	lua_pushnumber(lua, t->texturesolidcolor[0]);
	lua_pushnumber(lua, t->texturesolidcolor[1]);
	lua_pushnumber(lua, t->texturesolidcolor[2]);
	lua_pushnumber(lua, t->texturesolidcolor[3]);
	return 4;
}

int texture_SetTexCoord(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	if(lua_gettop(lua)==5)
	{
		float left = luaL_checknumber(lua, 2);
		float right = luaL_checknumber(lua, 3);
		float top = luaL_checknumber(lua, 4);
		float bottom = luaL_checknumber(lua, 5);
		t->texcoords[0] = left; //ULx
		t->texcoords[1] = top;  // ULy
		t->texcoords[2] = right; // URx
		t->texcoords[3] = top;   // URy
		t->texcoords[4] = left; // BLx 
		t->texcoords[5] = bottom; // BLy
		t->texcoords[6] = right; // BRx
		t->texcoords[7] = bottom; // BRy
		
	}
	else if(lua_gettop(lua)==9)
	{
		t->texcoords[0] = luaL_checknumber(lua, 2);
		t->texcoords[1] = luaL_checknumber(lua, 3);
		t->texcoords[2] = luaL_checknumber(lua, 4);
		t->texcoords[3] = luaL_checknumber(lua, 5);
		t->texcoords[4] = luaL_checknumber(lua, 6);
		t->texcoords[5] = luaL_checknumber(lua, 7);
		t->texcoords[6] = luaL_checknumber(lua, 8);
		t->texcoords[7] = luaL_checknumber(lua, 9);
	}
	return 0;
}

int texture_TexCoord(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	lua_pushnumber(lua, t->texcoords[0]);
	lua_pushnumber(lua, t->texcoords[1]);
	lua_pushnumber(lua, t->texcoords[2]);
	lua_pushnumber(lua, t->texcoords[3]);
	lua_pushnumber(lua, t->texcoords[4]);
	lua_pushnumber(lua, t->texcoords[5]);
	lua_pushnumber(lua, t->texcoords[6]);
	lua_pushnumber(lua, t->texcoords[7]);
	return 8;
}

int texture_SetRotation(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float angle = luaL_checknumber(lua, 2);
	angle = angle + 3.1415926/4.0;
	float s = sqrt(2.0)/2.0*sin(angle);
	float c = sqrt(2.0)/2.0*cos(angle);

//	x = r*math.sin(angle+math.pi/4)
//  y = r*math.cos(angle+math.pi/4)
//    hand.t:SetTexCoord(.5-x,.5+y, .5+y,.5+x, .5-y,.5-x, .5+x,.5-y)
//	r = math.sqrt(2)/2
	
	t->texcoords[0] = 0.5-s;
	t->texcoords[1] = 0.5+c;
	t->texcoords[2] = 0.5+c;
	t->texcoords[3] = 0.5+s;
	t->texcoords[4] = 0.5-c;
	t->texcoords[5] = 0.5-s;
	t->texcoords[6] = 0.5+s;
	t->texcoords[7] = 0.5-c;
	return 0;
}

int texture_SetTiling(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	t->isTiled = lua_toboolean(lua,2);
	return 0;
}

int texture_Tiling(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	lua_pushboolean(lua, t->isTiled);
	return 1;
}
	
int region_EnableClamping(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	bool clamped = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	region->isClamped = clamped;
	return 0;
}

int region_SetClampRegion(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
	t->clampleft = luaL_checknumber(lua, 2);
	t->clampbottom = luaL_checknumber(lua, 3);
	t->clampwidth = luaL_checknumber(lua, 4);
	t->clampheight = luaL_checknumber(lua, 5);
	return 0;
}

int region_ClampRegion(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
	lua_pushnumber(lua, t->clampleft);
	lua_pushnumber(lua, t->clampbottom);
	lua_pushnumber(lua, t->clampwidth);
	lua_pushnumber(lua, t->clampheight);
	return 4;
}

int region_RegionOverlap(lua_State* lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	urAPI_Region_t* region2 = checkregion(lua,2);
	if( region->left < region2->right &&
		region2->left < region->right &&
		region->bottom < region2->top &&
		region2->bottom < region->top)
	{
		lua_pushboolean(lua, true);
		return 1;
	}
	return 0;
}

int texture_SetTexCoordModifiesRect(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	bool modifyrect = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	t->modifyRect = modifyrect;
	return 0;
}

int texture_TexCoordModifiesRect(lua_State* lua)
{
	urAPI_Texture_t* t= checktexture(lua, 1);
	lua_pushboolean(lua, t->modifyRect);
	return 1;
}

int texture_SetDesaturated(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	bool isDesaturated = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	t->isDesaturated = isDesaturated;
	return 0;
}

int texture_IsDesaturated(lua_State* lua)
{
	urAPI_Texture_t* t= checktexture(lua, 1);
	lua_pushboolean(lua, t->isDesaturated);
	return 1;
}

const char BLENDSTR_DISABLED[] = "DISABLED";
const char BLENDSTR_BLEND[] = "BLEND";
const char BLENDSTR_ALPHAKEY[] = "ALPHAKEY";
const char BLENDSTR_ADD[] = "ADD";
const char BLENDSTR_MOD[] = "MOD";
const char BLENDSTR_SUB[] = "SUB";

int texture_SetBlendMode(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	const char* blendmode = luaL_checkstring(lua, 2);
		if(!strcmp(blendmode, BLENDSTR_DISABLED))
		   t->blendmode = BLEND_DISABLED;
		else if(!strcmp(blendmode, BLENDSTR_BLEND))
		   t->blendmode = BLEND_BLEND;
		else if(!strcmp(blendmode, BLENDSTR_ALPHAKEY))
			t->blendmode = BLEND_ALPHAKEY;
		else if(!strcmp(blendmode, BLENDSTR_ADD))
			t->blendmode = BLEND_ADD;
		else if(!strcmp(blendmode, BLENDSTR_MOD))
			t->blendmode = BLEND_MOD;
		else if(!strcmp(blendmode, BLENDSTR_SUB))
			t->blendmode = BLEND_SUB;
		else
		{
			luaL_error(lua, "Unknown blend mode: %s", blendmode);
		}
		   
	return 0;
}

int texture_BlendMode(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	const char* returnstr;
	switch(t->blendmode)
	{
		case BLEND_DISABLED:
			returnstr = BLENDSTR_DISABLED;
			break;
		case BLEND_BLEND:
			returnstr = BLENDSTR_BLEND;
			break;
		case BLEND_ALPHAKEY:
			returnstr = BLENDSTR_ALPHAKEY;
			break;
		case BLEND_ADD:
			returnstr = BLENDSTR_ADD;
			break;
		case BLEND_MOD:
			returnstr = BLENDSTR_MOD;
			break;
		case BLEND_SUB:
			returnstr = BLENDSTR_SUB;
			break;
		default:
			luaL_error(lua, "Bogus blend mode found! Please report.");
			return 0;
			break;
	}
	lua_pushstring(lua, returnstr);
	return 1;
}


void drawLineToTexture(urAPI_Texture_t *texture, float startx, float starty, float endx, float endy);
Texture2D* createBlankTexture(float width,float height);

int texture_Line(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float startx = luaL_checknumber(lua, 2);
	float starty = luaL_checknumber(lua, 3);
	float endx = luaL_checknumber(lua, 4);
	float endy = luaL_checknumber(lua, 5);

	if(t->backgroundTex == nil)// && t->texturepath != TEXTURE_SOLID)
		instantiateAllTextures(t->region);

	if(t->backgroundTex != nil)
		drawLineToTexture(t, startx, starty, endx, endy);
	return 0;
}

void drawEllipseToTexture(urAPI_Texture_t *texture, float x, float y, float w, float h);

int texture_Ellipse(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float x = luaL_checknumber(lua, 2);
	float y = luaL_checknumber(lua, 3);
	float w = luaL_checknumber(lua, 4);
	float h = luaL_checknumber(lua, 5);
	
	if(t->backgroundTex == nil)// && t->texturepath != TEXTURE_SOLID)
		instantiateAllTextures(t->region);

	if(t->backgroundTex != nil)
		drawEllipseToTexture(t, x, y, w, h);
	return 0;
}

void drawQuadToTexture(urAPI_Texture_t *texture, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);

int texture_Quad(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float x1 = luaL_checknumber(lua, 2);
	float y1 = luaL_checknumber(lua, 3);
	float x2 = luaL_checknumber(lua, 4);
	float y2 = luaL_checknumber(lua, 5);
	float x3 = luaL_checknumber(lua, 6);
	float y3 = luaL_checknumber(lua, 7);
	float x4 = luaL_checknumber(lua, 8);
	float y4 = luaL_checknumber(lua, 9);
	
	if(t->backgroundTex == nil)// && t->texturepath != TEXTURE_SOLID)
		instantiateAllTextures(t->region);

	if(t->backgroundTex != nil)
		drawQuadToTexture(t, x1, y1, x2, y2, x3, y3, x4, y4);
	return 0;
}

int texture_Rect(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float x = luaL_checknumber(lua, 2);
	float y = luaL_checknumber(lua, 3);
	float w = luaL_checknumber(lua, 4);
	float h = luaL_checknumber(lua, 5);
	
	if(t->backgroundTex == nil)// && t->texturepath != TEXTURE_SOLID)
		instantiateAllTextures(t->region);

	if(t->backgroundTex != nil)
		drawQuadToTexture(t, x, y, x+w, y, x+w, y+h, x, y+h);
	return 0;
}

int texture_SetFill(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	bool fill = lua_toboolean(lua,2); //!lua_isnil(lua,2);
	t->fill = fill;
	return 0;
}

void clearTexture(Texture2D* t, float r, float g, float b, float a);

int texture_Clear(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float r = 0.0;
	float g = 0.0;
	float b = 0.0;
	float a = 1.0;
	if(lua_gettop(lua)>3)
	{
		r = luaL_checknumber(lua, 2)/255.0;
		g = luaL_checknumber(lua, 3)/255.0;
		b = luaL_checknumber(lua, 4)/255.0;
	}
	if(lua_gettop(lua)==5)
	{
		a = luaL_checknumber(lua, 5)/255.0;
	}
	

	if(t->backgroundTex == nil)// && t->texturepath != TEXTURE_SOLID)
		instantiateAllTextures(t->region);

	if(t->backgroundTex != nil)
		clearTexture(t->backgroundTex,r,g,b,a);
	return 0;
}

int texture_Width(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	lua_pushnumber(lua, t->width);
	return 1;
}

int texture_Height(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	lua_pushnumber(lua, t->height);
	return 1;
}

void ClearBrushTexture();

int texture_ClearBrush(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	[t->backgroundTex release];
	t->backgroundTex = nil;
	ClearBrushTexture();
    return 0;
}

void drawPointToTexture(urAPI_Texture_t *texture, float x, float y);

int texture_Point(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float x = luaL_checknumber(lua, 2);
	float y = luaL_checknumber(lua, 3);
	
	if(t->backgroundTex != nil)
    {
		drawPointToTexture(t, x, y);
    }
	return 0;
}

void SetBrushSize(float size);

int texture_SetBrushSize(lua_State* lua)
{
//	urAPI_Texture_t* t = checktexture(lua, 1);
	float size = luaL_checknumber(lua, 2);
	SetBrushSize(size);
	return 0;
}


int texture_SetBrushColor(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	float vertR = luaL_checknumber(lua, 2);
	float vertG = luaL_checknumber(lua, 3);
	float vertB = luaL_checknumber(lua, 4);
	float vertA = 255;
	if(lua_gettop(lua)>=5)
		vertA = luaL_checknumber(lua, 5);
	t->texturebrushcolor[0] = vertR;
	t->texturebrushcolor[1] = vertG;
	t->texturebrushcolor[2] = vertB;
	t->texturebrushcolor[3] = vertA;
    if(lua_gettop(lua)>=8)
    {
        vertR = luaL_checknumber(lua, 6);
        vertG = luaL_checknumber(lua, 7);
        vertB = luaL_checknumber(lua, 8);
        vertA = 255;
        if(lua_gettop(lua)==9)
            vertA = luaL_checknumber(lua, 9);
    }
    t->texturebrushcolor[4] = vertR;
    t->texturebrushcolor[5] = vertG;
    t->texturebrushcolor[6] = vertB;
    t->texturebrushcolor[7] = vertA;
	return 0;
}

float BrushSize();

int texture_BrushSize(lua_State* lua)
{
//	urAPI_Texture_t* t = checktexture(lua, 1);
	float size = BrushSize();
	lua_pushnumber(lua, size);
	return 1;
}

bool UsesTextureBrush();
//void SetBrushTexture(Texture2D* t);
void SetBrushTexture(urAPI_Texture* t);
void SetBrushAsCamera(bool asdf);

int texture_UseCamera(lua_State* lua)
{
	urAPI_Texture_t* t = checktexture(lua, 1);
	if(t->backgroundTex == nil)
		instantiateAllTextures(t->region);
    if(t->usecamera == 0)
    {
//        AddRegionToChain(CameraChain,t->region);
        page_camerause++;
        incCameraUse();
    }
//        [g_glView IncCameraUse];
	t->usecamera = 1;
	t->isTiled = false; // Camera textures cannot be tiled
    t->width =480 ;//640;
    t->height = 640;
    
    if(UsesTextureBrush())
        SetBrushAsCamera(true);
	return 0;
}

void readPixelColor(GLuint t, int x, int y, unsigned char* colors);

extern GLuint	cameraTexture;

int texture_PixelColor(lua_State* lua)
{
   	urAPI_Texture_t* t = checktexture(lua, 1);
    lua_Number x = luaL_checknumber(lua,2);
    lua_Number y = luaL_checknumber(lua,3);
    unsigned char colors[4*4];
    y = t->height-y;
    GLuint	texture;
    if(t->usecamera)
        texture = cameraTexture;
    else
    {
        if(t->backgroundTex==0)
            instantiateTexture(t->region);
        texture = (GLuint)t->backgroundTex.name;
    }
    readPixelColor(texture, (int)x, (int)y, (unsigned char*)colors);
    lua_pushnumber(lua, colors[0]);
    lua_pushnumber(lua, colors[1]);
    lua_pushnumber(lua, colors[2]);
    lua_pushnumber(lua, colors[3]);
    return 4;
}

const char* urFilterModeNames[] = { "NONE", "SATURATION", "CONTRAST", "BRIGHTNESS", "EXPOSURE", "RGB", "SHARPEN", "UNSHARPMASK",
    "TRANSFORM", "TRANSFORM3D", "CROP", /*"MASK",*/ "GAMMA", "TONECURVE", "HAZE", "SEPIA", "COLORINVERT", "GRAYSCALE",
    "THRESHOLD", "ADAPTIVETHRESHOLD", "PIXELLATE", "POLARPIXELLATE", "CROSSHATCH", "SOBELEDGEDETECTION",
    "PREWITTEDGEDETECTION", "CANNYEDGEDETECTION", "XYGRADIENT", /*"HARRISCORNERDETECTION", "NOBLECORNERDETECTION",
                                                                 "SHITOMASIFEATUREDETECTION",*/ "SKETCH", "TOON", "SMOOTHTOON", "TILTSHIFT", "CGA", "POSTERIZE", /*"CONVOLUTION",*/
    "EMBOSS", /*"KUWAHARA",*/ "VIGNETTE", "GAUSSIAN", "GAUSSIAN_SELECTIVE", "FASTBLUR", "BOXBLUR", "MEDIAN", "BILATERAL",
    "SWIRL", "BULGE", "PINCH", "STRETCH", "DILATION", "EROSION", "OPENING", "CLOSING",
    /*"PERLINNOISE",*/ /*"VORONI",*/  "MOSAIC", /*"DISSOLVE", "CHROMAKEY", "MULTIPLY", "OVERLAY",*/ /*"LIGHTEN", "DARKEN", "COLORBURN",
                                                                                                     "COLORDODGE", "SCREENBLEND", "DIFFERENCEBLEND", "SUBTRACTBLEND", "EXCLUSIONBLEND", "HARDLIGHTBLEND", "SOFTLIGHTBLEND",*/
    /*"CUSTOM",*/ "SEPIAPIXELLATE",
    "POLKADOT", "HALFTONE", "LEVELS", "MONOCHROME", "HUE",
    "WHITEBALANCE", /*"LOWPASS", "HIGHPASS", "MOTIONDETECTOR",*/ /*"THRESHOLDSKETCH",*/
    "SPHEREREFRACTION", "GLASSSPHERE", /*"HIGHLIGHTSHADOW",*/ "LOCALBINARYPATTERN"
};


int texture_SetFilter(lua_State *lua)
{
   	urAPI_Texture_t* t = checktexture(lua, 1);
	const char* filtermode = luaL_checkstring(lua, 2);
#ifdef GPUIMAGE
    for(int i=0; i<maxFilterMode-1; i++)
    {
        if(!strcmp(filtermode,urFilterModeNames[i]))
        {
            
            if(i==0)
            {
                if(t->filterHandler)
                {
//                    [t->filterInput removeAllTargets];
//                    [t->filterInput release];
//                    t->filterInput = NULL;
                    [g_glView->videoCamera removeTarget:t->inputFilter];
                    [t->inputFilter removeTarget:t->filterOutput];
                    [t->inputFilter release];
                    t->inputFilter = NULL;
                    [t->filterOutput release];
                    t->filterOutput = NULL;
                    [t->filterHandler release];
                    t->filterHandler = NULL;
                    t->filtertype = GPUIMAGE_NONE;
                }
                return 0; // No filter
            }

            t->inputFilter = [g_glView createFilter:(GPUImageFilterType)i];
            if(t->inputFilter)
            {
                if(t->usecamera)
                {
//                    t->filterInput = [[GPUImageTextureInput alloc] initWithTexture:cameraTexture size:CGSizeMake(t->region->width, t->region->height)];
//                    t->filterInput = [[GPUImageTextureInput alloc] initWithTexture:cameraTexture size:CGSizeMake(640, 480)];
                }
                else
                    assert(0);
                t->filtertype = (GPUImageFilterType)i;
                t->filterOutput = [[GPUImageTextureOutput alloc] init];
                t->filterHandler = [[urFilterHandler alloc] init];
                t->filterHandler->region = t->region;
                t->filterOutput.delegate = t->filterHandler;
                [t->inputFilter addTarget:t->filterOutput];
                if(t->usecamera)
                    [g_glView->videoCamera addTarget:t->inputFilter];
                
//                [t->filterInput addTarget:t->filterOutput];
                
            }
            else
                assert(0);

            return 0;
        }
    }
#endif
    luaL_error(lua, "Unknown camera filter mode: %s", filtermode);
    return 0;
}

int texture_SetFilterParameter(lua_State *lua)
{
    urAPI_Texture_t* t = checktexture(lua, 1);
	double value = luaL_checknumber(lua,2);
#ifdef GPUIMAGE
    t->filterValue = value;
    [g_glView setFilterParameter:value forFilter:t->inputFilter withType:t->filtertype];
#endif
    return 0;
}


int region_UseAsBrush(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
    
    if (t->texture->usecamera) {
        SetBrushAsCamera(true);
    } else if(t->texture->backgroundTex == nil && t->texture->texturepath != TEXTURE_SOLID) {
		instantiateAllTextures(t);
        SetBrushAsCamera(false);
    }
    else
    {
        SetBrushAsCamera(false);
    }
    
//    SetBrushTexture(t->texture->backgroundTex);
    SetBrushTexture(t->texture);


	return 0;
}

int textlabel_Font(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushstring(lua, t->font);
	return 1;
}

int textlabel_SetFont(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->font = luaL_checkstring(lua,2);
    t->updatestring = true;
	return 0;
}


const char JUSTIFYH_STRING_CENTER[] = "CENTER";
const char JUSTIFYH_STRING_LEFT[] = "LEFT";
const char JUSTIFYH_STRING_RIGHT[] = "RIGHT";

const char JUSTIFYV_STRING_MIDDLE[] = "MIDDLE";
const char JUSTIFYV_STRING_TOP[] = "TOP";
const char JUSTIFYV_STRING_BOTTOM[] = "BOTTOM";


int textlabel_HorizontalAlign(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* justifyh = JUSTIFYH_STRING_CENTER;;
	switch(t->justifyh)
	{
		case JUSTIFYH_CENTER:
			justifyh = JUSTIFYH_STRING_CENTER;
			break;
		case JUSTIFYH_LEFT:
			justifyh = JUSTIFYH_STRING_LEFT;
			break;
		case JUSTIFYH_RIGHT:
			justifyh = JUSTIFYH_STRING_RIGHT;
			break;
	}
	lua_pushstring(lua, justifyh);
	return 1;
}


int textlabel_SetHorizontalAlign(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* justifyh = luaL_checkstring(lua, 2);
	
	if(!strcmp(justifyh, JUSTIFYH_STRING_CENTER))
		t->justifyh = JUSTIFYH_CENTER;
	else if(!strcmp(justifyh, JUSTIFYH_STRING_LEFT))
		t->justifyh = JUSTIFYH_LEFT;
	else if(!strcmp(justifyh, JUSTIFYH_STRING_RIGHT))
		t->justifyh = JUSTIFYH_RIGHT;
    
    t->updatestring = true;
	return 0;
}

int textlabel_VerticalAlign(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* justifyv= JUSTIFYV_STRING_MIDDLE;
	switch(t->justifyv)
	{
		case JUSTIFYV_MIDDLE:
			justifyv = JUSTIFYV_STRING_MIDDLE;
			break;
		case JUSTIFYV_TOP:
			justifyv = JUSTIFYV_STRING_TOP;
			break;
		case JUSTIFYV_BOTTOM:
			justifyv = JUSTIFYV_STRING_BOTTOM;
			break;
	}
	lua_pushstring(lua, justifyv);
	return 1;
}

int textlabel_SetVerticalAlign(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* justifyv = luaL_checkstring(lua, 2);
	
	if(!strcmp(justifyv, JUSTIFYV_STRING_MIDDLE))
		t->justifyv = JUSTIFYV_MIDDLE;
	else if(!strcmp(justifyv, JUSTIFYV_STRING_TOP))
		t->justifyv = JUSTIFYV_TOP;
	else if(!strcmp(justifyv, JUSTIFYV_STRING_BOTTOM))
		t->justifyv = JUSTIFYV_BOTTOM;
	return 0;
}

int textlabel_SetWrap(lua_State* lua)
{
	urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
	const char* wrap = luaL_checkstring(lua,2);
	if(wrap)
	{
		textlabel->wrap = textlabel_wrap2index(wrap);
	}
	return 0;
}


int textlabel_Wrap(lua_State* lua)
{
	urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
	lua_pushstring(lua, textlabel_wrapindex2str(textlabel->wrap));
	return 1;
}

int textlabel_CharPosition(lua_State* lua)
{
    urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
    lua_Number x = luaL_checknumber(lua,2);
    lua_Number y = luaL_checknumber(lua,3);

#ifndef UISTRINGS
    lua_pushnumber(lua, textlabel->textlabelTex->charIndexAtPos(x,y));
#endif
    return 1;
}

int textlabel_LabelChar(lua_State* lua)
{
    urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
    lua_Number start_l = 0;
    lua_Number end_l = strlen(textlabel->text);
    if(lua_gettop(lua)==3)
    {
        start_l = luaL_checknumber(lua,2);
        end_l = luaL_checknumber(lua,3);
    }
    
}

int textlabel_LabelBounds(lua_State* lua)
{
    urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
    lua_Number start_l = 0;
    lua_Number end_l = strlen(textlabel->text);
    if(lua_gettop(lua)==3)
    {
        start_l = luaL_checknumber(lua,2);
        end_l = luaL_checknumber(lua,3);
    }
    
    if(textlabel->textlabelTex == NULL)
        return 0;
#ifndef UISTRINGS
    charPos**  &charPosLines = textlabel->textlabelTex->charPosLines; //getCharPosLines();
    int start = (int)start_l;
    int end = (int)end_l;
    
    int pos=0;
    int returncnt = 0;
    bool started = false;
    assert((returncnt%2)==0);
    NSLog(@"Num Lines: %d", textlabel->textlabelTex->charLines());
    for(int i=0;i< textlabel->textlabelTex->charLines(); i++)
    {
        assert((returncnt%2)==0);
        int linelen = charPosLines[i][0].len + 1;
        if(start<pos+linelen && !started)
        {
            lua_pushnumber(lua, charPosLines[i][start-pos].x);
            lua_pushnumber(lua, charPosLines[i][start-pos].y + textlabel->textlabelTex->yalign);
            NSLog(@"Label Start: %d %d (%d)- %d %d: %c (%d)", start, start-pos, i, charPosLines[i][start-pos].x,charPosLines[i][start-pos].y + textlabel->textlabelTex->yalign, charPosLines[i][start-pos].value, charPosLines[i][start-pos].width);
            returncnt = returncnt + 2;
            started = true;
        }

        if(end<pos+linelen)
        {
            lua_pushnumber(lua, charPosLines[i][end-pos].x);
            lua_pushnumber(lua, charPosLines[i][end-pos].y+charPosLines[i][end-pos].height + textlabel->textlabelTex->yalign);
            NSLog(@"Label End: %d %d (%d)- %d %d: %c (%d)", end, end-pos, i, charPosLines[i][end-pos].x,charPosLines[i][end-pos].y + textlabel->textlabelTex->yalign, charPosLines[i][end-pos].value, charPosLines[i][end-pos].width);
            returncnt = returncnt + 2;
            started = false;
            assert((returncnt%2)==0);
            break;
        }
        
        pos = pos + linelen;
        assert((returncnt%2)==0);
        if(started)
        {
            lua_pushnumber(lua, charPosLines[i][linelen].x); // Old line end
            lua_pushnumber(lua, charPosLines[i][linelen].y+charPosLines[i][linelen].height + textlabel->textlabelTex->yalign);
            returncnt = returncnt + 2;
            if(i+1< textlabel->textlabelTex->charLines())
            {
                lua_pushnumber(lua, charPosLines[i+1][0].x); // New Line start
                lua_pushnumber(lua, charPosLines[i+1][0].y + textlabel->textlabelTex->yalign);
                returncnt = returncnt + 2;
                
            }
        }
        assert((returncnt%2)==0);
    }
    assert((returncnt%2)==0);
    if(started)
    {
        int i = textlabel->textlabelTex->charLines()-1;
        assert(i>=0);
        int linelen = charPosLines[i][0].len;
        lua_pushnumber(lua, charPosLines[i][linelen].x); // Old line end
        lua_pushnumber(lua, charPosLines[i][linelen].y+charPosLines[i][linelen].height + textlabel->textlabelTex->yalign);
        assert((returncnt%2)==0);
        returncnt = returncnt + 2;
        assert((returncnt%2)==0);
    }
    assert((returncnt%4)==0);
    return returncnt;
#else
    return 0;
#endif
}


/*
int textlabel_LabelAnchor(lua_State* lua)
{
    urAPI_TextLabel_t* textlabel = checktextlabel(lua,1);
    if(textlabel->textlabelTex == NULL)
        return 0;
    lua_pushnumber(lua, textlabel->textlabeTex->labelx);
    lua_pushnumber(lua, textlabel->textlabeTex->labely);
    return 2;
}
*/
int textlabel_ShadowColor(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->shadowcolor[0]);
	lua_pushnumber(lua, t->shadowcolor[1]);
	lua_pushnumber(lua, t->shadowcolor[2]);
	lua_pushnumber(lua, t->shadowcolor[3]);
	return 4;
}

int textlabel_SetShadowColor(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->shadowcolor[0] = luaL_checknumber(lua,2);
	t->shadowcolor[1] = luaL_checknumber(lua,3);
	t->shadowcolor[2] = luaL_checknumber(lua,4);
	t->shadowcolor[3] = luaL_checknumber(lua,5);
	t->drawshadow = true;
	t->updatestring = true;
	return 0;
}

int textlabel_ShadowOffset(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->shadowoffset[0]);
	lua_pushnumber(lua, t->shadowoffset[1]);
	return 2;
}

int textlabel_SetShadowOffset(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->shadowoffset[0] = luaL_checknumber(lua,2);
	t->shadowoffset[1] = luaL_checknumber(lua,3);
	t->updatestring = true;
	return 0;
}

int textlabel_ShadowBlur(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->shadowblur);
	return 1;
}

int textlabel_SetShadowBlur(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->shadowblur = luaL_checknumber(lua,2);
	t->updatestring = true;
	return 0;
}

int textlabel_Spacing(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->linespacing);
	return 1;
}

int textlabel_SetSpacing(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->linespacing = luaL_checknumber(lua,2);
    t->updatestring = true;
	return 0;
}

int textlabel_Color(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->textcolor[0]);
	lua_pushnumber(lua, t->textcolor[1]);
	lua_pushnumber(lua, t->textcolor[2]);
	lua_pushnumber(lua, t->textcolor[3]);
	return 4;
}

int textlabel_SetColor(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->textcolor[0] = luaL_checknumber(lua,2);
	t->textcolor[1] = luaL_checknumber(lua,3);
	t->textcolor[2] = luaL_checknumber(lua,4);
	t->textcolor[3] = luaL_checknumber(lua,5);
	return 0;
}

int textlabel_Height(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->stringheight);
	return 1;
}

int textlabel_Width(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->stringwidth);
	return 1;
}

int textlabel_SetFontHeight(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->textheight = luaL_checknumber(lua,2);
    t->updatestring = true;
	return 0;
}

int textlabel_FontHeight(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->textheight);
	return 1;
}
	
int textlabel_Label(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushstring(lua, t->text);
	return 1;
}

void renderTextLabel(urAPI_Region_t *t);

int textlabel_SetLabel(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* text = luaL_checkstring(lua,2);
	
	if(t->text != NULL && t->text != textlabel_empty)
		free(t->text);
	t->text = (char*)malloc(strlen(text)+1);
	strcpy(t->text, text);

//	t->updatestring = true;
    renderTextLabel(t->region);
    t->updatestring = false;
	return 0;
}

int textlabel_SetFormattedText(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	const char* text = luaL_checkstring(lua,2);
	
	if(t->text != NULL && t->text != textlabel_empty)
		free(t->text);
	t->text = (char*)malloc(strlen(text)+1);
	strcpy(t->text, text);

	// NYI
	
	return 0;
}

int textlabel_SetRotation(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->rotation = luaL_checknumber(lua,2);
	return 0;
}

int textlabel_Rotation(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->rotation);
	return 1;
}

int textlabel_SetOutline(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	t->outlinemode = luaL_checknumber(lua,2);
    if(lua_gettop(lua)>2)
    {
        t->outlinethickness = luaL_checknumber(lua,3);
    }
    else if( t->outlinemode > 0 )
        t->outlinethickness = 1; // 1 thickness if we have an outline
    t->updatestring = true;
}

int textlabel_Outline(lua_State* lua)
{
	urAPI_TextLabel_t* t = checktextlabel(lua, 1);
	lua_pushnumber(lua, t->outlinemode);
	lua_pushnumber(lua, t->outlinethickness);
    
    return 2;
}

// SOAR support API

#ifdef SOAR_SUPPORT

#ifdef SOAR_DEBUG
void soar_MyPrintEventHandler(sml::smlPrintEventId id, void* pUserData, sml::Agent* pAgent, char const* pMessage)
{
	std::cout << pMessage << std::endl;
}
#endif

const char* region_SoarAgentFile(const char* name, const char* ext)
{
	NSString* nsName = [NSString stringWithUTF8String:name];
	NSString* nsExt = [NSString stringWithUTF8String:ext];
	
	return [[[NSBundle mainBundle] pathForResource:nsName ofType:nsExt] UTF8String];
}

void region_SoarInit(urAPI_Region_t* region)
{
	if (!region->soarKernel)
	{
		region->soarKernel = sml::Kernel::CreateKernelInCurrentThread(sml::Kernel::kDefaultLibraryName, true, 0);
        
        {
            char buffer[50];
            sprintf(buffer, "agent-%d", region->soarKernel->GetNumberAgents());
            
            region->soarAgent = region->soarKernel->CreateAgent(buffer);
        }
		
#ifdef SOAR_DEBUG
		region->soarAgent->RegisterForPrintEvent(sml::smlEVENT_PRINT, soar_MyPrintEventHandler, NULL);
		region->soarAgent->ExecuteCommandLine("watch 1");
#endif
		
		region->soarIds = new std::map< int, sml::Identifier* >();
		(*region->soarIds)[0] = region->soarAgent->GetInputLink();
		region->soarIdCounter = 1;
		
		region->soarWMEs = new std::map< int, sml::WMElement* >();
		region->soarWMEcounter = 1;
	}
}

int region_SoarGetDecisions(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	region_SoarInit(region);
	lua_pushnumber(lua, region->soarAgent->GetDecisionCycleCounter());
	
	return 1;
}

int region_SoarCreateID(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	int inid = luaL_checknumber(lua,2);
	const char* inlabel = luaL_checkstring(lua,3);
	
	int idId = 0;
	int wmeId = 0;
	
	region_SoarInit(region);
	
	std::map< int, sml::Identifier* >::iterator it = region->soarIds->find( inid );
	if ( it != region->soarIds->end() )
	{
		wmeId = region->soarWMEcounter++;
		idId = region->soarIdCounter++;
		
		sml::WMElement* wme = it->second->CreateIdWME( inlabel );
		
		(*region->soarWMEs)[ wmeId ] = wme;
		(*region->soarIds)[ idId ] = wme->ConvertToIdentifier();
	}
	
	lua_pushnumber(lua, idId);
	lua_pushnumber(lua, wmeId);
	
	return 2;
}

int region_SoarDelete(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	int inid = luaL_checknumber(lua,2);
	
	region_SoarInit(region);
	
	std::map< int, sml::WMElement* >::iterator it = region->soarWMEs->find( inid );
	if ( it != region->soarWMEs->end() )
	{
		it->second->DestroyWME();
		region->soarWMEs->erase(it);
	}
	
	return 0;
}

int region_SoarCreateConstant(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	int inid = luaL_checknumber(lua,2);
	const char* inlabel = luaL_checkstring(lua,3);	
	
	int wmeId = 0;

	region_SoarInit(region);
	
	std::map< int, sml::Identifier* >::iterator it = region->soarIds->find( inid );
	if ( it != region->soarIds->end() )
	{
		if(lua_isnumber(lua,4))
		{
			wmeId = region->soarWMEcounter++;
			(*region->soarWMEs)[ wmeId ] = it->second->CreateFloatWME( inlabel, luaL_checknumber(lua, 4) );
		}
		else if(lua_isstring(lua,4))
		{
			wmeId = region->soarWMEcounter++;
			(*region->soarWMEs)[ wmeId ] = it->second->CreateStringWME( inlabel, luaL_checkstring(lua, 4) );
		}
	}
	
	lua_pushnumber(lua, wmeId);
	
	return 1;
}

int region_SoarLoadRules(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	const char* infile = luaL_checkstring(lua,2);
	const char* infileExt = luaL_checkstring(lua,3);
	
	region_SoarInit(region);
	region->soarAgent->LoadProductions(region_SoarAgentFile(infile, infileExt), false);
	
	return 0;
}

int region_SoarExec(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	const char *command = luaL_checkstring(lua,2);
	
	region_SoarInit(region);	
	lua_pushstring(lua, region->soarAgent->ExecuteCommandLine(command));
	
	return 1;
}

void region_SoarSetFieldInteger(lua_State* lua, const char* index, int value) 
{
	lua_pushstring(lua, index);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);
}

void region_SoarSetFieldFloat(lua_State* lua, const char* index, double value) 
{
	lua_pushstring(lua, index);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);
}

void region_SoarSetFieldString(lua_State* lua, const char* index, const char* value) 
{
	lua_pushstring(lua, index);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);
}

int region_SoarGetOutput(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	region_SoarInit(region);
	
	while (!region->soarAgent->Commands())
	{
		// std::cout << region->soarAgent->ExecuteCommandLine("print --depth 10 s1") << std::endl;
		region->soarAgent->RunSelfTilOutput();
	}
	
	// start easy: assume 1 command only
	if (region->soarAgent->GetNumberCommands() == 1)
	{
		sml::Identifier* cmd = region->soarAgent->GetCommand(0);
		
		lua_pushstring(lua, cmd->GetCommandName());
		lua_newtable(lua);
		
		for (int i=0; i<cmd->GetNumberChildren(); i++)
		{
			sml::WMElement* child = cmd->GetChild(i);
			
			if (child->ConvertToStringElement()!=NULL)
			{
				region_SoarSetFieldString(lua, child->GetAttribute(), child->ConvertToStringElement()->GetValue());
			}
			else if (child->ConvertToIntElement()!=NULL)
			{
				region_SoarSetFieldInteger(lua, child->GetAttribute(), child->ConvertToIntElement()->GetValue());
			}
			else if (child->ConvertToFloatElement()!=NULL)
			{
				region_SoarSetFieldFloat(lua, child->GetAttribute(), child->ConvertToFloatElement()->GetValue());
			}
		}
	}
	else 
	{
		lua_pushstring(lua, "");		
		lua_newtable(lua);
	}
	
	return 2;
}

int region_SoarSetOutputStatus(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	int status = luaL_checkinteger(lua, 2);
	
	region_SoarInit(region);
	
	if (region->soarAgent->GetNumberCommands() == 1)
	{
		if (status == 0)
		{
			region->soarAgent->GetCommand(0)->AddStatusError();
		}
		else if (status == 1)
		{
			region->soarAgent->GetCommand(0)->AddStatusComplete();
		}
	}
	
	region->soarAgent->ClearOutputLinkChanges();
	
	return 0;
}

int region_SoarFinish(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	region_SoarInit(region);
	
	do
	{
		region->soarAgent->RunSelf( 10 );
	} while ( region->soarAgent->GetRunState() != sml::sml_RUNSTATE_HALTED );
	
	return 0;
}

int region_SoarInit(lua_State *lua)
{
	urAPI_Region_t* region = checkregion(lua,1);
	
	region_SoarInit(region);
	region->soarAgent->InitSoar();
	
	return 0;
}

#endif

	
static const struct luaL_reg textlabelfuncs [] =
{
	{"Font", textlabel_Font},
	{"HorizontalAlign", textlabel_HorizontalAlign},
	{"VerticalAlign", textlabel_VerticalAlign},
	{"ShadowColor", textlabel_ShadowColor},
	{"ShadowOffset", textlabel_ShadowOffset},
	{"ShadowBlur", textlabel_ShadowBlur},
	{"Spacing", textlabel_Spacing},
	{"Color", textlabel_Color},
	{"SetFont", textlabel_SetFont},
	{"SetHorizontalAlign", textlabel_SetHorizontalAlign},
	{"SetVerticalAlign", textlabel_SetVerticalAlign},
	{"SetShadowColor", textlabel_SetShadowColor},
	{"SetShadowOffset", textlabel_SetShadowOffset},
	{"SetShadowBlur", textlabel_SetShadowBlur},
    {"SetOutline", textlabel_SetOutline},
    {"Outline", textlabel_Outline},
	{"SetSpacing", textlabel_SetSpacing},
	{"SetColor", textlabel_SetColor},
	{"Height", textlabel_Height},
	{"Width", textlabel_Width},
	{"Label", textlabel_Label},
	{"SetFormattedText", textlabel_SetFormattedText},
	{"SetWrap", textlabel_SetWrap},
	{"Wrap", textlabel_Wrap},
    {"CharPosition", textlabel_CharPosition},
    {"LabelBounds", textlabel_LabelBounds},
//    {"LabelAnchor", testlabel_LabelAnchor},
	{"SetLabel", textlabel_SetLabel},
	{"SetFontHeight", textlabel_SetFontHeight},
	{"FontHeight", textlabel_FontHeight},
	{"SetRotation", textlabel_SetRotation},
	{"Rotation", textlabel_Rotation},
	{NULL, NULL}
};

int texture_gc(lua_State* lua)
{
//	urAPI_Texture_t* region = checktexture(lua,1);
//	int a = 0;
    // NYI
	return 0;
}

static const struct luaL_reg texturefuncs [] =
{
	{"SetTexture", texture_SetTexture},
    {"WriteMovie", texture_SaveMovie},
    {"FinishMovie", texture_FinishMovie},
//	{"SetGradient", texture_SetGradient},
	{"SetGradientColor", texture_SetGradientColor},
	{"Texture", texture_Texture},
	{"SetSolidColor", texture_SetSolidColor},
	{"SolidColor", texture_SolidColor},
	{"SetTexCoord", texture_SetTexCoord},
	{"TexCoord", texture_TexCoord},
	{"SetRotation", texture_SetRotation},
	{"SetTexCoordModifiesRect", texture_SetTexCoordModifiesRect},
	{"TexCoordModifiesRect", texture_TexCoordModifiesRect},
	{"SetDesaturated", texture_SetDesaturated},
	{"IsDesaturated", texture_IsDesaturated},
	{"SetBlendMode", texture_SetBlendMode},
	{"BlendMode", texture_BlendMode},
	{"Line", texture_Line},
	{"Point", texture_Point},
	{"Ellipse", texture_Ellipse},
	{"Quad", texture_Quad},
	{"Rect", texture_Rect},
	{"Clear", texture_Clear},
	{"ClearBrush", texture_ClearBrush},
	{"SetFill", texture_SetFill},
	{"SetBrushSize", texture_SetBrushSize},
	{"BrushSize", texture_BrushSize},
	{"SetBrushColor", texture_SetBrushColor},
	{"SetTiling", texture_SetTiling},
	{"Tiling", texture_Tiling},
	{"Width", texture_Width},
	{"Height", texture_Height},
	{"UseCamera", texture_UseCamera},
    {"PixelColor", texture_PixelColor},
    {"SetFilter", texture_SetFilter},
    {"SetFilterParameter", texture_SetFilterParameter},
	{"__gc",       texture_gc},
	{NULL, NULL}
};

int region_gc(lua_State* lua)
{
//	urAPI_Region_t* region = checkregion(lua,1);
    // NYI
//	int a = 0;
	return 0;
}

static const struct luaL_reg regionfuncs [] = 
{
	{"EnableMoving", region_EnableMoving},
	{"EnableResizing", region_EnableResizing},
	{"Handle", region_Handle},
    {"ReadHandle", region_ReadHandle},
	{"SetHeight", region_SetHeight},
	{"SetWidth", region_SetWidth},
	{"Show", region_Show},
	{"Hide", region_Hide},
	{"EnableInput", region_EnableInput},
	{"EnableHorizontalScroll", region_EnableHorizontalScroll},
	{"EnableVerticalScroll", region_EnableVerticalScroll},
	{"SetAnchor", region_SetAnchor},
	{"SetLayer", region_SetLayer},
	{"Parent", region_Parent},
	{"Children", region_Children},
	{"Name", region_Name},
	{"Bottom", region_Bottom},
	{"Center", region_Center},
	{"Height", region_Height},
	{"Left", region_Left},
	{"NumAnchors", region_NumAnchors},
	{"Anchor", region_Anchor},
	{"Right", region_Right},
	{"Top", region_Top},
	{"Width", region_Width},
	{"IsShown", region_IsShown},
	{"IsVisible", region_IsVisible},
	{"SetParent", region_SetParent},
	{"SetAlpha", region_SetAlpha},
	{"Alpha", region_Alpha},
	{"Layer", region_Layer},
	{"Texture", region_Texture},
	{"TextLabel", region_TextLabel},
	{"Lower", region_Lower},
	{"Raise", region_Raise},
	{"IsToplevel", region_IsToplevel},
	{"MoveToTop", region_MoveToTop},
	{"EnableClamping", region_EnableClamping},
    {"SetClampRegion", region_SetClampRegion},
    {"ClampRegion", region_ClampRegion},
	{"RegionOverlap", region_RegionOverlap},
	{"UseAsBrush", region_UseAsBrush},
	{"EnableClipping", region_EnableClipping},
	{"SetClipRegion", region_SetClipRegion},
	{"ClipRegion", region_ClipRegion},
#ifdef SOAR_SUPPORT
	{"SoarCreateID", region_SoarCreateID},
    {"SoarGetDecisions", region_SoarGetDecisions},
	{"SoarDelete", region_SoarDelete},
	{"SoarCreateConstant", region_SoarCreateConstant},
	{"SoarLoadRules", region_SoarLoadRules},
	{"SoarExec", region_SoarExec},
	{"SoarGetOutput", region_SoarGetOutput},
	{"SoarSetOutputStatus", region_SoarSetOutputStatus},
	{"SoarFinish", region_SoarFinish},
	{"SoarInit", region_SoarInit},
#endif
	{"__gc",       region_gc},
	{NULL, NULL}
};



static const luaL_reg regionmetas[] = {
	{"__gc",       region_gc},
	{0, 0}
};



void addChild(urAPI_Region_t *parent, urAPI_Region_t *child)
{
	if(parent->firstchild == NULL)
    {
		parent->firstchild = child;
        if(child->nextchild != NULL)
        {
            child->nextchild = NULL;
//            child->parent = parent;
//            assert(0);
        }
    }
	else
	{
        child->nextchild = parent->firstchild;
        parent->firstchild = child;
/*		urAPI_Region_t *findlast = parent->firstchild;
		while(findlast->nextchild != NULL)
		{
			findlast = findlast->nextchild;
		}
		if(findlast->nextchild != child)
			findlast->nextchild = child;
*/
	}
}

void removeChild(urAPI_Region_t *parent, urAPI_Region_t *child)
{
	if(parent != NULL && parent->firstchild != NULL)
	{
		if(parent->firstchild == child)
		{
			parent->firstchild = parent->firstchild->nextchild;
            child->parent = NULL;
		}
		else
		{
			urAPI_Region_t *findlast = parent->firstchild;
			while(findlast->nextchild != NULL && findlast->nextchild != child)
			{
				findlast = findlast->nextchild;
			}
			if(findlast->nextchild == child)
			{
				findlast->nextchild = findlast->nextchild->nextchild;	
				child->nextchild = NULL;
                child->parent = NULL;
			}
		}
	}
    child->parent = NULL;
}

//------------------------------------------------------------------------------
// Flowbox Member urMus API
//------------------------------------------------------------------------------

int flowbox_Name(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	lua_pushstring(lua, fb->object->name);
	return 1;
}

// Object to to PushOut from.
// In to PushOut into.
// Needs ID on specific IN
int flowbox_SetPushLink(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
    
	int outindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int inindex = luaL_checknumber(lua, 4);
	
	if(outindex >= fb->object->nr_outs || inindex >= target->object->nr_ins)
	{
		return 0;
	}
	
    pthread_mutex_lock( &fb_mutex );
	fb->object->AddPushOut(outindex, &target->object->ins[inindex]);
    pthread_mutex_unlock( &fb_mutex );
   if(fb->object == cameraObject)
    {
        incCameraUse();
    }
    
    lua_pushboolean(lua, 1);
	return 1;
}

int flowboxout_SetPush(lua_State *lua)
{
	ursAPI_FlowBox_Port_t* fbout = checkflowboxport(lua, 1);
	ursAPI_FlowBox_Port_t* targetin = checkflowboxport(lua, 2);

    pthread_mutex_lock( &fb_mutex );
    fbout->object->AddPushOut(fbout->index, &targetin->object->ins[targetin->index]);
    pthread_mutex_unlock( &fb_mutex );
   if(fbout->object == cameraObject)
    {
        incCameraUse();
    }
    return 0;
}

void AddPull(ursObject* src, int inindex, ursObject* target, int outindex)
{
    pthread_mutex_lock( &fb_mutex );

	src->AddPullIn(inindex, &target->outs[outindex]);
    
    /*
	if(!strcmp(src->name,dacobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveDacTickSinkList.AddSink(&target->outs[outindex]);
	
	if(!strcmp(src->name,visobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveVisTickSinkList.AddSink(&target->outs[outindex]);
    
	if(!strcmp(src->name,netobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveNetTickSinkList.AddSink(&target->outs[outindex]);
     */
    pthread_mutex_unlock( &fb_mutex );

}

int flowbox_SetPullLink(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int inindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int outindex = luaL_checknumber(lua, 4);
	
	if(inindex >= fb->object->nr_ins || outindex >= target->object->nr_outs)
	{
		return 0;
	}
    
    AddPull(fb->object, inindex, target->object, outindex);

	lua_pushboolean(lua, 1);
	return 1;
}

int flowboxin_SetPull(lua_State *lua)
{
	ursAPI_FlowBox_Port_t* fbin = checkflowboxport(lua, 1);
	ursAPI_FlowBox_Port_t* targetout = checkflowboxport(lua, 2);
    
    AddPull(fbin->object, fbin->index, targetout->object, targetout->index);
    return 0;
}

int flowbox_IsPushed(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int outindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int inindex = luaL_checknumber(lua, 4);
    
	if(outindex >= fb->object->nr_outs || inindex >= target->object->nr_ins)
	{
		return 0;
	}
	
	if(fb->object->IsPushedOut(outindex, &target->object->ins[inindex]))
	{
		lua_pushboolean(lua,1);
		return 1;
	}
	else
		return 0;
}

int flowbox_IsPulled(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int inindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int outindex = luaL_checknumber(lua, 4);
	
	if(inindex >= fb->object->nr_ins || outindex >= target->object->nr_outs)
	{
		return 0;
	}
	
	if(fb->object->IsPulledIn(inindex, &target->object->outs[outindex]))
	{
	    lua_pushboolean(lua, 1);
		return 1;
	}
	else
		return 0;
}


/* UNFINISHED void removeAllPushLinks(ursAPI_FlowBox_t* fb)
 {
 for
 fb->object->RemovePushOut(outindex, &target->object->ins[inindex]);
 }	
*/

int flowbox_RemovePushLink(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int outindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int inindex = luaL_checknumber(lua, 4);
	
	if(outindex >= fb->object->nr_outs || inindex >= target->object->nr_ins)
	{
		return 0;
	}
	
    pthread_mutex_lock( &fb_mutex );
	fb->object->RemovePushOut(outindex, &target->object->ins[inindex]);
    pthread_mutex_unlock( &fb_mutex );
    if(fb->object == cameraObject)
    {
        decCameraUse();
    }
    
    lua_pushboolean(lua, 1);
	return 1;
}

int flowboxout_RemovePush(lua_State *lua)
{
	ursAPI_FlowBox_Port_t* fbout = checkflowboxport(lua, 1);
	ursAPI_FlowBox_Port_t* targetin = checkflowboxport(lua, 2);
	
    pthread_mutex_lock( &fb_mutex );
	fbout->object->RemovePushOut(fbout->index, &targetin->object->ins[targetin->index]);
    pthread_mutex_unlock( &fb_mutex );
   if(fbout->object == cameraObject)
    {
        decCameraUse();
    }

    return 0;
}


void RemovePull(ursObject* src, int inindex, ursObject* target, int outindex)
{
    pthread_mutex_lock( &fb_mutex );
	src->RemovePullIn(inindex, &target->outs[outindex]);
    pthread_mutex_unlock( &fb_mutex );

	/*
	if(!strcmp(src->name,dacobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveDacTickSinkList.RemoveSink(&target->outs[outindex]);
	
	if(!strcmp(src->name,visobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveVisTickSinkList.RemoveSink(&target->outs[outindex]);
    
	if(!strcmp(src->name,netobject->name)) // This is hacky and should be done differently. Namely in the sink pulling
		urActiveNetTickSinkList.RemoveSink(&target->outs[outindex]);  
     */
}

int flowbox_RemovePullLink(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int inindex = luaL_checknumber(lua,2);
	ursAPI_FlowBox_t* target = checkflowbox(lua, 3);
	int outindex = luaL_checknumber(lua, 4);
	
	if(inindex >= fb->object->nr_ins || outindex >= target->object->nr_outs)
	{
		return 0;
	}
	
    RemovePull(fb->object, inindex, target->object, outindex);

    lua_pushboolean(lua, 1);
	return 1;
}

int flowboxin_RemovePull(lua_State *lua)
{
	ursAPI_FlowBox_Port_t* fbin = checkflowboxport(lua, 1);
	ursAPI_FlowBox_Port_t* targetout = checkflowboxport(lua, 2);
    
    RemovePull(fbin->object, fbin->index, targetout->object, targetout->index);
    return 0;
}

int flowbox_IsPushing(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	int index = luaL_checknumber(lua,2);
	lua_pushboolean(lua, fb->object->firstpullin[index]!=NULL);
	return 1;
}

int flowbox_IsPulling(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	int index = luaL_checknumber(lua,2);
	lua_pushboolean(lua, fb->object->firstpushout[index]!=NULL);
	return 1;
}

int flowbox_IsPlaced(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	int index = luaL_checknumber(lua,2);
	lua_pushboolean(lua, fb->object->ins[index].isplaced);
	return 1;
}

int flowbox_NumIns(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
    
	lua_pushnumber(lua, fb->object->nr_ins);
	return 1;
}

int flowbox_NumOuts(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	lua_pushnumber(lua, fb->object->nr_outs);
	return 1;
}

int flowbox_Ins(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
	int nrins = fb->object->lastin;
	for(int j=0; j< nrins; j++)
        //		if(fb->object->ins[j].name!=(void*)0x1)
        lua_pushstring(lua, fb->object->ins[j].name);
    //		else {
    //			int a=2;
    //		}
    
	return nrins;
}

int flowbox_Outs(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
    
	int nrouts = fb->object->lastout;
	for(int j=0; j< nrouts; j++)
		lua_pushstring(lua, fb->object->outs[j].name);
	return nrouts;
}

extern double lastpushout;

int flowbox_Push(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	float indata = luaL_checknumber(lua, 2);
    
    lastpushout = indata;
    
	fb->object->CallAllPushOuts(indata);
    /*	if(fb->object->firstpushout[0]!=NULL)
     {
     ursObject* inobject;
     urSoundPushOut* pushto = fb->object->firstpushout[0];
     for(;pushto!=NULL; pushto = pushto->next)
     {	
     urSoundIn* in = pushto->in;
     inobject = in->object;
     in->inFuncTick(inobject, indata);
     }
     }*/
    //	callAllPushSources(indata);
    
	return 0;
}

int flowbox_Pull(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	
    if(fb->object->firstpullin[0]!=NULL)
    {
        fb->object->lastindata[0] = fb->object->CallAllPullIns();
    }
	
    lua_pushnumber(lua, fb->object->lastindata[0]);
    
	return 1;
}

extern double visoutdata;

int flowbox_Get(lua_State *lua)
{
    //	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
    //	float indata = luaL_checknumber(lua, 2);
	
	lua_pushnumber(lua, visoutdata);
	
	return 1;
}

int flowbox_Note(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
    if(fb->object && fb->object->note)
    {
        lua_pushstring(lua, fb->object->note);
        return 1;
    }
    return 0;
    
}

int flowbox_AddFile(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	const char* filename = luaL_checkstring(lua, 2);
    
	if(!strcmp(fb->object->name, "Sample"))
	{
        const char * filepath = accessiblePathSystemFirst(filename);
        if(filepath==NULL)
        {
            luaL_error(lua, "File \"%s\" not found.", filename);
        }
        else
            Sample_AddFile(fb->object, filepath);
	}
    else
    {
        luaL_error(lua, "Method AddFile not available for this flowbox.");
    }
    return 0;
}

int flowbox_ReadFile(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	const char* filename = luaL_checkstring(lua, 2);
    
	if(!strcmp(fb->object->name, "Looper"))
	{
		Looper_ReadFile(fb->object, filename);
	}
    return 0;
    
}

int flowbox_WriteFile(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	const char* filename = luaL_checkstring(lua, 2);
    
	if(!strcmp(fb->object->name, "Looper"))
	{
		Looper_WriteFile(fb->object, filename);
	}
    return 0;
    
}

int flowbox_IsInstantiable(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	lua_pushboolean(lua, !fb->object->noninstantiable);
	return 1;
}

int flowbox_InstanceNumber(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	lua_pushnumber(lua, fb->object->instancenumber);
	return 1;
}

int flowbox_NumberInstances(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	lua_pushnumber(lua, fb->object->instancelist->Last());
	return 1;
}

int flowbox_Couple(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	if(fb->object->iscoupled)
	{
		lua_pushnumber(lua, fb->object->couple_in);
		lua_pushnumber(lua, fb->object->couple_out);
		return 2;
	}
	else
		return 0;
}

int flowbox_IsCoupled(lua_State *lua)
{
	ursAPI_FlowBox_t* fb = checkflowbox(lua, 1);
	lua_pushboolean(lua, fb->object->iscoupled);
	return 1;
}

int flowbox_gc(lua_State* lua)
{
    //	urAPI_Region_t* region = checkregion(lua,1);
    // NYI
    //	int a = 0;
	return 0;
}

// Methods table for the flowbox API

static const struct luaL_reg flowboxfuncs [] = 
{
    {"Name", flowbox_Name},
    {"NumIns", flowbox_NumIns},
    {"NumOuts", flowbox_NumOuts},
    {"Ins", flowbox_Ins},
    {"Outs", flowbox_Outs},
    {"SetPushLink", flowbox_SetPushLink},
    {"SetPullLink", flowbox_SetPullLink},
    {"RemovePushLink", flowbox_RemovePushLink},
    {"RemovePullLink", flowbox_RemovePullLink},
    {"IsPushed", flowbox_IsPushed},
    {"IsPulled", flowbox_IsPulled},
    {"Push", flowbox_Push},
    {"Pull", flowbox_Pull},
    {"Get", flowbox_Get},
    {"Note", flowbox_Note},
    {"AddFile", flowbox_AddFile},
    {"ReadFile", flowbox_ReadFile},
    {"WriteFile", flowbox_WriteFile},
    {"IsInstantiable", flowbox_IsInstantiable},
    {"InstanceNumber", flowbox_InstanceNumber},
    {"NumberInstances", flowbox_NumberInstances},
    {"Couple", flowbox_Couple},
    {"IsCoupled", flowbox_IsCoupled},
    {"__gc",       flowbox_gc},
    {NULL, NULL}
};

static const struct luaL_reg flowboxoutfuncs [] =
{
    {"SetPush",flowboxout_SetPush},
    {"RemovePush", flowboxout_RemovePush},
    {NULL, NULL}
};

static const struct luaL_reg flowboxinfuncs [] =
{
    {"SetPull",flowboxin_SetPull},
    {"RemovePull", flowboxin_RemovePull},
    {NULL, NULL}
};

static int addToPatch(ursAPI_FlowBox_t* flowbox)
{
	if(firstFlowbox[currentPatch] == NULL)
	{
		firstFlowbox[currentPatch] = (ursAPI_FlowBox_t**)malloc(sizeof(ursAPI_FlowBox_t**));

		numFlowBoxes[currentPatch]++;
		firstFlowbox[currentPatch][0]=flowbox;
	}
	else {
		numFlowBoxes[currentPatch]++;
		firstFlowbox[currentPatch] = (ursAPI_FlowBox_t**)realloc(firstFlowbox[currentPatch],sizeof(ursAPI_FlowBox_t**)*numFlowBoxes[currentPatch]);
		firstFlowbox[currentPatch][numFlowBoxes[currentPatch]-1] = flowbox;
	}
    return 0;
}

static void removeFlowboxLinks(ursAPI_FlowBox_t* flowbox)
{
    flowbox->object->RemoveAllPullIns();
    flowbox->object->RemoveAllPushOuts();
    flowbox->object->RemoveFromSinks();
    flowbox->object->RemoveFromSources();

}

//------------------------------------------------------------------------------
// Logging support (for HTML editor)
//------------------------------------------------------------------------------

#include <vector>
#include <string>

static std::vector<std::string> ur_log;

void ur_Log(const char * str) {
	ur_log.push_back(str);
}

char * ur_GetLog(int since, int *nlog) {
	if(since<0) since=0;
	std::string str="";
    long loglen = ur_log.size();
    int i;
	for(i=since;i<loglen && (str.length()+ur_log[i].length()+1) < 7800;i++) { // HTTP Length restriction is 8192, giving 300+ char slack for header.
		str+=ur_log[i];
		str+="\n";
	}
	char *result=(char *)malloc(str.length()+1);
	strcpy(result, str.c_str());
    if (since >= loglen)
        *nlog=loglen;
    else
        *nlog = i;
	return result;
}

//------------------------------------------------------------------------------
// Global urMus lua API function implementations
//------------------------------------------------------------------------------

// stackDump - to print out what's in stack for debugging purpose
// ---- added by Sang

static void stackDump (lua_State *L) {
    int i;
    int top = lua_gettop(L);
    printf("stackDump:\n");
    for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
                
            case LUA_TSTRING:  /* strings */
                printf("`%s'", lua_tostring(L, i));
                break;
                
            case LUA_TBOOLEAN:  /* booleans */
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;
                
            case LUA_TNUMBER:  /* numbers */
                printf("%g", lua_tonumber(L, i));
                break;
                
            default:  /* other values */
                printf("%s", lua_typename(L, t));
                break;
                
        }
        printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}
//------------------------------------------------------------------------------
// Debug Printing
//------------------------------------------------------------------------------

int l_DPrint(lua_State* lua)
{
	const char* str = luaL_checkstring(lua,1);
	if(str!=nil)
	{
		ur_Log(str);
		errorstr = str;
		newerror = true;
	}
	return 0;
}

#define RETURN_STRING_SIZE 25
#define RETURN_BUFFER_SIZE 8000


int urMus_return_length = RETURN_STRING_SIZE;

void assert_length(int size, int length){
  //  printf("size:%d, length:%d\n", size, length);
    assert(size > length);

}
//------------------------------------------------------------------------------
// Generate XMl for Element in Table
//------------------------------------------------------------------------------

int l_ElemToXml(lua_State* lua)
{
    int type = lua_type(lua, -1);
    const char *strNull = "null";
    const char *value = NULL;
    const char *strType = NULL;
    int length = strlen(lua_typename(lua, type)) + 100;
//    const char
  //  printf("type name : %s, %s\n", lua_typename(lua, type), lua_tostring(lua, -1));
    
/*    if ( lua_tostring(lua, -1) == NULL)
    {
        luaL_error(lua, "ElemToXml(): %s, the argument is not a valid expression", lua_typename(lua, type));
        return 0;
    }
*/
    if (lua_type(lua,-1) == LUA_TBOOLEAN)
    {
        if (lua_toboolean(lua, -1))
            value = "true";
        else
            value = "false";
    }
    else if (lua_type(lua, -1) == LUA_TTABLE) // to differentiate region
    {
        lua_pushstring(lua, "Texture"); // stack now contains: -1 => "Texture"; -2 => table;
        lua_gettable(lua, -2); // stack now contains : -1 => nil (if it is not region) or function (if it is a region)
        if (lua_isfunction(lua, -1))
        {
            strType = "region";
        }
        lua_pop(lua, 1);
        
        lua_pushstring(lua, "IsInstantiable"); // stack now contains: -1 => "IsInstantiable"; -2 => table;
        lua_gettable(lua, -2); // stack now contains : -1 => nil (if it is not flowbox) or function (if it is a region)
        if (lua_isfunction(lua, -1))
        {
            strType = "flowbox";
            lua_pop(lua,1);
            flowbox_Name(lua);
            value = lua_tostring(lua, -1);
        }
        lua_pop(lua, 1);
    }
    else if (lua_type(lua, -1) == LUA_TUSERDATA) // to differentiate region
    {
        lua_pushstring(lua, "SetTexture"); // stack now contains: -1 => "SetTexture"; -2 => userdata;
        lua_gettable(lua, -2); // stack now contains : -1 => nil (if it is not texture) or function (if it is a region)
        if (lua_isfunction(lua, -1))
        {
            strType = "texture";
        }
        lua_pop(lua, 1);
        lua_pushstring(lua, "SetLabel"); // stack now contains: -1 => "SetLabel"; -2 => userdata;
        lua_gettable(lua, -2); // stack now contains : -1 => nil (if it is not text label) or function (if it is a region)
        if (lua_isfunction(lua, -1))
        {
            strType = "text label";
            lua_pop(lua,1);
            textlabel_Label(lua);
            value = lua_tostring(lua, -1);
        }
        lua_pop(lua, 1);
    }
    else{
        value = lua_tostring(lua, -1);
    }
    
    if (value)
        length += strlen(value);
    else
        length += strlen(strNull);
    
    char * str_return = (char *)malloc(length * sizeof(char));
    strcpy(str_return, "<e><t>");
    assert_length(length, strlen(str_return));
    if (strType)
        strcat(str_return, strType);
    else
        strcat(str_return, lua_typename(lua, type));
    assert_length(length, strlen(str_return));
    strcat(str_return, "</t><v>");
    assert_length(length, strlen(str_return));
    if (value)
        strcat(str_return, value);
    else
        strcat(str_return, strNull);
    assert_length(length, strlen(str_return));
    strcat(str_return, "</v></e>");
    assert_length(length, strlen(str_return));
    
    lua_pushstring(lua, str_return);
    //   lua_setglobal(lua,"__urMusReturn");
    
    free( (void *)str_return);
    return 1;
    
}


//------------------------------------------------------------------------------
// Generate XMl for Table
//------------------------------------------------------------------------------

int l_TableToXml(lua_State* lua)
{
    int iType = lua_type(lua, -1);
    int kType;
    if (!lua_istable(lua, -1)){
        luaL_error(lua, "TableToXml(): %s, not a valid table", lua_typename(lua, iType));
        return 0;
    }
    // convert table into a string
    int return_str_size = RETURN_STRING_SIZE;
    char * str_return = (char *)malloc(return_str_size * sizeof(char));
    lua_pushvalue(lua,-1);
    /* table is in the stack at index 't' */
    lua_pushnil(lua);  /* first key*/
    const char * start  = "<lua_table>\n";
    const char * end  = "</lua_table>";
    const char * strNull = "null";
    const char * element_key  = "<e><k>";
    const char * key_keytype  = "</k><kt>";
    const char * keytype_type = "</kt><t>";
    const char * type_value = "</t><v>";
    const char * value_element = "</v></e>\n";
    strcpy(str_return,start);

    int tag_length = strlen(element_key) + strlen(key_keytype) + strlen(keytype_type) + strlen(type_value) + strlen(value_element);
    
    int prev_length  = return_str_size;
#ifdef SANGDEBUG
    printf("l_TableToXml start\n");
#endif  
    int prev_chunk_length = 0;

    // SNAG :  this itereating table code is from http://stackoverflow.com/questions/6137684/iterate-through-lua-table
    while (lua_next(lua, -2) != 0) {
        lua_pushvalue(lua, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
        kType = lua_type(lua,-1);
        const char *key = lua_tostring(lua, -1);
        iType = lua_type(lua, -2);
        const char *value = lua_tostring(lua, -2);
        const char *type = lua_typename(lua, iType);
        const char *strKeyType = lua_typename(lua, kType);
        
        if (iType == LUA_TBOOLEAN)
        {
            if (lua_toboolean(lua, -2   ))
                value = "true";
            else
                value = "false";
        }
        
        //urAPI specific
 /*       if (iType == LUA_TTABLE){
            lua_pushstring(lua, "Texture")  ; // stack now contains: -1 => "Texture"; -2 => key; -3 => value; -4 => key; -5 => table
            lua_gettable(lua, -3); // stack now contains : -1 => nil (if it is not region) or function (if it is a region)
            if (lua_isfunction(lua, -1))
            {
                type = "region";
            }
            lua_pop(lua, 1);
        }
      */  // came back to  // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table

#ifdef SANGDEBUG
        printf("key:%s, keyType:%s, value:%s, type:%s\n", key,strKeyType, value, type);
#endif
        int cat_length=strlen(end) + 10; // 10 for cushion size.
        if (value)
            cat_length += strlen(value);
        else
            cat_length += strlen(strNull);
            
        cat_length += strlen(str_return) + strlen(key) + strlen(type) + strlen(strKeyType) +  tag_length ;
        //   printf("str_return:%s\n",str_return);
      
        if ( strlen(str_return) +cat_length - prev_chunk_length  >= RETURN_BUFFER_SIZE)
        {
            luaL_error(lua, "Table is too long to transfer. ");
            break;
        }
        
        while ( cat_length >= return_str_size){
            return_str_size *=2;
        }
        
        if ( prev_length != return_str_size){
            char * new_str_return = (char *)malloc(return_str_size * sizeof(char));
            strcpy(new_str_return, str_return);
            int test1 = strlen(new_str_return);
            int test2 = strlen(str_return);
#ifdef SANGDEBUG
        printf("%d, %d, %d, %d, %d\n", prev_length, return_str_size, cat_length, test1, test2);
#endif
            free((void *)str_return);
            str_return = new_str_return;
            prev_length  = return_str_size;
        }
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,element_key);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,key);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,key_keytype);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,strKeyType);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,keytype_type);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,type);
        assert_length(return_str_size, strlen(str_return));
        strcat(str_return,type_value);
        assert_length(return_str_size, strlen(str_return));
        if (iType != LUA_TTABLE && value){
            strcat(str_return,value);
        }
        else
            strcat(str_return, strNull);
        assert_length(return_str_size, strlen(str_return));
        /*   else{
         const char * subTableXML = lua_table_to_xml(lua, -2);
         cat_length += strlen(subTableXML) + strlen(str_return) + 10;
         while ( cat_length >= return_str_size){
         return_str_size *=2;
         }
         if ( prev_length != return_str_size){
         char * new_str_return = (char *)malloc(return_str_size * sizeof(char));
         strcpy(new_str_return, str_return);
         delete[] str_return;
         str_return = new_str_return;
         prev_length  = return_str_size;
         }
         
         strcat(str_return,subTableXML);
         free((void *)subTableXML);
         }*/
        strcat(str_return,value_element);
        assert_length(return_str_size, strlen(str_return));
        
        //"%s(%s) => %s\n", key,lua_typename(lua, lua_type(lua, -2)), value);
        // pop value + copy of key, leaving original key
        lua_pop(lua, 2);
        // stack now contains: -1 => key; -2 => table
    }
    // stack now contains: -1 => table (when lua_next returns 0 it pops the key
    // but does not push anything.)
    strcat(str_return,end);
    assert_length(return_str_size, strlen(str_return));
 //   lua_pop(lua, 1);
  
    lua_pushstring(lua, str_return);
 //   lua_setglobal(lua,"__urMusReturn");

    free( (void *)str_return);
    return 1;
}


//------------------------------------------------------------------------------
// return string via httpServer
//------------------------------------------------------------------------------

int l_NPrint(lua_State* lua)
{
	const char* str = luaL_checkstring(lua,1);
    if(str!=nil)
	{
        lua_pushstring(lua, str);
        lua_setglobal(lua,"__urMusReturn");
    }
	return 0;
}


//------------------------------------------------------------------------------
// Region-related global API
//------------------------------------------------------------------------------

static int l_Region(lua_State *lua)
{
	const char *regiontype = NULL;
	const char *regionName = NULL;
	urAPI_Region_t *parentRegion = NULL;
    
	if(lua_gettop(lua)>0) // Allow for no arg construction
	{
        
		regiontype = luaL_checkstring(lua, 1);
		regionName = luaL_checkstring(lua, 2);
        
		//	urAPI_Region_t *parentRegion = (urAPI_Region_t*)luaL_checkudata(lua, 4, "URAPI.region");
		luaL_checktype(lua, 3, LUA_TTABLE);
		lua_rawgeti(lua, 3, 0);
		parentRegion = (urAPI_Region_t*)lua_touserdata(lua,4);
		luaL_argcheck(lua, parentRegion!= NULL && parentRegion->ud_id==UR_REGION, 3, "'region' expected");
		//	const char *inheritsRegion = luaL_checkstring(lua, 1); //NYI
	}
	else
	{
		parentRegion = UIParent;
	}
    
	lua_newtable(lua);
	luaL_register(lua, NULL, regionfuncs);
	//	urAPI_Region_t *myregion = (urAPI_Region_t*)lua_newuserdata(lua, sizeof(urAPI_Region_t)); // User data is our value
	urAPI_Region_t *myregion = (urAPI_Region_t*)malloc(sizeof(urAPI_Region_t)); // User data is our value
    myregion->ud_id = UR_REGION;
    
	lua_pushlightuserdata(lua, myregion);
    //	luaL_register(lua, NULL, regionmetas);
    //	luaL_openlib(lua, 0, regionmetas, 0);  /* fill metatable */
	lua_rawseti(lua, -2, 0); // Set this to index 0
	myregion->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
	lua_rawgeti(lua, LUA_REGISTRYINDEX, myregion->tableref);
    lua_pushliteral(lua, "__gc");  /* mutex destructor */
    lua_pushcfunction(lua, region_gc);
    lua_rawset(lua, -3);
	
    //	luaL_getmetatable(lua, "URAPI.region");
    //	lua_setmetatable(lua, -2);
	
	
	myregion->next = nil;
	myregion->parent = parentRegion;
	myregion->firstchild = NULL;
	myregion->nextchild = NULL;
    //	addChild(parentRegion, myregion);
	//	myregion->name = regionName; // NYI
	
	// Link it into the global region list
	
	myregion->name = regionName;
	myregion->type = regiontype;
	myregion->ofsx = 0.0;
	myregion->ofsy = 0.0;
	myregion->width = 160.0;
	myregion->height = 160.0;
	myregion->bottom = 1.0;
	myregion->left = 1.0;
	myregion->top = myregion->bottom + myregion->height;
	myregion->right = myregion->left + myregion->width;
	myregion->cx = 80.0;
	myregion->cy = 80.0;
	
	myregion->clipleft = 0.0;
	myregion->clipbottom = 0.0;
	myregion->clipwidth = SCREEN_WIDTH;
	myregion->clipheight = SCREEN_HEIGHT;
	
	myregion->clampleft = 0.0;
	myregion->clampbottom = 0.0;
	myregion->clampwidth = SCREEN_WIDTH;
	myregion->clampheight = SCREEN_HEIGHT;
	
	myregion->alpha = 1.0;
	
	myregion->isMovable = false;
	myregion->isResizable = false;
	myregion->isTouchEnabled = false;
	myregion->isScrollXEnabled = false;
	myregion->isScrollYEnabled = false;
	myregion->isVisible = false;
	myregion->isDragged = false;
	myregion->isClamped = false;
	myregion->isClipping = false;
    
    myregion->numinputs = 0;
	
#ifdef SOAR_SUPPORT
    myregion->soarKernel = NULL;
    myregion->soarAgent = NULL;
    myregion->soarIds = NULL;
    myregion->soarIdCounter = 0;
    myregion->soarWMEs = NULL;
    myregion->soarWMEcounter = 0;
#endif
	
	myregion->entered = false;
	
	myregion->strata = STRATA_PARENT;
    
	for(int i =0; i< MAX_EVENTS; i++)
        myregion->OnEvents[i] = 0;
	
	myregion->texture = NULL;
	myregion->textlabel = NULL;
	
	myregion->point = NULL;
	myregion->relativePoint = NULL;
	myregion->relativeRegion = NULL;
	myregion->page = currentPage;
	
	if(firstRegion[currentPage] == nil) // first region ever
	{
		firstRegion[currentPage] = myregion;
		lastRegion[currentPage] = myregion;
		myregion->next = NULL;
		myregion->prev = NULL;
	}
	else
	{
		myregion->prev = lastRegion[currentPage];
		lastRegion[currentPage]->next = myregion;
		lastRegion[currentPage] = myregion;
		l_setstrataindex(myregion , myregion->strata);
	}
    
	numRegions[currentPage] ++;
    
	setParent(myregion, parentRegion);
	
	return 1;
}

int l_FreeAllRegions(lua_State* lua)
{
    pthread_mutex_lock( &r_mutex );
	urAPI_Region_t* t=lastRegion[currentPage];
	urAPI_Region_t* p;
	
	while(t != nil)
	{
		t->isVisible = false;
		t->isShown = false;
		t->isMovable = false;
		t->isResizable = false;
		t->isTouchEnabled = false;
		t->isScrollXEnabled = false;
		t->isScrollYEnabled = false;
		t->isVisible = false;
		t->isShown = false;
		t->isDragged = false;
		t->isResized = false;
		t->isClamped = false;
		t->isClipping = false;
		
		p=t->prev;
		
		freeRegion(t);
        
		t = p;
	}
    pthread_mutex_unlock( &r_mutex );

	return 0;
}

int l_InputFocus(lua_State* lua)
{
	// NYI
	return 0;
}

int l_HasInput(lua_State* lua)
{
	urAPI_Region_t* t = checkregion(lua, 1);
	bool isover = false;
	
	float x,y;
    
	// NYI
	
	if(x >= t->left && x <= t->left+t->width &&
	   y >= t->bottom && y <= t->bottom+t->height /*&& t->isTouchEnabled*/)
		isover = true;
	lua_pushboolean(lua, isover);
	return 1;
}

extern float cursorpositionx[MAX_FINGERS];
extern float cursorpositiony[MAX_FINGERS];

// UR: New arg "finger" allows to specify which finger to get position for. nil defaults to 0.
int l_InputPosition(lua_State* lua)
{
	int finger = 0;
	if(lua_gettop(lua) > 0 && !lua_isnil(lua, 1))
		finger = luaL_checknumber(lua, 1);
	lua_pushnumber(lua, cursorpositionx[finger]);
	lua_pushnumber(lua, SCREEN_HEIGHT-cursorpositiony[finger]);
	return 2;
}

static int l_NumRegions(lua_State *lua)
{
	lua_pushnumber(lua, numRegions[currentPage]);
	return 1;
}

static int l_EnumerateRegions(lua_State *lua)
{
	urAPI_Region_t* region;
    
    // NYI
    
	if(lua_isnil(lua,1))
	{
		region = UIParent->next;
	}
	else
	{
		region = checkregion(lua,1);
		if(region!=nil)
			region = region->next;
	}
	
	lua_rawgeti(lua,LUA_REGISTRYINDEX, region->tableref);
	
	return 1;
}

//------------------------------------------------------------------------------
// Display-related global API
//------------------------------------------------------------------------------

static int l_setanimspeed(lua_State *lua)
{
	double ds = luaL_checknumber(lua, 1);
	g_glView.animationInterval = ds;
	return 0;
}

int l_ScreenHeight(lua_State* lua)
{
	lua_pushnumber(lua, SCREEN_HEIGHT);
	return 1;
}

int l_ScreenWidth(lua_State* lua)
{
	lua_pushnumber(lua, SCREEN_WIDTH);
	return 1;
}

//------------------------------------------------------------------------------
// System-related global API
//------------------------------------------------------------------------------

int l_Time(lua_State* lua)
{
	lua_pushnumber(lua, systimer->elapsedSec());
	return 1;
}

//------------------------------------------------------------------------------
// Lua-related global API
//------------------------------------------------------------------------------


int l_RunScript(lua_State* lua)
{
	const char* script = luaL_checkstring(lua,1);
	if(script != NULL)
		luaL_dostring(lua,script);
	return 0;
}

//------------------------------------------------------------------------------
// HTTP Server global API
//------------------------------------------------------------------------------

int l_StartHTTPServer(lua_State *lua)
{
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
    {
		documentPath = [paths objectAtIndex:0];
    
	// start off http server
	http_start([resourcePath UTF8String],
			   [documentPath UTF8String]);
    }
    else {
        assert(false); // No document path
    }
	return 0;
}

int l_StopHTTPServer(lua_State *lua)
{
	http_stop();
	return 0;
}

int l_HTTPServer(lua_State *lua)
{
	const char *ip = http_ip_address();
	if (ip) {
		lua_pushstring(lua, ip);
		lua_pushstring(lua, http_ip_port());
		return 2;
	} else {
		return 0;
	}
}

//------------------------------------------------------------------------------
// OSC-related global API
//------------------------------------------------------------------------------

MoNet myoscnet;
MoNet myoscnet2;

void oscCallBack(osc::ReceivedMessageArgumentStream & argument_stream, osc::ReceivedMessage::const_iterator & arg, void * data)
{
    //	float num;
    //	argument_stream >> num;
	callAllOnOSCMessage(argument_stream, arg);
}	

void oscCallBack2(osc::ReceivedMessageArgumentStream & argument_stream, osc::ReceivedMessage::const_iterator & arg, void * data)
{
	const char *str;
    try {
        argument_stream >> str >> osc::EndMessage;
        callAllOnOSCString(str);
    }catch( osc::Exception& e ){
        std::cout << "error while parsing message: "
        << e.what() << "\n";
    }

}

int l_StartOSCListener(lua_State *lua)
{
	myoscnet.addAddressCallback("/urMus/numbers",oscCallBack);
	myoscnet.addAddressCallback("/urMus/text",oscCallBack2);
	myoscnet.startListening();
	lua_pushstring(lua, myoscnet.getMyIPaddress().c_str());
	lua_pushnumber(lua, myoscnet.getListeningPort());
	return 2;
}

int l_StopOSCListener(lua_State *lua)
{
	myoscnet.stopListening();
	return 0;
}

int l_SetOSCPort(lua_State *lua)
{
	int port = luaL_checknumber(lua,1);
	myoscnet.setListeningPort(port);
	return 0;
}

int l_OSCPort(lua_State *lua)
{
	lua_pushnumber(lua, myoscnet.getListeningPort());
	return 1;
}

int l_IPAddress(lua_State *lua)
{
	lua_pushstring(lua, myoscnet.getMyIPaddress().c_str());
	return 1;
}

char types[255];

//#define OSCLEAKDEBUG

int l_SendOSCMessage(lua_State *lua)
{
	const char* ip = luaL_checkstring(lua,1);
	int port = luaL_checknumber(lua,2);
	const char* pattern = luaL_checkstring(lua,3);

#ifndef OSCLEAKDEBUG
    
//    lock_events();
    
	myoscnet2.startSendStream(ip,port);
	myoscnet2.startSendMessage(pattern);
	
	int len = 1;
	while (lua_isnoneornil(lua,len+3)==0)
	{
		if(lua_isnumber(lua, len+3)==1)
		{
			myoscnet2.addSendFloat(luaL_checknumber(lua,len+3));
		}
		else if(lua_isstring(lua, len+3)==1)
		{
			myoscnet2.addSendString(luaL_checkstring(lua,len+3));
		}
		// TODO: handle OSC-blob type, defined in the OSC specs
		len = len +1;
	}
	
	myoscnet2.endSendMessage();
	myoscnet2.closeSendStream();
    
//    unlock_events();
#else
    const char ttttttt[4] ="YAR";
    int len = 1;
    if(lua_isnumber(lua, len+3)==1)
        callAllOnOSCString(ttttttt);
    else if(lua_isstring(lua, len+3)==1)
        callAllOnOSCString(luaL_checkstring(lua,len+3));
#endif
    
	return 0;
}

//------------------------------------------------------------------------------
// ZeroConf-related global API
//------------------------------------------------------------------------------

int l_NetAdvertise(lua_State* lua)
{
	const char* nsid = luaL_checkstring(lua, 1);
	int port = luaL_checknumber(lua, 2);
	
	Net_Advertise(nsid, port);
    return 0;
}

int l_NetFind(lua_State* lua)
{
	const char* nsid = luaL_checkstring(lua, 1);
	
	Net_Find(nsid);
    return 0;
}

int l_StopNetAdvertise(lua_State* lua)
{
	const char* nsid = luaL_checkstring(lua, 1);
	Stop_Net_Advertise(nsid);
    return 0;
}

int l_StopNetFind(lua_State* lua)
{
	const char* nsid = luaL_checkstring(lua, 1);
	Stop_Net_Find(nsid);
    return 0;
}

//------------------------------------------------------------------------------
// Audio-related global API
//------------------------------------------------------------------------------

static int audio_initialized = false;

int l_StartAudio(lua_State* lua)
{
	if(!audio_initialized)
	{
#ifdef USEMUMOAUDIO
        MoAudio::init(SRATE, FRAMESIZE, NUMCHANNELS);
        MoAudio::start( audioCallback, NULL);
#else        
		initializeRIOAudioLayer();
#endif
	}
	else
#ifdef USEMUMOAUDIO
        MoAudio::start( audioCallback, NULL);
#else
    playRIOAudioLayer();
#endif
	return 0;
}

int l_PauseAudio(lua_State* lua)
{
#ifdef USEMUMOAUDIO
    MoAudio::stop();
#else
	stopRIOAudioLayer();
#endif
	return 0;
}

//------------------------------------------------------------------------------
// SOAR-related global API
//------------------------------------------------------------------------------

int l_SoarEnabled(lua_State* lua)
{
	bool soar_enabled = false;
	
#ifdef SOAR_SUPPORT
	soar_enabled = true;
#endif
	
	lua_pushboolean(lua, soar_enabled);
	return 1;
}

//------------------------------------------------------------------------------
// Flowbox-related global API
//------------------------------------------------------------------------------

void FreeAllFlowboxes(int patch)
{
	for(int i=0; i < numFlowBoxes[patch]; i++)
	{
        assert(firstFlowbox[patch][i]);
        removeFlowboxLinks(firstFlowbox[patch][i]);
        if(!firstFlowbox[patch][i]->object->noninstantiable) // instanced
            delete firstFlowbox[patch][i]->object;
        else
        {
            int a=0;
        }
		free(firstFlowbox[patch][i]);
        firstFlowbox[patch][i] = NULL;
 
	}
	free(firstFlowbox[patch]);
	firstFlowbox[patch] = NULL;
	numFlowBoxes[patch] = 0;
   
}

#ifdef FLAGBASEDCAFB
bool fb_clearing = false;
#endif

static int l_FreeAllFlowboxes(lua_State* lua)
{
#ifndef FLAGBASEDCAFB
    pthread_mutex_lock( &fb_mutex );
    FreeAllFlowboxes(currentPatch);
    pthread_mutex_unlock( &fb_mutex );
#else
    pthread_mutex_lock( &fb_mutex );
    fb_clearing = true;
    pthread_mutex_unlock( &fb_mutex );
#endif
#ifdef RELOCATE_FAFB
    if(freePatches[currentPatch]==0)
        freePatches[currentPatch]=1;
#else
#endif   
	return 0;
}
	
// Service function to add in and out port userdata to a flowbox.
// This function requires that the table of the flowbox is on top of the stack!!
static void populateFlowboxPorts(ursAPI_FlowBox_t *myflowbox)
{
    for(int i=0; i< myflowbox->object->nr_ins; i++)
    {
        lua_pushstring(lua, myflowbox->object->ins[i].name);
        lua_newtable(lua);
        luaL_register(lua, NULL, flowboxinfuncs);
        ursAPI_FlowBox_Port_t *myflowboxport = (ursAPI_FlowBox_Port_t*)malloc(sizeof(ursAPI_FlowBox_Port_t)); // User data is our value
        myflowboxport->ud_id = UR_FLOWBOXPORT;

        lua_pushlightuserdata(lua, myflowboxport);
        lua_rawseti(lua, -2, 0); // Set this to index 0
        myflowboxport->tableref = myflowbox->tableref;
        myflowboxport->index = i;
        myflowboxport->object = myflowbox->object;
        lua_settable(lua, -3);
    }
    for(int i=0; i< myflowbox->object->nr_outs; i++)
    {
        lua_pushstring(lua, myflowbox->object->outs[i].name);
        lua_newtable(lua);
        luaL_register(lua, NULL, flowboxoutfuncs);
        ursAPI_FlowBox_Port_t *myflowboxport = (ursAPI_FlowBox_Port_t*)malloc(sizeof(ursAPI_FlowBox_Port_t)); // User data is our value
        myflowboxport->ud_id = UR_FLOWBOXPORT;

        lua_pushlightuserdata(lua, myflowboxport);
        lua_rawseti(lua, -2, 0); // Set this to index 0
        myflowboxport->tableref = myflowbox->tableref;
        myflowboxport->index = i;
        myflowboxport->object = myflowbox->object;
        lua_settable(lua, -3);
    }
}

static int l_FlowBox(lua_State* lua)
{
	int idx = 1;
	if(lua_gettop(lua)>1) // Allow for no arg construction
	{
//		const char *flowboxtype = luaL_checkstring(lua, 1);
//		const char *flowboxName = luaL_checkstring(lua, 2);
		idx = 3;
		// Backward compatibility
	}
	luaL_checktype(lua, idx, LUA_TTABLE);

    pthread_mutex_lock( &fb_mutex );
    
	//	urAPI_flowbox_t *parentflowbox = (urAPI_flowbox_t*)luaL_checkudata(lua, 4, "URAPI.flowbox");
	lua_rawgeti(lua, idx, 0);
	ursAPI_FlowBox_t *parentFlowBox = (ursAPI_FlowBox_t*)lua_touserdata(lua,idx+1);
	luaL_argcheck(lua, parentFlowBox!= NULL  && parentFlowBox->ud_id == UR_FLOWBOX, idx+1, "'flowbox' expected");
	//	const char *inheritsflowbox = luaL_checkstring(lua, 1); //NYI

	lua_newtable(lua);
	luaL_register(lua, NULL, flowboxfuncs);
	ursAPI_FlowBox_t *myflowbox = (ursAPI_FlowBox_t*)malloc(sizeof(ursAPI_FlowBox_t)); // User data is our value
    myflowbox->ud_id = UR_FLOWBOX;

	lua_pushlightuserdata(lua, myflowbox);
	lua_rawseti(lua, -2, 0); // Set this to index 0
	myflowbox->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
	lua_rawgeti(lua, LUA_REGISTRYINDEX, myflowbox->tableref);

	myflowbox->object = parentFlowBox->object->Clone();

    populateFlowboxPorts(myflowbox);
    
	if(myflowbox->object != parentFlowBox->object) // instanced
        addToPatch(myflowbox);
//	myflowbox->object->instancenumber = parentFlowBox->object->instancenumber + 1;
	
	//	luaL_getmetatable(lua, "URAPI.flowbox");
	//	lua_setmetatable(lua, -2);
    pthread_mutex_unlock( &fb_mutex );

	return 1;

}

void ur_GetSoundBuffer(SInt32* buffer, int channel, int size)
{
	lua_getglobal(lua,"urSoundData");
	lua_rawgeti(lua, -1, channel);
	if(lua_isnil(lua, -1) || !lua_istable(lua,-1)) // Channel doesn't exist or is falsely set up
	{
		lua_pop(lua,1);
		return;
	}
	
	for(int i=0; i<size; i++)
	{
		lua_rawgeti(lua, -1, i+1);
		if(lua_isnumber(lua, -1))
			buffer[i] = lua_tonumber(lua, -1);
		
		lua_pop(lua,1);	
	}
	
	lua_pop(lua, 2);
}



int l_SourceNames(lua_State *lua)
{
	int nr = urs_NumUrSourceObjects();
	for(int i=0; i<nr; i++)
	{
		lua_pushstring(lua, urs_GetSourceObjectName(i));
	}
	return nr;	
}

int l_ManipulatorNames(lua_State *lua)
{
	int nr = urs_NumUrManipulatorObjects();
	for(int i=0; i<nr; i++)
	{
		lua_pushstring(lua, urs_GetManipulatorObjectName(i));
	}
	return nr;
}

int l_SinkNames(lua_State *lua)
{
	int nr = urs_NumUrSinkObjects();
	for(int i=0; i<nr; i++)
	{
		lua_pushstring(lua, urs_GetSinkObjectName(i));
	}
	return nr;
}

#ifdef ALLOW_DEFUNCT
int l_NumUrIns(lua_State *lua)
{
	const char* obj = luaL_checkstring(lua, 1);
	int nr = urs_NumUrManipulatorObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetManipulatorObjectName(i)))
		{
			lua_pushnumber(lua, urs_NumUrManipulatorIns(i));
			return 1;
		}
	}	
	nr = urs_NumUrSinkObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetSinkObjectName(i)))
		{
			lua_pushnumber(lua, urs_NumUrSinkIns(i));
			return 1;
		}
	}
	return 0;
}

int l_NumUrOuts(lua_State *lua)
{
	const char* obj = luaL_checkstring(lua, 1);
	int nr = urs_NumUrSourceObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetSourceObjectName(i)))
		{
			lua_pushnumber(lua, urs_NumUrSourceOuts(i));
			return 1;
		}
	}	
	nr = urs_NumUrManipulatorObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetManipulatorObjectName(i)))
		{
			lua_pushnumber(lua, urs_NumUrManipulatorOuts(i));
			return 1;
		}
	}
	return 0;
}

int l_GetUrIns(lua_State *lua)
{
	const char* obj = luaL_checkstring(lua, 1);
	int nr = urs_NumUrManipulatorObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetManipulatorObjectName(i)))
		{
			int nrins = urs_NumUrManipulatorIns(i);
			for(int j=0; j< nrins; j++)
				lua_pushstring(lua, urs_GetManipulatorIn(i, j));
			return nrins;
		}
	}	
	nr = urs_NumUrSinkObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetSinkObjectName(i)))
		{
			int nrins = urs_NumUrSinkIns(i);
			for(int j=0; j< nrins; j++)
				lua_pushstring(lua, urs_GetSinkIn(i, j));
			return nrins;
		}
	}
	return 0;
}

int l_GetUrOuts(lua_State *lua)
{
	const char* obj = luaL_checkstring(lua, 1);
	int nr = urs_NumUrSourceObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetSourceObjectName(i)))
		{
			int nrouts = urs_NumUrSourceOuts(i);
			for(int j=0; j< nrouts; j++)
				lua_pushstring(lua, urs_GetSourceOut(i, j));
			return nrouts;
		}
	}
	nr = urs_NumUrManipulatorObjects();
	for(int i=0; i<nr; i++)
	{
		if(!strcmp(obj, urs_GetManipulatorObjectName(i)))
		{
			int nrouts = urs_NumUrManipulatorOuts(i);
			for(int j=0; j< nrouts; j++)
				lua_pushstring(lua, urs_GetManipulatorOut(i, j));
			return nrouts;
		}
	}	
	return 0;
}
#endif

//------------------------------------------------------------------------------
// Path-related global API
//------------------------------------------------------------------------------

const char* getSystemPath()
{
	NSString *filename2 = @"";
	NSString *filePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:filename2];
    return [filePath UTF8String];
}

const char* getDocumentPath()
{
	NSString *filename2 = @"";
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if ([paths count] > 0) {
		NSString *filePath = [paths objectAtIndex:0];
		NSString *resultPath = [NSString stringWithFormat:@"%@/%@", filePath, filename2];
		return [resultPath UTF8String];
	}
	else
	{
        assert(0); // This should never happen!
        return NULL;
	}
}

int l_SystemPath(lua_State *lua)
{
	const char* filename = luaL_checkstring(lua,1);
	NSString *filename2 = [[NSString alloc] initWithUTF8String:filename]; 
	NSString *filePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:filename2];
	const char* filestr = [filePath UTF8String];
	lua_pushstring(lua, filestr);
//    [filename2 release]; LEAK
	return 1;
}

int l_DocumentPath(lua_State *lua)
{
	const char* filename = luaL_checkstring(lua,1);
	NSString *filename2 = [[NSString alloc] initWithUTF8String:filename]; 
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if ([paths count] > 0) {
		NSString *filePath = [paths objectAtIndex:0];
		NSString *resultPath = [NSString stringWithFormat:@"%@/%@", filePath, filename2];
		const char* filestr = [resultPath UTF8String];
		lua_pushstring(lua, filestr);
	}
	else
	{
		luaL_error(lua, "Cannot find the Document path.");
	}
//    [filename2 release]; LEAK
	return 1;
}

//------------------------------------------------------------------------------
// Paging-related global API
//------------------------------------------------------------------------------

int l_NumMaxPages(lua_State *lua)
{
	int max = MAX_PAGES;
	lua_pushnumber(lua, max);
	return 1;
}

int l_Page(lua_State *lua)
{
	lua_pushnumber(lua, currentPage+1);
	return 1;
}

int l_SetPage(lua_State *lua)
{
	int oldcurrent;
	int num = luaL_checknumber(lua,1);
	if(num >= 1 and num <= MAX_PAGES)
	{
		callAllOnPageLeft(num-1);
		oldcurrent = currentPage;
        FreeAllChains();
//        FreeCameraChain();
        decCameraUseBy(page_camerause);
        page_camerause = 0;
		currentPage = num-1;
        if(linkExternal)
            currentExternalPage = currentPage;

//        OnUpdateRegions = FreeChain(OnUpdateRegions);
//        OnUpdateRegions = PopulateChain(OnUpdateRegions, firstRegion[currentPage]);
        PopulateAllChains(firstRegion[currentPage]);
		callAllOnPageEntered(oldcurrent);
	}
	else
	{
		// Error!!
		luaL_error(lua, "Invalid page number: %d",num);
	}
	return 0;
}

//------------------------------------------------------------------------------
// External Display-related global API
//------------------------------------------------------------------------------

int l_DisplayExternalPage(lua_State *lua)
{
 	int num = luaL_checknumber(lua,1);
    currentExternalPage = num-1;
    linkExternal = false;
    return 0;
}       

int l_LinkExternalDisplay(lua_State *lua)
{
    bool link = lua_toboolean(lua,1);
    linkExternal = link;
    
    if(link) 
        currentExternalPage = currentPage;
    return 0;
}

int l_SetExternalOrientation(lua_State *lua)
{
    int num = luaL_checknumber(lua,1);
    if(num == 0)
    {
        [[UIApplication sharedApplication] setStatusBarOrientation:UIDeviceOrientationPortrait animated:NO];
        TOUCH_SCREEN_WIDTH = SCREEN_WIDTH;
        TOUCH_SCREEN_HEIGHT = SCREEN_HEIGHT;
        portrait = true;
    }
    else if(num == 1)
    {
        [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationPortraitUpsideDown animated:NO];
        TOUCH_SCREEN_WIDTH = SCREEN_WIDTH;
        TOUCH_SCREEN_HEIGHT = SCREEN_HEIGHT;
        portrait = true;
   }
    else if(num == 2)
    {
        [[UIApplication sharedApplication] setStatusBarOrientation:UIDeviceOrientationLandscapeLeft animated:NO];
        portrait = false;
 //       TOUCH_SCREEN_WIDTH = SCREEN_HEIGHT;
 //       TOUCH_SCREEN_HEIGHT = SCREEN_WIDTH;
    }
    else
    {
        [[UIApplication sharedApplication] setStatusBarOrientation:UIDeviceOrientationLandscapeRight animated:NO];
        portrait = false;
//        TOUCH_SCREEN_WIDTH = SCREEN_HEIGHT;
//        TOUCH_SCREEN_HEIGHT = SCREEN_WIDTH;
    }
    return 0;
}
    	
//------------------------------------------------------------------------------
// Camera-related global API
//------------------------------------------------------------------------------

int currentCamera = 1;
	
int l_SetActiveCamera(lua_State *lua)
{
	int cam = luaL_checknumber(lua,1);
	
	if(currentCamera != cam)
	{
#ifdef GPUIMAGE
        [g_glView->videoCamera rotateCamera];
#else
		[g_glView->captureManager toggleCameraSelection];
#endif
        currentCamera = cam;
	}
	
	return 0;
}
	
int l_ActiveCamera(lua_State *lua)
{
	lua_pushnumber(lua, currentCamera);
	return 1;
}

int l_SetCameraAutoBalance(lua_State *lua)
{
    bool ab = lua_toboolean(lua,1);
    int toggle = ab ? 1 : 0;
    
    [g_glView->captureManager autoWhiteBalanceAndExposure:toggle];
    
    return 0;
}
    
int l_SetTorchFlashFrequency(lua_State *lua)
{
	double freq = luaL_checknumber(lua,1);
	[g_glView->captureManager setTorchToggleFrequency:freq];

	return 0;
}

int l_SetTorch(lua_State *lua)
{
    bool ab = lua_toboolean(lua,1);
    int toggle = ab ? 1 : 0;
    
    AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    NSError *error;
    if([device isTorchModeSupported:AVCaptureTorchModeOn] && toggle) {
        
        if ([device lockForConfiguration:&error]) {
            [device setTorchMode:AVCaptureTorchModeOn];
            [device unlockForConfiguration];
        }
        
    } else if ([device isTorchModeSupported:AVCaptureTorchModeOff] && !toggle) {
        
        if ([device lockForConfiguration:&error]) {
            [device setTorchMode:AVCaptureTorchModeOff];
            [device unlockForConfiguration];
        }
    }
    return 0;
}

int l_CameraFilters(lua_State *lua)
{
#ifdef GPUIMAGE
    for(int i=0; i<maxFilterMode-1; i++)
    {
        lua_pushstring(lua,urFilterModeNames[i]);
    }
    return maxFilterMode-1;
#else
    return 0;
#endif
}

int l_SetCameraFilter(lua_State *lua)
{
	const char* filtermode = luaL_checkstring(lua, 1);
#ifdef GPUIMAGE
    for(int i=0; i<maxFilterMode-1; i++)
    {
        if(!strcmp(filtermode,urFilterModeNames[i]))
        {
            [g_glView setCameraFilter:(GPUImageFilterType)i];
            return 0;
        }
    }
#endif
    luaL_error(lua, "Unknown camera filter mode: %s", filtermode);
    return 0;
}

int l_SetCameraFilterParameter(lua_State *lua)
{
	double value = luaL_checknumber(lua,1);
#ifdef GPUIMAGE
    [g_glView setCameraFilterParameter:value];
#endif
    return 0;
}
//------------------------------------------------------------------------------
// Media Writing-related global API
//------------------------------------------------------------------------------

int l_WriteScreenshot(lua_State *lua)
{
	const char *infile = luaL_checkstring(lua,1);
//	UIImage* img = [g_glView saveImageFromGLView];
//	[g_glView saveImageToFile:img filename:infile];
//	[img release];
	[g_glView saveScreenToFile:infile];
	return 0;
}
	
int l_StartMovieMaking(lua_State *lua)
{
	const char *infile = luaL_checkstring(lua,1);
	[g_glView startMovieWriter:infile];
	return 0;
}

int l_AddScreenshot(lua_State *lua)
{
	double elapsed = luaL_checknumber(lua,1);
	[g_glView writeScreenshotToMovie:elapsed];
	return 0;
}

int l_FinishMovieMaking(lua_State *lua)
{
	[g_glView closeMovieWriter];
	return 0;
}

#ifdef GPUIMAGE
extern GLuint bgname;
#endif

int l_SaveMovie(lua_State *lua)
{
	const char* filename = luaL_checkstring(lua, 1);
#ifdef GPUIMAGE
    float cropleft = 0.0;
    float cropbottom = 0.0;
    float cropright = 1.0;
    float croptop = 1.0;
    
    if(lua_gettop(lua)==5)
    {
        cropleft = luaL_checknumber(lua, 2)/SCREEN_WIDTH;
        cropbottom = luaL_checknumber(lua, 3)/SCREEN_HEIGHT;
        cropright = luaL_checknumber(lua, 4)/SCREEN_WIDTH;
        croptop = luaL_checknumber(lua, 5)/SCREEN_HEIGHT;
    }
    
    NSString* filename2 = [NSString stringWithUTF8String:filename];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if ([paths count] > 0) {
		NSString *filePath = [paths objectAtIndex:0];
		NSString *resultPath = [NSString stringWithFormat:@"%@/%@", filePath, filename2];
        
        [g_glView writeMovie:resultPath ofSize:CGSizeMake(SCREEN_WIDTH, SCREEN_HEIGHT) withCrop:CGRectMake(cropleft,cropbottom,cropright-cropleft,croptop-cropbottom) fromTexture:bgname];
    }
#endif
    return 0;
}

int l_FinishMovie(lua_State *lua)
{
#ifdef GPUIMAGE
    [g_glView finishMovie];
#endif
    return 0;
}

	
//------------------------------------------------------------------------------
// URL (CURLY)-related global API
//------------------------------------------------------------------------------

#ifdef CURLY    
int l_WriteURLData(lua_State *lua)
{
    const char *inurl = luaL_checkstring(lua,1);
	const char *outfile = luaL_checkstring(lua,2);
    
    CURL *curl;
    CURLcode res;
    char bodyfilename[255];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
    
	if ([paths count] > 0)
    {
		documentPath = [paths objectAtIndex:0];
        strcpy(bodyfilename,[documentPath UTF8String]);
        strcat(bodyfilename,"/");
        strcat(bodyfilename, outfile);
        FILE *bodyfile;
        
        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL,inurl);
            /* no progress meter please */ 
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            /* send all data to this function  */ 
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            /* open the files */ 
            bodyfile = fopen(bodyfilename,"w");
            if (bodyfile != NULL) {
                curl_easy_setopt(curl,   CURLOPT_WRITEDATA, bodyfile);
                res = curl_easy_perform(curl);
            }
            
            /* always cleanup */ 
            curl_easy_cleanup(curl);
        }
    }
    return 0;
}

int l_PutURLData(lua_State *lua)
{
    const char *inurl = luaL_checkstring(lua,1);
	const char *outfile = luaL_checkstring(lua,2);
    
    CURL *curl;
    CURLcode res;
    char bodyfilename[255];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
    
	if ([paths count] > 0)
    {
		documentPath = [paths objectAtIndex:0];
        strcpy(bodyfilename,[documentPath UTF8String]);
        strcat(bodyfilename,"/");
        strcat(bodyfilename, outfile);
        FILE *bodyfile;
        int hd ;
        struct stat file_info;

        /* get the file size of the local file */ 
        hd = open(bodyfilename, O_RDONLY) ;
        fstat(hd, &file_info);
        close(hd) ;
        
        /* get a FILE * of the same file, could also be made with
         fdopen() from the previous descriptor, but hey this is just
         an example! */ 
        bodyfile = fopen(bodyfilename, "rb");
        
        /* In windows, this will init the winsock stuff */ 
        curl_global_init(CURL_GLOBAL_ALL);
        
        /* get a curl handle */ 
        curl = curl_easy_init();
        if(curl) {
            /* we want to use our own read function */ 
//            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
            
            /* enable uploading */ 
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            
            /* HTTP PUT please */ 
            curl_easy_setopt(curl, CURLOPT_PUT, 1L);
            
            /* specify target URL, and note that this URL should include a file
             name, not only a directory */ 
            curl_easy_setopt(curl, CURLOPT_URL, inurl);
            
            /* now specify which file to upload */ 
            curl_easy_setopt(curl, CURLOPT_READDATA, bodyfile);
            
            /* provide the size of the upload, we specicially typecast the value
             to curl_off_t since we must be sure to use the correct data size */ 
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                             (curl_off_t)file_info.st_size);
            
            /* Now run off and do what you've been told! */ 
            res = curl_easy_perform(curl);
            
            /* always cleanup */ 
            curl_easy_cleanup(curl);
        }
        fclose(bodyfile); /* close the local file */ 
        
//        curl_global_cleanup();
    }
        
    return 0;
}
#endif

//------------------------------------------------------------------------------
// Register our API
//------------------------------------------------------------------------------

void l_setupAPI(lua_State *lua)
{
	CGRect screendimensions;
	screendimensions = [[UIScreen mainScreen] bounds];
	SCREEN_WIDTH = screendimensions.size.width;
	SCREEN_HEIGHT = screendimensions.size.height;
	// Set global userdata
	// Create UIParent
//	luaL_newmetatable(lua, "URAPI.region");
//	lua_pushstring(lua, "__index");
//	lua_pushvalue(lua, -2);
//	lua_settable(lua, -3);
//	luaL_openlib(lua, NULL, regionfuncs, 0);
	lua_newtable(lua);
	luaL_register(lua, NULL, regionfuncs);
//	urAPI_Region_t *myregion = (urAPI_Region_t*)lua_newuserdata(lua, sizeof(urAPI_Region_t)); // User data is our value
	urAPI_Region_t *myregion = (urAPI_Region_t*)malloc(sizeof(urAPI_Region_t)); // User data is our value
    myregion->ud_id = UR_REGION;
	lua_pushlightuserdata(lua, myregion);
	lua_rawseti(lua, -2, 0); // Set this to index 0
	myregion->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
	lua_rawgeti(lua, LUA_REGISTRYINDEX, myregion->tableref);
//	luaL_getmetatable(lua, "URAPI.region");
//	lua_setmetatable(lua, -2);
	myregion->strata = STRATA_BACKGROUND;
	myregion->parent = NULL;
	myregion->top = SCREEN_HEIGHT;
	myregion->bottom = 0;
	myregion->left = 0;
	myregion->right = SCREEN_WIDTH;
	myregion->cx = SCREEN_WIDTH/2;
	myregion->cy = SCREEN_HEIGHT/2;
	myregion->firstchild = NULL;
	myregion->point = NULL;
	myregion->relativePoint = NULL;
    myregion->next = nil;
	myregion->parent = NULL;
	myregion->firstchild = NULL;
	myregion->nextchild = NULL;
	UIParent = myregion;
	lua_setglobal(lua, "UIParent");

	urs_SetupObjects();

	char fbname[255];
	for(int source=0; source<ursourceobjectlist.Last(); source++)
	{
		lua_newtable(lua);
		luaL_register(lua, NULL, flowboxfuncs);
		ursAPI_FlowBox_t *myflowbox = (ursAPI_FlowBox_t*)malloc(sizeof(ursAPI_FlowBox_t)); // User data is our value
        myflowbox->ud_id = UR_FLOWBOX;

		lua_pushlightuserdata(lua, myflowbox);
		lua_rawseti(lua, -2, 0); // Set this to index 0
		myflowbox->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
		lua_rawgeti(lua, LUA_REGISTRYINDEX, myflowbox->tableref);
		//	luaL_getmetatable(lua, "URAPI.region");
		//	lua_setmetatable(lua, -2);
		myflowbox->object = ursourceobjectlist[source];
        populateFlowboxPorts(myflowbox);
        //		FBNope = myflowbox;
		strcpy(fbname, "FB");
		strcat(fbname, myflowbox->object->name);
		lua_setglobal(lua, fbname);
        //        lua_getglobal(lua, fbname);
	}
	for(int manipulator=0; manipulator<urmanipulatorobjectlist.Last(); manipulator++)
	{
		lua_newtable(lua);
		luaL_register(lua, NULL, flowboxfuncs);
		ursAPI_FlowBox_t *myflowbox = (ursAPI_FlowBox_t*)malloc(sizeof(ursAPI_FlowBox_t)); // User data is our value
        myflowbox->ud_id = UR_FLOWBOX;

		lua_pushlightuserdata(lua, myflowbox);
		lua_rawseti(lua, -2, 0); // Set this to index 0
		myflowbox->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
		lua_rawgeti(lua, LUA_REGISTRYINDEX, myflowbox->tableref);
		//	luaL_getmetatable(lua, "URAPI.region");
		//	lua_setmetatable(lua, -2);
		myflowbox->object = urmanipulatorobjectlist[manipulator];
        populateFlowboxPorts(myflowbox);
        //		FBNope = myflowbox;
		strcpy(fbname, "FB");
		strcat(fbname, myflowbox->object->name);
		lua_setglobal(lua, fbname);
        //        lua_getglobal(lua, fbname);
	}
	for(int sink=0; sink<ursinkobjectlist.Last(); sink++)
	{
		lua_newtable(lua);
		luaL_register(lua, NULL, flowboxfuncs);
		ursAPI_FlowBox_t *myflowbox = (ursAPI_FlowBox_t*)malloc(sizeof(ursAPI_FlowBox_t)); // User data is our value
        myflowbox->ud_id = UR_FLOWBOX;

		lua_pushlightuserdata(lua, myflowbox);
		lua_rawseti(lua, -2, 0); // Set this to index 0
		myflowbox->tableref = luaL_ref(lua, LUA_REGISTRYINDEX);
		lua_rawgeti(lua, LUA_REGISTRYINDEX, myflowbox->tableref);
		//	luaL_getmetatable(lua, "URAPI.region");
		//	lua_setmetatable(lua, -2);
		myflowbox->object = ursinkobjectlist[sink];
        populateFlowboxPorts(myflowbox);
        //		FBNope = myflowbox;
		strcpy(fbname, "FB");
		strcat(fbname, myflowbox->object->name);
		lua_setglobal(lua, fbname);
        //        lua_getglobal(lua, fbname);
	}
	
	luaL_newmetatable(lua, "URAPI.texture");
	lua_pushstring(lua, "__index");
	lua_pushvalue(lua, -2);
	lua_settable(lua, -3);
//	luaL_openlib(lua, NULL, texturefuncs, 0);
	luaL_register(lua, NULL, texturefuncs);
	
	luaL_newmetatable(lua, "URAPI.textlabel");
	lua_pushstring(lua, "__index");
	lua_pushvalue(lua, -2);
	lua_settable(lua, -3);
//	luaL_openlib(lua, NULL, textlabelfuncs, 0);
	luaL_register(lua, NULL, textlabelfuncs);
	
	// Soar!
	lua_pushcfunction(lua, l_SoarEnabled);
	lua_setglobal(lua, "SoarEnabled");
	
	// Compats
	lua_pushcfunction(lua, l_Region);
	lua_setglobal(lua, "Region");
	lua_pushcfunction(lua, l_NumRegions);
	lua_setglobal(lua, "NumRegions");
	lua_pushcfunction(lua, l_InputFocus);
	lua_setglobal(lua, "InputFocus");
	lua_pushcfunction(lua, l_HasInput);
	lua_setglobal(lua, "HasInput");
	lua_pushcfunction(lua, l_InputPosition);
	lua_setglobal(lua, "InputPosition");
	lua_pushcfunction(lua, l_ScreenHeight);
	lua_setglobal(lua, "ScreenHeight");
	lua_pushcfunction(lua, l_ScreenWidth);
	lua_setglobal(lua, "ScreenWidth");
	lua_pushcfunction(lua, l_Time);
	lua_setglobal(lua, "Time");
	lua_pushcfunction(lua, l_RunScript);
	lua_setglobal(lua, "RunScript");
	lua_pushcfunction(lua,l_StartAudio);
	lua_setglobal(lua,"StartAudio");
	lua_pushcfunction(lua,l_PauseAudio);
	lua_setglobal(lua,"PauseAudio");
	
	// HTTP
	lua_pushcfunction(lua,l_StartHTTPServer);
	lua_setglobal(lua,"StartHTTPServer");
	lua_pushcfunction(lua,l_StopHTTPServer);
	lua_setglobal(lua,"StopHTTPServer");
	lua_pushcfunction(lua,l_HTTPServer);
	lua_setglobal(lua,"HTTPServer");

	// OSC
	lua_pushcfunction(lua, l_StartOSCListener);
	lua_setglobal(lua,"StartOSCListener");
	lua_pushcfunction(lua, l_StopOSCListener);
	lua_setglobal(lua,"StopOSCListener");
	lua_pushcfunction(lua, l_SetOSCPort);
	lua_setglobal(lua,"SetOSCPort");
	lua_pushcfunction(lua, l_OSCPort);
	lua_setglobal(lua,"OSCPort");
	lua_pushcfunction(lua, l_IPAddress);
	lua_setglobal(lua,"IPAddress");
	lua_pushcfunction(lua, l_SendOSCMessage);
	lua_setglobal(lua,"SendOSCMessage");

	// Bonjour Net discovery
	lua_pushcfunction(lua, l_NetAdvertise);
	lua_setglobal(lua,"StartNetAdvertise");
	lua_pushcfunction(lua, l_NetFind);
	lua_setglobal(lua,"StartNetDiscovery");
	lua_pushcfunction(lua, l_StopNetAdvertise);
	lua_setglobal(lua,"StopNetAdvertise");
	lua_pushcfunction(lua, l_StopNetFind);
	lua_setglobal(lua,"StopNetDiscovery");
	
	// UR!
	lua_pushcfunction(lua, l_setanimspeed);
	lua_setglobal(lua, "SetFrameRate");
	lua_pushcfunction(lua, l_DPrint);
	lua_setglobal(lua, "DPrint");
    lua_pushcfunction(lua,l_TableToXml);
	lua_setglobal(lua, "TableToXml");
    lua_pushcfunction(lua,l_ElemToXml );
	lua_setglobal(lua, "ElemToXml");
    lua_pushcfunction(lua,l_NPrint );
	lua_setglobal(lua, "NPrint");

	// URSound!
	lua_pushcfunction(lua, l_SourceNames);
	lua_setglobal(lua, "SourceNames");
	lua_pushcfunction(lua, l_ManipulatorNames);
	lua_setglobal(lua, "ManipulatorNames");
	lua_pushcfunction(lua, l_SinkNames);
	lua_setglobal(lua, "SinkNames");
#ifdef ALLOW_DEFUNCT
	lua_pushcfunction(lua, l_NumUrIns);
	lua_setglobal(lua, "NumUrIns");
	lua_pushcfunction(lua, l_NumUrOuts);
	lua_setglobal(lua, "NumUrOuts");
	lua_pushcfunction(lua, l_GetUrIns);
	lua_setglobal(lua, "GetUrIns");
	lua_pushcfunction(lua, l_GetUrOuts);
	lua_setglobal(lua, "GetUrOuts");
#endif
	lua_pushcfunction(lua, l_FlowBox);
	lua_setglobal(lua, "FlowBox");
	
	lua_pushcfunction(lua, l_SystemPath);
	lua_setglobal(lua, "SystemPath");
	lua_pushcfunction(lua, l_DocumentPath);
	lua_setglobal(lua, "DocumentPath");

	lua_pushcfunction(lua, l_NumMaxPages);
	lua_setglobal(lua, "NumMaxPages");
	lua_pushcfunction(lua, l_Page);
	lua_setglobal(lua, "Page");
	lua_pushcfunction(lua, l_SetPage);
	lua_setglobal(lua, "SetPage");
	lua_pushcfunction(lua, l_DisplayExternalPage);
	lua_setglobal(lua, "DisplayExternalPage");
	lua_pushcfunction(lua, l_LinkExternalDisplay);
	lua_setglobal(lua, "LinkExternalDisplay");
    lua_pushcfunction(lua, l_SetExternalOrientation);
    lua_setglobal(lua, "SetExternalOrientation");
	
#ifdef SOAR_SUPPORT_OLD
	lua_pushcfunction(lua, l_SoarCreateID);
	lua_setglobal(lua,"SoarCreateID");
	lua_pushcfunction(lua, l_SoarDelete);
	lua_setglobal(lua,"SoarDelete");
	lua_pushcfunction(lua, l_SoarCreateConstant);
	lua_setglobal(lua,"SoarCreateConstant");
	lua_pushcfunction(lua, l_SoarLoadRules);
	lua_setglobal(lua,"SoarLoadRules");
#endif	
	
	lua_pushcfunction(lua, l_SetActiveCamera);
	lua_setglobal(lua, "SetActiveCamera");
	lua_pushcfunction(lua, l_ActiveCamera);
	lua_setglobal(lua, "ActiveCamera");
	lua_pushcfunction(lua, l_SetTorchFlashFrequency);
	lua_setglobal(lua, "SetTorchFlashFrequency");
   lua_pushcfunction(lua, l_SetTorch);
   lua_setglobal(lua, "SetTorch");
  
	lua_pushcfunction(lua, l_SetCameraAutoBalance);
    lua_setglobal(lua, "SetCameraAutoBalance");
    lua_pushcfunction(lua, l_CameraFilters);
    lua_setglobal(lua, "CameraFilters");
    lua_pushcfunction(lua, l_SetCameraFilter);
    lua_setglobal(lua, "SetCameraFilter");
    lua_pushcfunction(lua, l_SetCameraFilterParameter);
    lua_setglobal(lua, "SetCameraFilterParameter");
    
	lua_pushcfunction(lua, l_WriteScreenshot);
	lua_setglobal(lua, "WriteScreenshot");
	
	lua_pushcfunction(lua, l_StartMovieMaking);
	lua_setglobal(lua, "StartMovieMaking");
	lua_pushcfunction(lua, l_AddScreenshot);
	lua_setglobal(lua, "AddScreenshot");
	lua_pushcfunction(lua, l_FinishMovieMaking);
	lua_setglobal(lua, "FinishMovieMaking");
    
    lua_pushcfunction(lua, l_SaveMovie);
    lua_setglobal(lua, "WriteMovie");
    lua_pushcfunction(lua, l_FinishMovie);
    lua_setglobal(lua, "FinishMovie");
	
	lua_pushcfunction(lua, l_FreeAllRegions);
	lua_setglobal(lua, "FreeAllRegions");
	lua_pushcfunction(lua, l_FreeAllFlowboxes);
	lua_setglobal(lua, "FreeAllFlowboxes");

#ifdef CURLY    
	lua_pushcfunction(lua, l_WriteURLData);
	lua_setglobal(lua, "WriteURLData");
	lua_pushcfunction(lua, l_PutURLData);
	lua_setglobal(lua, "PutURLData");
#endif
	
	// Initialize the global mic buffer table
#ifdef MIC_ARRAY
	lua_newtable(lua);
	lua_setglobal(lua, "urMicData");
#endif	
	
#ifdef SOUND_ARRAY
	// NOTE: SOMETHING IS WEIRD HERE. CAUSES VARIOUS BUGS IF ONE WRITES TO THIS TABLE
	lua_newtable(lua);
	lua_newtable(lua);
	lua_rawseti(lua, -2, 1); // Setting up for stereo for now
	lua_newtable(lua);
	lua_rawseti(lua, -2, 2); // Can be extended to any number of channels here
	lua_setglobal(lua,"urSoundData");
#endif
	systimer = new MachTimer();
	systimer->start();
}

#endif
