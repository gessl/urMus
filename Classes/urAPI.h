/*
 *  urAPI.h
 *  urMus
 *
 *  Created by Georg Essl on 6/20/09.
 *  Copyright 2009-11 Georg Essl. All rights reserved. See LICENSE.txt for license details.
 *
 */
#ifndef __URAPI_H__
#define __URAPI_H__

// Uncomment the line below to get some thread safety using recursive mutex (using lubr
//#define THREADSAFETY
#ifdef THREADSAFETY
#include "rmutex.h"

#define UISTRINGS
//extern RMutex luamutex;

#define lua_lock luamutex.lock();
#define lua_unlock luamutex.unlock();
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "urSound.h"
#include "mo_net.h"

#import "EAGLView.h"

#import "Texture2d.h"
#ifndef UISTRINGS
#include "urTexture.h"
#endif

#ifdef GPUIMAGE
#import "GPUImage.h"
#endif

#undef SANDWICH_SUPPORT

// To enable Soar, take the following steps...
// 0. Download the iOS Soar distribution from the Soar web-page as linked on urmus.eecs.umich.edu
// 1. set header search path to "include" of Soar release
//    in urMus target
// 2. add all "lib" of Soar release to "Link Binary with Libraries"
//    Build Phase of urMus target (sans SQLite)
//    (making sure to compile for ios-device or -simulator)
// 3. add sqlite to Build Phase from built-in frameworks (see #2)
// 4. Un-comment the following line
//#define SOAR_SUPPORT "Soar!"
#ifdef SOAR_SUPPORT

#define SOAR_ASYNCH_PER_UPDATE 1
//#define SOAR_DEBUG "I see Soar"

#include "portability.h"
#include "sml_Client.h"
#include <map> // <- much disliked around here!
#endif

// Events registered by ID
enum eventIDs { OnDragStart, OnDragStop, OnHide, OnShow, OnTouchDown, OnTouchUp, OnDoubleTap, OnSizeChanged, OnEnter, OnLeave, OnUpdate, OnNetIn, OnNetConnect, OnNetDisconnect, OnOSCMessage, 
#ifdef SANDWICH_SUPPORT
    OnPressure,
#endif
#ifdef SOAR_SUPPORT
    OnSoarOutput,
#endif
    OnAccelerate, OnAttitude, OnRotation, OnHeading, OnLocation, OnMicrophone, OnHorizontalScroll, OnVerticalScroll, OnMove, OnPageEntered, OnPageLeft, OnDragging, OnKeyboard, OnKeyboardBackspace,
    
    EventsCount
};

//int MAX_EVENTS = EventsCount;
#define MAX_EVENTS EventsCount

#define BLEND_DISABLED 0
#define BLEND_BLEND 1
#define BLEND_ALPHAKEY 2
#define BLEND_ADD 3
#define BLEND_MOD 4
#define BLEND_SUB 5

#define JUSTIFYH_CENTER 0
#define JUSTIFYH_LEFT 1
#define JUSTIFYH_RIGHT 2

#define JUSTIFYV_MIDDLE 0
#define JUSTIFYV_TOP 1
#define JUSTIFYV_BOTTOM 2

#define WRAP_WORD 0
#define WRAP_CHAR 1
#define WRAP_CLIP 2

enum USERDATA_IDS { UR_REGION, UR_TEXTURE, UR_TEXTLABEL, UR_FLOWBOX, UR_FLOWBOXPORT, numUserDataIDs };

extern char TEXTURE_SOLID[];

extern lua_State *lua;

typedef struct urAPI_Region urAPI_Region_t;

// TextLabel user data
typedef struct urAPI_TextLabel
{
    enum USERDATA_IDS ud_id = UR_TEXTLABEL;
	char* text;
	const char* font;
	int	justifyh;
	int justifyv;
	float shadowcolor[4];
	float shadowoffset[2];
	float shadowblur;
	bool drawshadow;
	float linespacing;
	float textcolor[4];
    int outlinemode;
    int outlinethickness;
	float textheight;
	float stringheight;
	float stringwidth;
	int wrap;
	bool updatestring;
	float rotation;
    urAPI_Region_t *region;
	// Private
#ifdef UISTRINGS
	Texture2D		*textlabelTex;
#else
    urTexture       *textlabelTex;
#endif
} urAPI_TextLabel_t;

typedef struct DrawQueueEntry DrawQueueEntry_t;

// Texture user data
typedef struct urAPI_Texture
	{
//        enum USERDATA_IDS ud_id = UR_TEXTURE;
		int blendmode;
		float texcoords[8];
		char* texturepath;
		bool modifyRect;
		bool isDesaturated;
		bool isTiled;
		bool fill;
		float width;
		float height;
		float gradientUL[4]; // RGBA
		float gradientUR[4]; // RGBA
		float gradientBL[4]; // RGB for 4 corner color magic
		float gradientBR[4]; // RGB for 4 corner color magic		
		float texturesolidcolor[4]; // for solid
		float texturebrushcolor[8]; // for brushes
		int usecamera;
		// Private
		Texture2D	*backgroundTex;
#ifdef GPUIMAGE
        GLuint movieTexture;
        GPUImageMovie *movieTex;
        urRegionMovie* regionMovie;
        GPUImageTextureOutput *textureOutput;
        GPUImageTextureInput *textureInput;
        GPUImageTextureInput *filterInput;
        GPUImageTextureOutput *filterOutput;
        GPUImageOutput<GPUImageInput> *inputFilter;
        float filterValue;
        urFilterHandler *filterHandler;
        GLuint	_filterTexture;
        GPUImageFilterType filtertype;

#endif
		urAPI_Region_t *region;
	} urAPI_Texture_t;

// FlowBox user data

typedef struct ursAPI_FlowBox
	{
        enum USERDATA_IDS ud_id = UR_FLOWBOX;
		int tableref; // table reference which contains this flowbox
		ursObject* object;
	} ursAPI_FlowBox_t;

typedef struct ursAPI_FlowBox_Port
    {
        enum USERDATA_IDS ud_id = UR_FLOWBOXPORT;
        int tableref; // table reference which contains this flowbox
        int index; // Port (in/out) index
        ursObject* object;
    } ursAPI_FlowBox_Port_t;

// Region user data

typedef struct urAPI_Region
	{
        enum USERDATA_IDS ud_id = UR_REGION;
		// internals
		struct urAPI_Region* prev; // Chained list of Regions
		struct urAPI_Region* next;
		int page;
		// actual data
		struct urAPI_Region* parent;
		struct urAPI_Region* firstchild;
		struct urAPI_Region* nextchild;
		const char* name;
		const char* type;
		urAPI_Texture_t* texture;
		urAPI_TextLabel_t* textlabel;
		
		int tableref; // table reference which contains this Region
		
		bool isMovable;
		bool isResizable;
		bool isTouchEnabled;
		bool isScrollXEnabled;
		bool isScrollYEnabled;
		bool isVisible;
		bool isShown;
		bool isDragged;
		bool isResized;
		bool isClamped;
		bool isClipping;

		float cx;
		float cy;
		float top;
		float bottom;
		float left;
		float right;
		float width;
		float height;

		float clipleft;
		float clipbottom;
		float clipwidth;
		float clipheight;
		
		float clampleft;
		float clampbottom;
		float clampwidth;
		float clampheight;
		
		float alpha;
		
		struct urAPI_Region* relativeRegion;
		char* relativePoint;
		char* point;
		lua_Number ofsx;
		lua_Number ofsy;
		
		float lastinputx[MAX_FINGERS];
		float lastinputy[MAX_FINGERS];
		int lastinputid[MAX_FINGERS];
		int numinputs;
		
		bool update;
		
		bool entered;
		
		int strata;
		
        int OnEvents[MAX_EVENTS];
#ifdef SOAR_SUPPORT
		
		sml::Kernel* soarKernel;
		sml::Agent* soarAgent;
		
		std::map< int, sml::Identifier* >* soarIds;
		int soarIdCounter;
		
		std::map< int, sml::WMElement* >* soarWMEs;
		int soarWMEcounter;
#endif
		
	}urAPI_Region_t;

typedef struct Region_Chain
{
    urAPI_Region_t* region;
    struct Region_Chain* next;
} Region_Chain_t;

typedef struct Region_Chain_Iterator
{
    Region_Chain_t* first;
    Region_Chain_t* current;
    Region_Chain_t* next;
    Region_Chain_t* prev;
} Region_Chain_Iterator_t;

/*static int l_setanimspeed(lua_State *lua);
static int l_Region(lua_State *lua);*/

void l_setupAPI(lua_State *lua);
void l_setstrataindex(urAPI_Region_t* region , int strataindex);
bool callScript(enum eventIDs event, int func_ref, urAPI_Region_t* region);
urAPI_Region_t* findRegionDraggable(float x, float y);
urAPI_Region_t* findRegionHit(float x, float y);
urAPI_Region_t* findRegionXScrolled(float x, float y, float dx);
urAPI_Region_t* findRegionYScrolled(float x, float y, float dy);
urAPI_Region_t* findRegionMoved(float x, float y, float dx, float dy);
bool callAllOnUpdate(float time);
bool callAllOnAccelerate(float x, float y, float z);
bool callAllOnNetIn(float a);
bool callAllOnNetConnect(const char* name, const char* btype);
bool callAllOnNetDisconnect(const char* name, const char* btype);
bool callAllOnOSCMessage(float num);
#ifdef SANDWICH_SUPPORT
bool callAllOnPressure(float p);
#endif
#ifdef SOAR_SUPPORT
bool callAllOnSoarOutput();
#endif
bool callAllOnKeyboard(const char *c);
bool callAllOnKeyboardBackspace();
bool callAllOnAttitude(float x, float y, float z, float w);
bool callAllOnRotRate(float x, float y, float z);
bool callAllOnHeading(float x, float y, float z, float north);
bool callAllOnLocation(float latitude, float longitude);
bool callAllOnMicrophone(SInt16* mic_buffer, UInt32 bufferlen);
void callAllOnLeaveRegions(int nr, float* x, float* y, float* ox, float* oy);
void callAllOnEnterLeaveRegions(int nr, float* x, float* y, float* ox, float* oy);
bool callScriptWithOscArgs(enum eventIDs event, int func_ref, urAPI_Region_t* region, osc::ReceivedMessageArgumentStream & s);
bool callScriptWith5Args(enum eventIDs event, int func_ref, urAPI_Region_t* region ,float a, float b, float c, float d, float e);
bool callScriptWith4Args(enum eventIDs event, int func_ref, urAPI_Region_t* region ,float a, float b, float c, float d);
bool callScriptWith3Args(enum eventIDs event, int func_ref, urAPI_Region_t* region ,float a, float b, float c);
bool callScriptWith2Args(enum eventIDs event, int func_ref, urAPI_Region_t* region ,float a, float b);
bool callScriptWith1Args(enum eventIDs event, int func_ref, urAPI_Region_t* region ,float a);
bool callScriptWith1Global(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* globaldata);
bool callScriptWith1String(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* name);
bool callScriptWith2String(enum eventIDs event, int func_ref, urAPI_Region_t* region, const char* name, const char* btype);

void addChild(urAPI_Region_t *parent, urAPI_Region_t *child);
void removeChild(urAPI_Region_t *parent, urAPI_Region_t *child);
bool layout(urAPI_Region_t* region);
void changeLayout(urAPI_Region_t* region);
void reanchor(urAPI_Region_t* region);

void ur_GetSoundBuffer(SInt16* buffer, int channel, int size);
void FreeAllFlowboxes(int patch);

void ur_Log(const char * str);

const char* getSystemPath();
const char* getDocumentPath();

void lock_events();
void unlock_events();

#endif /* __URAPI_H__ */

