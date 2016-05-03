//
//  EAGLView.m
//  urMus
//
//  Created by Georg Essl on 6/20/09.
//  Copyright Georg Essl 2009. All rights reserved. See LICENSE.txt for license details.
//
#import <QuartzCore/QuartzCore.h>

#include "config.h"
#import "EAGLView.h"

#ifndef OPENGLES2
#import <OpenGLES/EAGLDrawable.h>
#endif

#import "urAPI.h"
#import "Texture2d.h"

#ifndef UISTRINGS
#import "urFont.h"
#include "urTexture.h"
#endif

#import "MachTimer.h"
#import "urSound.h"
#import "httpServer.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>

#define DEGREES_TO_RADIANS(x) (M_PI * x / 180.0)

#define SLEEPER
/* If GPUIMAGE is used or not is defined in EAGLView.h" */

#ifdef SANDWICH_SUPPORT
static float pressure[4] = {0,0,0,0};
#endif

#ifdef OPENGLES2
enum { ATTRIB_VERTEX, ATTRIB_COLOR, ATTRIB_TEXTUREPOSITON, NUM_ATTRIBUTES };

GLint _positionSlot;
GLint _texcoordSlot;
GLuint _textureUniform;
#endif

#define USE_DEPTH_BUFFER 0

extern int currentPage;
extern int currentExternalPage;
extern urAPI_Region_t* firstRegion[];
extern urAPI_Region_t* lastRegion[];

extern urAPI_Region_t* UIParent;

MachTimer* mytimer;

// Interfacing with C of the lua API
extern EAGLView* g_glView;

@implementation urRegionMovie

// Shared GL pipeline fed data structures
GLfloat squareVertices[] = {
    -0.5f, -0.5f,
    0.5f,  -0.5f,
    -0.5f,  0.5f,
    0.5f,   0.5f,
};
#ifdef OPENGLES2
GLubyte squareColors[] = {
    //    GLfloat squareColors[] = {
    255, 255,   0, 255,
    0,   255, 255, 255,
    0,     0,   0,   0,
    255,   0, 255, 255,
};
#else
GLubyte squareColors[] = {
    255, 255,   0, 255,
    0,   255, 255, 255,
    0,     0,   0,   0,
    255,   0, 255, 255,
};
#endif

GLfloat shadowColors[] = {
    0.0, 0.0, 0.0, 50.0
};

// Path to the default font

using namespace std;

string g_fontPath;
string g_storagePath;
static string &fontPath=g_fontPath;
static string &storagePath=g_storagePath;
string errorfontPath;

#ifdef GPUIMAGE
#pragma mark -
#pragma mark GPUImageTextureOutputDelegate delegate method

- (void)newFrameReadyFromTextureOutput:(GPUImageTextureOutput *)callbackTextureOutput;
{
//    GLuint texture = callbackTextureOutput.texture;
#ifdef FREEOVERFLOWTEXTURE  
    if(region->texture->movieTexture)
    {
        glDeleteTextures(1,&(region->texture->movieTexture));
        region->texture->movieTexture = 0;
    }
#endif
    region->texture->movieTexture = callbackTextureOutput.texture;
}
#endif

@end



@implementation urFilterHandler
#ifdef GPUIMAGE
#pragma mark -
#pragma mark GPUImageTextureOutputDelegate delegate metho

- (void)newFrameReadyFromTextureOutput:(GPUImageTextureOutput *)callbackTextureOutput;
{
    if(callbackTextureOutput && region && region->texture)
        region->texture->_filterTexture = callbackTextureOutput.texture;
}
#endif
@end

// A class extension to declare private methods
@interface EAGLView ()

//@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, assign) NSTimer *animationTimer;

@property (nonatomic, retain, readwrite) NSNetService *ownEntry;
@property (nonatomic, assign, readwrite) BOOL showDisclosureIndicators;
@property (nonatomic, retain, readwrite) NSMutableArray *services;
@property (nonatomic, retain, readwrite) NSNetServiceBrowser *netServiceBrowser;
@property (nonatomic, retain, readwrite) NSNetService *currentResolve;
@property (nonatomic, retain, readwrite) NSTimer *timer;
@property (nonatomic, assign, readwrite) BOOL needsActivityIndicator;
@property (nonatomic, assign, readwrite) BOOL initialWaitOver;
@property (nonatomic, retain, readwrite) NSMutableArray *remoteIPs;
@property (nonatomic, retain, readwrite) NSMutableDictionary *searchtype;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end


@implementation EAGLView

@synthesize context;
@synthesize animationTimer;
@synthesize animationInterval;

@synthesize locationManager;

//@synthesize delegate = _delegate;
@synthesize ownEntry = _ownEntry;
@synthesize showDisclosureIndicators = _showDisclosureIndicators;
@synthesize currentResolve = _currentResolve;
@synthesize netServiceBrowser = _netServiceBrowser;
@synthesize services = _services;
@synthesize needsActivityIndicator = _needsActivityIndicator;
@dynamic timer;
@synthesize initialWaitOver = _initialWaitOver;
@synthesize netService;
@synthesize remoteIPs;
@synthesize searchtype;

@synthesize captureManager;

// You must implement this method
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

static const double ACCELEROMETER_RATE = 0.030;
static const int ACCELEROMETER_SCALE = 256;
static const int HEADING_SCALE = 256;
static const int LOCATION_SCALE = 256;

// Tracking All touches
NSMutableArray *ActiveTouches;              ///< Used to keep track of all current touches.

struct urDragTouch
{
	urAPI_Region_t* dragregion;
	int touch1;
	int touch2;
//	UITouch* touch1;
//	UITouch* touch2;
	float left;
	float top;
	float right;
	float bottom;
	float dragwidth;
	float dragheight;
	bool active;
	bool flagged;
	urDragTouch() { active = false; flagged = false; touch1 = -1; touch2 = -1;}
};

typedef struct urDragTouch urDragTouch_t;

#define MAX_DRAGS 10
urDragTouch_t dragtouches[MAX_DRAGS];

int FindDragRegion(urAPI_Region_t*region)
{
	for(int i=0; i< MAX_DRAGS; i++)
	{
		if(dragtouches[i].active && dragtouches[i].dragregion == region)
			return i;
	}
	return -1;
}

void AddDragRegion(int idx, int t)
{
	if(dragtouches[idx].touch1 == -1 && dragtouches[idx].touch2!=t)
		dragtouches[idx].touch1 = t;
	else if(dragtouches[idx].touch2 == -1 && dragtouches[idx].touch1!=t)
		dragtouches[idx].touch2 = t;
}

/*void AddDragRegion(int idx, UITouch* t)
{
	if(dragtouches[idx].touch1 == NULL && dragtouches[idx].touch2!=t)
		dragtouches[idx].touch1 = t;
	else if(dragtouches[idx].touch2 == NULL && dragtouches[idx].touch1!=t)
		dragtouches[idx].touch2 = t;
}*/

void ClearAllDragFlags()
{
	for(int i=0; i< MAX_DRAGS; i++)
	{
		dragtouches[i].flagged = false;
	}
}

int FindAvailableDragTouch()
{
	for(int i=0; i< MAX_DRAGS; i++)
		if(dragtouches[i].active == false)
			return i;
	
	int a=0;
	return -1;
}


UITouch* UITTrans[MAX_FINGERS];

void InitUITouchTranslation()
{
	for(int i=0; i< MAX_FINGERS; i++)
	{
		UITTrans[i] = NULL;
	}
}

int UITouch2UTID(UITouch* t)
{
	for(int i=0; i<MAX_FINGERS;i++)
		if( UITTrans[i] == t) return i;
	
	return -1;
}

UITouch* UTID2UITouch(int t)
{
	return UITTrans[t];
}

int AddUITouch(UITouch* t)
{
	int n = -1;
	for(int i=0; i< MAX_FINGERS; i++)
	{
		if(UITTrans[i] == t)
        {
            return i;
        }
		if(n == -1 && UITTrans[i] == NULL) n = i;
	}
	
	if(n != -1)
		UITTrans[n] = t;
	
	return n;
}

void RemoveUTID(int t)
{
	UITTrans[t] = NULL;
}

void RemoveUITouchUTID(UITouch* t)
{
	for(int i=0; i<MAX_FINGERS;i++)
		if( UITTrans[i] == t) UITTrans[i] = NULL;
}

int FindDoubleDragTouch(int t1, int t2)
{
	for(int i=0; i< MAX_DRAGS; i++)
		if(dragtouches[i].active && ((dragtouches[i].touch1 == t1 && dragtouches[i].touch2 == t2) || (dragtouches[i].touch1 == t2 && dragtouches[i].touch2 == t1)))
		{
			return i;
		}
	return -1;
}


/*int FindDoubleDragTouch(UITouch* t1, UITouch* t2)
{
	for(int i=0; i< MAX_DRAGS; i++)
		if(dragtouches[i].active && ((dragtouches[i].touch1 == t1 && dragtouches[i].touch2 == t2) || (dragtouches[i].touch1 == t2 && dragtouches[i].touch2 == t1)))
		{
			return i;
		}
	return -1;
}*/

int FindSingleDragTouch(int t)
{
	if(t>=0)
	{
		for(int i=0; i< MAX_DRAGS; i++)
			if((dragtouches[i].active && dragtouches[i].touch1 == t /* && dragtouches[i].touch2 == NULL*/) || (/*dragtouches[i].touch1 == NULL &&*/ dragtouches[i].touch2 == t))
			{
				return i;
			}
	}
	return -1;
}

/*
int FindSingleDragTouch(UITouch* t)
{
	for(int i=0; i< MAX_DRAGS; i++)
		if((dragtouches[i].active && dragtouches[i].touch1 == t) || (dragtouches[i].touch2 == t))
		{
			return i;
		}
	return -1;
}*/

float cursorpositionx[MAX_FINGERS];
float cursorpositiony[MAX_FINGERS];
float oldcursorpositionx[MAX_FINGERS];
float oldcursorpositiony[MAX_FINGERS];
int cursorpositionid[MAX_FINGERS];
bool differentialdrag;

float cursorscrollspeedx[MAX_FINGERS];
float cursorscrollspeedy[MAX_FINGERS];



// Arrays to pass multi-touch finger to enter/leave handling. This allows smart decisions for enter/leave based on all fingers being considered. Should never be more than 5 and is fixed to avoid problems if MAX_FINGERS should be set to less for some reason.
int argmoved[MAX_FINGERS];
int argids[MAX_FINGERS];
float argcoordx[MAX_FINGERS];
float argcoordy[MAX_FINGERS];
float arg2coordx[MAX_FINGERS];
float arg2coordy[MAX_FINGERS];

// This is the texture to hold DPrint and lua error messages.
#ifdef UISTRINGS
Texture2D       *errorStrTex = nil;
#else
urTexture       *errorStrTex = nil;
#endif
std::string errorstr = "";
bool newerror = false;

//#define LATE_LAUNCH
// Main drawing loop. This does everything but brew coffee.
extern lua_State *lua;

// Only run video capture if the data is actually used somewhere.
// This is a global camera instance counter combining both dataflow pipeline and graphical texture loads.
long camerause = 0;

// Obj-C interface Camera Instance counter
- (void)IncCameraUse
{
    if(camerause <= 0)
    {
#ifdef USECAMERA
#ifdef GPUIMAGE
        [videoCamera startCameraCapture];
#else
        [captureManager.captureSession startRunning];
#endif
#endif
    }
    camerause++;
}

- (void)DecCameraUse
{
    if(camerause > 0)
    {  
        camerause--;
#ifdef USECAMERA
        if(camerause == 0)
#ifdef GPUIMAGE
            [videoCamera stopCameraCapture];
#else
            [captureManager.captureSession stopRunning];
#endif
#endif
    }
}

- (void)DecCameraUseBy:(int)dec
{
    if(camerause > 0)
    {  
        camerause=camerause-dec;
#ifdef USECAMERA
        if(camerause == 0)
#ifdef GPUIMAGE
            [videoCamera stopCameraCapture];
#else
            [captureManager.captureSession stopRunning];
#endif
#endif
    }
}



// C interface Camera Instance counter
void incCameraUse()
{
    [g_glView IncCameraUse];
}

void decCameraUse()
{
    [g_glView DecCameraUse];
}

void decCameraUseBy(int dec)
{
    [g_glView DecCameraUseBy:dec];
}

#ifdef GPUIMAGE
#pragma mark -
#pragma mark GPUImageTextureOutputDelegate delegate method

- (void)newFrameReadyFromTextureOutput:(GPUImageTextureOutput *)callbackTextureOutput;
{
    GLuint texture = callbackTextureOutput.texture;
    _cameraTexture = texture;
    cameraTexture = texture;
    
/*    
    GLubyte *rawImagePixels = (GLubyte *)malloc(totalBytesForImage);
    glReadPixels(0, 0, (int)currentFBOSize.width, (int)currentFBOSize.height, GL_RGBA, GL_UNSIGNED_BYTE, rawImagePixels);
    // Do something with the image
    free(rawImagePixels);
    
    CVPixelBufferLockBaseAddress( pixelBuffer, 0 );
	
	int bufferHeight = CVPixelBufferGetHeight(pixelBuffer);
	int bufferWidth = CVPixelBufferGetWidth(pixelBuffer);
	int bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
	unsigned char *rowBase = (unsigned char *)CVPixelBufferGetBaseAddress(pixelBuffer);
	
	////////////////////////////////////////////////////////////////
	
	//GLint	saveName;
	//glGetIntegerv(GL_TEXTURE_BINDING_2D, &saveName);
	
	//Create the texture if it doesn't exist
	if (!_cameraTexture) {
		glGenTextures(1, &_cameraTexture);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, _cameraTexture);
		
		// Set appropriate parameters for when the texture is resized
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferWidth, bufferHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, CVPixelBufferGetBaseAddress(pixelBuffer));
		
		[self performSelectorOnMainThread:@selector(informViewsOfCameraTexture) withObject:nil waitUntilDone:NO];
		
	} else {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, _cameraTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bufferWidth, bufferHeight, GL_BGRA, GL_UNSIGNED_BYTE,  CVPixelBufferGetBaseAddress(pixelBuffer));
	}
    
	
	//glBindTexture(GL_TEXTURE_2D, saveName);
	
	////////////////////////////////////////////////////////////////
	
	// Color initializations
	int64_t redTotal = 0;
	int64_t greenTotal = 0;
	int64_t blueTotal = 0;
	double totalTotal = 0;
	
	// Mid-level vision initializations
	int gradientX = 0;
	int gradientY = 0;
	double edginess = 0;
	
	// We don't look at every single pixel. Doing so would cause unneccesary load on the
	//	system for such basic features. This MUST be a factor of both the bufferHeight
	//	and bufferWidth.
	int downSampleFactor = 8;
	
	// Parse the image from left to right, bottom to top.
	for( int row = 0; row < bufferHeight; row += downSampleFactor ) {
		for( int column = 0; column < bufferWidth; column += downSampleFactor ) {
			
			unsigned char *pixel = rowBase + (row * bytesPerRow) + (column * BYTES_PER_PIXEL);
			
			// Reassignment is done here both to cast the unsigned char into the 8-bit integer
			//	as well as to remain compatible with helper functions above and to keep original
			//	memory buffer in tact.
			uint8_t pixelColor[3];
			for (int j = 0; j < 3; j++)
				pixelColor[j] = (uint8_t)pixel[j];
			
			//pixelColor now contains the BGR values of the current pixel
			
			blueTotal += pixelColor[0];
			greenTotal += pixelColor[1];
			redTotal += pixelColor[2];
			
			
			// Ensure we are not at the boundaries of the image so we can use the Roberts edge detector
			if (row < (bufferHeight-2*downSampleFactor) && column < (bufferWidth-2*downSampleFactor)) {
				
				// Access the other pixels in the neighborhood to calculate the gradient
				unsigned char *pixel9 = rowBase + ((row+downSampleFactor) * bytesPerRow) + ((column+downSampleFactor) * BYTES_PER_PIXEL);
				unsigned char *pixel8 = rowBase + ((row+downSampleFactor) * bytesPerRow) + (column * BYTES_PER_PIXEL);
				unsigned char *pixel6 = rowBase + (row * bytesPerRow) + ((column+downSampleFactor) * BYTES_PER_PIXEL);
				
				//Average of pixel 9 minus the average of the current pixel
				gradientX = (((uint8_t)pixel9[0]+(uint8_t)pixel9[1]+(uint8_t)pixel9[2])/3) - ((pixelColor[0]+pixelColor[1]+pixelColor[2])/3);
				gradientY = (((uint8_t)pixel8[0]+(uint8_t)pixel8[1]+(uint8_t)pixel8[2])/3) - (((uint8_t)pixel6[0]+(uint8_t)pixel6[1]+(uint8_t)pixel6[2])/3);
				edginess += pow(gradientX/255.0,2) + pow(gradientY/255.0,2);
			}
			
			
		}
		
	}
	
	// Normalize the sums into the 0 to 255 range 
	blueTotal = blueTotal/((bufferWidth/downSampleFactor)*(bufferHeight/downSampleFactor));
	greenTotal = greenTotal/((bufferWidth/downSampleFactor)*(bufferHeight/downSampleFactor));
	redTotal = redTotal/((bufferWidth/downSampleFactor)*(bufferHeight/downSampleFactor));
	
	totalTotal = ((double)blueTotal+(double)greenTotal+(double)redTotal)/3;
	
	// Normalize the edginess between 0 and 1
	edginess = log10(edginess)/log10(((bufferWidth/downSampleFactor)-1)*((bufferHeight/downSampleFactor)-1));
	
	// and then the 0 to 1 range
	callAllCameraSources(totalTotal/255.0,blueTotal/255.0,greenTotal/255.0,redTotal/255.0, edginess);
	
	//printf("The new red value is %f\n",redTotal/255.);
	
	CVPixelBufferUnlockBaseAddress( pixelBuffer, 0 );
*/
    
}
#endif


- (BOOL)loadShaders
{
    [self setupShaders];
    return YES;
}


- (void)awakeFromNib3
{
    EAGLContext *aContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if (!aContext)
    {
        aContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    }
    
    if (!aContext)
        NSLog(@"Failed to create ES context");
    else if (![EAGLContext setCurrentContext:aContext])
        NSLog(@"Failed to set ES context current");
    
	self.context = aContext;
	[aContext release];
	
    [self setContext:context];
    [self setFramebuffer];
    
    if ([context API] == kEAGLRenderingAPIOpenGLES2) {
        [self loadShaders];
		[self setupView];
	}
    
    animating = FALSE;
    displayLinkSupported = FALSE;
    animationFrameInterval = 1;
    displayLink = nil;
    animationTimer = nil;
    
    // Use of CADisplayLink requires iOS version 3.1 or greater.
	// The NSTimer object is used as fallback when it isn't available.
    NSString *reqSysVer = @"3.1";
    NSString *currSysVer = [[UIDevice currentDevice] systemVersion];
    if ([currSysVer compare:reqSysVer options:NSNumericSearch] != NSOrderedAscending)
        displayLinkSupported = TRUE;
}

- (void)viewDidLoad:(BOOL)animated
{
    // IOS 7
    //viewDidload
    if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)]) {
    // iOS 7
    [self prefersStatusBarHidden];
    [self performSelector:@selector(setNeedsStatusBarAppearanceUpdate)];
    } else {
    // iOS 6
    // Hide top navigation bar
            [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];
    //        [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationSlide];
        }

}

- (void)viewWillAppear:(BOOL)animated
{
    [self startAnimation];
    
    [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [self stopAnimation];
    
    [super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
}

#ifdef GPUIMAGE
- (void)writeMovieFromTexture:(GLuint)textureID ofSize:(CGSize)size
{
    if(textureInput == NULL)
        textureInput = [[GPUImageTextureInput alloc] initWithTexture:textureID size:size];
    else
        [textureInput initWithTexture:textureID size:size];
    
    // Configure this for video from the movie file, where we want to preserve all video frames and audio samples
    //    movieWriter.shouldPassthroughAudio = YES;
    
    // Recording from another movie stream
    if(movieFile != nil)
    {
        //        movieFile.audioEncodingTarget = movieWriter;
        //        [movieFile enableSynchronizedEncodingUsingMovieWriter:movieWriter];
    }
//    [textureInput addTarget:movieWriter];
    if(rotateFilter != NULL)
    {
        [rotateFilter dealloc];
        rotateFilter = NULL;
    }
    rotateFilter = [[GPUImageTransformFilter alloc] init];
//    [rotateFilter setAffineTransform:CGAffineTransformMakeRotation(DEGREES_TO_RADIANS(180))];
    [rotateFilter setAffineTransform:CGAffineTransformScale(CGAffineTransformMakeRotation(DEGREES_TO_RADIANS(0)), 1, 1)];
    [textureInput addTarget:rotateFilter];
    [rotateFilter addTarget:movieWriter];
    // kGPUImageRotateRightFlipVertical
    //    [textureInput addTarget:movieWriter];
//    [movieWriter setInputRotation:kGPUImageFlipVertical atIndex:0];
    [movieWriter startRecording];
    
    [movieWriter setCompletionBlock:^{
//        [textureInput removeTarget:movieWriter];
//        [textureInput removeAllTargets];
//        [movieWriter finishRecording];
//        [textureInput dealloc];
//        [movieWriter dealloc];
//        [rotateFilter dealloc];
//        rotateFilter = NULL;
    }];
}

- (void)writeMovieFromTexture:(GLuint)textureID ofSize:(CGSize)size withCrop:(CGRect)crop
{
    if(textureInput == NULL)
        textureInput = [[GPUImageTextureInput alloc] initWithTexture:textureID size:size];
    else
        [textureInput initWithTexture:textureID size:size];
    
    // Configure this for video from the movie file, where we want to preserve all video frames and audio samples
    //    movieWriter.shouldPassthroughAudio = YES;
    
    // Recording from another movie stream
    if(movieFile != nil)
    {
        //        movieFile.audioEncodingTarget = movieWriter;
//        [movieFile enableSynchronizedEncodingUsingMovieWriter:movieWriter];
    }
    if(cropfilter != NULL)
    {
        [cropfilter dealloc];
        cropfilter = NULL;
    }
    cropfilter = [[GPUImageCropFilter alloc] initWithCropRegion:CGRectMake(0.0,0.0,1.0,1.0)];
    [cropfilter setCropRegion:crop];
    [textureInput addTarget:cropfilter];
    if(rotateFilter != NULL)
    {
        [rotateFilter dealloc];
        rotateFilter = NULL;
    }
    rotateFilter = [[GPUImageTransformFilter alloc] init];
    [rotateFilter setAffineTransform:CGAffineTransformScale(CGAffineTransformMakeRotation(DEGREES_TO_RADIANS(180)), -1, 1)];
//    [rotateFilter setAffineTransform:CGAffineTransformMake(CGRectGetWidth(crop),0,CGRectGetHeight(crop),0,0,0)];
    [cropfilter addTarget:rotateFilter];
    [rotateFilter addTarget:movieWriter];
    // kGPUImageRotateRightFlipVertical
//    [textureInput addTarget:movieWriter];
//    [movieWriter setInputRotation:kGPUImageFlipVertical atIndex:0];
    [movieWriter setInputRotation:kGPUImageFlipHorizonal atIndex:0];
    [movieWriter startRecording];
    
    [movieWriter setCompletionBlock:^{
        /*
        [textureInput removeAllTargets];
//        [textureInput removeTarget:movieWriter];
        [movieWriter finishRecording];
//        [textureInput dealloc];
//        [textureInput removeAllTargets];
        [movieWriter dealloc];
        [cropfilter dealloc];
        cropfilter = NULL;*/
    }];
}

- (void)writeMovie:(NSString*)filename ofSize:(CGSize)size fromTexture:(GLuint)textureID
{
    // write out a processed version of the movie to disk
    unlink([filename UTF8String]); // If a file already exists, AVAssetWriter won't let you record new frames, so delete the old movie
    NSURL *movieURL = [NSURL fileURLWithPath:filename];
    
    long w=size.width;
    long h=size.height;
    
    if(movieWriter)
    {
        char errorstrbuf[120];
        sprintf(errorstrbuf,"Recording of a movie is currently in progress.\nCall FinishMovie() before starting a new movie recording.");
        errorstr = errorstrbuf;
        newerror = true;
        ur_Log(errorstr.c_str());
        return;
    }
    
    movieWriter = [[GPUImageMovieWriter alloc] initWithMovieURL:movieURL size:CGSizeMake(w,h)];
    
    
    if(textureID > 0)
        [self writeMovieFromTexture:textureID ofSize:size];
    else
    {   
        if(movieFile != nil)
        {
            recordfrom = SOURCE_MOVIE;
        }
        else 
        {
            recordfrom = SOURCE_CAMERA;
        }
        latelaunchmovie = true;
    }
    sourcesize = size;
}

- (void)writeMovie:(NSString*)filename ofSize:(CGSize)size withCrop:(CGRect)crop fromTexture:(GLuint)textureID
{
    // write out a processed version of the movie to disk
    unlink([filename UTF8String]); // If a file already exists, AVAssetWriter won't let you record new frames, so delete the old movie
    NSURL *movieURL = [NSURL fileURLWithPath:filename];
    
    long w=size.width*crop.size.width;
    long h=size.height*crop.size.height;
    
    if(w < 128) w = 128; // Crash prevention initiative (donate to "UsingBuggyLibraryFund")
    if(h < 128) h = 128;
    
    if(movieWriter)
    {
        char errorstrbuf[120];
        sprintf(errorstrbuf,"Recording of a movie is currently in progress.\nCall FinishMovie() before starting a new movie recording.");
        errorstr = errorstrbuf;
        newerror = true;
        ur_Log(errorstr.c_str());
        return;
    }
    
    movieWriter = [[GPUImageMovieWriter alloc] initWithMovieURL:movieURL size:CGSizeMake(w,h)];

    
    if(textureID > 0)
    {
        [self writeMovieFromTexture:textureID ofSize:size withCrop:crop];
        latelaunchmovie = false;
    }
    else
    {   
        if(movieFile != nil)
        {
            recordfrom = SOURCE_MOVIE;
        }
        else 
        {
            recordfrom = SOURCE_CAMERA;
        }
        latelaunchmovie = true;
    }
    sourcesize = size;
}

- (void)finishMovie
{
    if(movieWriter)
        [movieWriter finishRecording];

    if(textureInput)
    {
        [textureInput removeAllTargets];
        [textureInput initWithTexture:0 size:CGSizeMake(1,1)];
    }
//        [textureInput removeTarget:movieWriter];
    
    
//    if(textureInput)
//        [textureInput dealloc];
    
    if(cropfilter != NULL)
    {
       [cropfilter removeAllTargets];
       [cropfilter dealloc];
        cropfilter = NULL;
    }
    
    if(rotateFilter != NULL)
    {
        [rotateFilter removeAllTargets];
       [rotateFilter dealloc];
        rotateFilter = NULL;
    }
    
    [movieWriter dealloc];
    movieWriter = NULL;

    recordfrom = SOURCE_TEXTURE;
}

- (GPUImageMovie *)loadMovie:(NSString*)filename
{
    NSString *filePath;
    BOOL isDir;
    BOOL fileExists = [[NSFileManager defaultManager] fileExistsAtPath:texturepathstr isDirectory:&isDir];

    if(fileExists && !isDir)
    {
        filePath = filename;
    }
    else
    {
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
        filePath = [resourcePath stringByAppendingPathComponent:filename];
        
        fileExists = [[NSFileManager defaultManager] fileExistsAtPath:filePath isDirectory:&isDir];
        if(!fileExists || isDir)
            return nil;
    }
//    NSURL *movieURL = [[NSBundle mainBundle] URLForResource:filename withExtension:nil];
    NSURL *movieURL = [NSURL fileURLWithPath:filePath];
    
    movieFile = [[GPUImageMovie alloc] initWithURL:movieURL];
//    movieFile.runBenchmark = YES;
    //    filter = [[GPUImagePixellateFilter alloc] init];
    return movieFile;
}

// Based on GPUImage example code

- (void)setFilterParameter:(double)value forFilter:(GPUImageOutput<GPUImageInput> *)inputFilter withType:(GPUImageFilterType)currentfiltertype
{
    value = (value+1)/2.0;
    switch(currentfiltertype)
    {
        case GPUIMAGE_SEPIA: [(GPUImageSepiaFilter *)inputFilter setIntensity:value]; break;
        case GPUIMAGE_PIXELLATE: [(GPUImagePixellateFilter *)inputFilter setFractionalWidthOfAPixel:value]; break;
        case GPUIMAGE_POLARPIXELLATE: [(GPUImagePolarPixellateFilter *)inputFilter setPixelSize:CGSizeMake(value, value)]; break;
        case GPUIMAGE_SATURATION: [(GPUImageSaturationFilter *)inputFilter setSaturation:value]; break;
        case GPUIMAGE_CONTRAST: [(GPUImageContrastFilter *)inputFilter setContrast:value]; break;
        case GPUIMAGE_BRIGHTNESS: [(GPUImageBrightnessFilter *)inputFilter setBrightness:value]; break;
        case GPUIMAGE_EXPOSURE: [(GPUImageExposureFilter *)inputFilter setExposure:value]; break;
        case GPUIMAGE_RGB: [(GPUImageRGBFilter *)inputFilter setGreen:value]; break;
        case GPUIMAGE_SHARPEN: [(GPUImageSharpenFilter *)inputFilter setSharpness:value]; break;
        case GPUIMAGE_UNSHARPMASK: [(GPUImageUnsharpMaskFilter *)inputFilter setIntensity:value]; break;
            //        case GPUIMAGE_UNSHARPMASK: [(GPUImageUnsharpMaskFilter *)inputFilter setBlurSize:value]; break;
        case GPUIMAGE_GAMMA: [(GPUImageGammaFilter *)inputFilter setGamma:value]; break;
        case GPUIMAGE_CROSSHATCH: [(GPUImageCrosshatchFilter *)inputFilter setCrossHatchSpacing:(value)/3.0]; break;
        case GPUIMAGE_POSTERIZE: [(GPUImagePosterizeFilter *)inputFilter setColorLevels:round((value)*20)]; break;
		case GPUIMAGE_HAZE: [(GPUImageHazeFilter *)inputFilter setDistance:value]; break;
		case GPUIMAGE_THRESHOLD: [(GPUImageLuminanceThresholdFilter *)inputFilter setThreshold:value]; break;
        case GPUIMAGE_ADAPTIVETHRESHOLD: [(GPUImageAdaptiveThresholdFilter *)inputFilter setBlurSize:value]; break;
            //        case GPUIMAGE_DISSOLVE: [(GPUImageDissolveBlendFilter *)inputFilter setMix:value]; break;
            //        case GPUIMAGE_CHROMAKEY: [(GPUImageChromaKeyBlendFilter *)inputFilter setThresholdSensitivity:value]; break;
            //        case GPUIMAGE_KUWAHARA: [(GPUImageKuwaharaFilter *)inputFilter setRadius:round(value)]; break;
        case GPUIMAGE_SWIRL: [(GPUImageSwirlFilter *)inputFilter setAngle:value*3]; break;
        case GPUIMAGE_EMBOSS: [(GPUImageEmbossFilter *)inputFilter setIntensity:value]; break;
        case GPUIMAGE_CANNYEDGEDETECTION: [(GPUImageCannyEdgeDetectionFilter *)inputFilter setBlurSize:value]; break;
            //        case GPUIMAGE_CANNYEDGEDETECTION: [(GPUImageCannyEdgeDetectionFilter *)inputFilter setLowerThreshold:value]; break;
            //        case GPUIMAGE_HARRISCORNERDETECTION: [(GPUImageHarrisCornerDetectionFilter *)inputFilter setThreshold:value]; break;
            //        case GPUIMAGE_NOBLECORNERDETECTION: [(GPUImageNobleCornerDetectionFilter *)inputFilter setThreshold:value]; break;
            //        case GPUIMAGE_SHITOMASIFEATUREDETECTION: [(GPUImageShiTomasiFeatureDetectionFilter *)inputFilter setThreshold:value]; break;
            //        case GPUIMAGE_HARRISCORNERDETECTION: [(GPUImageHarrisCornerDetectionFilter *)inputFilter setSensitivity:value]; break;
        case GPUIMAGE_SMOOTHTOON: [(GPUImageSmoothToonFilter *)inputFilter setBlurSize:value]; break;
            //        case GPUIMAGE_BULGE: [(GPUImageBulgeDistortionFilter *)inputFilter setRadius:value]; break;
        case GPUIMAGE_BULGE: [(GPUImageBulgeDistortionFilter *)inputFilter setScale:value*3]; break;
        case GPUIMAGE_TONECURVE: [(GPUImageToneCurveFilter *)inputFilter setBlueControlPoints:[NSArray arrayWithObjects:[NSValue valueWithCGPoint:CGPointMake(0.0, 0.0)], [NSValue valueWithCGPoint:CGPointMake(0.5, value)], [NSValue valueWithCGPoint:CGPointMake(1.0, 0.75)], nil]]; break;
        case GPUIMAGE_PINCH: [(GPUImagePinchDistortionFilter *)inputFilter setScale:value*2]; break;
            //        case GPUIMAGE_PERLINNOISE:  [(GPUImagePerlinNoiseFilter *)inputFilter setScale:value]; break;
        case GPUIMAGE_MOSAIC:  [(GPUImageMosaicFilter *)inputFilter setDisplayTileSize:CGSizeMake(value, value)]; break;
        case GPUIMAGE_VIGNETTE: [(GPUImageVignetteFilter *)inputFilter setVignetteEnd:value]; break;
        case GPUIMAGE_GAUSSIAN: [(GPUImageGaussianBlurFilter *)inputFilter setBlurSize:value]; break;
        case GPUIMAGE_BILATERAL: [(GPUImageBilateralFilter *)inputFilter setBlurSize:value]; break;
//        case GPUIMAGE_FASTBLUR: [(GPUImageFastBlurFilter *)inputFilter setBlurPasses:round(value)]; break;
        case GPUIMAGE_GAUSSIAN_SELECTIVE: [(GPUImageGaussianSelectiveBlurFilter *)inputFilter setExcludeCircleRadius:value]; break;
        case GPUIMAGE_FILTERGROUP: [(GPUImagePixellateFilter *)[(GPUImageFilterGroup *)inputFilter filterAtIndex:1] setFractionalWidthOfAPixel:value]; break;
        case GPUIMAGE_CROP: [(GPUImageCropFilter *)inputFilter setCropRegion:CGRectMake(0.0, 0.0, 1.0, value)]; break;
        case GPUIMAGE_TRANSFORM: [(GPUImageTransformFilter *)inputFilter setAffineTransform:CGAffineTransformMakeRotation(value)]; break;
        case GPUIMAGE_TRANSFORM3D:
        {
            CATransform3D perspectiveTransform = CATransform3DIdentity;
            perspectiveTransform.m34 = 0.4;
            perspectiveTransform.m33 = 0.4;
            perspectiveTransform = CATransform3DScale(perspectiveTransform, 0.75, 0.75, 0.75);
            perspectiveTransform = CATransform3DRotate(perspectiveTransform, value, 0.0, 1.0, 0.0);
            
            [(GPUImageTransformFilter *)inputFilter setTransform3D:perspectiveTransform];
            
        }; break;
        case GPUIMAGE_TILTSHIFT:
        {
            CGFloat midpoint = value;
            [(GPUImageTiltShiftFilter *)inputFilter setTopFocusLevel:midpoint - 0.1];
            [(GPUImageTiltShiftFilter *)inputFilter setBottomFocusLevel:midpoint + 0.1];
        }; break;
        case GPUIMAGE_POLKADOT: [(GPUImagePolkaDotFilter *)inputFilter setFractionalWidthOfAPixel:value]; break;
        case GPUIMAGE_HALFTONE: [(GPUImageHalftoneFilter *)inputFilter setFractionalWidthOfAPixel:value]; break;
        case GPUIMAGE_LEVELS:
        {
            [(GPUImageLevelsFilter *)inputFilter setRedMin:value gamma:1.0 max:1.0 minOut:0.0 maxOut:1.0];
            [(GPUImageLevelsFilter *)inputFilter setGreenMin:value gamma:1.0 max:1.0 minOut:0.0 maxOut:1.0];
            [(GPUImageLevelsFilter *)inputFilter setBlueMin:value gamma:1.0 max:1.0 minOut:0.0 maxOut:1.0];
        }; break;
        case GPUIMAGE_MONOCHROME: [(GPUImageMonochromeFilter *)inputFilter setIntensity:value]; break;
        case GPUIMAGE_HUE: [(GPUImageHueFilter *)inputFilter setHue:(value+1.0)*180.0]; break;
        case GPUIMAGE_WHITEBALANCE: [(GPUImageWhiteBalanceFilter *)inputFilter setTemperature:value]; break;
            //        case GPUIMAGE_LOWPASS: [(GPUImageLowPassFilter *)inputFilter setFilterStrength:value]; break;
            //        case GPUIMAGE_HIGHPASS: [(GPUImageHighPassFilter *)inputFilter setFilterStrength:value]; break;
            //        case GPUIMAGE_MOTIONDETECTOR: [(GPUImageMotionDetector *)inputFilter setLowPassFilterStrength:value]; break;
            //        case GPUIMAGE_THRESHOLDSKETCH: [(GPUImageThresholdSketchFilter *)inputFilter setThreshold:value]; break;
        case GPUIMAGE_SPHEREREFRACTION: [(GPUImageSphereRefractionFilter *)inputFilter setRadius:value]; break;
        case GPUIMAGE_GLASSSPHERE: [(GPUImageGlassSphereFilter *)inputFilter setRadius:value]; break;
            //        case GPUIMAGE_HIGHLIGHTSHADOW: [(GPUImageHighlightShadowFilter *)inputFilter setHighlights:value]; break;
        case GPUIMAGE_LOCALBINARYPATTERN:
        {
            [(GPUImageLocalBinaryPatternFilter *)inputFilter setTexelWidth:value];
            [(GPUImageLocalBinaryPatternFilter *)inputFilter setTexelHeight:value];
        }; break;
        default: break;
    }
}

- (void)setCameraFilterParameter:(double)value;
{
    [self setFilterParameter:value forFilter:inputFilter withType:currentfiltertype];
}

- (GPUImageOutput<GPUImageInput> *)createFilter:(GPUImageFilterType)filterType
{
    switch (filterType)
    {
        case GPUIMAGE_SEPIA:
        {
            inputFilter = [[GPUImageSepiaFilter alloc] init];
        }; break;
        case GPUIMAGE_PIXELLATE:
        {
            inputFilter = [[GPUImagePixellateFilter alloc] init];
        }; break;
        case GPUIMAGE_POLARPIXELLATE:
        {
            inputFilter = [[GPUImagePolarPixellateFilter alloc] init];
        }; break;
        case GPUIMAGE_CROSSHATCH:
        {
            inputFilter = [[GPUImageCrosshatchFilter alloc] init];
        }; break;
        case GPUIMAGE_COLORINVERT:
        {
            inputFilter = [[GPUImageColorInvertFilter alloc] init];
        }; break;
        case GPUIMAGE_GRAYSCALE:
        {
            inputFilter = [[GPUImageGrayscaleFilter alloc] init];
        }; break;
        case GPUIMAGE_SATURATION:
        {
            inputFilter = [[GPUImageSaturationFilter alloc] init];
        }; break;
        case GPUIMAGE_CONTRAST:
        {
            inputFilter = [[GPUImageContrastFilter alloc] init];
        }; break;
        case GPUIMAGE_BRIGHTNESS:
        {
            inputFilter = [[GPUImageBrightnessFilter alloc] init];
        }; break;
        case GPUIMAGE_RGB:
        {
            inputFilter = [[GPUImageRGBFilter alloc] init];
        }; break;
        case GPUIMAGE_EXPOSURE:
        {
            inputFilter = [[GPUImageExposureFilter alloc] init];
        }; break;
        case GPUIMAGE_SHARPEN:
        {
            inputFilter = [[GPUImageSharpenFilter alloc] init];
        }; break;
        case GPUIMAGE_UNSHARPMASK:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageUnsharpMaskFilter alloc] init];
        }; break;
        case GPUIMAGE_GAMMA:
        {
            inputFilter = [[GPUImageGammaFilter alloc] init];
        }; break;
        case GPUIMAGE_TONECURVE:
        {
            inputFilter = [[GPUImageToneCurveFilter alloc] init];
            [(GPUImageToneCurveFilter *)inputFilter setBlueControlPoints:[NSArray arrayWithObjects:[NSValue valueWithCGPoint:CGPointMake(0.0, 0.0)], [NSValue valueWithCGPoint:CGPointMake(0.5, 0.5)], [NSValue valueWithCGPoint:CGPointMake(1.0, 0.75)], nil]];
        }; break;
		case GPUIMAGE_HAZE:
        {
            inputFilter = [[GPUImageHazeFilter alloc] init];
        }; break;
		case GPUIMAGE_THRESHOLD:
        {
            inputFilter = [[GPUImageLuminanceThresholdFilter alloc] init];
        }; break;
		case GPUIMAGE_ADAPTIVETHRESHOLD:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageAdaptiveThresholdFilter alloc] init];
        }; break;
        case GPUIMAGE_CROP:
        {
            inputFilter = [[GPUImageCropFilter alloc] initWithCropRegion:CGRectMake(0.0, 0.0, 1.0, 0.25)];
        }; break;
            /*		case GPUIMAGE_MASK:
             {
             [(GPUImageFilter*)inputFilter setBackgroundColorRed:0.0 green:1.0 blue:0.0 alpha:1.0];
             }; break;*/
        case GPUIMAGE_TRANSFORM:
        {
            inputFilter = [[GPUImageTransformFilter alloc] init];
            [(GPUImageTransformFilter *)inputFilter setAffineTransform:CGAffineTransformMakeRotation(2.0)];
            //            [(GPUImageTransformFilter *)inputFilter setIgnoreAspectRatio:YES];
        }; break;
        case GPUIMAGE_TRANSFORM3D:
        {
            inputFilter = [[GPUImageTransformFilter alloc] init];
            CATransform3D perspectiveTransform = CATransform3DIdentity;
            perspectiveTransform.m34 = 0.4;
            perspectiveTransform.m33 = 0.4;
            perspectiveTransform = CATransform3DScale(perspectiveTransform, 0.75, 0.75, 0.75);
            perspectiveTransform = CATransform3DRotate(perspectiveTransform, 0.75, 0.0, 1.0, 0.0);
            
            [(GPUImageTransformFilter *)inputFilter setTransform3D:perspectiveTransform];
		}; break;
        case GPUIMAGE_SOBELEDGEDETECTION:
        {
            inputFilter = [[GPUImageSobelEdgeDetectionFilter alloc] init];
        }; break;
        case GPUIMAGE_XYGRADIENT:
        {
            inputFilter = [[GPUImageXYDerivativeFilter alloc] init];
        }; break;
            /*        case GPUIMAGE_HARRISCORNERDETECTION:
             {
             inputFilter = (GPUImageFilter*)[[GPUImageHarrisCornerDetectionFilter alloc] init];
             [(GPUImageHarrisCornerDetectionFilter *)inputFilter setThreshold:0.20];
             }; break;
             case GPUIMAGE_NOBLECORNERDETECTION:
             {
             inputFilter = (GPUImageFilter*)[[GPUImageNobleCornerDetectionFilter alloc] init];
             [(GPUImageNobleCornerDetectionFilter *)inputFilter setThreshold:0.20];
             }; break;
             case GPUIMAGE_SHITOMASIFEATUREDETECTION:
             {
             inputFilter = (GPUImageFilter*)[[GPUImageShiTomasiFeatureDetectionFilter alloc] init];
             [(GPUImageShiTomasiFeatureDetectionFilter *)inputFilter setThreshold:0.20];
             }; break;*/
        case GPUIMAGE_PREWITTEDGEDETECTION:
        {
            inputFilter = [[GPUImagePrewittEdgeDetectionFilter alloc] init];
        }; break;
        case GPUIMAGE_CANNYEDGEDETECTION:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageCannyEdgeDetectionFilter alloc] init];
        }; break;
            
        case GPUIMAGE_SKETCH:
        {
            inputFilter = [[GPUImageSketchFilter alloc] init];
        }; break;
        case GPUIMAGE_TOON:
        {
            inputFilter = [[GPUImageToonFilter alloc] init];
        }; break;
        case GPUIMAGE_SMOOTHTOON:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageSmoothToonFilter alloc] init];
        }; break;
        case GPUIMAGE_TILTSHIFT:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageTiltShiftFilter alloc] init];
            [(GPUImageTiltShiftFilter *)inputFilter setTopFocusLevel:0.4];
            [(GPUImageTiltShiftFilter *)inputFilter setBottomFocusLevel:0.6];
            [(GPUImageTiltShiftFilter *)inputFilter setFocusFallOffRate:0.2];
        }; break;
        case GPUIMAGE_CGA:
        {
            inputFilter = [[GPUImageCGAColorspaceFilter alloc] init];
        }; break;
            /*        case GPUIMAGE_CONVOLUTION:
             {
             inputFilter = [[GPUImage3x3ConvolutionFilter alloc] init];
             [(GPUImage3x3ConvolutionFilter *)inputFilter setConvolutionKernel:(GPUMatrix3x3){
             {-1.0f,  0.0f, 1.0f},
             {-2.0f, 0.0f, 2.0f},
             {-1.0f,  0.0f, 1.0f}
             }];
             
             }; break;*/
        case GPUIMAGE_EMBOSS:
        {
            inputFilter = [[GPUImageEmbossFilter alloc] init];
        }; break;
        case GPUIMAGE_POSTERIZE:
        {
            inputFilter = [[GPUImagePosterizeFilter alloc] init];
        }; break;
        case GPUIMAGE_SWIRL:
        {
            inputFilter = [[GPUImageSwirlFilter alloc] init];
        }; break;
        case GPUIMAGE_BULGE:
        {
            inputFilter = [[GPUImageBulgeDistortionFilter alloc] init];
        }; break;
        case GPUIMAGE_PINCH:
        {
            inputFilter = [[GPUImagePinchDistortionFilter alloc] init];
        }; break;
        case GPUIMAGE_STRETCH:
        {
            inputFilter = [[GPUImageStretchDistortionFilter alloc] init];
        }; break;
        case GPUIMAGE_DILATION:
        {
            inputFilter = [[GPUImageRGBDilationFilter alloc] initWithRadius:4];
		}; break;
        case GPUIMAGE_EROSION:
        {
            inputFilter = [[GPUImageRGBErosionFilter alloc] initWithRadius:4];
		}; break;
        case GPUIMAGE_OPENING:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageRGBOpeningFilter alloc] initWithRadius:4];
		}; break;
        case GPUIMAGE_CLOSING:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageRGBClosingFilter alloc] initWithRadius:4];
		}; break;
            
            /*        case GPUIMAGE_PERLINNOISE:
             {
             inputFilter = [[GPUImagePerlinNoiseFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_VORONI:
             {
             GPUImageJFAVoroniFilter *jfa = [[GPUImageJFAVoroniFilter alloc] init];
             [jfa setSizeInPixels:CGSizeMake(1024.0, 1024.0)];
             
             sourcePicture = [[GPUImagePicture alloc] initWithImage:[UIImage imageNamed:@"voroni_points2.png"]];
             
             [sourcePicture addTarget:jfa];
             
             inputFilter = [[GPUImageVoroniConsumerFilter alloc] init];
             
             [jfa setSizeInPixels:CGSizeMake(1024.0, 1024.0)];
             [(GPUImageVoroniConsumerFilter *)inputFilter setSizeInPixels:CGSizeMake(1024.0, 1024.0)];
             
             [videoCamera addTarget:filter];
             [jfa addTarget:filter];
             [sourcePicture processImage];
             
             }; break; */
        case GPUIMAGE_MOSAIC:
        {
            inputFilter = [[GPUImageMosaicFilter alloc] init];
            [(GPUImageMosaicFilter *)inputFilter setTileSet:@"Ornament1.png"];
            [(GPUImageMosaicFilter *)inputFilter setColorOn:NO];
            //[(GPUImageMosaicFilter *)inputFilter setTileSet:@"dotletterstiles.png"];
            //[(GPUImageMosaicFilter *)inputFilter setTileSet:@"curvies.png"];
            
            [inputFilter setInputRotation:kGPUImageRotateRight atIndex:0];
            
        }; break;
            /*        case GPUIMAGE_CHROMAKEY:
             {
             inputFilter = [[GPUImageChromaKeyBlendFilter alloc] init];
             [(GPUImageChromaKeyBlendFilter *)inputFilter setColorToReplaceRed:0.0 green:1.0 blue:0.0];
             }; break;*/
            /*        case GPUIMAGE_MULTIPLY:
             {
             inputFilter = [[GPUImageMultiplyBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_OVERLAY:
             {
             inputFilter = [[GPUImageOverlayBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_LIGHTEN:
             {
             inputFilter = [[GPUImageLightenBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_DARKEN:
             {
             inputFilter = [[GPUImageDarkenBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_DISSOLVE:
             {
             inputFilter = [[GPUImageDissolveBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_SCREENBLEND:
             {
             inputFilter = [[GPUImageScreenBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_COLORBURN:
             {
             inputFilter = [[GPUImageColorBurnBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_COLORDODGE:
             {
             inputFilter = [[GPUImageColorDodgeBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_EXCLUSIONBLEND:
             {
             inputFilter = [[GPUImageExclusionBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_DIFFERENCEBLEND:
             {
             inputFilter = [[GPUImageDifferenceBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_SUBTRACTBLEND:
             {
             inputFilter = [[GPUImageSubtractBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_HARDLIGHTBLEND:
             {
             inputFilter = [[GPUImageHardLightBlendFilter alloc] init];
             }; break;
             case GPUIMAGE_SOFTLIGHTBLEND:
             {
             inputFilter = [[GPUImageSoftLightBlendFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_CUSTOM:
             {
             inputFilter = [[GPUImageFilter alloc] initWithFragmentShaderFromFile:@"CustomFilter"];
             }; break;*/
            /*        case GPUIMAGE_KUWAHARA:
             {
             inputFilter = [[GPUImageKuwaharaFilter alloc] init];
             }; break;*/
            
        case GPUIMAGE_VIGNETTE:
        {
            inputFilter = [[GPUImageVignetteFilter alloc] init];
        }; break;
        case GPUIMAGE_GAUSSIAN:
        {
            inputFilter = [[GPUImageGaussianBlurFilter alloc] init];
        }; break;
        case GPUIMAGE_FASTBLUR:
        {
            inputFilter = [[GPUImageFastBlurFilter alloc] init];
		}; break;
        case GPUIMAGE_BOXBLUR:
        {
            inputFilter = [[GPUImageBoxBlurFilter alloc] init];
		}; break;
        case GPUIMAGE_MEDIAN:
        {
            inputFilter = [[GPUImageMedianFilter alloc] init];
		}; break;
        case GPUIMAGE_GAUSSIAN_SELECTIVE:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageGaussianSelectiveBlurFilter alloc] init];
            [(GPUImageGaussianSelectiveBlurFilter*)inputFilter setExcludeCircleRadius:40.0/320.0];
        }; break;
        case GPUIMAGE_BILATERAL:
        {
            inputFilter = [[GPUImageBilateralFilter alloc] init];
        }; break;
        case GPUIMAGE_FILTERGROUP:
        {
            inputFilter = (GPUImageFilter*)[[GPUImageFilterGroup alloc] init];
            
            GPUImageSepiaFilter *sepiainputFilter = [[GPUImageSepiaFilter alloc] init];
            [(GPUImageFilterGroup *)inputFilter addFilter:sepiainputFilter];
            
            GPUImagePixellateFilter *pixellateinputFilter = [[GPUImagePixellateFilter alloc] init];
            [(GPUImageFilterGroup *)inputFilter addFilter:pixellateinputFilter];
            
            [sepiainputFilter addTarget:pixellateinputFilter];
            [(GPUImageFilterGroup *)inputFilter setInitialFilters:[NSArray arrayWithObject:sepiainputFilter]];
            [(GPUImageFilterGroup *)inputFilter setTerminalFilter:pixellateinputFilter];
        }; break;
        case GPUIMAGE_POLKADOT:
        {
            inputFilter = [[GPUImagePolkaDotFilter alloc] init];
        }; break;
        case GPUIMAGE_HALFTONE:
        {
            inputFilter = [[GPUImageHalftoneFilter alloc] init];
        }; break;
        case GPUIMAGE_LEVELS:
        {
            inputFilter = [[GPUImageLevelsFilter alloc] init];
        }; break;
        case GPUIMAGE_MONOCHROME:
        {
            inputFilter = [[GPUImageMonochromeFilter alloc] init];
            [(GPUImageMonochromeFilter *)filter setColor:(GPUVector4){0.0f, 0.0f, 1.0f, 1.f}];
        }; break;
        case GPUIMAGE_HUE:
        {
            inputFilter = [[GPUImageHueFilter alloc] init];
        }; break;
        case GPUIMAGE_WHITEBALANCE:
        {
            inputFilter = [[GPUImageWhiteBalanceFilter alloc] init];
        }; break;
            /*        case GPUIMAGE_LOWPASS:
             {
             inputFilter = [[GPUImageLowPassFilter alloc] init];
             }; break;
             case GPUIMAGE_HIGHPASS:
             {
             inputFilter = [[GPUImageHighPassFilter alloc] init];
             }; break;*/
            /*        case GPUIMAGE_MOTIONDETECTOR:
             {
             inputFilter = [[GPUImageMotionDetector alloc] init];
             }; break;*/
            /*        case GPUIMAGE_THRESHOLDSKETCH:
             {
             inputFilter = [[GPUImageThresholdSketchFilter alloc] init];
             }; break;*/
        case GPUIMAGE_SPHEREREFRACTION:
        {
            inputFilter = [[GPUImageSphereRefractionFilter alloc] init];
        }; break;
        case GPUIMAGE_GLASSSPHERE:
        {
            inputFilter = [[GPUImageGlassSphereFilter alloc] init];
        }; break;
            /*        case GPUIMAGE_HIGHLIGHTSHADOW:
             {
             inputFilter = [[GPUImageHighlightShadowFilter alloc] init];
             }; break;*/
        case GPUIMAGE_LOCALBINARYPATTERN:
        {
            inputFilter = [[GPUImageLocalBinaryPatternFilter alloc] init];
        }; break;
            
            
        default: 
            /*            inputFilter = nil; 
             currentfiltertype = GPUIMAGE_NONE;
             [videoCamera addTarget:textureOutput];
             return;*/
            inputFilter = [[GPUImageSaturationFilter alloc] init];
            break;
    }
    return inputFilter;
}

- (void)setCameraFilter:(GPUImageFilterType)filterType
{
    if(inputFilter == nil)
    {
        [videoCamera removeTarget:textureOutput];
    }
    else
    {
        [videoCamera removeTarget:inputFilter];
        [inputFilter removeTarget:textureOutput];
    }

    currentfiltertype = filterType;
    
    inputFilter = [self createFilter:filterType];
    [inputFilter addTarget:textureOutput];
    [videoCamera addTarget:inputFilter];
//    [videoCamera addTarget:filter];
        
//    GPUImageView *filterView = (GPUImageView *)self.view;
//    [filter addTarget:filterView];

}
#endif

// Post iOS 7.0 to hide statusbar for sure
- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (void)awakeFromNib
{
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	// path to the default font
    g_storagePath = [resourcePath UTF8String];
//    errorfontPath = storagePath + "/arial.ttf";
#ifndef UISTRINGS
    errorfontPath = storagePath + "/DroidSansMono.ttf";
#else
    errorfontPath = "Helvetica";
#endif
    
    // IOS 7
    //viewDidload
    if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)]) {
        // iOS 7
        [self prefersStatusBarHidden];
        [self performSelector:@selector(setNeedsStatusBarAppearanceUpdate)];
    } else {
        // iOS 6
        // Hide top navigation bar
        [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];
        //        [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationSlide];
    }
    
    

    
//	[[UIApplication sharedApplication] setStatusBarHidden:YES animated:NO];
	// To notes here: First I also added this to info.plist to make it vanish faster, which just looks nicer.
	// More importantly there is a bug with the statusbar still intercepting when hidden.
	// For that purpose I enabled landscapemode in info.plist. This seems to remove the problem and has no negative side-effect I could find.
	// Now one can enter the touch area from both sides without problems. (gessl 11/9/09)
	
	// Setup accelerometer collection
    [UIAccelerometer sharedAccelerometer].delegate = self;
    [UIAccelerometer sharedAccelerometer].updateInterval = ACCELEROMETER_RATE;
	// Set up the ability to track multiple touches.
	[self setMultipleTouchEnabled:YES];
	self.multipleTouchEnabled = YES;

	float version = [[[UIDevice currentDevice] systemVersion] floatValue];
	
	if (version >= 4.0)
    {
		// setup the gyroscope collection
		motionManager = [[CMMotionManager alloc] init];
 		motionManager.deviceMotionUpdateInterval = 1.0/60.0;
/*		motionManager.gyroUpdateInterval = 1.0/60.0;
		
		if (motionManager.gyroAvailable) {
			opQ = [[NSOperationQueue currentQueue] retain];
			CMGyroHandler gyroHandler = ^ (CMGyroData *gyroData, NSError *error) {
				CMRotationRate rotate = gyroData.rotationRate;
				// handle rotation-rate data here......
				float rate_x = rotate.x/7.0;//128.0;
				float rate_y = rotate.y/7.0;//128.0;
				float rate_z = rotate.z/7.0;//128.0;
				
	//			float heading_north = ([heading trueHeading]-180.0)/180.0;
				
				// lua API events
				callAllOnRotRate(rate_x, rate_y, rate_z);
				// UrSound pipeline
				callAllGyroSources(rate_x, rate_y, rate_z);
				
			};
			[motionManager startGyroUpdatesToQueue:[NSOperationQueue currentQueue]
									   withHandler: gyroHandler];
			
		} else*/
        
        if (motionManager.deviceMotionAvailable) {
			CMDeviceMotionHandler deviceMotionHandler = ^ (CMDeviceMotion *motion, NSError *error) {
                CMRotationRate rotate = motion.rotationRate;
                // handle rotation-rate data here......
                float rate_x = rotate.x/7.0;//128.0;
                float rate_y = rotate.y/7.0;//128.0;
                float rate_z = rotate.z/7.0;//128.0;
                
                //			float heading_north = ([heading trueHeading]-180.0)/180.0;
                
                // lua API events
                callAllOnRotRate(rate_x, rate_y, rate_z);
                // UrSound pipeline
                callAllGyroSources(rate_x, rate_y, rate_z);
                CMAttitude *attitude = motion.attitude;
                callAllOnAttitude(attitude.quaternion.x,attitude.quaternion.y,attitude.quaternion.z,attitude.quaternion.w);
            };
            [motionManager startDeviceMotionUpdatesToQueue:[NSOperationQueue currentQueue]
                                               withHandler: deviceMotionHandler];
//            [motionManager startDeviceMotionUpdates];
        }
        else {
	//        NSLog(@"No gyroscope on device.");
			[motionManager release];
		}
        
    }
	
	// setup the location manager
	self.locationManager = [[[CLLocationManager alloc] init] autorelease];
	
	// check if the hardware has a compass
	if ([[locationManager class] headingAvailable] == NO) { // Fixed for post ios 4.0
		// No compass is available. This application cannot function without a compass, 
        // so a dialog will be displayed and no magnetic data will be measured.
 //       self.locationManager = nil;
		// Disable compass flowboxes in this case. TODO
	} else {
        // heading service configuration
        locationManager.headingFilter = kCLHeadingFilterNone;
        
        
        // setup delegate callbacks
        locationManager.delegate = self;
        // start the compass
        [locationManager startUpdatingHeading];
		
    }
    // We got location service?
    if([[locationManager class] locationServicesEnabled]==YES)
    {
        // location service configuration
        locationManager.distanceFilter = kCLDistanceFilterNone;
        locationManager.desiredAccuracy = kCLLocationAccuracyBest;
        // setup delegate callbacks
        locationManager.delegate = self;
        // start the GPS
        [locationManager startUpdatingLocation];
    }
    

    
    if ([UIImagePickerController isSourceTypeAvailable: UIImagePickerControllerSourceTypeCamera])
        // Has camera
	{
#ifdef USECAMERA
#ifdef GPUIMAGE

    videoCamera = [[GPUImageVideoCamera alloc] initWithSessionPreset:AVCaptureSessionPreset640x480 cameraPosition:AVCaptureDevicePositionBack];
    videoCamera.outputImageOrientation = UIInterfaceOrientationPortrait;
//    videoCamera.outputImageOrientation = UIInterfaceOrientationLandscapeLeft;
    inputFilter = nil;

    textureOutput = [[GPUImageTextureOutput alloc] init];
    textureOutput.delegate = self;
    
    inputFilter = [[GPUImageSaturationFilter alloc] init];
    averageFilter = [[GPUImageAverageColor alloc] init];
    [averageFilter setColorAverageProcessingFinishedBlock:^(CGFloat redComponent, CGFloat greenComponent, CGFloat blueComponent, CGFloat alphaComponent, CMTime frameTime) {
        callAllCameraSources((redComponent+greenComponent+blueComponent)/3.0,blueComponent,greenComponent,redComponent, 0.0);
//        NSLog(@"Average color: %f, %f, %f, %f", redComponent, greenComponent, blueComponent, alphaComponent);
    }];
    
    [videoCamera addTarget:inputFilter];
    [inputFilter addTarget:textureOutput];
    [inputFilter addTarget:averageFilter];
//    [videoCamera addTarget:textureOutput];
    currentfiltertype = GPUIMAGE_NONE;
    
#else /* Non GPUImage camera handlibng */
	// Initiate the camera initiation sequence. <-- Whoa.
	captureManager = [[CaptureSessionManager alloc] init];
	captureManager.delegate = self;
	[captureManager addVideoInput];
	[captureManager addVideoDataOutput];
	[captureManager autoWhiteBalanceAndExposure:0];
    camerause = 0;
//	[captureManager.captureSession startRunning];
//    [captureManager.captureSession stopRunning];
#endif
#endif	
    }
	//Create and advertise networking and discover others
//	[self setup];
	[self setupNetConnects];
    
	mytimer = new MachTimer();
	mytimer->start();
	
#ifdef LATE_LAUNCH
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urMus.lua"];
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
	
	// start off http server
	http_start([resourcePath UTF8String],
			   [documentPath UTF8String]);
	
	const char* filestr = [filePath UTF8String];
	
	if(luaL_dofile(lua, filestr)!=0)
	{
		const char* error = lua_tostring(lua, -1);
		errorstr = error; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
	}
#endif
}

#define TEST_CAMERA

GLuint	cameraTexture= 0;
static bool cameraBeingUsedAsBrush = false;

- (void)newCameraTextureForDisplay:(GLuint)texture {

	_cameraTexture = texture;
	cameraTexture = texture;
}


- (void)accelerometer:(UIAccelerometer *)accelerometer didAccelerate:(UIAcceleration *)acceleration
{
	// This feeds the lua API events
	callAllOnAccelerate(acceleration.x, acceleration.y, acceleration.z);

	// We call the UrSound pipeline second so that the lua engine can actually change it based on acceleration data before anything happens.
	callAllAccelerateSources(acceleration.x, acceleration.y, acceleration.z);
}

// This delegate method is invoked when the location manager has heading data.
- (void)locationManager:(CLLocationManager *)manager didUpdateHeading:(CLHeading *)heading {

	float heading_x = heading.x/128.0;
	float heading_y = heading.y/128.0;
	float heading_z = heading.z/128.0;
	
	float heading_north = ([heading trueHeading]-180.0)/180.0;
	
	// lua API events
	callAllOnHeading(heading_x, heading_y, heading_z, heading_north);
	// UrSound pipeline
	callAllCompassSources(heading_x, heading_y, heading_z, heading_north);
}

// TODO depricated with 6.0 gessl NYI
- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation {
	CLLocationDegrees  latitude = newLocation.coordinate.latitude;
	CLLocationDegrees longitude = newLocation.coordinate.longitude;
	
	float loc_latitude = latitude/180.0; // Normalize!
	float loc_longitude = longitude/180.0; // Normalize!
	
	// lua API events
	callAllOnLocation(loc_latitude, loc_longitude);
	// UrSound pipeline
	callAllLocationSources(loc_latitude, loc_longitude);
}

// This delegate method is invoked when the location managed encounters an error condition.
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error {
    if ([error code] == kCLErrorDenied) {
        // This error indicates that the user has denied the application's request to use location services.
        [manager stopUpdatingHeading];
    } else if ([error code] == kCLErrorHeadingFailure) {
        // This error indicates that the heading could not be determined, most likely because of strong magnetic interference.
    }
}

static EAGLSharegroup* theSharegroup = nil;

#ifdef OPENGLES2

- (BOOL)linkProgram:(GLuint)prog
{
    GLint status;
    glLinkProgram(prog);
    
#if defined(DEBUG)
    GLint logLength;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program link log:\n%s", log);
        free(log);
    }
#endif
    
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == 0) {
        return NO;
    }
    
    return YES;
}

- (BOOL)validateProgram:(GLuint)prog
{
    GLint logLength, status;
    
    glValidateProgram(prog);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program validate log:\n%s", log);
        free(log);
    }
    
    glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
    if (status == 0) {
        return NO;
    }
    
    return YES;
}

- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file
{
    GLint status;
    const GLchar *source;
    
    source = (GLchar *)[[NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil] UTF8String];
    if (!source) {
        NSLog(@"Failed to load vertex shader");
        return NO;
    }
#if defined(SHADERDEBUG)
    else
        NSLog(@"Source:\n%s",source);
#endif
    
    *shader = glCreateShader(type);
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glShaderSource(*shader, 1, &source, NULL);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glCompileShader(*shader);
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    GLint logLength=0;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(*shader, logLength, &logLength, log);
        NSLog(@"Shader compile log:\n%s", log);
        free(log);
    }
    
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        glDeleteShader(*shader);
        return NO;
    }
    
    return YES;
}


- (void)setupShaders
{
    if(!shaderProgram)
    {
    NSString *vertShaderPathname, *fragShaderPathname;
    GLuint vertShader=0, fragShader=0;

    // Create and compile vertex shader.
    vertShaderPathname = [[NSBundle mainBundle] pathForResource:@"Shader" ofType:@"vsh"];
    if (![self compileShader:&vertShader type:GL_VERTEX_SHADER file:vertShaderPathname]) {
        NSLog(@"Failed to compile vertex shader");
        assert(0);
        return;
//        return NO;
    }
    
    // Create and compile fragment shader.
    fragShaderPathname = [[NSBundle mainBundle] pathForResource:@"Shader" ofType:@"fsh"];
    if (![self compileShader:&fragShader type:GL_FRAGMENT_SHADER file:fragShaderPathname]) {
        NSLog(@"Failed to compile fragment shader");
        assert(0);
        return;
//        return NO;
    }
    // Create shaders, link in source and compile
    shaderProgram = glCreateProgram();
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    // create program, attack, link and use
//    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertShader);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glAttachShader(shaderProgram, fragShader);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

    glBindAttribLocation(shaderProgram, ATTRIB_VERTEX, "a_Position");
    glBindAttribLocation(shaderProgram, ATTRIB_COLOR, "a_Color");
    glBindAttribLocation(shaderProgram, ATTRIB_TEXTUREPOSITON, "a_TexCoord");

    // Link program.
    if (![self linkProgram:shaderProgram]) {
        NSLog(@"Failed to link program: %d", shaderProgram);
        
        if (vertShader) {
            glDeleteShader(vertShader);
            vertShader = 0;
        }
        if (fragShader) {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (shaderProgram) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
        
        assert(0);
        return;
//        return NO;
    }
//    glLinkProgram(shaderProgram);

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    // Validate program
    glValidateProgram(shaderProgram);
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#if defined(SHADERDEBUG)
    GLint idx1,idx2,idx3 = -2;
    idx1 = glGetAttribLocation(shaderProgram,"a_Position");
    idx2 = glGetAttribLocation(shaderProgram,"a_Color");
    idx3 = glGetAttribLocation(shaderProgram,"a_TexCoord");
    

    GLint infoLength=0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLength);
    
    if (infoLength > 0)
    {
        
        char infoLog[512];
        
        glGetProgramInfoLog(shaderProgram, infoLength, NULL, infoLog);
        NSLog(@"Program compile log:\n%s", infoLog);
    }
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif
        
    _textureUniform = glGetUniformLocation(shaderProgram, "u_Texture");
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	_modelviewprojUniform = glGetUniformLocation(shaderProgram, "u_ModelViewProjMatrix");
    uniformColour = glGetUniformLocation(shaderProgram, "colour");

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

    // Release vertex and fragment shaders.
    if (vertShader)
        glDeleteShader(vertShader);
    if (fragShader)
        glDeleteShader(fragShader);

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

    glUseProgram(shaderProgram);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    int total = -1;
    glGetProgramiv( shaderProgram, GL_ACTIVE_UNIFORMS, &total ); 
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#if defined(SHADERDEBUG)
    for(int i=0; i<total; ++i)  {
        int name_len=-1, num=-1;
        GLenum type = GL_ZERO;
        char name[100];
        glGetActiveUniform( shaderProgram, GLuint(i), sizeof(name)-1,
                           &name_len, &num, &type, name );
        name[name_len] = 0;
        GLuint location = glGetUniformLocation( shaderProgram, name );
        NSLog(@"Active uniforms:\n%s", name);
    }
#endif
    
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
        GLubyte whitepixel[4] = {255,255,255,255};
        
        glGenTextures( 1, &whiteTexture);
        glBindTexture(GL_TEXTURE_2D, whiteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitepixel);

    }
}
#endif

- (EAGLContext*)createContext
{
    //EAGLContext* context = nil;
	
    if (theSharegroup)
    {
#ifdef OPENGLES2
        context = [[EAGLContext alloc] 
				   initWithAPI:kEAGLRenderingAPIOpenGLES2
				   sharegroup:theSharegroup];
#else
        context = [[EAGLContext alloc] 
				   initWithAPI:kEAGLRenderingAPIOpenGLES1
				   sharegroup:theSharegroup];
#endif
		displaynumber = 2;
        [self setFramebuffer];
        [self setupView];
        [self setupShaders];
    }
    else
    {
#ifdef OPENGLES2
#ifdef GPUIMAGE
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:[[[GPUImageOpenGLESContext sharedImageProcessingOpenGLESContext] context] sharegroup]];
#else
        context = [[EAGLContext alloc]
				   initWithAPI:kEAGLRenderingAPIOpenGLES2];
#endif
		displaynumber = 1;
        [self setFramebuffer];
        [self setupView];
        [self setupShaders];

/*
 //        const GLfloat zNear = 1, zFar = 600, fieldOfView = 40*M_PI/180.0;
        
        esMatrixLoadIdentity(&projection);
//        GLfloat size = zNear * tanf(fieldOfView / 2.0); 
        esOrtho(&projection, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
//        glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
  //      esFrustum(&projection, -size, size, -size / (SCREEN_WIDTH / SCREEN_HEIGHT), size / (SCREEN_WIDTH / SCREEN_HEIGHT), zNear, zFar); 
        
        esMatrixLoadIdentity(&modelView);
        // translate back into the screen by 6 units and down by 1 unit  
        esTranslate(&modelView, 0.0f , -1.0f, -6.0f);  
//        glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
        esRotate(&modelView, 0.0f, 0.0f, 0.0f, 1.0f);
        glUseProgram(shaderProgram);
 */
#else
        context = [[EAGLContext alloc]
				   initWithAPI:kEAGLRenderingAPIOpenGLES1];
#endif
//#endif
        theSharegroup = context.sharegroup;
		displaynumber = 1;
    }
	
    return context;
}

- (id)initWithCoder2:(NSCoder*)coder
{
    self = [super initWithCoder:coder];
	if (self)
    {
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = @{
                                         kEAGLDrawablePropertyRetainedBacking: [NSNumber numberWithBool:YES],
                                         kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
                                         };
//        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
//                                        [NSNumber numberWithBool:YES], kEAGLDrawablePropertyRetainedBacking,
//                                        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
//                                        nil];
    }
    
    return self;
}




//The GL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithCoder:(NSCoder*)coder {
    
    if ((self = [super initWithCoder:coder])) {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = @{
                                         kEAGLDrawablePropertyRetainedBacking: [NSNumber numberWithBool:YES],
                                         kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
                                         };
        
		context = [self createContext];
		//       context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
        
        if (!context || ![EAGLContext setCurrentContext:context]) {
            [self release];
            return nil;
        }
 
#ifdef OPENGLES2
        if ([context API] == kEAGLRenderingAPIOpenGLES2) {
            [self setFramebuffer];
            [self setupView];
            [self setupShaders];
        }
#endif
        animationInterval = 1.0 / 60.0; // We look for 60 FPS

        ExternalKeyboardReaderView *control = [[ExternalKeyboardReaderView alloc] initWithFrame:CGRectZero];
        [self addSubview:control];
        control.active = YES;
        control.delegate = self;
        [control release];
    }
	
	// Set up the ability to track multiple touches.
	[self setMultipleTouchEnabled:YES];
	self.multipleTouchEnabled = YES;

	return self;
}

- (id)initWithFrame:(CGRect)frame {
	if ((self = [super initWithFrame:frame])) {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = @{
                                         kEAGLDrawablePropertyRetainedBacking: [NSNumber numberWithBool:YES],
                                         kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
                                         };
        
		context = [self createContext];
		//       context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
        
        if (!context || ![EAGLContext setCurrentContext:context]) {
            [self release];
            return nil;
        }
        
#ifdef OPENGLES2
        if ([context API] == kEAGLRenderingAPIOpenGLES2) {
            [self setFramebuffer];
            [self setupView];
            [self setupShaders];
        }
#endif
        animationInterval = 1.0 / 60.0; // We look for 60 FPS
		
        ExternalKeyboardReaderView *control = [[ExternalKeyboardReaderView alloc] initWithFrame:CGRectZero];
        [self addSubview:control];
        control.active = YES;
        control.delegate = self;
        [control release];
    }
	
	// Set up the ability to track multiple touches.
	[self setMultipleTouchEnabled:YES];
	self.multipleTouchEnabled = YES;
	return self;
}
		
- (id)initWithFrame:(CGRect)frame andContextSharegroup:(EAGLContext*)passedContext {
    
    NSLog(@"Special initWithFrame:andContextSharegroup: call");
    
	if ((self = [super initWithFrame:frame])) {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = @{
                                         kEAGLDrawablePropertyRetainedBacking: [NSNumber numberWithBool:YES],
                                         kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
                                         };
        
		//context = passedContext;
		//       context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
        
//        context = [[EAGLContext alloc] 
//				   initWithAPI:kEAGLRenderingAPIOpenGLES1
//				   sharegroup:passedContext.sharegroup];
		context = [self createContext];

//        context = passedContext;
		displaynumber = 2;
        
        if (!context || ![EAGLContext setCurrentContext:context]) {
            [self release];
            return nil;
        }
        
        animationInterval = 1.0 / 60.0; // We look for 60 FPS
		
    }
	
	return self;
}


// Hard-wired screen dimension constants. This will soon be system-dependent variable!
int SCREEN_WIDTH = 320;
int SCREEN_HEIGHT = 480;
int TOUCH_SCREEN_WIDTH = 320; // To handle oriented touch
int TOUCH_SCREEN_HEIGHT = 480;
int HALF_SCREEN_WIDTH = 160;
int HALF_SCREEN_HEIGHT = 240;

int EXT_SCREEN_WIDTH = 320;
int EXT_SCREEN_HEIGHT = 480;

bool portrait = true;

//#define SCREEN_WIDTH 320
//#define SCREEN_HEIGHT 480

// Enables/Disables that error and DPrint texture is rendered. Should always be on really.
#define RENDERERRORSTRTEXTUREFONT
// Enables/Disables debug output for multi-touch debugging. Should be always off now.
#undef DEBUG_TOUCH

// Various texture font strongs
#ifdef UISTRINGS
NSString *textlabelstr = @"";
NSString *fontname = @"";
NSString *texturepathstr; // = @"Ship.png";
#else
string textlabelstr = "";
string fontname = "";
NSString *texturepathstr; // = @"Ship.png";
//string texturepathstr; // = @"Ship.png";
#endif
// Below is modeled after GLPaint

#define kBrushOpacity		(1.0 / 3.0)
#define kBrushPixelStep		3
#define kBrushScale			2
#define kLuminosity			0.75
#define kSaturation			1.0

static Texture2D* brushtexture = NULL;
static urAPI_Texture* brushtexturedata = NULL;
static float brushsize = 1;

// 2D Painting functionality

// Brush handling

void SetBrushAsCamera(bool s) {
    cameraBeingUsedAsBrush = s;
}

bool UsesTextureBrush()
{
    if(brushtexture)
        return true;
    else
        return false;
}

void SetBrushTexture(urAPI_Texture * t)
{
	brushtexture = t->backgroundTex;
	brushsize = brushtexture.pixelsWide;
    brushtexturedata = t;
}

/*void SetBrushTexture(Texture2D * texture)
{
	brushtexture = texture;
	brushsize = texture.pixelsWide;
}*/

void SetBrushSize(float size)
{
	brushsize = size;
}

void ClearBrushTexture()
{
	brushtexture = NULL;
	brushsize = 1;
}

float BrushSize()
{
	return brushsize;
}

#ifdef OPENGLES2
ESMatrix *matrixstack;
int stacksize = 1;
int maxstack = 1;

- (void) urInitMatrixStack
{
    matrixstack = (ESMatrix*)malloc(sizeof(ESMatrix));
    matrixstack[0] = modelView;
}

-(void) urLoadIdentity
{
    esMatrixLoadIdentity(&modelView);
    // translate back into the screen by 6 units and down by 1 unit
    esMatrixMultiply(&mvp, &modelView, &projection );
}

void urLoadIdentity()
{
    [g_glView urLoadIdentity];
}

- (void) urPushMatrix
{
//    glUseProgram(shaderProgram);
    stacksize++;
    if(stacksize > maxstack)
    {
        matrixstack = (ESMatrix*)realloc(matrixstack,sizeof(ESMatrix)*stacksize);
        maxstack++;
    }
    
    matrixstack[stacksize-1] = modelView;
    
//    esMatrixLoadIdentity(&modelView);
}

void urPushMatrix()
{
	[g_glView urPushMatrix];
}

- (void) urPopMatrix
{
    modelView = matrixstack[stacksize-1];
    stacksize--;
    
    // reset our modelview
    esMatrixMultiply(&mvp, &modelView, &projection );
    glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
}

void urPopMatrix()
{
	[g_glView urPopMatrix];
}

- (void) urTranslatef:(GLfloat) x withY:(GLfloat) y withZ:(GLfloat) z
{
    esTranslate(&modelView, x, y, z);
    esMatrixMultiply(&mvp, &modelView, &projection );
    glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
}

void urTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	[g_glView urTranslatef:x withY:y withZ:z];
}

- (void) urScalef:(GLfloat) sx withY:(GLfloat) sy withZ:(GLfloat) sz
{
    esScale(&modelView, sx,sy,sz);
    esMatrixMultiply(&mvp, &modelView, &projection );
    glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
}

void urScalef(GLfloat sx, GLfloat sy, GLfloat sz)
{
    [g_glView urScalef:sx withY:sy withZ:sz];

}

- (void) urRotatef:(GLfloat)angle withX:(GLfloat) x withY:(GLfloat) y withZ:(GLfloat) z
{
    esRotate(&modelView, angle, x, y, z);
    esMatrixMultiply(&mvp, &modelView, &projection );
    glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );

}

void urRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    [g_glView urRotatef:angle withX:x withY:y withZ:z];
}
#endif


void SetupBrush()
{
    GLenum err = glGetError();
	if(brushtexture != NULL || cameraBeingUsedAsBrush)
	{

#ifdef OPENGLES2
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#endif
        
        if (cameraBeingUsedAsBrush) {
            glBindTexture(GL_TEXTURE_2D, cameraTexture);
        } else {
            glBindTexture(GL_TEXTURE_2D, brushtexture.name);
        }
        
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glDisable(GL_DITHER);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#ifndef OPENGLES2
        glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#endif
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Multiplicative "blending", color of background mixes with brush alpha. Can never saturate if either one or the other is not 1.
#ifndef OPENGLES2
		glEnable(GL_TEXTURE_2D);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
//		glDisable(GL_BLEND);
        
		// Make the current material colour track the current color
//		glEnable( GL_COLOR_MATERIAL );
		// Multiply the texture colour by the material colour.
//		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
/*		switch(t->texture->blendmode)
		{
			case BLEND_DISABLED:
				glDisable(GL_BLEND);
				break;
			case BLEND_BLEND:
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case BLEND_ALPHAKEY:
				// NYI
				glAlphaFunc(GL_GEQUAL, 0.5f); // UR! This may be different
				glEnable(GL_ALPHA_TEST);
				break;
			case BLEND_ADD:
				glBlendFunc(GL_ONE, GL_ONE);
				break;
			case BLEND_MOD:
				glBlendFunc(GL_DST_COLOR, GL_ZERO);
				break;
			case BLEND_SUB: // Experimental blend category. Can be changed wildly NYI marking this for revision.
				glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
				break;
		}*/
//		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Additive "blending", color keeps being applied weight by alpha of the brush 
//		glDisable(GL_BLEND); // Solid color mode
		glEnable(GL_POINT_SPRITE_OES);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glTexEnvf(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#endif
	}	
	else
	{
        GLenum err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glEnable(GL_DITHER);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#ifdef OPENGLES2
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        glBindTexture(GL_TEXTURE_2D, g_glView->whiteTexture);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#else
		glDisable(GL_TEXTURE_2D);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#endif
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Additive "blending", color keeps being applied weight by alpha of the brush 
//		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Multiplicative "blending", color of background mixes with brush alpha. Can never saturate if either one or the other is not 1.
//		glDisable(GL_BLEND); // Solid color mode
#ifndef OPENGLES2
        glDisable(GL_POINT_SPRITE_OES);
#endif
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
	}
#ifndef OPENGLES2
	glPointSize(brushsize);
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif
}

GLuint textureFrameBuffer=-1;
GLuint bgtextureFrameBuffer=-1;
GLuint bgname=-1;

#ifndef LEGACY42
#define RENDERTOTEXTURE
#endif

void CreateFrameBuffer()
{
	// create framebuffer
	glGenFramebuffersOES(1, &textureFrameBuffer);
}

// Joint opengl functions to set up brush drawing

#ifdef OPENGLES2
GLubyte pointColors[4*4];
GLfloat coordinates[8];

void initBrushColor(urAPI_Texture* texture)
{
    for(int i=0;i<4;i++)
    {
        pointColors[i*4+0]=texture->texturebrushcolor[0];
        pointColors[i*4+1]=texture->texturebrushcolor[1];
        pointColors[i*4+2]=texture->texturebrushcolor[2];
        pointColors[i*4+3]=texture->texturebrushcolor[3];
    }
    
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, pointColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
}

void initBrushRotation()
{
/*   GLfloat coordinates[] = {
        brushtexturedata->texcoords[0], brushtexturedata->texcoords[1],
        brushtexturedata->texcoords[2], brushtexturedata->texcoords[3],
        brushtexturedata->texcoords[4], brushtexturedata->texcoords[5],
        brushtexturedata->texcoords[6], brushtexturedata->texcoords[7]
    };*/
    
    coordinates[0] = brushtexturedata->texcoords[0];
    coordinates[1] = brushtexturedata->texcoords[1];
    coordinates[2] = brushtexturedata->texcoords[2];
    coordinates[3] = brushtexturedata->texcoords[3];
    coordinates[4] = brushtexturedata->texcoords[4];
    coordinates[5] = brushtexturedata->texcoords[5];
    coordinates[6] = brushtexturedata->texcoords[6];
    coordinates[7] = brushtexturedata->texcoords[7];
    
    /*
     GLfloat         coordinates[] = { 
     0,  0, 
     1.0f,  0,
     0,              1.0f,
     1.0f,    1.0f,
     };
     */
    
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0, coordinates);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
}
#endif

// Render point drawing into a texture

void drawPointToTexture(urAPI_Texture_t *texture, float x, float y)
{
    [EAGLContext setCurrentContext:g_glView->context];
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);
	
#ifndef OPENGLES2
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
	Texture2D *bgtexture = texture->backgroundTex;
//	y = texture->backgroundTex->_height - y;
	y = SCREEN_HEIGHT - y;

	// allocate frame buffer
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	// bind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
	
	// attach renderbuffer
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, bgtexture.name, 0);
	
	SetupBrush();
	
    GLenum err = glGetError();
#ifdef OPENGLES2
    glUseProgram(g_glView->shaderProgram);
    
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
//    glDisable(GL_BLEND);
//    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Additive "blending", color keeps being applied weight by alpha of the brush 
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Multiplicative "blending", color of background mixes with brush alpha. 
    
//    glActiveTexture(GL_TEXTURE0);
//    glUniform1i(_textureUniform,0);
//    glBindTexture(GL_TEXTURE_2D, g_glView->whiteTexture);
    
    initBrushColor(texture);
#else
	glDisableClientState(GL_COLOR_ARRAY);
	glColor4ub(texture->texturebrushcolor[0], texture->texturebrushcolor[1], texture->texturebrushcolor[2], texture->texturebrushcolor[3]);		
#endif

	
	//Render the vertex array
#ifdef OPENGLES2    
/*    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, vertexBuffer);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    //                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
    //                        glEnableVertexAttribArray(ATTRIB_VERTEX);
    // set the position vertex attribute with our quads vertices
 */
	const GLfloat pointVertices[] = {
		x-brushsize/2, y-brushsize/2, 0.0f,
		x+brushsize/2, y-brushsize/2, 0.0f,
		x-brushsize/2, y+brushsize/2, 0.0f,
		x+brushsize/2, y+brushsize/2, 0.0f
	};
    glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, pointVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    initBrushRotation();
    
    // and finally tell the GPU to draw our triangle!
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);		
#else
	static GLfloat		vertexBuffer[2];
	
	vertexBuffer[0] = (int)x;
	vertexBuffer[1] = (int)y;

    glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
	glDrawArrays(GL_POINTS, 0, 1);
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

	// unbind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
}

int prepareBrushedLine(float startx, float starty, float endx, float endy, int vertexCount, int vertexMax, GLfloat* vertexBuffer)
{
	NSUInteger	count, i;

	//Add points to the buffer so there are drawing points every X pixels
	count = MAX(ceilf(sqrtf((endx - startx) * (endx - startx) + (endy - starty) * (endy - starty)) / kBrushPixelStep), 1);
	for(i = 0; i < count; ++i) {
		if(vertexCount == vertexMax) {
			vertexMax = 2 * vertexMax;
			vertexBuffer = (GLfloat*)realloc(vertexBuffer, vertexMax * 2 * sizeof(GLfloat));
		}
		
		vertexBuffer[2 * vertexCount + 0] = startx + (endx - startx) * ((GLfloat)i / (GLfloat)count);
		vertexBuffer[2 * vertexCount + 1] = starty + (endy - starty) * ((GLfloat)i / (GLfloat)count);
		vertexCount += 1;
	}
	return vertexCount;
}

bool drawactive = false;

#ifdef OPENGLES2
void drawTexturedLineToTexture(float startx, float starty, float endx, float endy)
{
    NSUInteger			vertexCount = 0,
    count,
    i;
    float x,y;
    count = MAX(ceilf(sqrtf((endx - startx) * (endx - startx) + (endy - starty) * (endy - starty)) / kBrushPixelStep), 1);
    for(i = 0; i < count; ++i) {
        
        x = startx + (endx - startx) * ((GLfloat)i / (GLfloat)count);
        y = starty + (endy - starty) * ((GLfloat)i / (GLfloat)count);
        
        const GLfloat pointVertices[] = {
            x-brushsize/2, y-brushsize/2, 0.0f,
            x+brushsize/2, y-brushsize/2, 0.0f,
            x-brushsize/2, y+brushsize/2, 0.0f,
            x+brushsize/2, y+brushsize/2, 0.0f
        };
        
        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, pointVertices);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        // and finally tell the GPU to draw our triangle!
        glEnable(GL_BLEND);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);	
    }
}
#endif

// Render a quadrangle to a texture
void drawQuadToTexture(urAPI_Texture_t *texture, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
#ifdef OPENGLES2
    glUseProgram(g_glView->shaderProgram);
#endif
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    [EAGLContext setCurrentContext:g_glView->context];
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifndef OPENGLES2    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifndef OPENGLES2
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
	Texture2D *bgtexture = texture->backgroundTex;
/*	y1 = texture->backgroundTex->_height - y1;
	y2 = texture->backgroundTex->_height - y2;
	y3 = texture->backgroundTex->_height - y3;
	y4 = texture->backgroundTex->_height - y4;*/
	y1 = SCREEN_HEIGHT - y1;
	y2 = SCREEN_HEIGHT - y2;
	y3 = SCREEN_HEIGHT - y3;
	y4 = SCREEN_HEIGHT - y4;
	
	// allocate frame buffer
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	// bind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
	
	// attach renderbuffer
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, bgtexture.name, 0);
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	SetupBrush();

#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, texture->texturebrushcolor);
    glEnableVertexAttribArray(ATTRIB_COLOR);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#else
	glEnable(GL_LINE_SMOOTH);
	glDisableClientState(GL_COLOR_ARRAY);
	glColor4ub(texture->texturebrushcolor[0], texture->texturebrushcolor[1], texture->texturebrushcolor[2], texture->texturebrushcolor[3]);		
#endif

	if(brushtexture==NULL && !cameraBeingUsedAsBrush)
	{
		static GLfloat		vertexBuffer[8];
		
		vertexBuffer[0] = x1;
		vertexBuffer[1] = y1;
		vertexBuffer[2] = x2;
		vertexBuffer[3] = y2;
		vertexBuffer[4] = x3;
		vertexBuffer[5] = y3;
		vertexBuffer[6] = x4;
		vertexBuffer[7] = y4;
		
		glLineWidth(brushsize);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		//Render the vertex array
#ifdef OPENGLES2    
        static GLubyte colorBuffer[16];
        colorBuffer[0]=texture->texturebrushcolor[0];
        colorBuffer[1]=texture->texturebrushcolor[1];
        colorBuffer[2]=texture->texturebrushcolor[2];
        colorBuffer[3]=texture->texturebrushcolor[3];
        colorBuffer[4]=texture->texturebrushcolor[0];
        colorBuffer[5]=texture->texturebrushcolor[1];
        colorBuffer[6]=texture->texturebrushcolor[2];
        colorBuffer[7]=texture->texturebrushcolor[3];
        colorBuffer[8]=texture->texturebrushcolor[0];
        colorBuffer[9]=texture->texturebrushcolor[1];
        colorBuffer[10]=texture->texturebrushcolor[2];
        colorBuffer[11]=texture->texturebrushcolor[3];
        colorBuffer[12]=texture->texturebrushcolor[0];
        colorBuffer[13]=texture->texturebrushcolor[1];
        colorBuffer[14]=texture->texturebrushcolor[2];
        colorBuffer[15]=texture->texturebrushcolor[3];
        
        glUseProgram(g_glView->shaderProgram);
        
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        glBindTexture(GL_TEXTURE_2D, g_glView->whiteTexture);
        
        glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colorBuffer);
        glEnableVertexAttribArray(ATTRIB_COLOR);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexBuffer);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        //                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
        //                        glEnableVertexAttribArray(ATTRIB_VERTEX);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#else
        glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
#endif
		if(texture->fill)
			glDrawArrays(GL_TRIANGLE_FAN,0,4);
		else
			glDrawArrays(GL_LINE_LOOP, 0, 4);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
	}
	else
	{
#ifdef OPENGLES2
        initBrushColor(texture);
        initBrushRotation();

        drawTexturedLineToTexture(x1,y1,x2,y2);
        drawTexturedLineToTexture(x2,y2,x3,y3);
        drawTexturedLineToTexture(x3,y3,x4,y4);
        drawTexturedLineToTexture(x4,y4,x1,y1);
#else
		static GLfloat*		vertexBuffer = NULL;
		static NSUInteger	vertexMax = sqrt(SCREEN_HEIGHT*SCREEN_HEIGHT+SCREEN_WIDTH*SCREEN_WIDTH); //577; // Sqrt(480^2+320^2)
		NSUInteger			vertexCount = 0;
//		NSUInteger			count, i;
		
		//Allocate vertex array buffer
		if(vertexBuffer == NULL)
			vertexBuffer = (GLfloat*)malloc(vertexMax * 2 * sizeof(GLfloat));
	
		vertexCount = prepareBrushedLine(x1,y1,x2,y2,vertexCount,vertexMax,vertexBuffer);
		vertexCount = prepareBrushedLine(x2,y2,x3,y3,vertexCount,vertexMax,vertexBuffer);
		vertexCount = prepareBrushedLine(x3,y3,x4,y4,vertexCount,vertexMax,vertexBuffer);
		vertexCount = prepareBrushedLine(x4,y4,x1,y1,vertexCount,vertexMax,vertexBuffer);
		
		//Render the vertex array
        glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
		glDrawArrays(GL_POINTS, 0, vertexCount);
#endif
	}
	
	// unbind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
}

#define PI 3.1415926536

// Render an ellipse to a texture
void drawEllipseToTexture(urAPI_Texture_t *texture, float x, float y, float w, float h)
{
    [EAGLContext setCurrentContext:g_glView->context];
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);
	
#ifndef OPENGLES2
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
	Texture2D *bgtexture = texture->backgroundTex;
//	y = texture->backgroundTex->_height - y;
	y = SCREEN_HEIGHT - y;
	
	// allocate frame buffer
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	// bind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
	
	// attach renderbuffer
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, bgtexture.name, 0);
		
	SetupBrush();
	
#ifndef OPENGLES2
	glEnable(GL_LINE_SMOOTH);
	glDisableClientState(GL_COLOR_ARRAY);
	glColor4ub(texture->texturebrushcolor[0], texture->texturebrushcolor[1], texture->texturebrushcolor[2], texture->texturebrushcolor[3]);		
#endif
    
	if(brushtexture==NULL && !cameraBeingUsedAsBrush)
	{
		GLfloat vertices[720];
	
		for (int i = 0; i < 720; i += 2) {
			// x value
			vertices[i]   = x+w*cos(2.0*PI*i/720.0);
			// y value
			vertices[i+1] = y+h*sin(2.0*PI*i/720.0);
		}
		glLineWidth(brushsize);
		//Render the vertex array
#ifdef OPENGLES2    
        static GLubyte colorBuffer[360*4];
        for(int i=0;i<360*4;i +=4)
        {
            colorBuffer[i]=texture->texturebrushcolor[0];
            colorBuffer[i+1]=texture->texturebrushcolor[1];
            colorBuffer[i+2]=texture->texturebrushcolor[2];
            colorBuffer[i+3]=texture->texturebrushcolor[3];
        }
        
        glUseProgram(g_glView->shaderProgram);
        
        GLenum err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        glBindTexture(GL_TEXTURE_2D, g_glView->whiteTexture);
        
        glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colorBuffer);
        glEnableVertexAttribArray(ATTRIB_COLOR);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertices);
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        //                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
        //                        glEnableVertexAttribArray(ATTRIB_VERTEX);
#else
        glVertexPointer(2, GL_FLOAT, 0, vertices);
#endif
		if(texture->fill)
			glDrawArrays(GL_TRIANGLE_FAN,0,360);
		else
			glDrawArrays(GL_LINE_LOOP, 0, 360);
	}
	else
	{
		
//		static GLfloat*		vertexBuffer = NULL;
//		static NSUInteger	vertexMax = sqrt(SCREEN_HEIGHT*SCREEN_HEIGHT+SCREEN_WIDTH*SCREEN_WIDTH); //577; // Sqrt(480^2+320^2)
//		NSUInteger			i;
		

		
#ifdef OPENGLES2 
        initBrushColor(texture);
        initBrushRotation();
        
        float cx,cy;
		for (int i = 0; i < 720; i += 2) {
            
			// x value
			cx   = x+w*cos(2.0*PI*i/360.0);
			// y value
			cy = y+h*sin(2.0*PI*i/360.0);
            
            const GLfloat pointVertices[] = {
                cx-brushsize/2, cy-brushsize/2, 0.0f,
                cx+brushsize/2, cy-brushsize/2, 0.0f,
                cx-brushsize/2, cy+brushsize/2, 0.0f,
                cx+brushsize/2, cy+brushsize/2, 0.0f
            };
            glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, pointVertices);
            glEnableVertexAttribArray(ATTRIB_VERTEX);
            // and finally tell the GPU to draw our triangle!
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);	
        }
#else
        GLfloat vertices[720];
		
		for (int i = 0; i < 720; i += 2) {
			// x value
			vertices[i]   = x+w*cos(2.0*PI*i/360.0);
			// y value
			vertices[i+1] = y+h*sin(2.0*PI*i/360.0);
		}
		
		//Render the vertex array
        glVertexPointer(2, GL_FLOAT, 0, vertices);
		//		glDrawArrays(GL_LINES, 0, vertexCount);
		glDrawArrays(GL_POINTS, 0, 360);
#endif
	}
	
	// unbind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
	
}

// Render line drawing to a texture

void drawLineToTexture(urAPI_Texture_t *texture, float startx, float starty, float endx, float endy)
{
    [EAGLContext setCurrentContext:g_glView->context];
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	
#ifndef OPENGLES2
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
	Texture2D *bgtexture = texture->backgroundTex;
	
//    std::cerr << starty << " " << texture->backgroundTex->_height << std::endl;
//    NSLog(@"%f %d",starty,texture->backgroundTex->_height);
//	starty = texture->backgroundTex->_height - starty;
//	endy = texture->backgroundTex->_height - endy;
    starty = SCREEN_HEIGHT - starty;
    endy = SCREEN_HEIGHT - endy;

	// allocate frame buffer
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	// bind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	
	// attach renderbuffer
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, bgtexture.name, 0);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	
	SetupBrush();

    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
//	if(bgtexture==NULL)
	if(brushtexture==NULL && !cameraBeingUsedAsBrush)
	{
/*#ifdef OPENGLES2
		static GLfloat		vertexBuffer[6];
		
		vertexBuffer[0] = startx;
		vertexBuffer[1] = starty;
        vertexBuffer[2] = 0;
		vertexBuffer[3] = endx;
		vertexBuffer[4] = endy;
        vertexBuffer[5] = 0;
#else   */     
		static GLfloat		vertexBuffer[4];
		
		vertexBuffer[0] = startx;
		vertexBuffer[1] = starty;
		vertexBuffer[2] = endx;
		vertexBuffer[3] = endy;
/*#endif*/

#ifdef OPENGLES2
        static GLubyte colorBuffer[8];
/*
        colorBuffer[0]=texture->gradientUL[0]; // R
        colorBuffer[0]=texture->gradientUL[1]; // G
        colorBuffer[0]=texture->gradientUL[2]; // B
        colorBuffer[0]=texture->gradientUL[3]; // A
        colorBuffer[0]=texture->gradientUR[0]; // R
        colorBuffer[0]=texture->gradientUR[1]; // G
        colorBuffer[0]=texture->gradientUR[2]; // B
        colorBuffer[0]=texture->gradientUR[3]; // A
 */

        colorBuffer[0]=texture->texturebrushcolor[0];
        colorBuffer[1]=texture->texturebrushcolor[1];
        colorBuffer[2]=texture->texturebrushcolor[2];
        colorBuffer[3]=texture->texturebrushcolor[3];
        colorBuffer[4]=texture->texturebrushcolor[4];
        colorBuffer[5]=texture->texturebrushcolor[5];
        colorBuffer[6]=texture->texturebrushcolor[6];
        colorBuffer[7]=texture->texturebrushcolor[7];
        
        glUseProgram(g_glView->shaderProgram);
        
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
/*
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        glBindTexture(GL_TEXTURE_2D, g_glView->whiteTexture);
*/
        glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, colorBuffer);
        glEnableVertexAttribArray(ATTRIB_COLOR);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glLineWidth(brushsize);
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#else
		glEnable(GL_LINE_SMOOTH);
        err = glGetError();
		glDisableClientState(GL_COLOR_ARRAY);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glColor4ub(texture->texturebrushcolor[0], texture->texturebrushcolor[1], texture->texturebrushcolor[2], texture->texturebrushcolor[3]);		
		//		glColor4ub(0,0,255,30);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }

		glLineWidth(brushsize);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
#endif
        
		//Render the vertex array
#ifdef OPENGLES2    
/*        glUseProgram(g_glView->shaderProgram);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }*/
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, vertexBuffer);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        //                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
        //                        glEnableVertexAttribArray(ATTRIB_VERTEX);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		glDrawArrays(GL_LINE_STRIP, 0, 2);
#else
        glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
		glDrawArrays(GL_LINES, 0, 2);
#endif
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
	}
	else
	{
		
#ifdef OPENGLES2
/*        glUseProgram(g_glView->shaderProgram);
        
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(_textureUniform,0);
        glBindTexture(GL_TEXTURE_2D, brushtexture.name);
*/        
        initBrushColor(texture);
        initBrushRotation();
        
        drawTexturedLineToTexture(startx,starty,endx,endy);
#else
            
            static GLfloat*		vertexBuffer = NULL;
            static NSUInteger	vertexMax = sqrt(SCREEN_HEIGHT*SCREEN_HEIGHT+SCREEN_WIDTH*SCREEN_WIDTH); //577; // Sqrt(480^2+320^2)
            NSUInteger			vertexCount = 0,
            count,
            i;
            
            //Allocate vertex array buffer
            if(vertexBuffer == NULL)
                vertexBuffer = (GLfloat*)malloc(vertexMax * 2 * sizeof(GLfloat));
            
            //Add points to the buffer so there are drawing points every X pixels
            count = MAX(ceilf(sqrtf((endx - startx) * (endx - startx) + (endy - starty) * (endy - starty)) / kBrushPixelStep), 1);
            for(i = 0; i < count; ++i) {
                if(vertexCount == vertexMax) {
                    vertexMax = 2 * vertexMax;
                    vertexBuffer = (GLfloat*)realloc(vertexBuffer, vertexMax * 2 * sizeof(GLfloat));
                }
                
                vertexBuffer[2 * vertexCount + 0] = startx + (endx - startx) * ((GLfloat)i / (GLfloat)count);
                vertexBuffer[2 * vertexCount + 1] = starty + (endy - starty) * ((GLfloat)i / (GLfloat)count);
                vertexCount += 1;
            }

            glDisableClientState(GL_COLOR_ARRAY);
            err = glGetError();
            if(err != GL_NO_ERROR)
            {
                int a = err;
            }
            glColor4ub(texture->texturebrushcolor[0], texture->texturebrushcolor[1], texture->texturebrushcolor[2], texture->texturebrushcolor[3]);		
            //Render the vertex array
            err = glGetError();
            if(err != GL_NO_ERROR)
            {
                int a = err;
            }
            
            glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
            glDrawArrays(GL_POINTS, 0, vertexCount);
//        } BACKC
#endif
        
	}
	
	// unbind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifndef OPENGLES2
    glDisable(GL_POINT_SPRITE_OES);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#endif
}

// Clear a texture with a given RGB color

void clearTexture(Texture2D* texture, float r, float g, float b, float a)
{
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    [EAGLContext setCurrentContext:g_glView->context];
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
#ifndef OPENGLES2
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	// attach renderbuffer
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, texture.name, 0);
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	// unbind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);

    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

}

void readPixelColor(GLuint	t,int x, int y, unsigned char *colors)
{
    [EAGLContext setCurrentContext:g_glView->context];
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, g_glView->viewFramebuffer);
    glViewport(0, 0, g_glView->backingWidth, g_glView->backingHeight);
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	// allocate frame buffer
	if(textureFrameBuffer == -1)
		CreateFrameBuffer();
	// bind frame buffer
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, textureFrameBuffer);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, t, 0);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    GLenum errf = glCheckFramebufferStatus(GL_FRAMEBUFFER_OES);
    if(errf!= GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Frame buffer incomplete: %d",errf);
        int a=0;
    }
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colors);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
}

// Create a texture instance for a given region

Texture2D* createBlankTexture(float width, float height)
{
	CGSize size;
	size.width = width;
	size.height = height;

	return [[Texture2D alloc] initWithSize:size];
}

void freeMovieTexture(urAPI_Region_t* t)
{
#ifdef GPUIMAGE
    [t->texture->movieTex removeAllTargets];
    urRegionMovie* rm = (urRegionMovie*)t->texture->textureOutput.delegate;
    [rm dealloc];
    [t->texture->textureOutput dealloc];
    t->texture->textureOutput = NULL;
    [t->texture->movieTex dealloc];
    t->texture->movieTex = NULL;
    t->texture->texturepath = TEXTURE_SOLID;
#endif
}

char currentmediapath[PATH_MAX];

const char* accessiblePathSystemFirst(const char* fn)
{
    //if(strlen(fn)<1) return NULL;

    struct stat s;
    if( stat(fn,&s) == 0 )
    {
        if( s.st_mode & S_IFDIR )
        {
            return NULL;
        }
        else if( s.st_mode & S_IFREG )
        {
            return fn;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        //error
        const char* syspath = getSystemPath();
        strcpy(currentmediapath,syspath);
        int len = strlen(currentmediapath);
        currentmediapath[len]='/'; // System path does not have a trailing / in iOS
        currentmediapath[len+1]='\0';
        strcat(currentmediapath,fn);
        if( stat(currentmediapath,&s) == 0 )
        {
            if( s.st_mode & S_IFDIR )
            {
                return NULL;
            }
            else if( s.st_mode & S_IFREG )
            {
                return currentmediapath;
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            //error
            const char* docpath = getDocumentPath();
            strcpy(currentmediapath,docpath);
            //            int len = strlen(currentmediapath); // Document path DOES have a trailing / in iOS. Consistency ftw.
            //            currentmediapath[len]='/';
            //            currentmediapath[len+1]='\0';
            
            strcat(currentmediapath,fn);
            if( stat(currentmediapath,&s) == 0 )
            {
                if( s.st_mode & S_IFDIR )
                {
                    return NULL;
                }
                else if( s.st_mode & S_IFREG )
                {
                    return currentmediapath;
                }
                else
                {
                    return NULL;
                }
            }
            else
            {
                //error
                return NULL; // No path worked.
            }
        }
    }
    
    
/*
    if( access( fn, F_OK ) != -1 ) {
        return fn;
        // file exists
    } else {
        const char* syspath = getSystemPath();
        strcpy(currentmediapath,syspath);
        int len = strlen(currentmediapath);
        currentmediapath[len]='/'; // System path does not have a trailing / in iOS
        currentmediapath[len+1]='\0';
        strcat(currentmediapath,fn);
        if( access( currentmediapath, F_OK ) != -1 ) {
            return currentmediapath;
            // file exists
        } else {
            const char* docpath = getDocumentPath();
            strcpy(currentmediapath,docpath);
            //            int len = strlen(currentmediapath); // Document path DOES have a trailing / in iOS. Consistency ftw.
            //            currentmediapath[len]='/';
            //            currentmediapath[len+1]='\0';
            
            strcat(currentmediapath,fn);
            if( access( currentmediapath, F_OK ) != -1 ) {
                return currentmediapath;
                // file exists
            } else {
                return NULL;
                // file doesn't exist
            }
        }
    }
 */
}

void instantiateTexture(urAPI_Region_t* t)
{
//#ifdef UISTRINGS
//	texturepathstr = [[NSString alloc] initWithUTF8String:t->texture->texturepath];
//	NSString *filePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:texturepathstr]; // Leak here, fix.
//	UIImage* textureimage = [UIImage imageNamed:texturepathstr];
    
    const char* pathstr = accessiblePathSystemFirst(t->texture->texturepath);
    if(pathstr!=NULL)
    {
    texturepathstr = [[NSString alloc] initWithUTF8String:pathstr];
    
    // Try as image file
	UIImage* textureimage = [UIImage imageWithContentsOfFile:texturepathstr];
	if(textureimage==NULL)
		textureimage = [UIImage imageNamed:texturepathstr];
//#else
//	texturepathstr = storagePath+"/"+t->texture->texturepath;
//	urImage *textureimage = new urImage(texturepathstr.c_str());
//#endif
    
    if(textureimage==NULL)
    {
#ifdef GPUIMAGE
        GPUImageMovie* texturemovie = [g_glView loadMovie:texturepathstr];
        if(texturemovie!=NULL)
        {
            urRegionMovie* rm = [[urRegionMovie alloc] init];
            t->texture->textureOutput = [[GPUImageTextureOutput alloc] init];
            t->texture->regionMovie = rm;
            rm->region  = t;
                       
            t->texture->textureOutput.delegate = rm;
            
            [texturemovie addTarget:t->texture->textureOutput];
            t->texture->movieTex = texturemovie;
            t->texture->movieTexture = 0;
#undef PERFRAME
#ifdef PERFRAME
            [t->texture->movieTex enableManualFrameTrigger];
#endif
            [t->texture->movieTex startProcessing];
#ifdef PERFRAME
            /*
            if(![t->texture->movieTex processNextFrame])
            {
                freeMovieTexture(t);
            }
             */
#endif
        }
#endif

    }
	else
	{
		CGSize rectsize;
		rectsize.width = t->width;
		rectsize.height = t->height;
		t->texture->backgroundTex = [[Texture2D alloc] initWithImage:textureimage rectsize:rectsize];
		t->texture->width = [textureimage size].width;
		t->texture->height = [textureimage size].height;
	}
	[texturepathstr release];
    }
    else
        instantiateBlankTexture(t);
    
    if(t->texture->isTiled)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        GLenum err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    
}

void instantiateBlankTexture(urAPI_Region_t* t)
{
	t->texture->backgroundTex = createBlankTexture(t->width, t->height);
	t->texture->width = t->width;
	t->texture->height = t->height;
	clearTexture(t->texture->backgroundTex, t->texture->texturesolidcolor[0], t->texture->texturesolidcolor[1], t->texture->texturesolidcolor[2], t->texture->texturesolidcolor[3]);
}

void instantiateAllTextures(urAPI_Region_t* t)
{
	if(t->texture->texturepath != TEXTURE_SOLID)
	{
		instantiateTexture(t);
	}
	else {
		instantiateBlankTexture(t);
	}
}

// TextLabels
//------------------------------------------

// Convert line break modes to UILineBreakMode enums

UILineBreakMode tolinebreakmode(int wrap)
{
	switch(wrap)
	{
		case WRAP_WORD:
			return UILineBreakModeWordWrap;
		case WRAP_CHAR:
			return UILineBreakModeCharacterWrap;
		case WRAP_CLIP:
			return UILineBreakModeClip;
	}
	return UILineBreakModeWordWrap;
}

// Compute Y Alignment

int refreshLabelYAlign(urAPI_Region_t* t)
{
#ifndef UISTRINGS
    int fontheight = t->textlabel->textlabelTex->getFontBlockHeight(); // NYI
//    int lineheight = t->textlabel->textlabelTex->getLineHeight();
    int linegap = t->textlabel->textlabelTex->getLineGap();
#else
    int fontheight = [t->textlabel->textlabelTex fontblockHeight]; // NYI
    int linegap = t->textlabel->textheight*1.43; // NYI BACKC
#endif
    int justify = 0;
    switch(t->textlabel->justifyv)
    {
            /*
             case JUSTIFYV_MIDDLE:
             justify = -t->height+fontheight/2;
             break;
             case JUSTIFYV_TOP:
             justify = -t->height/2;
             break;
             case JUSTIFYV_BOTTOM:
             justify = -3*t->height/2+fontheight;
             break;
             */
        case JUSTIFYV_MIDDLE:
            justify = -(fontheight+linegap)/2;
            break;
        case JUSTIFYV_TOP:
            justify = t->height/2.0-fontheight-linegap/2;
            break;
        case JUSTIFYV_BOTTOM:
            justify = -t->height/2.0;
            break;
    }
    
    if(t->textlabel->textlabelTex)
    {
#ifndef UISTRINGS
        t->textlabel->textlabelTex->setYAlign(justify);
#endif
    }
    return justify;
}


// Render TextLabel

void renderTextLabel(urAPI_Region_t* t)
{
    // text will need blending
    glDisable(GL_BLEND);
    
    for(int i=0;i<4;i++) // default regions are white
    {
        squareColors[4*i] = 255;
        squareColors[4*i+1] = 255;
        squareColors[4*i+2] = 255;
        squareColors[4*i+3] = 255;
    }
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
#else
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
    glEnableClientState(GL_COLOR_ARRAY);
#endif
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    if(t->textlabel->textlabelTex)
#ifdef UISTRINGS
        [t->textlabel->textlabelTex dealloc];
#else
    if(t->textlabel->textlabelTex)
        delete t->textlabel->textlabelTex; //[t->textlabel->textlabelTex dealloc];
#endif
    UITextAlignment align = UITextAlignmentCenter;
    switch(t->textlabel->justifyh)
    {
        case JUSTIFYH_CENTER:
            align = UITextAlignmentCenter;
            break;
        case JUSTIFYH_LEFT:
            align = UITextAlignmentLeft;
            break;
        case JUSTIFYH_RIGHT:
            align = UITextAlignmentRight;
            break;
    }
#ifdef UISTRINGS
    textlabelstr = [[NSString alloc] initWithUTF8String:t->textlabel->text]; // Leak here. Fix.
    fontname = [[NSString alloc] initWithUTF8String:t->textlabel->font];
#else
    textlabelstr = t->textlabel->text; // Leak here. Fix.
    fontname = t->textlabel->font;
    fontPath = storagePath + "/" + fontname;
    if(fontPath.substr(fontPath.find_last_of(".") + 1) != "ttf") {
        fontPath = fontPath + ".ttf";
    }
#endif
    t->textlabel->updatestring = false;
    if(t->textlabel->drawshadow==false)
    {
#ifdef UISTRINGS
        t->textlabel->textlabelTex = [[Texture2D alloc] initWithString:textlabelstr
                                                            dimensions:CGSizeMake(t->width, t->height) alignment:align
                                                              fontName:fontname fontSize:t->textlabel->textheight lineBreakMode:tolinebreakmode(t->textlabel->wrap)];
#else
        t->textlabel->textlabelTex = new urTexture(textlabelstr.c_str(),fontPath.c_str(),t->textlabel->textheight,t->width,t->height,align,tolinebreakmode(t->textlabel->wrap),
                                                   CGSizeMake(t->textlabel->shadowoffset[0],t->textlabel->shadowoffset[1]),
                                                   t->textlabel->shadowblur,t->textlabel->shadowcolor,
                                                   t->textlabel->outlinemode,t->textlabel->outlinethickness);
#endif
    }
    else
    {
        shadowColors[0] = t->textlabel->shadowcolor[0];
        shadowColors[1] = t->textlabel->shadowcolor[1];
        shadowColors[2] = t->textlabel->shadowcolor[2];
        shadowColors[3] = t->textlabel->shadowcolor[3];
#ifdef UISTRINGS
        t->textlabel->textlabelTex = [[Texture2D alloc] initWithString:textlabelstr
                                                            dimensions:CGSizeMake(t->width, t->height) alignment:align
                                                              fontName:fontname fontSize:t->textlabel->textheight lineBreakMode:tolinebreakmode(t->textlabel->wrap)
                                                          shadowOffset:CGSizeMake(t->textlabel->shadowoffset[0],t->textlabel->shadowoffset[1]) shadowBlur:t->textlabel->shadowblur shadowColor:t->textlabel->shadowcolor];
#else
        //                        t->textlabel->textlabelTex->labelx =
        t->textlabel->textlabelTex = new urTexture(textlabelstr.c_str(),fontPath.c_str(),t->textlabel->textheight,t->width,t->height,
                                                   align,tolinebreakmode(t->textlabel->wrap),
                                                   CGSizeMake(t->textlabel->shadowoffset[0],t->textlabel->shadowoffset[1]),
                                                   t->textlabel->shadowblur,t->textlabel->shadowcolor,
                                                   t->textlabel->outlinemode,t->textlabel->outlinethickness);
#endif
    }
#ifdef UISTRINGS
    [fontname release];
    [textlabelstr release];
#endif
#ifndef UISTRINGS
    refreshLabelYAlign(t);
#endif
}

-(void) startMovieWriter:(const char*)fname
{
	NSError *error = nil;
	NSString *file = [[NSString alloc] initWithUTF8String:fname];
	// Create paths to output images
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
    else
        assert(false);
	NSString  *movPath = [documentPath stringByAppendingPathComponent:file];
	videoWriter = [[AVAssetWriter alloc] initWithURL:
								  [NSURL fileURLWithPath:movPath] fileType:AVFileTypeQuickTimeMovie
															  error:&error];
	NSParameterAssert(videoWriter);
	
	NSDictionary *videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								   AVVideoCodecH264, AVVideoCodecKey,
								   [NSNumber numberWithInt:SCREEN_WIDTH], AVVideoWidthKey,
								   [NSNumber numberWithInt:SCREEN_HEIGHT], AVVideoHeightKey,
								   nil];
	writerInput = [[AVAssetWriterInput
										assetWriterInputWithMediaType:AVMediaTypeVideo
										outputSettings:videoSettings] retain];

	adaptor = [[AVAssetWriterInputPixelBufferAdaptor
                                                     assetWriterInputPixelBufferAdaptorWithAssetWriterInput:writerInput
                                                     sourcePixelBufferAttributes:nil] retain];
	
	NSParameterAssert(writerInput);
	NSParameterAssert([videoWriter canAddInput:writerInput]);
	[videoWriter addInput:writerInput];	

    //Start a session:
    [videoWriter startWriting];
    [videoWriter startSessionAtSourceTime:kCMTimeZero];
}

// This function is experimental and has known memory issues. Not currently used.
- (CVPixelBufferRef) pixelBufferFromCGImage: (CGImageRef) image
{
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
							 [NSNumber numberWithBool:YES], kCVPixelBufferCGImageCompatibilityKey,
							 [NSNumber numberWithBool:YES], kCVPixelBufferCGBitmapContextCompatibilityKey,
							 nil];
    CVPixelBufferRef pxbuffer = NULL;
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, SCREEN_WIDTH,
										  SCREEN_HEIGHT, kCVPixelFormatType_32ARGB, (CFDictionaryRef) options, 
										  &pxbuffer);
    NSParameterAssert(status == kCVReturnSuccess && pxbuffer != NULL);
	
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
    NSParameterAssert(pxdata != NULL);
	
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef lcontext = CGBitmapContextCreate(pxdata, SCREEN_WIDTH,
												 SCREEN_HEIGHT, 8, 4*SCREEN_WIDTH, rgbColorSpace, 
												 kCGImageAlphaNoneSkipFirst);
    NSParameterAssert(lcontext);
 //   CGContextConcatCTM(context, frameTransform);
    CGContextDrawImage(lcontext, CGRectMake(0, 0, CGImageGetWidth(image), 
										   CGImageGetHeight(image)), image);
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(lcontext);
	
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
	
    return pxbuffer;
}

-(void) writeImageToMovie:(CGImageRef)image elapsed:(float)duration
{
	CVPixelBufferRef buffer = [self pixelBufferFromCGImage:image];
    [adaptor appendPixelBuffer:buffer withPresentationTime:CMTimeMake(duration, 1)];
	CVPixelBufferRelease(buffer);
}

-(void) closeMovieWriter
{
#ifdef GPUIMAGE
    [textureInput removeTarget:movieWriter];
	[writerInput markAsFinished];
//	[videoWriter endSessionAtSourceTime:…];
	[videoWriter finishWriting];
    [textureInput dealloc];
#endif
}

-(void) writeScreenshotToMovie:(float)duration
{
    NSInteger myDataLength = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *) malloc(myDataLength);
    glReadPixels(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    // gl renders "upside down" so swap top to bottom into new array.
    // there's gotta be a better way, but this works.
    GLubyte *buffer2 = (GLubyte *) malloc(myDataLength);
    for(int y = 0; y <SCREEN_HEIGHT; y++)
    {
        for(int x = 0; x <SCREEN_WIDTH * 4; x++)
        {
            buffer2[(SCREEN_HEIGHT -1 - y) * SCREEN_WIDTH * 4 + x] = buffer[y * 4 * SCREEN_WIDTH + x];
        }
    }
    // make data provider with data.
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer2, myDataLength, NULL);
    // prep the ingredients
    int bitsPerComponent = 8;
    int bitsPerPixel = 32;
    int bytesPerRow = 4 * SCREEN_WIDTH;
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    // make the cgimage
    CGImageRef imageRef = CGImageCreate(SCREEN_WIDTH, SCREEN_HEIGHT, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, NULL, NO, renderingIntent);
	
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
							 [NSNumber numberWithBool:YES], kCVPixelBufferCGImageCompatibilityKey,
							 [NSNumber numberWithBool:YES], kCVPixelBufferCGBitmapContextCompatibilityKey,
							 nil];
    CVPixelBufferRef pxbuffer = NULL;
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, SCREEN_WIDTH,
										  SCREEN_HEIGHT, kCVPixelFormatType_32ARGB, (CFDictionaryRef) options, 
										  &pxbuffer);
    NSParameterAssert(status == kCVReturnSuccess && pxbuffer != NULL);
	
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
    NSParameterAssert(pxdata != NULL);
	
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef lcontext = CGBitmapContextCreate(pxdata, SCREEN_WIDTH,
												  SCREEN_HEIGHT, 8, 4*SCREEN_WIDTH, rgbColorSpace, 
												  kCGImageAlphaNoneSkipFirst);
    NSParameterAssert(lcontext);
	//   CGContextConcatCTM(context, frameTransform);
    CGContextDrawImage(lcontext, CGRectMake(0, 0, CGImageGetWidth(imageRef), 
											CGImageGetHeight(imageRef)), imageRef);
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(lcontext);
	
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
	
	//	[adaptor appendPixelBuffer:buffer withPresentationTime:kCMTimeZero];
    [adaptor appendPixelBuffer:pxbuffer withPresentationTime:CMTimeMake(duration, 1)];
	CVPixelBufferRelease(pxbuffer);
	CGDataProviderRelease(provider);
	if( buffer != NULL ) { free(buffer); }
	if( buffer2 != NULL ) { free(buffer2); }
	
}

-(void) saveImageToFile:(UIImage*)image filename:(const char*)fname
{
	NSString *file = [[NSString alloc] initWithUTF8String:fname];
	// Create paths to output images
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
    else {
        assert(false);
    }
	NSString  *pngPath = [documentPath stringByAppendingPathComponent:file];
//	NSString  *jpgPath = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/Test.jpg"];
	
	// Write a UIImage to JPEG with minimum compression (best quality)
	// The value 'image' must be a UIImage object
	// The value '1.0' represents image compression quality as value from 0.0 to 1.0
//	[UIImageJPEGRepresentation(image, 1.0) writeToFile:jpgPath atomically:YES];
//	NSLog(@"Documents directory: %@", pngPath);
	// Write image to PNG
#ifndef WRITETOPHOTOS
	[UIImagePNGRepresentation(image) writeToFile:pngPath atomically:YES];
#else
	UIImageWriteToSavedPhotosAlbum(image, self, @selector(imageSavedToPhotosAlbum: didFinishSavingWithError: contextInfo:), context);  	
#endif
	// Let's check to see if files were successfully written...
	
	// Create file manager
//	NSError *error;
//	NSFileManager *fileMgr = [NSFileManager defaultManager];
	
	// Point to Document directory
//	NSString *documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
	
	// Write out the contents of home directory to console
//	NSLog(@"Documents directory: %@", [fileMgr contentsOfDirectoryAtPath:documentsDirectory error:&error]);
//	[image release];
}

// This function is experimental and has known memory issues. Not currently used.
-(CGImageRef) getImageRefFromGLView
{
    NSInteger myDataLength = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *) malloc(myDataLength);
    glReadPixels(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    // gl renders "upside down" so swap top to bottom into new array.
    // there's gotta be a better way, but this works.
    GLubyte *buffer2 = (GLubyte *) malloc(myDataLength);
    for(int y = 0; y <SCREEN_HEIGHT; y++)
    {
        for(int x = 0; x <SCREEN_WIDTH * 4; x++)
        {
            buffer2[(SCREEN_HEIGHT -1 - y) * SCREEN_WIDTH * 4 + x] = buffer[y * 4 * SCREEN_WIDTH + x];
        }
    }
    // make data provider with data.
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer2, myDataLength, NULL);
    // prep the ingredients
    int bitsPerComponent = 8;
    int bitsPerPixel = 32;
    int bytesPerRow = 4 * SCREEN_WIDTH;
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    // make the cgimage
    CGImageRef imageRef = CGImageCreate(SCREEN_WIDTH, SCREEN_HEIGHT, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, NULL, NO, renderingIntent);
	
//	CGDataProviderRelease(provider);
//	if( buffer != NULL ) { free(buffer); }
//	if( buffer2 != NULL ) { free(buffer2); }
	return imageRef;
}

// This function is experimental and has known memory issues. Not currently used.
-(UIImage *) saveImageFromGLView
{
	CGImageRef imageRef = [self getImageRefFromGLView];
    // then make the uiimage from that
    UIImage *myImage = [UIImage imageWithCGImage:imageRef];
//	CGImageRelease(imageRef);
//	[(id)CFMakeCollectable(imageRef) autorelease];
	
    return myImage;
}

-(void) saveScreenToFile:(const char*)fname
{
    NSInteger myDataLength = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *) malloc(myDataLength);
    glReadPixels(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    // gl renders "upside down" so swap top to bottom into new array.
    // there's gotta be a better way, but this works.
    GLubyte *buffer2 = (GLubyte *) malloc(myDataLength);
    for(int y = 0; y <SCREEN_HEIGHT; y++)
    {
        for(int x = 0; x <SCREEN_WIDTH * 4; x++)
        {
            buffer2[(SCREEN_HEIGHT -1 - y) * SCREEN_WIDTH * 4 + x] = buffer[y * 4 * SCREEN_WIDTH + x];
        }
    }
    // make data provider with data.
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer2, myDataLength, NULL);
    // prep the ingredients
    int bitsPerComponent = 8;
    int bitsPerPixel = 32;
    int bytesPerRow = 4 * SCREEN_WIDTH;
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    // make the cgimage
    CGImageRef imageRef = CGImageCreate(SCREEN_WIDTH, SCREEN_HEIGHT, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, NULL, NO, renderingIntent);
	UIImage *image = [[UIImage imageWithCGImage:imageRef] retain];
	
	NSString *file = [[NSString alloc] initWithUTF8String:fname];
	// Create paths to output images
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
	NSString  *pngPath = [documentPath stringByAppendingPathComponent:file];
	//	NSString  *jpgPath = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/Test.jpg"];
	
	// Write a UIImage to JPEG with minimum compression (best quality)
	// The value 'image' must be a UIImage object
	// The value '1.0' represents image compression quality as value from 0.0 to 1.0
	//	[UIImageJPEGRepresentation(image, 1.0) writeToFile:jpgPath atomically:YES];
	//	NSLog(@"Documents directory: %@", pngPath);
	// Write image to PNG
#ifndef WRITETOPHOTOS
	[UIImagePNGRepresentation(image) writeToFile:pngPath atomically:YES];
#else
	UIImageWriteToSavedPhotosAlbum(image, self, @selector(imageSavedToPhotosAlbum: didFinishSavingWithError: contextInfo:), context);  	
#endif
	CGDataProviderRelease(provider);
	if( buffer != NULL ) { free(buffer); }
	if( buffer2 != NULL ) { free(buffer2); }
	CGImageRelease(imageRef);
	[image release];
}

- (void)drawView {
  
    GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    drawactive = true;
    
    float elapsedtime = 0;
	
    if(displaynumber == 1)
	{
	// eval http buffer
		eval_buffer_exec(lua);
        
		urs_PullVis(); // update vis data before we call events, this way we have a rate based pulling that is available in all events.
		// Clock ourselves.
		elapsedtime = mytimer->elapsedSec();
        totalelapsedtime = totalelapsedtime + elapsedtime;
		mytimer->start();
		callAllOnUpdate(elapsedtime); // Call lua APIs OnUpdates when we render a new region. We do this first so that stuff can still be drawn for this region.
#ifdef SOAR_SUPPORT
        callAllOnSoarOutput();
#endif
  
	}	
	CGRect  bounds = [self bounds];
	
    // Replace the implementation of this method to do your own custom drawing
    
    [EAGLContext setCurrentContext:context];
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glViewport(0, 0, backingWidth, backingHeight);
/*
    if (portrait)
        glViewport(0, 0, backingWidth, backingHeight);
    else
    {
        glViewport(0, 0, backingHeight, backingWidth);
    }
    if (!portrait) {
        glTranslatef(160.0f, 240.0f, 0.0f);
        glRotatef(90.0, 0.0, 0.0, 1.0);
    }
    */
#ifdef RENDERTOTEXTURE
    if(bgtextureFrameBuffer == -1)
        glGenFramebuffersOES(1, &bgtextureFrameBuffer);
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, bgtextureFrameBuffer);
	
	// attach renderbuffer
//    static GLubyte bgpixel[1024*1024*4];
    if(bgname == -1)
    {
        glGenTextures(1, &bgname);
       
        
//        for(int i=0;i<1024*1024*4; i++)
//            bgpixel[i] = 255;
    }
    
    glBindTexture(GL_TEXTURE_2D, bgname);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, backingWidth, backingHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, bgname, 0);
    
    GLenum errf = glCheckFramebufferStatus(GL_FRAMEBUFFER_OES);
    if(errf!= GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Frame buffer incomplete: %d",errf);
        int a=0;
    }
#endif
    
#ifndef OPENGLES2    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
#endif
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#ifdef OPENGLES2 
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    // reset our modelview to identity
    esMatrixLoadIdentity(&modelView);
    // translate back into the screen by 6 units and down by 1 unit
    esMatrixMultiply(&mvp, &modelView, &projection );
    // set the mvp uniform
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
//    glUniformMatrix4fv(uniformMvp, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#else
	if(displaynumber == 1)
        glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    else
        glOrthof(0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
//	glOrthof(0.0f, w, 0.0f, h, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glRotatef(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    
//    glClearColor(0, 104.0/255.0, 55.0/255.0, 1.0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Background color
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
    // Render all (visible and unclipped) regions on a given page.
	
//    [self drawTest];
    
	CGRect screendimensions = [self bounds];
	
	int cw = screendimensions.size.width;
	int ch = screendimensions.size.height;
    int page;
    
    if (displaynumber == 1)
        page = currentPage;
    else
        page = currentExternalPage;
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
	for(urAPI_Region_t* t=firstRegion[page]; t != nil; t=t->next)
	{
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }
		if(t->isClipping)
		{
			glScissor(t->clipleft*(float)cw/(float)SCREEN_WIDTH,t->clipbottom*(float)ch/(float)SCREEN_HEIGHT,t->clipwidth*(float)cw/(float)SCREEN_WIDTH,t->clipheight*(float)ch/(float)SCREEN_HEIGHT);
			glEnable(GL_SCISSOR_TEST);
		}
		else
		{
			glDisable(GL_SCISSOR_TEST);
		}
		
        err = glGetError();
        if(err != GL_NO_ERROR)
        {
            int a = err;
        }

		if(t->isVisible && t->left <= SCREEN_WIDTH && t->right >= 0 && t->bottom <= SCREEN_HEIGHT && t->top >= 0)
		{
			squareVertices[0] = t->left;
			squareVertices[1] = t->bottom;
			squareVertices[2] = t->left;
			squareVertices[3] = t->bottom+t->height;
			squareVertices[4] = t->left+t->width;
			squareVertices[5] = t->bottom;
			squareVertices[6] = t->left+t->width;
			squareVertices[7] = t->bottom+t->height;
			
#ifdef OPENGLES2
            /*            for(int i=0;i<4;i++)
            {
                squareVertices[2*i] = squareVertices[2*i]/backingWidth;
                squareVertices[2*i+1] = squareVertices[2*i+1]/backingHeight;
            }*/
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_U, GL_CLAMP_TO_EDGE);
//            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_V, GL_CLAMP_TO_EDGE); 
            glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
            glEnableVertexAttribArray(ATTRIB_VERTEX);
#else
            glVertexPointer(2, GL_FLOAT, 0, squareVertices);
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
#endif
            err = glGetError();
            if(err != GL_NO_ERROR)
            {
                int a = err;
            }

			float alpha = t->alpha;
			if(t->texture!=NULL)
			{
/*				if(t->texture->texturepath == TEXTURE_SOLID)
				{
					squareColors[0] = t->texture->texturesolidcolor[0];
					squareColors[1] = t->texture->texturesolidcolor[1];
					squareColors[2] = t->texture->texturesolidcolor[2];
					squareColors[3] = t->texture->texturesolidcolor[3]*alpha;
					
					squareColors[4] = t->texture->texturesolidcolor[0];
					squareColors[5] = t->texture->texturesolidcolor[1];
					squareColors[6] = t->texture->texturesolidcolor[2];
					squareColors[7] = t->texture->texturesolidcolor[3]*alpha;
					
					squareColors[8] = t->texture->texturesolidcolor[0];
					squareColors[9] = t->texture->texturesolidcolor[1];
					squareColors[10] = t->texture->texturesolidcolor[2];
					squareColors[11] = t->texture->texturesolidcolor[3]*alpha;
					
					squareColors[12] = t->texture->texturesolidcolor[0];
					squareColors[13] = t->texture->texturesolidcolor[1];
					squareColors[14] = t->texture->texturesolidcolor[2];
					squareColors[15] = t->texture->texturesolidcolor[3]*alpha;
				}
				else
				{*/
					squareColors[0] = t->texture->gradientBL[0];
					squareColors[1] = t->texture->gradientBL[1];
					squareColors[2] = t->texture->gradientBL[2];
					squareColors[3] = t->texture->gradientBL[3]*alpha;
					
					squareColors[4] = t->texture->gradientBR[0];
					squareColors[5] = t->texture->gradientBR[1];
					squareColors[6] = t->texture->gradientBR[2];
					squareColors[7] = t->texture->gradientBR[3]*alpha;
					
					squareColors[8] = t->texture->gradientUL[0];
					squareColors[9] = t->texture->gradientUL[1];
					squareColors[10] = t->texture->gradientUL[2];
					squareColors[11] = t->texture->gradientUL[3]*alpha;
					
					squareColors[12] = t->texture->gradientUR[0];
					squareColors[13] = t->texture->gradientUR[1];
					squareColors[14] = t->texture->gradientUR[2];
					squareColors[15] = t->texture->gradientUR[3]*alpha;
//				}
#ifdef OPENGLES2

                glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors)             ;
//                glVertexAttribPointer(ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 0, squareColors);
                
                glEnableVertexAttribArray(ATTRIB_COLOR);
#else
				glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
				glEnableClientState(GL_COLOR_ARRAY);
#endif				
                err = glGetError();
                if(err != GL_NO_ERROR)
                {
                    int a = err;
                }

#ifdef GPUIMAGE
				if(t->texture->backgroundTex == nil && t->texture->movieTex == nil && t->texture->texturepath != TEXTURE_SOLID)
#else
                    if(t->texture->backgroundTex == nil && t->texture->texturepath != TEXTURE_SOLID)
#endif
				{
					instantiateTexture(t);
				}
				
                err = glGetError();
                if(err != GL_NO_ERROR)
                {
                    int a = err;
                }
                
				switch(t->texture->blendmode)
				{
					case BLEND_DISABLED:
						glDisable(GL_BLEND);
						break;
					case BLEND_BLEND:
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						break;
					case BLEND_ALPHAKEY:
						// NYI
#ifndef OPENGLES2
						glAlphaFunc(GL_GEQUAL, 0.5f); // UR! This may be different
						glEnable(GL_ALPHA_TEST);
#endif
						break;
					case BLEND_ADD:
						glBlendFunc(GL_ONE, GL_ONE);
						break;
					case BLEND_MOD:
						glBlendFunc(GL_DST_COLOR, GL_ZERO);
						break;
					case BLEND_SUB: // Experimental blend category. Can be changed wildly NYI marking this for revision.
						glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
						break;
				}

                err = glGetError();
                if(err != GL_NO_ERROR)
                {
                    int a = err;
                }
               
#ifdef GPUIMAGE
				if(t->texture->backgroundTex || t->texture->usecamera || t->texture->movieTex)
#else
                    if(t->texture->backgroundTex || t->texture->usecamera )
#endif
				{
					GLfloat  coordinates[] = {  t->texture->texcoords[0],          t->texture->texcoords[1],
						t->texture->texcoords[2],  t->texture->texcoords[3],
						t->texture->texcoords[4],              t->texture->texcoords[5],
                        t->texture->texcoords[6],  t->texture->texcoords[7]  };

#ifdef OPENGLES2
                    glUseProgram(shaderProgram);
                    glActiveTexture(GL_TEXTURE0);
//                    glBindTexture(GL_TEXTURE_2D, videoFrameTexture);

                    glUniform1i(_textureUniform,0);
                    
                    
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
                    
                    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
                    
                    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, GL_FALSE, 0, coordinates);
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
                    
//                    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
//                    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, GL_FALSE, 0, coordinates);
#else
                    glEnable(GL_TEXTURE_2D);

					glEnableClientState(GL_TEXTURE_COORD_ARRAY);

					glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
#endif

                    if(t->texture->isTiled)
                    {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                    }
                    else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    }
                    
					if(t->texture->usecamera
#ifdef GPUIMAGE
                       || t->texture->inputFilter
#endif
                       )
					{
						CGRect rect = CGRectMake(t->left,t->bottom,t->width,t->height);
						GLfloat vertices[] = {  rect.origin.x,                                                  rect.origin.y,                                                  0.0,
							rect.origin.x + rect.size.width,                rect.origin.y,                                                  0.0,
							rect.origin.x,                                                  rect.origin.y + rect.size.height,               0.0,
							rect.origin.x + rect.size.width,                rect.origin.y + rect.size.height,               0.0 };
						
#ifdef GPUIMAGE
                        if(!t->texture->filterHandler)
#endif
                            glBindTexture(GL_TEXTURE_2D, cameraTexture);
#ifdef GPUIMAGE
                        else
                        {
//                            [t->texture->filterInput processTextureWithFrameTime:CMTimeMake(totalelapsedtime*1000,1000)];
                            glBindTexture(GL_TEXTURE_2D, t->texture->_filterTexture);
                        }
#endif

#ifdef DEBUGSHOWFRAMECOUNT
                        char errorstrbuf[16];
                        sprintf(errorstrbuf,"Frame %d",cameraTexture);
                        errorstr = errorstrbuf;
                        newerror = true;
                        ur_Log(errorstr.c_str());
#endif

#ifdef OPENGLES2    
                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
                        glEnableVertexAttribArray(ATTRIB_VERTEX);
//                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
//                        glEnableVertexAttribArray(ATTRIB_VERTEX);
#else
						glVertexPointer(3, GL_FLOAT, 0, vertices);
#endif
						//	glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef GPUIMAGE
                        if(/*textureInput==nil*/ latelaunchmovie && recordfrom == SOURCE_CAMERA)
                        {
//                            [self writeMovieFromTexture:_cameraTexture ofSize:sourcesize withCrop:CGRectMake(0.0,0.0,1.0,1.0)];
                            [self writeMovieFromTexture:_cameraTexture ofSize:sourcesize];
                            [textureInput processTextureWithFrameTime:CMTimeMake(totalelapsedtime*1000,1000)];
                            latelaunchmovie = false;
                        }
#endif
					}
#ifdef GPUIMAGE
					else if(t->texture->movieTex)
                    {
                        if(t->texture->movieTexture)
                        {

						CGRect rect = CGRectMake(t->left,t->bottom,t->width,t->height);
						GLfloat vertices[] = {  rect.origin.x,                                                  rect.origin.y,                                                  0.0,
							rect.origin.x + rect.size.width,                rect.origin.y,                                                  0.0,
							rect.origin.x,                                                  rect.origin.y + rect.size.height,               0.0,
							rect.origin.x + rect.size.width,                rect.origin.y + rect.size.height,               0.0 };
						
						glBindTexture(GL_TEXTURE_2D, t->texture->movieTexture);
//						glBindTexture(GL_TEXTURE_2D, 490);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
#ifdef OPENGLES2    
                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                        glEnableVertexAttribArray(ATTRIB_VERTEX);
                        //                        glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
                        //                        glEnableVertexAttribArray(ATTRIB_VERTEX);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
#else
						glVertexPointer(3, GL_FLOAT, 0, vertices);
#endif
						//	glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                        }
                        if(/*textureInput==nil*/ latelaunchmovie && recordfrom == SOURCE_MOVIE)
                        {
                            [self writeMovieFromTexture:t->texture->movieTexture ofSize:sourcesize withCrop:CGRectMake(0.0,0.0,1.0,1.0)];
                            latelaunchmovie = false;
                            err = glGetError();
                            if(err != GL_NO_ERROR)
                            {
                                int a = err;
                            }
                        }
//                        [t->texture->movieTex flushTexture];
#ifdef OPENGLES2
#ifdef PERFRAME
                        if(![t->texture->movieTex processNextFrame])
                        {
                            freeMovieTexture(t);
                        }
                        
#ifdef FREEOVERFLOWTEXTURE
                        glDeleteTextures(1,&(t->texture->movieTexture));
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                        t->texture->movieTex ture = 0;
#endif
#else
               //         if([t->texture->movieTex playStatus] == GPUMOVIE_FINISHED)
                        {
                            glDeleteTextures(1,&(t->texture->movieTexture));
                            t->texture->movieTexture = 0;
                            freeMovieTexture(t);
                        }
#endif
#endif	
                    }
#endif
                    else
					{
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        
						[t->texture->backgroundTex drawInRect:CGRectMake(t->left,t->bottom,t->width,t->height)];
					}
                    

                    if(t->texture->isTiled)
                    {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        err = glGetError();
                        if(err != GL_NO_ERROR)
                        {
                            int a = err;
                        }
                    }
                    else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    }

					glEnable(GL_BLEND);
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
#ifndef OPENGLES2
					glDisable(GL_ALPHA_TEST);
#endif
				}
				else
				{
#ifdef OPENGLES2	
//                    glUseProgram(plainshaderProgram);
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
                    glActiveTexture(GL_TEXTURE0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                    glUniform1i(_textureUniform,0);
                    glBindTexture(GL_TEXTURE_2D, whiteTexture);
                    
/*                    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, squareColors);
                    glEnableVertexAttribArray(ATTRIB_COLOR);*/
#else
					glDisable(GL_TEXTURE_2D);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
					glEnableClientState(GL_COLOR_ARRAY);
#endif
                    err = glGetError();
                        if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    err = glGetError();
                    if(err != GL_NO_ERROR)
                    {
                        int a = err;
                    }
					glEnable(GL_BLEND);
#ifndef OPENGLES2
					glDisable(GL_ALPHA_TEST);
#endif
				}
				// switch it back to GL_ONE for other types of images, rather than text because Texture2D uses CG to load, which premultiplies alpha
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
			}
			
            err = glGetError();
            if(err != GL_NO_ERROR)
            {
                int a = err;
            }

			if(t->textlabel!=NULL)
			{
				// texturing will need these
#ifdef OPENGLES2
                glUseProgram(shaderProgram);
                glActiveTexture(GL_TEXTURE0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glUniform1i(_textureUniform,0);
#else
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glEnableClientState(GL_VERTEX_ARRAY);
				glEnable(GL_TEXTURE_2D);
#endif
				
				if(t->textlabel->updatestring)
				{
                    renderTextLabel(t);
				}
				
				// text will need blending
				glEnable(GL_BLEND);
//                glDisable(GL_BLEND);
				
				// text from Texture2D uses A8 tex format, so needs GL_SRC_ALPHA
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

				for(int i=0;i<4;i++) // default regions are white
				{
					squareColors[4*i] = t->textlabel->textcolor[0];
					squareColors[4*i+1] = t->textlabel->textcolor[1];
					squareColors[4*i+2] = t->textlabel->textcolor[2];
					squareColors[4*i+3] = t->textlabel->textcolor[3]*t->alpha;
				}
#ifdef OPENGLES2		
                glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
                glEnableVertexAttribArray(ATTRIB_COLOR);
#else
                glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
				glEnableClientState(GL_COLOR_ARRAY);
#endif
                err = glGetError();
                if(err != GL_NO_ERROR)
                {
                    int a = err;
                }
				
#ifdef UISTRINGS
				int fontheight = [t->textlabel->textlabelTex fontblockHeight];
				int justify = 0;
				switch(t->textlabel->justifyv)
				{
					case JUSTIFYV_MIDDLE:
						justify = -t->height+fontheight/2;
						break;
					case JUSTIFYV_TOP:
						justify = -t->height/2;
						break;
					case JUSTIFYV_BOTTOM:
						justify = -3*t->height/2+fontheight;
						break;
				}
#else
/*                int bottom = t->bottom;
				int fontheight = t->textlabel->textlabelTex->getFontBlockHeight(); // NYI fontblockheight
				switch(t->textlabel->justifyv)
				{
					case JUSTIFYV_MIDDLE:
						bottom -= t->height/2-fontheight/2;
						break;
					case JUSTIFYV_TOP:
						bottom = bottom;
						break;
					case JUSTIFYV_BOTTOM:
						bottom -= t->height-fontheight;
						break;
				}
                */
                
                int justify = refreshLabelYAlign(t);

                
#endif
				
#ifdef OPENGLES2
                glUseProgram(shaderProgram);
                urPushMatrix();
//                esMatrixLoadIdentity(&modelView);
#ifdef UISTRINGS
                urTranslatef(t->left+t->width/2, t->bottom+t->height/2, 0.0f);
//                esTranslate(&modelView, t->left+t->width/2, t->bottom+t->height/2, 0.0f);
#else
//                esTranslate(&modelView, t->left/*+t->width/2*/, t->bottom+t->height/2, 0.0f);
                urTranslatef(t->left+t->width/2, t->bottom+t->height/2, 0.0f);
//                esTranslate(&modelView, t->left+t->width/2, t->bottom+t->height/2, 0.0f);
//               esTranslate(&modelView, t->left+t->width/2, bottom+t->height, 0.0f);
#endif
                urRotatef(t->textlabel->rotation, 0.0f, 0.0f, 1.0f);
//                esRotate(&modelView, t->textlabel->rotation, 0.0f, 0.0f, 1.0f);
                // create our new mvp matrix
//                esMatrixMultiply(&mvp, &modelView, &projection );
                // update the mvp uniform
//                glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
#ifdef UISTRINGS
				[t->textlabel->textlabelTex drawAtPoint:CGPointMake(-t->width/2, justify) tile:true];
#else
//                t->textlabel->textlabelTex->labely = justify+3*t->height/2-fontheight;
//                t->textlabel->textlabelTex->drawAtPoint(CGPointMake(-t->width/2, justify), true);
                t->textlabel->textlabelTex->drawAtPoint(CGPointMake(-t->width/2, justify), true);
//				t->textlabel->textlabelTex->drawAtPoint(CGPointMake(t->left,bottom), true);
#endif
                err = glGetError();
                if(err != GL_NO_ERROR)
                {
                    int a = err;
                }
                
                // reset our modelview to identity
                urPopMatrix();
//                esMatrixLoadIdentity(&modelView);
//                esMatrixMultiply(&mvp, &modelView, &projection );
//                glUniformMatrix4fv(_modelviewprojUniform, 1, GL_FALSE, (GLfloat*) &mvp.m[0][0] );
#else
				glPushMatrix();
				glTranslatef(t->left+t->width/2, t->bottom+t->height/2, 0);
				glRotatef(t->textlabel->rotation, 0.0f, 0.0f, 1.0f);
				[t->textlabel->textlabelTex drawAtPoint:CGPointMake(-t->width/2, justify) tile:true];
				glPopMatrix();
				
#endif
				// switch it back to GL_ONE for other types of images, rather than text because Texture2D uses CG to load, which premultiplies alpha
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
    }
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

	glDisable(GL_SCISSOR_TEST);
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#ifndef OPENGLES2
	glDisable(GL_TEXTURE_2D);
#endif	
#ifdef RENDERERRORSTRTEXTUREFONT
	// texturing will need these
#ifndef OPENGLES2
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
#endif
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#ifndef OPENGLES2
	glEnable(GL_TEXTURE_2D);
#endif	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

    if(errorStrTex == nil || newerror)
    {
        for(int i=0;i<16;i++) // default regions are white
            squareColors[i] = 255;
#ifdef OPENGLES2
        glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
        glEnableVertexAttribArray(ATTRIB_COLOR);
#else
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
        glEnableClientState(GL_COLOR_ARRAY);
#endif
    }
    if (errorStrTex == nil)
	{
		newerror = false;
//		errorStrTex = [[Texture2D alloc] initWithString:[NSString stringWithUTF8String:errorstr.c_str()]
//										 dimensions:CGSizeMake(SCREEN_WIDTH, 128) alignment:UITextAlignmentCenter
//										   fontName:@"Helvetica" fontSize:14 lineBreakMode:UILineBreakModeWordWrap ];
#ifdef UISTRINGS
        shadowColors[0] = 80;
        shadowColors[1] = 80;
        shadowColors[2] = 80;
        shadowColors[3] = 80;
        errorStrTex = [[Texture2D alloc] initWithString:[NSString stringWithUTF8String:errorstr.c_str()]
                                                            dimensions:CGSizeMake(SCREEN_WIDTH, 128) alignment:UITextAlignmentCenter
                                               fontName:[NSString stringWithUTF8String:errorfontPath.c_str()] fontSize:14 lineBreakMode:UILineBreakModeWordWrap
                                                          shadowOffset:CGSizeMake(0,0) shadowBlur:2 shadowColor:shadowColors];
#else
        errorStrTex = new urTexture(errorstr.c_str(),errorfontPath.c_str(),20,SCREEN_WIDTH,128);
#endif

	}
	else if(newerror)
	{
#ifdef UISTRINGS
		[errorStrTex dealloc];
#else
        delete errorStrTex;
#endif
		newerror = false;
//		errorStrTex = [[Texture2D alloc] initWithString:[NSString stringWithUTF8String:errorstr.c_str()]
//										 dimensions:CGSizeMake(SCREEN_WIDTH, 128) alignment:UITextAlignmentCenter
//										   fontName:@"Helvetica" fontSize:14 lineBreakMode:UILineBreakModeWordWrap];
#ifdef UISTRINGS
        shadowColors[0] = 80;
        shadowColors[1] = 80;
        shadowColors[2] = 80;
        shadowColors[3] = 80;
        errorStrTex = [[Texture2D alloc] initWithString:[NSString stringWithUTF8String:errorstr.c_str()]
                                             dimensions:CGSizeMake(SCREEN_WIDTH, 128) alignment:UITextAlignmentCenter
                                               fontName:[NSString stringWithUTF8String:errorfontPath.c_str()] fontSize:14 lineBreakMode:UILineBreakModeWordWrap
                                           shadowOffset:CGSizeMake(0,0) shadowBlur:2 shadowColor:shadowColors];
#else
        errorStrTex = new urTexture(errorstr.c_str(),errorfontPath.c_str(),20,SCREEN_WIDTH,128);
#endif
	}
	
	// text will need blending
	glEnable(GL_BLEND);
	
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

	// text from Texture2D uses A8 tex format, so needs GL_SRC_ALPHA
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }

    for(int i=0;i<16;i++) // default regions are white
        squareColors[i] = 255;
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
#else
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
    glEnableClientState(GL_COLOR_ARRAY);
#endif
    
#ifdef UISTRINGS
	[errorStrTex drawAtPoint:CGPointMake(0.0,
									 bounds.size.height * 0.5f) tile:true];
#else
//	urPushMatrix();
//	urTranslatef(SCREEN_WIDTH*0.5f,SCREEN_HEIGHT*0.5f+64,0.0);
	errorStrTex->drawAtPoint(CGPointMake(0, SCREEN_HEIGHT*0.5f), true);
//    urPopMatrix();
#endif
	
	// switch it back to GL_ONE for other types of images, rather than text because Texture2D uses CG to load, which premultiplies alpha
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
#ifdef RENDERTOTEXTURE
    glDisable(GL_BLEND);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);

    glBindTexture(GL_TEXTURE_2D, bgname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
    
    for(int i=0;i<16;i++) // default regions are white
		squareColors[i] = 255;
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, squareColors);
    glEnableVertexAttribArray(ATTRIB_COLOR);
#endif
	GLfloat         coordinates[] = { 0,              0,
        1.0,  0,
        0,    1.0,
		1.0,  1.0,
		
         };
	
	GLfloat         vertices[] = {  0.0,                        0.0,        0.0,
		backingWidth,        0.0,        0.0,
		0.0,                        backingHeight,      0.0,
        backingWidth,        backingHeight,      0.0 };
    
#ifdef OPENGLES2
    glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, vertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0, coordinates);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
#endif
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#endif

    if (context)
    {
        isRendering_PATCH_VARIABLE = YES;
        
        [EAGLContext setCurrentContext:context];
#ifdef OPENGLES2
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    
    [context presentRenderbuffer:GL_RENDERBUFFER];
#else
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
#endif
        isRendering_PATCH_VARIABLE = NO;
    }
#ifdef GPUIMAGE
    if(movieWriter)// && movieFile == nil) // Write texture to movie
    {
        static int subsample = 0;
        if(subsample > 1)
        {
            subsample = 0;
#ifdef DEBUGSHOWFRAMECOUNT
            char errorstrbuf[16];
            sprintf(errorstrbuf,"Frame %d",framecnt);
            framecnt ++;
            errorstr = errorstrbuf;
            newerror = true;
            ur_Log(errorstr.c_str());
#endif
            [textureInput processTextureWithFrameTime:CMTimeMake(totalelapsedtime*1000,1000)];
        }
        else
            subsample ++;;
    }
#endif
    
    glEnable(GL_BLEND);
    drawactive = false;
    err = glGetError();
    if(err != GL_NO_ERROR)
    {
        int a = err;
    }
	
}

- (void)layoutSubviews {
 
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
//    [self createFramebuffer];
    [self createFramebuffer2];
    [self drawView];
  
}
/*
- (void) movingToBackground: (NSNotification *) notification {
    [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
}

- (void) movingToForeground: (NSNotification *) notification {
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
}

- (void)viewChanging:(NSNotification*)notification
{
    NSLog(@"View is changing");
    previousOrientation = orientation;
    orientation = [[UIDevice currentDevice] orientation];
    
    if (previousOrientation==orientation && forceReorientation==NO) {
        return;
    }
}

- (BOOL) shouldAutorotate {
    return NO;
}
*/
- (BOOL)createFramebuffer {
	CGRect screendimensions = [[UIScreen mainScreen] bounds];
//	CGRect screendimensions = [self bounds];

    if(displaynumber == 1)
    {
	SCREEN_WIDTH = screendimensions.size.width;
	SCREEN_HEIGHT = screendimensions.size.height;
    TOUCH_SCREEN_WIDTH = SCREEN_WIDTH;
    TOUCH_SCREEN_HEIGHT = SCREEN_HEIGHT;
	HALF_SCREEN_WIDTH = SCREEN_WIDTH/2;
	HALF_SCREEN_HEIGHT = SCREEN_HEIGHT/2;
    }
    else
    {
        UIWindow* externalWindow = [self window];
        
        float extScreenWidth = externalWindow.frame.size.width;
        float extScreenHeight = externalWindow.frame.size.height;
        NSArray			*screens;
        screens = [UIScreen screens];
/*        UIScreen		*aScreen;
        UIScreenMode	*mode;
        UIScreenMode *bestmode = NULL;
        uint32_t  bestwidth=0;
        uint32_t  bestheight=0;
        

        
        uint32_t screenNum = 1;
        for (aScreen in screens) {
            NSArray *displayModes;
            
            displayModes = [aScreen availableModes];
            for (mode in displayModes) {
                    uint32_t width = mode.size.width;
                    uint32_t height = mode.size.height;
                    if(abs(height-SCREEN_HEIGHT)<abs(bestheight-SCREEN_HEIGHT))
                    {
                        bestmode = mode;
                    }
           }
            
            extScreen.currentMode
            
            screenNum++;
        }
        
        NSUInteger screenCount = [screens count];
        
        if (screenCount > 1) {
            // 2.
            
            // Select first external screen
            self.extScreen = [screens objectAtIndex:1];
            self.availableModes = [extScreen availableModes];
            
            // Update picker with display modes
            [modePicker reloadAllComponents];
            
            // Enable mode set option
            modeSetButton.enabled = YES;
            
            // Set initial display mode to highest resolution
            [modePicker selectRow:([modePicker numberOfRowsInComponent:0] - 1) inComponent:0 animated:NO];
            [self buttonAction:modeSetButton];
        }
        */
        
        float deviceScreenRatio = [[screens objectAtIndex:0] bounds].size.width/[[screens objectAtIndex:0] bounds].size.height;
        CGRect finalExtFrame;
        
        if (extScreenHeight < extScreenWidth) {
            // Height is limiting factor
            
            EXT_SCREEN_WIDTH = extScreenHeight*deviceScreenRatio;
            EXT_SCREEN_HEIGHT = extScreenHeight;
        } else {
            EXT_SCREEN_WIDTH = extScreenWidth;
            EXT_SCREEN_HEIGHT = extScreenWidth/deviceScreenRatio;
        }
    }
        
#ifdef OPENGLES2
    glGenFramebuffers(1, &viewFramebuffer);
    glGenRenderbuffers(1, &viewRenderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);
    
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);				

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
#else
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
#endif
    
    
    
    if (USE_DEPTH_BUFFER) {
        glGenRenderbuffersOES(1, &depthRenderbuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
        glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
    }
    
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
        return NO;
    }
    
    return YES;
}

- (void)createFramebuffer2
{
	CGRect screendimensions = [[UIScreen mainScreen] bounds];
    //	CGRect screendimensions = [self bounds];
    
    if(displaynumber <= 1)
    {
//        SCREEN_WIDTH = self.frame.size.width;
//        SCREEN_HEIGHT = self.frame.size.height;
        SCREEN_WIDTH = screendimensions.size.width;
        SCREEN_HEIGHT = screendimensions.size.height;
        TOUCH_SCREEN_WIDTH = SCREEN_WIDTH;
        TOUCH_SCREEN_HEIGHT = SCREEN_HEIGHT;
        HALF_SCREEN_WIDTH = SCREEN_WIDTH/2;
        HALF_SCREEN_HEIGHT = SCREEN_HEIGHT/2;
    }
    else
    {
        NSArray			*screens;
        
        screens = [UIScreen screens];
        UIScreen* extScreen = [screens objectAtIndex:1];
//        extScreen.currentMode = [availableModes objectAtIndex:selectedRow];
//        UIWindow* externalWindow = [self window];
        UIScreenMode	*mode;
        NSArray *displayModes;
        UIScreenMode *bestmode = NULL;
        uint32_t  bestwidth=0;
        uint32_t  bestheight=0;
        displayModes = [extScreen availableModes];
        for (mode in displayModes) {
            uint32_t width = mode.size.width;
            uint32_t height = mode.size.height;
            if(abs((int)(height-SCREEN_HEIGHT))<abs((int)(bestheight-SCREEN_HEIGHT)))
            {
                bestmode = mode;
                bestwidth = width;
                bestheight = height;
            }
        }
        
        extScreen.currentMode = bestmode;
        
        UIWindow* externalWindow;
        externalWindow = [[UIWindow alloc] initWithFrame:[extScreen bounds]];
       
        float extScreenWidth = externalWindow.frame.size.width;
        float extScreenHeight = externalWindow.frame.size.height;
        
        float deviceScreenRatio = [extScreen bounds].size.width/[extScreen bounds].size.height;
        CGRect finalExtFrame;
        
        if (extScreenHeight < extScreenWidth) {
            // Height is limiting factor
            
            EXT_SCREEN_WIDTH = extScreenHeight*deviceScreenRatio;
            EXT_SCREEN_HEIGHT = extScreenHeight;
        } else {
            EXT_SCREEN_WIDTH = extScreenWidth;
            EXT_SCREEN_HEIGHT = extScreenWidth/deviceScreenRatio;
        }
    }
    
    if (context && !viewFramebuffer)
    {
        [EAGLContext setCurrentContext:context];

#ifdef OPENGLES2
        glGenFramebuffers(1, &viewFramebuffer);
        glGenRenderbuffers(1, &viewRenderbuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);
        
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
        
		glGenRenderbuffers(1, &depthRenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);

        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, backingWidth, backingHeight);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
#else
        glGenFramebuffersOES(1, &viewFramebuffer);
        glGenRenderbuffersOES(1, &viewRenderbuffer);
        glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
        [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
        
        glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
        glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
#endif
        
    }
}

/*
- (void) setupView {
	const GLfloat zNear = 1, zFar = 600, fieldOfView = 40*M_PI/180.0;
	
	esMatrixLoadIdentity(&projection);
	GLfloat size = zNear * tanf(fieldOfView / 2.0); 
	esFrustum(&projection, -size, size, -size / (backingWidth / backingHeight), size / (backingWidth / backingHeight), zNear, zFar); 
	
	esMatrixLoadIdentity(&modelView);
	
	glViewport(0, 0, backingWidth, backingHeight);  
}
*/

- (void) setupView {
#ifdef OPENGLES2 // BACKC
//	const GLfloat zNear = 1, zFar = 600, fieldOfView = 40*M_PI/180.0;
	
/*	esMatrixLoadIdentity(&projection);
	GLfloat size = zNear * tanf(fieldOfView / 2.0); 
	esFrustum(&projection, -size, size, -size / (self.frame.size.width / self.frame.size.height), size / (self.frame.size.width / self.frame.size.height), zNear, zFar); 
*/
    esMatrixLoadIdentity(&projection);
//	GLfloat size = zNear * tanf(fieldOfView / 2.0); 
/*    const GLfloat zNear = -10, zFar = 10, fieldOfView = 40*M_PI/180.0;
    GLfloat size = 1;
	esOrtho(&projection, -size, size, -size / (self.frame.size.width / self.frame.size.height), size / (self.frame.size.width / self.frame.size.height), zNear, zFar); 
 */
//    SCREEN_WIDTH = self.frame.size.width;
//    SCREEN_HEIGHT = self.frame.size.height;
//    esOrtho(&projection, 0.0f, backingWidth, 0.0f, backingHeight, -1.0f, 1.0f);
//    esOrtho(&projection, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
    if(displaynumber == 2)
    {

        double scalex = 1;//SCREEN_WIDTH/backingWidth;
        double scaley = 1;//SCREEN_HEIGHT/backingHeight;
        esOrtho(&projection, 0.0f, SCREEN_WIDTH*scalex, 0.0f, SCREEN_HEIGHT*scaley, -1.0f, 1.0f);
        esMatrixLoadIdentity(&modelView);
        glViewport(0, 0, backingWidth, backingHeight);
//        glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        /*
        esOrtho(&projection, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
        esMatrixLoadIdentity(&modelView);
        glViewport(0, 0, backingWidth, backingHeight);
         */
    }
    else
    {
        esOrtho(&projection, 0.0f, SCREEN_WIDTH, 0.0f, SCREEN_HEIGHT, -1.0f, 1.0f);
        esMatrixLoadIdentity(&modelView);
        glViewport(0, 0, backingWidth, backingHeight);
    }
    
	
//    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
//	glViewport(0, 0, self.frame.size.width, self.frame.size.height);
#endif
}



- (void)setFramebuffer
{
    if (context)
    {
        [EAGLContext setCurrentContext:context];
        
        if (!viewFramebuffer)
            [self createFramebuffer2];
#ifdef OPENGLES2        // BACKC
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
#endif
        
        glViewport(0, 0, backingWidth, backingHeight);
    }
}

- (BOOL)presentFramebuffer
{
    BOOL success = FALSE;
    
    if (context)
    {
        [EAGLContext setCurrentContext:context];
        
#ifdef OPENGLES2 // BACKC
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
        
        success = [context presentRenderbuffer:GL_RENDERBUFFER];
#endif
    }
    
    return success;
}




- (void)destroyFramebuffer {
    if (isRendering_PATCH_VARIABLE)
    {
        NSLog(@"GOTCHA - CRASH AVOIDED");
    }
    
    if (context && !isRendering_PATCH_VARIABLE)
    {
    
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;
    
    if(depthRenderbuffer) {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
        
    }
}


- (void)startAnimation {
    self.animationTimer = [NSTimer scheduledTimerWithTimeInterval:animationInterval target:self selector:@selector(drawView) userInfo:nil repeats:YES];
//#define LATE_LAUNCH2
#ifdef LATE_LAUNCH2
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
#ifndef SLEEPER
	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urMus.lua"];
#else
	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urSleeperLaunch.lua"];
#endif
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
	
	// start off http server
#define HTTP_EDITING
#ifdef HTTP_EDITING
	http_start([resourcePath UTF8String],
			   [documentPath UTF8String]);
#endif
	
	const char* filestr = [filePath UTF8String];
	
	if(luaL_dofile(lua, filestr)!=0)
	{
		const char* error = lua_tostring(lua, -1);
		errorstr = [[NSString alloc] initWithCString:error ]; // DPrinting errors for now
		newerror = true;
        ur_Log(errorstr.c_str());
	}
#endif
	
}


- (void)stopAnimation {
    self.animationTimer = nil;
    glFinish();
}

- (void)stopAudio {
    // CLean up audio
#ifdef USEMUMOAUDIO
    MoAudio::stop();
#else
	stopRIOAudioLayer();
#endif
}

- (void)stopOSCListener {
    // Clean up OSC Networking
    myoscnet.stopListening();
}


- (void)setAnimationTimer:(NSTimer *)newTimer {
    [animationTimer invalidate];
    animationTimer = newTimer;
}


- (void)setAnimationInterval:(NSTimeInterval)interval {
    
    animationInterval = interval;
    if (animationTimer) {
        [self stopAnimation];
        [self startAnimation];
    }
}


- (void)dealloc {
    
    [self stopAnimation];
    
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }

    // Shut down networking
	
	[self stopCurrentResolve];
	self.services = nil;
    for(id key in searchtype) {
        NSNetServiceBrowser *obj = [searchtype objectForKey:key];      // We use the (unique) key to access the (possibly non-unique) object.
        [obj stop];
        obj = nil;
    }
	[self.netService removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
	self.netService = nil;
	[_searchingForServicesString release];
	[_ownName release];
	[_ownEntry release];

    [ActiveTouches release];
    [context release];  
    [super dealloc];
}


#pragma mark -
#pragma mark === Touch handling  ===
#pragma mark

#define NR_FINGERS 2

CGFloat distanceBetweenPoints(CGPoint first, CGPoint second)
{
	CGFloat deltax = second.x-first.x;
	CGFloat deltay = second.y-first.y;
	return sqrt(deltax*deltax + deltay*deltay);
}

/*
int NumHitMatches(urAPI_Region_t* hitregion[], int max, int idx, int repeat)
{
	int count = 0;
	for(int i=0; i<max; i++)
		if(hitregion[idx] == hitregion[i])
			count++;

}
*/

// Platform independent stuff

urAPI_Region_t* hitregion[MAX_FINGERS];

void onTouchDownParse(int t, int numTaps, float posx, float posy, int id)
{
//	if(t>=0)
	{
        t = id; // GE
        
		hitregion[t] = NULL;
		cursorpositionx[t] = posx;
		cursorpositiony[t] = posy;
		
		hitregion[t] = findRegionHit(posx, TOUCH_SCREEN_HEIGHT-posy);
		if(hitregion[t]!=nil)
		{
			float x = posx - hitregion[t]->left;
			float y = (TOUCH_SCREEN_HEIGHT - posy) - hitregion[t]->bottom;
            //			assert(hitregion[t]->numinputs< MAX_FINGERS);
			if(hitregion[t]->numinputs< MAX_FINGERS && findInputPointIndex(hitregion[t], id)==-1)
			{
                hitregion[t]->lastinputx[hitregion[t]->numinputs] = x;  // Input coordinates are relative to bottom left of region
                hitregion[t]->lastinputy[hitregion[t]->numinputs] = y;
                hitregion[t]->lastinputid[hitregion[t]->numinputs] = id;
                hitregion[t]->numinputs++;
  //              LOGI("added touch: %p %d %d %d %f %f",hitregion[t], t, id, hitregion[t]->numinputs,posx,posy);
			}
//			else if(hitregion[t]->numinputs >= MAX_FINGERS)
//				LOGI("OUCHIEOUCH");
            
            //			LOGI("x: %f y: %f", x,y);
			// A double tap.
            lock_events();
			if (numTaps == 2 && hitregion[t]->OnEvents[OnDoubleTap]) 
			{
				callScript(OnDoubleTap,hitregion[t]->OnEvents[OnDoubleTap], hitregion[t]);
				//					callScript(hitregion[t]->OnTouchUp, hitregion[t]);
			}
			else if (numTaps == 3 && false)
			{
				// Tripple Tap NYI
			}
			else if (numTaps == 1)
				callScriptWith2Args(OnTouchDown,hitregion[t]->OnEvents[OnTouchDown], hitregion[t],x,y);
			else {
				callScriptWith2Args(OnTouchDown,hitregion[t]->OnEvents[OnTouchDown], hitregion[t],x,y);
//				callScriptWith2Args(OnTouchDown,hitregion[t]->OnTouchUp, hitregion[t],x,y); // GESSL: Double Tap issue.
			}
            unlock_events();
		}
	}
}

int arg = 0;
void onTouchArgInit()
{
	arg = 0;
}

void onTouchMoveUpdate(int t, int t2, float oposx, float oposy, float posx, float posy, int id)
{
//	if(t2 >=0)
	{
		cursorscrollspeedx[t2] = posx - oposx;
		cursorscrollspeedy[t2] = posy - oposy;
		cursorpositionx[t2] = (int)posx;
		cursorpositiony[t2] = (int)posy;
		cursorpositionid[t2] = id;
		oldcursorpositionx[t2] = (int)oposx;
		oldcursorpositiony[t2] = (int)oposy;
		differentialdrag = false;
		argmoved[arg] = t;
        argids[arg] = id;
		argcoordx[arg] = posx;
		argcoordy[arg] = TOUCH_SCREEN_HEIGHT-posy;
		arg2coordx[arg] = oposx;
		arg2coordy[arg] = TOUCH_SCREEN_HEIGHT-oposy;
		arg++;
	}
}

int findInputPointIndex(urAPI_Region_t* t, int id)
{
	for(int i=0; i < t->numinputs; i++)
	{
		if(t->lastinputid[i] == id)
		{
            //			LOGI("FIP: %p %i",t, i);
			return i;
		}
        //		LOGI("FI: %p %d != %d", t, t->lastinputid[i], id);
	}
//	LOGI("FPI: %p NONE %d",t, t->numinputs);
	return -1;
}

void removeInputPoint(urAPI_Region_t* t, int id)
{
	int idx = findInputPointIndex(t,id);
	if(idx !=-1)
	{
//		LOGI("RemoveInput: %d %d %d %d %f %f", id, idx, t->lastinputid[idx], t->numinputs, t->lastinputx[idx], t->lastinputy[idx]);
        
		for(int i = idx+1; i < t->numinputs; i++)
		{
			t->lastinputid[i-1] = t->lastinputid[i];
			t->lastinputx[i-1] = t->lastinputx[i];
			t->lastinputy[i-1] = t->lastinputy[i];
            //			LOGI("!");
		}
		t->numinputs = t->numinputs-1;
	}
}

bool findInputPointById(urAPI_Region_t* t, int id, float &x, float &y)
{
	int i = findInputPointIndex(t, id);
	if(i == -1)
		return false; // None found
	else
	{
        x = t->lastinputx[i];
        y = t->lastinputy[i];
        return true;
	}
    
}

void adjustInputXOffsetById(urAPI_Region_t* t, int id, float x)
{
	int i = findInputPointIndex(t, id);
	if(i == -1)
		return; // None found
	else
	{
		t->lastinputx[i] = x;
	}
}

void adjustInputYOffsetById(urAPI_Region_t* t, int id, float y)
{
	int i = findInputPointIndex(t, id);
	if(i == -1)
		return; // None found
	else
	{
		t->lastinputy[i] = y;
	}
}

void onTouchEnds(int numTaps, float oposx, float oposy, float posx, float posy, int id)
{
	urAPI_Region_t* hitregion = findRegionHit(posx, TOUCH_SCREEN_HEIGHT-posy);
	if(hitregion /* && numTaps <= 1 */) // GESSL: Double tab issue
	{
		float x,y;
        lock_events();
		if(findInputPointById(hitregion, id, x, y))
		{
            callScriptWith2Args(OnTouchUp, hitregion->OnEvents[OnTouchUp],
                                hitregion, x, y);
            //				hitregion, hitregion->lastinputx, hitregion->lastinputy);
            //		callAllOnLeaveRegions(posx, TOUCH_SCREEN_HEIGHT-posy); // GESSL: Double tab issue
			removeInputPoint(hitregion, id);
		}
        //		else
		else
			callScriptWith2Args(OnTouchUp, hitregion->OnEvents[OnTouchUp],
                                hitregion, posx-hitregion->left, posy-hitregion->bottom);
        
        unlock_events();

	}
	else
	{
		argcoordx[arg] = posx;
		argcoordy[arg] = TOUCH_SCREEN_HEIGHT-posy;
		arg2coordx[arg] = oposx;
		arg2coordy[arg] = TOUCH_SCREEN_HEIGHT-oposy;
		arg++;
	}
}


void ClampRegion(urAPI_Region_t*region)
{
    
	if(region->left < region->clampleft)
    {
        float dx = region->clampleft - region->left;
        region->ofsx = region->ofsx + dx; // adjust anchor
        region->left = region->clampleft;
    }
	if(region->bottom < region->clampbottom)
    {
        float dy = region->clampbottom - region->bottom;
        region->ofsy = region->ofsy + dy; // adjust anchor
        region->bottom = region->clampbottom;
    }
	if(region->width > region->clampwidth) region->width = region->clampwidth;
	if(region->height > region->clampheight) region->height = region->clampheight;
	if(region->left+region->width > region->clampleft + region->clampwidth)
    {
        float dx = region->left+region->width - (region->clampleft + region->clampwidth);
        region->ofsx = region->ofsx - dx; // adjust anchor
        region->left = (region->clampleft + region->clampwidth)-region->width;
    }
	if(region->bottom+region->height > region->clampbottom + region->clampheight)
    {
        float dy = region->bottom+region->height - (region->clampbottom + region->clampheight);
        region->ofsy = region->ofsy - dy; // adjust anchor
        region->bottom = (region->clampbottom + region->clampheight)-region->height;
    }
//    if(region->ofsx+region->width > TOUCH_SCREEN_WIDTH) region->ofsx = TOUCH_SCREEN_WIDTH-region->width;;
//    if(region->ofsy+region->height > TOUCH_SCREEN_HEIGHT) region->ofsy = TOUCH_SCREEN_HEIGHT-region->height;
}


void onTouchDoubleDragUpdate(int t, int dragidx, float pos1x, float pos1y, float pos2x, float pos2y)
{
//	if(t>=0)
	{
		float dx = cursorscrollspeedx[t];
		float dy = -(cursorscrollspeedy[t]);
//		if( dx !=0.0 || dy != 0.0)
		{
            urAPI_Region_t* dragregion = dragtouches[dragidx].dragregion;
            if(dragregion->isMovable)
            {
                if(differentialdrag)
				{
                    
                    dragregion->left += dx;
                    dragregion->bottom += dy;
                    float cursorpositionx2 = pos2x;
                    float cursorpositiony2 = pos2y;
                    if(dragregion->isResizable)
                    {
                        float deltanewwidth = fabs(cursorpositionx2-pos1x);
                        float deltanewheight = fabs(cursorpositiony2-pos1y);
                        float width = dragtouches[dragidx].dragwidth + deltanewwidth;
                        float height = dragtouches[dragidx].dragheight + deltanewheight;
                        if(dragregion->textlabel!=NULL && (width != dragregion->width || height != dragregion->height))
                            dragregion->textlabel->updatestring = true;
                        
                        dragregion->width = width;
                        dragregion->height = height;
                    }
                    dragregion->right = dragregion->left + dragregion->width;
                    dragregion->top = dragregion->bottom + dragregion->height;
                    dragregion->ofsx += dx;
                    dragregion->ofsy += dy;
                }
                else
                {
                    float x,y;
                    //					if(findInputPointById(dragregion, cursorpositionid[t], x, y))
//                    if(cursorpositionid[t] != dragtouches[dragidx].touch1)
                        //                    LOGI("YELP %p %d != %d %d",dragregion,cursorpositionid[t],dragtouches[dragidx].touch1, dragtouches[dragidx].touch2);
                        //					if(findInputPointById(dragregion, cursorpositionid[t], x, y))
                        if(findInputPointById(dragregion, dragtouches[dragidx].touch1, x, y))
                        {
                            //                    LOGI("DD1 %p %f %f %d %d %d", dragregion, x,y, dragtouches[dragidx].touch1, dragtouches[dragidx].touch2, dragtouches[dragidx].touch1);
                            //						float left1 = cursorpositionx[t] - (dragregion->left-x);
                            float bottom1 = (TOUCH_SCREEN_HEIGHT-pos1y) - y;
                            //						float bottom1 = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) - y;
                            //						dragregion->left = cursorpositionx[t] - x;
                            //						dragregion->bottom = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) - y;
                            if (dragregion->isResizable) {
                                float x2,y2;
                                if(findInputPointById(dragregion, dragtouches[dragidx].touch2, x2, y2))
                                {
                                    //                            LOGI("DD2 %p %f %f", dragregion, x2,y2);
                                    //							float left2 = pos2x - (dragregion->left+x2);
                                    float bottom2 = (TOUCH_SCREEN_HEIGHT-pos2y) - y2;
                                    
                                    float width = 0.0;
                                    float height = 0.0;
                                    
                                    float fwidth = pos2x - pos1x;
                                    float fheight = pos1y - pos2y;
                                    
                                    float right1 = dragregion->width-x;
                                    float right2 = dragregion->width-x2;
                                    float top1 = dragregion->height-y;
                                    float top2 = dragregion->height-y2;
                                    
                                    if (x2 > x)
                                    {
                                        if(fwidth >= 0)
                                        {
                                            width = fwidth + x + right2;
                                            adjustInputXOffsetById(dragregion, dragtouches[dragidx].touch2, width-right2);
                                            //										dragregion->left = cursorpositionx[t] - x;
                                            dragregion->left = pos1x - x;
                                        }
                                        else // Finger crossover in X
                                        {
                                            width = -fwidth + x2 + right1;
                                            adjustInputXOffsetById(dragregion, dragtouches[dragidx].touch1, width-right1);
                                            //										adjustInputXOffsetById(dragregion, cursorpositionid[t], width-right1);
                                            dragregion->left = pos2x - x2;
                                        }
                                        
                                        //									LOGI("1: width: %f %f %f %f %f %f %f", width, x,  x2, pos1x, pos2x, x+fwidth, (dragregion->width-x2));
                                    }
                                    else
                                    {
                                        if(fwidth <= 0)
                                        {
                                            width = -fwidth + x2 + right1;
                                            //										adjustInputXOffsetById(dragregion, cursorpositionid[t], width-right1);
                                            adjustInputXOffsetById(dragregion, dragtouches[dragidx].touch1, width-right1);
                                            dragregion->left = pos2x - x2;
                                        }
                                        else // Finger crossover in Y
                                        {
                                            width = fwidth + x + right2;
                                            adjustInputXOffsetById(dragregion, dragtouches[dragidx].touch2, width-right2);
                                            //										dragregion->left = cursorpositionx[t] - x;
                                            dragregion->left = pos1x - x;
                                        }
                                        //									LOGI("2: width: %f %f %f %f %f %f %f", width, x,  x2, pos1x, pos2x, x+fwidth, (dragregion->width-x));
                                        
                                    }
                                    
                                    if( y2 > y)
                                    {
                                        if(fheight >= 0)
                                        {
                                            height = fheight + y + top2;
                                            adjustInputYOffsetById(dragregion, dragtouches[dragidx].touch2, height - top2);
                                            //										dragregion->bottom = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) - y;
                                            dragregion->bottom = (TOUCH_SCREEN_HEIGHT-pos1y) - y;
                                        }
                                        else // Finger crossover in Y
                                        {
                                            height = -fheight + y2 + top1;
                                            //										adjustInputYOffsetById(dragregion, cursorpositionid[t], height - top1);
                                            adjustInputYOffsetById(dragregion, dragtouches[dragidx].touch1, height - top1);
                                            dragregion->bottom = (TOUCH_SCREEN_HEIGHT-pos2y) - y2;
                                            
                                        }
                                    }
                                    else
                                    {
                                        if(fheight <= 0)
                                        {
                                            height = -fheight + y2 + top1;
                                            //										adjustInputYOffsetById(dragregion, cursorpositionid[t], height-top1);
                                            adjustInputYOffsetById(dragregion, dragtouches[dragidx].touch1, height-top1);
                                            dragregion->bottom = (TOUCH_SCREEN_HEIGHT-pos2y) - y2;
                                        }
                                        else // Finger crossover in Y
                                        {
                                            height = fheight + y + top2;
                                            adjustInputYOffsetById(dragregion, dragtouches[dragidx].touch2, height - top2);
                                            //										dragregion->bottom = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) - y;
                                            dragregion->bottom = (TOUCH_SCREEN_HEIGHT-pos1y) - y;
                                        }
                                    }
                                    if (dragregion->textlabel != NULL
                                        && (width != dragregion->width
                                            || height != dragregion->height))
                                        dragregion->textlabel->updatestring = true;

                                    dragregion->width = width;
                                    dragregion->height = height;
                                    //								LOGI("Dragging D %d %f %f %f %f %f %f %f %f %f %f", dragidx, x,x2,y,y2,left1, left2, bottom1, bottom2, width,height);
                                }
                                else
                                {
                                    float deltanewwidth = fabs(pos2x - pos1x) /* * xscale*/;
                                    float deltanewheight = fabs(pos2y - pos1y) /* * yscale*/;
                                    float width = dragtouches[dragidx].dragwidth
                                    + deltanewwidth;
                                    float height = dragtouches[dragidx].dragheight
                                    + deltanewheight;
                                    if (dragregion->textlabel != NULL
                                        && (width != dragregion->width
                                            || height != dragregion->height))
                                        dragregion->textlabel->updatestring = true;
                                    
                                    dragregion->width = width;
                                    dragregion->height = height;
                                }
                            }
                            dragregion->right = dragregion->left+dragregion->width;
                            dragregion->top = dragregion->bottom+dragregion->height;
                            dragregion->cx = dragregion->left + dragregion->width / 2;
                            dragregion->cy = dragregion->bottom + dragregion->height / 2;
                            reanchor(dragregion); // Fix up anchor offset from screen coordinates
                        }
//                        else
//                            LOGI("NO DD ID FOUND %f %f %d %d %d",x,y, dragtouches[dragidx].touch1, dragtouches[dragidx].touch2, dragtouches[dragidx].touch1);
                    
                }
                
                if(dragregion->isClamped) ClampRegion(dragregion);
                changeLayout(dragregion);
                callScriptWith5Args(OnDragging,dragregion->OnEvents[OnDragging], dragregion, dragregion->cx,dragregion->cy, dx, dy,t+1);
            }
            if(dragregion->isResizable)
                callScriptWith4Args(OnSizeChanged,dragregion->OnEvents[OnSizeChanged], dragregion, pos1x,pos1y,pos2x,pos2y);
		}
	}
}

bool testDoubleDragStart(int t1, int t2)
{
	if(hitregion[t1] != NULL && hitregion[t1] == hitregion[t2] && (hitregion[t1]->isMovable || hitregion[t1]->isResizable)) // Pair of fingers on draggable region?
		return true;
	else
		return false;
}

void doTouchDoubleDragStart(int t1,int t2,int touch1, int touch2)
{
    //	if (t1 >= 0 && t2 >= 0) { GE
	{
		t1 = touch1;
		t2 = touch2;
		hitregion[t1]->isDragged = true; // YAYA
		hitregion[t1]->isResized = true;
		int dragidx = FindSingleDragTouch(touch1);
		int dragidx2 = FindSingleDragTouch(touch2);
        //		int dragidx = touch1;
        //		int dragidx2 = touch2;
        
		if(dragidx == -1)
		{
			dragidx = dragidx2;
			dragidx2 = -1;
		}
		if(dragidx == -1 && dragidx2 == -1)
			dragidx = FindAvailableDragTouch();
//		else
//			LOGI("FOUND EXISTING DRAG: %d", dragidx);
        
		dragtouches[dragidx].dragregion = hitregion[t1];
//		LOGI("SDDS: %p %d %d %d %d %d %d", hitregion[t1], t1, t2, dragidx, dragidx2, touch1, touch2);
		dragtouches[dragidx].touch1 = touch1; //UITouch2UTID([[touches allObjects] objectAtIndex:t1]);
		dragtouches[dragidx].touch2 = touch2; //UITouch2UTID([[touches allObjects] objectAtIndex:t2]);
		dragtouches[dragidx].dragwidth = hitregion[t1]->width - fabs(cursorpositionx[t2] - cursorpositionx[t1]);
		dragtouches[dragidx].dragheight = hitregion[t1]->height - fabs(cursorpositiony[t2] - cursorpositiony[t1]);
		dragtouches[dragidx].active = true;
	}
}

bool testSingleDragStart(int t)
{
	if(hitregion[t]!=nil && (hitregion[t]->isMovable || hitregion[t]->isResizable))
		return true;
	else 
		return false;
}

int getSingleDoubleTouchConversionID(int t)
{
	int dragidx = FindDragRegion(hitregion[t]);
	if (dragidx == -1)
		return -1;
	else
		if(dragtouches[dragidx].touch1 != -1)
			return dragtouches[dragidx].touch1;
		else
			return dragtouches[dragidx].touch2;
}

void ComputeDragWidth(int dragidx, float pos1x, float pos1y,
                      float pos2x, float pos2y) {
	if (dragtouches[dragidx].touch1 != -1 && dragtouches[dragidx].touch2 != -1) {
		dragtouches[dragidx].dragwidth =
		dragtouches[dragidx].dragregion->width
		- fabs(pos2x - pos1x);
		dragtouches[dragidx].dragheight =
		dragtouches[dragidx].dragregion->height
		- fabs(pos2y - pos1y);
	}
}

void doTouchSingleDragStart(int t, int touch1, float pos1x, float pos1y, float pos2x, float pos2y)
{
//	TestAllDragFlags("SDS");
        // GE
        t = touch1;
        hitregion[t]->isDragged = true; // YAYA
        int dragidx = FindDragRegion(hitregion[t]);
        if (dragidx == -1) {
            dragidx = FindAvailableDragTouch();
            dragtouches[dragidx].dragregion = hitregion[t];
            dragtouches[dragidx].touch1 = touch1;
            dragtouches[dragidx].touch2 = -1;
            dragtouches[dragidx].active = true;
            // Equivalent to OnTouchDown when dragging is enabled
            //            callScript(OnDragStart,dragtouches[dragidx].dragregion->OnEvents[OnDragStart], dragtouches[dragidx].dragregion);
        } else {
            AddDragRegion(dragidx, touch1);
            ComputeDragWidth(dragidx, pos1x, pos1y,
                             pos2x, pos2y);
        }
}

void onTouchSingleDragUpdate(int t, int dragidx)
{
	if(t>=0 && dragidx>=0)
	{
		float dx = cursorscrollspeedx[t];
		float dy = -(cursorscrollspeedy[t]);
		if( dx !=0.0 || dy != 0.0)
		{
			urAPI_Region_t* dragregion = dragtouches[dragidx].dragregion;
            if(dragregion->isMovable)
            {
                float offsx = dragregion->ofsx - oldcursorpositionx[t];
                float offsy = dragregion->ofsy - (TOUCH_SCREEN_HEIGHT - oldcursorpositiony[t]);
                float offleft = dragregion->left - oldcursorpositionx[t];
                float offbottom = dragregion->bottom - (TOUCH_SCREEN_HEIGHT - oldcursorpositiony[t]);
                
                if(differentialdrag)
				{
					dragregion->left = cursorpositionx[t] + offleft;
					dragregion->bottom = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) + offbottom;
					dragregion->right = dragregion->left+dragregion->width;
					dragregion->top = dragregion->bottom+dragregion->height;
                    //					dragregion->right += dx;
                    //					dragregion->top += dy;
                    //					dragregion->ofsx += dx;//*0.85;
                    //					dragregion->ofsy += dy;//*0.85;
                    dragregion->ofsx = cursorpositionx[t]+offsx;
                    dragregion->ofsy = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t])+offsy;
                    dragregion->ofsx = cursorpositionx[t];
                    dragregion->ofsy = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]);
				}
				else
				{
					float x,y;
					if(findInputPointById(dragregion, cursorpositionid[t], x, y))
					{
                        //						dragregion->left = cursorpositionx[t] - (dragregion->left + x);
                        //						dragregion->bottom = cursorpositiony[t] - (dragregion->bottom + y);
                        //						dragregion->right = 0;
                        //						dragregion->top = 0;
                        //						LOGI("SD: %f %f", x, y);
						dragregion->left = cursorpositionx[t] - x;
						dragregion->bottom = (TOUCH_SCREEN_HEIGHT-cursorpositiony[t]) - y;
						dragregion->right = dragregion->left+dragregion->width;
						dragregion->top = dragregion->bottom+dragregion->height;
						dragregion->cx = dragregion->left + dragregion->width / 2;
						dragregion->cy = dragregion->bottom + dragregion->height / 2;
						reanchor(dragregion); // Fix up anchor offset from screen coordinates
					}
					else // Old school scrolling 
					{
                        //					dragregion->left += dx;
                        //					dragregion->bottom += dy;
                        //					dragregion->right += dx;
                        //					dragregion->top += dy;
                        dragregion->ofsx += dx;
                        dragregion->ofsy += dy;
					}
				}

                changeLayout(dragregion);
                callScriptWith5Args(OnDragging,dragregion->OnEvents[OnDragging], dragregion, dragregion->cx,dragregion->cy, dx, dy,t+1);
            }
		}
	}
}

void onTouchScrollUpdate(int t)
{
	urAPI_Region_t* scrollregion = findRegionXScrolled(cursorpositionx[t],TOUCH_SCREEN_HEIGHT-cursorpositiony[t],cursorscrollspeedx[t]);
	if(scrollregion != nil)
	{
		callScriptWith1Args(OnHorizontalScroll,scrollregion->OnEvents[OnHorizontalScroll], scrollregion, cursorscrollspeedx[t]);
	}

	scrollregion = findRegionYScrolled(cursorpositionx[t],TOUCH_SCREEN_HEIGHT-cursorpositiony[t],-cursorscrollspeedy[t]);
	if(scrollregion != nil)
	{
		callScriptWith1Args(OnVerticalScroll,scrollregion->OnEvents[OnVerticalScroll], scrollregion, -cursorscrollspeedy[t]);
	}

	scrollregion = findRegionMoved(cursorpositionx[t],TOUCH_SCREEN_HEIGHT-cursorpositiony[t],cursorscrollspeedx[t],-cursorscrollspeedy[t]);
	
	if(scrollregion != nil)
	{
        
		callScriptWith5Args(OnMove,scrollregion->OnEvents[OnMove], scrollregion, cursorpositionx[t]-scrollregion->left-cursorscrollspeedx[t],TOUCH_SCREEN_HEIGHT-cursorpositiony[t]-scrollregion->bottom+cursorscrollspeedy[t], cursorscrollspeedx[t], -cursorscrollspeedy[t],t+1);
	}
}

void onTouchDragEnd(int t,int touch, float posx, float posy)
{
	if(touch >=0 && t>=0)
	{
		
		cursorpositionx[t] = posx;
		cursorpositiony[t] = posy;
        
        RemoveUTID(touch);

		int dragidx = FindSingleDragTouch(touch);
		
		if(dragidx != -1)
		{
			if(dragtouches[dragidx].touch1 == touch)
			{
//				RemoveUTID(dragtouches[dragidx].touch1);
				dragtouches[dragidx].touch1 = -1;
			}
			if(dragtouches[dragidx].touch2 == touch)
			{
//				RemoveUTID(dragtouches[dragidx].touch2);
				dragtouches[dragidx].touch2 = -1;
			}
			if(	dragtouches[dragidx].touch1 == -1 && dragtouches[dragidx].touch2 == -1)
			{
				dragtouches[dragidx].active = false;
				dragtouches[dragidx].dragregion->isDragged = false;
                // Equivalent to OnTouchUp when dragging is enabled.
				callScript(OnDragStop,dragtouches[dragidx].dragregion->OnEvents[OnDragStop], dragtouches[dragidx].dragregion);
			}
			/*else if(dragtouches[dragidx].touch2 != -1)
			{
				RemoveUTID(dragtouches[dragidx].touch1);
				dragtouches[dragidx].touch1 = dragtouches[dragidx].touch2;
				dragtouches[dragidx].touch2 = -1;
			}*/
			if (dragtouches[dragidx].touch1 != -1
                || dragtouches[dragidx].touch2 != -1)
                dragtouches[dragidx].dragregion->isResized = false;

			if (dragtouches[dragidx].touch1 == -1 && dragtouches[dragidx].touch2 != -1)
			{
				dragtouches[dragidx].touch1 = dragtouches[dragidx].touch2;
				dragtouches[dragidx].touch2 = -1;
			}
		}
	}
}


// Handles the start of a touch
- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if (ActiveTouches == nil)
        ActiveTouches = [[NSMutableArray alloc] init];
    
    for (UITouch *touch in touches) {
        if (![ActiveTouches containsObject:touch])
            [ActiveTouches addObject:touch];
    }
	NSUInteger numTouches = [touches count];

#ifdef DEBUG_TOUCH
	char errorstrbuf[16];
	sprintf(errorstrbuf,"Begin %d",numTouches);
	errorstr = errorstrbuf;
	newerror = true;
    ur_Log(errorstr.c_str());
#endif
	
	// Event for all fingers (global). We do this first so people can choose to create/remove regions that can also receive events for the locations (yay)
	for(int t =0; t<numTouches; t++)
	{
		UITouch *touch = [[touches allObjects] objectAtIndex:t];
		CGPoint position = [touch locationInView:self];
		callAllTouchSources(position.x/(float)HALF_SCREEN_WIDTH-1.0, 1.0-position.y/(float)HALF_SCREEN_HEIGHT,t);
	}
	
	for(int t=0; t< numTouches; t++)
	{
		UITouch *touch = [[touches allObjects] objectAtIndex:t];
		NSUInteger numTaps = [touch tapCount];
		CGPoint position = [touch locationInView:self];

        int id = AddUITouch([[touches allObjects] objectAtIndex:t]);
		onTouchDownParse(t, numTaps, position.x, position.y, id);
	}
	
	// Find two-finger drags
	for(int t1 = 0; t1<numTouches-1; t1++)
	{
		for(int t2 = t1+1; t2<numTouches; t2++)
		{
            int touch1 = AddUITouch([[touches allObjects] objectAtIndex:t1]); // Add does not overwrite if it exists.
            int touch2 = AddUITouch([[touches allObjects] objectAtIndex:t2]);
			if(touch1 != -1 && touch2 != -1 && testDoubleDragStart(touch1,touch2))
			{
				doTouchDoubleDragStart(t1,t2,touch1, touch2);
			}
		}
	}
	
	// Find single finger drags (not already classified as two-finger ones.
	for(int t = 0; t<numTouches; t++)
	{
        int touch1 = AddUITouch([[touches allObjects] objectAtIndex:t]);
		if(FindSingleDragTouch(touch1) == -1 && testSingleDragStart(touch1)) // Figure out if we need FindSingleDragTouch here...
		{
			CGPoint position1 = [[[touches allObjects] objectAtIndex:t] locationInView:self];
			CGPoint position2;

			int touch2 = getSingleDoubleTouchConversionID(touch1);
			
			if(touch2 != -1)
			{
                UITouch* ttt = UTID2UITouch(touch2);
				position2 = [ttt locationInView:self];
//  				position2 = [UTID2UITouch(touch2) locationInView:self];
			}
				
			doTouchSingleDragStart(t, touch1, position1.x, position1.y, position2.x, position2.y);
		}
	}		
}

// Handles the continuation of a touch.
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{  
	
	NSUInteger numTouches = [touches count];
#ifdef DEBUG_TOUCH
	char errorstrbuf[16];
	sprintf(errorstrbuf,"Move %d",numTouches);
	errorstr = errorstrbuf;
	newerror = true;
    ur_Log(errorstr.c_str());
#endif

	
	// Event for all fingers (global)
	for(int t =0; t<numTouches; t++)
	{
		UITouch *touch = [[touches allObjects] objectAtIndex:t];
		CGPoint position = [touch locationInView:self];
		callAllTouchSources(position.x/(float)HALF_SCREEN_WIDTH-1.0, 1.0-position.y/(float)HALF_SCREEN_HEIGHT,t);
	}

	onTouchArgInit();
	for(int t=0; t< numTouches; t++)
	{
		UITouch *touch = [[touches allObjects] objectAtIndex:t];
		UITouchPhase phase = [touch phase];
		CGPoint position = [touch locationInView:self];
		if(phase == UITouchPhaseMoved) 
		{
			CGPoint oldposition = [[[touches allObjects] objectAtIndex:t] previousLocationInView:self];
			int t2 = t;
/*			if(oldposition.x != cursorpositionx[t] || oldposition.y != cursorpositiony[t])
			{
				for(t2=0; t2<MAX_FINGERS && (oldposition.x != cursorpositionx[t2] || oldposition.y != cursorpositiony[t2]); t2++);
				if(t2==MAX_FINGERS)
				{
					int a=0;
					t2=t;
				}
			}	*/
			onTouchMoveUpdate(t, t2, oldposition.x, oldposition.y, position.x, position.y,FindSingleDragTouch(UITouch2UTID(touch)));
		}
		else
		{
			int a=0;
		}
	}
	
	for(int i=0; i < arg; i++)
	{
		int t = argmoved[i];
		int dragidx = FindSingleDragTouch(UITouch2UTID([[touches allObjects] objectAtIndex:t]));
		if(dragidx != -1)
		{
			if(dragtouches[dragidx].touch2 != -1) // Double Touch here.
			{
				CGPoint position1 = [UTID2UITouch(dragtouches[dragidx].touch1) locationInView:self];
				CGPoint position2 = [UTID2UITouch(dragtouches[dragidx].touch2) locationInView:self];
				
				onTouchDoubleDragUpdate(t, dragidx, position1.x, position1.y, position2.x, position2.y);
				
			}
			else
			{
				onTouchSingleDragUpdate(t, dragidx);
//                onTouchScrollUpdate(t); CONFLICT: Resolve. Needed for video edit, but messes with drag over scroll
			}
		}
		else 
		{
			onTouchScrollUpdate(t);
		}
	}
	
	callAllOnEnterLeaveRegions(arg, argcoordx, argcoordy,arg2coordx,arg2coordy);
}

// Handles the end of a touch event.
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
#ifdef DEBUG_TOUCH
	errorstr = "End";
	newerror = true;
    ur_Log(errorstr.c_str());
#endif
    for (UITouch *touch in touches) {
        [ActiveTouches removeObject:touch];
    }
	NSUInteger numTouches = [touches count];

	// Event for all fingers (global). We do this first so people can choose to create/remove regions that can also receive events for the locations (yay)
	for(int t =0; t<numTouches; t++)
	{
		UITouch *touch = [[touches allObjects] objectAtIndex:t];
		CGPoint position = [touch locationInView:self];
		callAllTouchSources(position.x/(float)HALF_SCREEN_WIDTH-1.0, 1.0-position.y/(float)HALF_SCREEN_HEIGHT,t);
	}
	
	onTouchArgInit();
	
	for(int t=0; t< numTouches; t++)
	{
		UITouch *touchip = [[touches allObjects] objectAtIndex:t];
		int touch = UITouch2UTID(touchip);
		UITouchPhase phase = [touchip phase];
		CGPoint position = [touchip locationInView:self];

		if(phase == UITouchPhaseEnded)		{

			onTouchDragEnd(t,touch,position.x,position.y);
			CGPoint oldposition = [touchip previousLocationInView:self];
			NSUInteger numTaps = [touchip tapCount];
			onTouchEnds(numTaps, oldposition.x, oldposition.y, position.x, position.y, touch);
		}
		else
		{
			int a = 0;
		}
	}

	callAllOnLeaveRegions(arg, argcoordx, argcoordy,arg2coordx,arg2coordy);
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    // Enumerates through all touch object
    for (UITouch *touch in touches){
		// Sends to the dispatch method, which will make sure the appropriate subview is acted upon
	}
}

#ifdef SANDWICH_SUPPORT
// sandwich update Delegate functions
- (void) rearTouchUpdate: (SandwichEventManager * ) sender;
{
	
	CGPoint touchCoords = [sender touchCoordsForTouchAtIndex: 0];
	//	tx = touchCoords.x; gessl disabling rear
	//	ty = touchCoords.y;
}

- (void) pressureUpdate: (SandwichEventManager * ) sender;
{
	pressure[0] = sender.pressureValues[0];
	pressure[1] = sender.pressureValues[1];
	pressure[2] = sender.pressureValues[2];
	pressure[3] = sender.pressureValues[3];
	
	float avg = pressure[3];	
	
	// This feeds the lua API events
	callAllOnPressure(avg);
	
	// We call the UrSound pipeline second so that the lua engine can actually change it based on acceleration data before anything happens.
//	callAllPressureSources(avg);
	
}
#endif


// Networking

// The Bonjour application protocol, which must:
// 1) be no longer than 14 characters
// 2) contain only lower-case letters, digits, and hyphens
// 3) begin and end with lower-case letter or digit
// It should also be descriptive and human-readable
// See the following for more information:
// http://developer.apple.com/networking/bonjour/faq.html
#define kurNetIdentifier		@"urmus"


#define kurNetTestID	@"_urmus._udp."

extern MoNet myoscnet;

void Net_Send(float data)
{
    const char* oscip;
    for(int i=0; i<[g_glView->remoteIPs count]; i++)
    {
        oscip=[[g_glView->remoteIPs objectAtIndex:i] UTF8String];
        myoscnet.startSendStream(oscip,8888);
        myoscnet.startSendMessage("/urMus/netstream");
        
        myoscnet.addSendFloat(data);
        
        myoscnet.endSendMessage();
        myoscnet.closeSendStream();
    }
}

void Net_Advertise(const char* nsid, int port)
{
	NSString *nsid2 =  [[NSString alloc] initWithUTF8String: nsid];
	NSString *fullnsid = [NSString stringWithFormat:@"_%@urmus._udp.", nsid2];
	[g_glView advertiseService:[[UIDevice currentDevice] name] withID:fullnsid atPort:port];
//    [nsid2 release];
}

- (void) stopAdvertisingService
{
    [self.netService stop];
    [self.netService release];
}

- (void) stopFindService:(NSString *)btype
{
    NSString *fullnsid = [NSString stringWithFormat:@"_%@urmus._udp.", btype];
    NSNetServiceBrowser *aNetServiceBrowser = [searchtype objectForKey:fullnsid];
    if(aNetServiceBrowser!=NULL)
    {
        [aNetServiceBrowser stop];
    }
    //    [self.netServiceBrowser stop];
    //	[self.services removeAllObjects];
}

void Stop_Net_Advertise(const char* nsid)
{
	[g_glView stopAdvertisingService];
}

void Stop_Net_Find(const char* nsid)
{
    NSString *nsid2 =  [[NSString alloc] initWithUTF8String: nsid];
    [g_glView stopFindService:nsid2];
}

void Net_Find(const char* nsid)
{
	NSString *nsid2 =  [[NSString alloc] initWithUTF8String: nsid];
	NSString *fullnsid = [NSString stringWithFormat:@"_%@urmus._udp.", nsid2];
	[g_glView searchForServicesOfType:fullnsid inDomain:@""];
}

- (void) advertiseService:(NSString *)name withID:(NSString *)nsid atPort:(int)port {
	self.netService = [[NSNetService alloc] initWithDomain:@""
											  type:nsid
											  name:name
											  port:port];
	// Delegate is informed of status asynchronously

	[self.netService scheduleInRunLoop:[NSRunLoop currentRunLoop]
							   forMode:NSRunLoopCommonModes];
	
	[self.netService setDelegate:self];
	[self.netService publish];
	[self.netService retain];
}

#define MAX_THROTTLE 1

int throttle = MAX_THROTTLE;

void oscCallBack3(osc::ReceivedMessageArgumentStream & argument_stream, osc::ReceivedMessage::const_iterator & arg, void * data)
{
    if(throttle > 0)
    {
        throttle --;
    }
    else
    {
        throttle = MAX_THROTTLE;
        float num;
        argument_stream >> num;
        callAllNetSingleTickSources(num*128.0);
    }
}

- (void) setupNetConnects {
    remoteIPs = [[NSMutableArray alloc] init];
    _services = [[NSMutableArray alloc] init];
    searchtype = [[NSMutableDictionary alloc] init];
    
    NSString *fullnsid = [NSString stringWithFormat:@"_%@urmus._udp.", @"net1"];
    [self searchForServicesOfType:fullnsid inDomain:@""];
	
	[self advertiseService:[[UIDevice currentDevice] name] withID:fullnsid atPort:8888];
    myoscnet.addAddressCallback("/urMus/netstream",oscCallBack3);
    myoscnet.setListeningPort(8888);
    myoscnet.startListening();
}

- (void) setup {
    
	[self advertiseService:[[UIDevice currentDevice] name] withID:kurNetTestID atPort:8888];

	[self searchForServicesOfType:@"_urmus._udp." inDomain:@""];
}
	

// Creates an NSNetServiceBrowser that searches for services of a particular type in a particular domain.
// If a service is currently being resolved, stop resolving it and stop the service browser from
// discovering other services.
- (BOOL)searchForServicesOfType:(NSString *)type inDomain:(NSString *)domain {
	
//    urNetServiceDiscovery *netdiscoverer;
    NSNetServiceBrowser *aNetServiceBrowser;
    if([searchtype objectForKey:type]==NULL)
    {
//        netdiscoverer = [[urNetServiceDiscovery alloc] init];
        aNetServiceBrowser = [[NSNetServiceBrowser alloc] init];
        aNetServiceBrowser.delegate = self;
       if(!aNetServiceBrowser) {
            // The NSNetServiceBrowser couldn't be allocated and initialized.
            return NO;
        }
//        [searchtype setObject:netdiscoverer forKey:type];
        [searchtype setObject:aNetServiceBrowser forKey:type];
    }
    else
    {
        aNetServiceBrowser = [searchtype objectForKey:type];
        [aNetServiceBrowser stop];
    }
//	[self.netServiceBrowser stop];
//	[self.services removeAllObjects];
	
//	self.netServiceBrowser = aNetServiceBrowser;
	[aNetServiceBrowser searchForServicesOfType:type inDomain:domain];
//    [aNetServiceBrowser release];
	return YES;
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)netServiceBrowser didRemoveService:(NSNetService *)service moreComing:(BOOL)moreComing {
	// If a service went away, stop resolving it if it's currently being resolved,
	// remove it from the list and update the table view if no more events are queued.

	for (int i = 0; i < [self.services count]; i++)
	{
        NSNetService* tservice = [self.services objectAtIndex:i];
    
        if([[tservice name] isEqualToString:[service name]] && [[tservice type] isEqualToString:[service type]])
        {  
            
			NSString* ipaddress = [self.remoteIPs objectAtIndex:i];
            NSString* btype = [service type];
            NSRange range = [btype rangeOfString:@"urmus._udp."];
            btype = [btype substringWithRange:NSMakeRange(1, range.location-1)];
            callAllOnNetDisconnect([ipaddress UTF8String],[btype UTF8String]);
            [self.remoteIPs removeObjectAtIndex:i];
            [self.services removeObject:service];
            [self.services removeObject:tservice];
        }
    }
}	

- (void)netServiceBrowser:(NSNetServiceBrowser *)netServiceBrowser didFindService:(NSNetService *)service moreComing:(BOOL)moreComing {
	// If a service came online, add it to the list and update the table view if no more events are queued.
	[service setDelegate:self];
	[service resolveWithTimeout:10];
//	 NSString* temp = [service.name copy];	
	[service retain];
}	

// This should never be called, since we resolve with a timeout of 0.0, which means indefinite
- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict {
//	[self stopCurrentResolve];
	int a;
}

- (NSString *)getStringFromAddressData:(NSData *)dataIn {
	struct sockaddr_in  *socketAddress = nil;
	NSString            *ipString = nil;
	
	socketAddress = (struct sockaddr_in *)[dataIn bytes];
	ipString = [NSString stringWithFormat: @"%s",
				inet_ntoa(socketAddress->sin_addr)];  ///problem here
	return ipString;
}
	
	
- (void)netServiceDidResolveAddress:(NSNetService *)service {
//	int cnt = [[service addresses] count];
	for (int i = 0; i < [[service addresses] count]; i++)
	{
		if ([service.name isEqual:[[UIDevice currentDevice] name]]) {
			self.ownEntry = service;
			[_services removeObject:service];
		}
		else
		{
			NSString* ipaddress = [self getStringFromAddressData:[[service addresses] objectAtIndex:i]];
			if (![ipaddress isEqual:@"0.0.0.0"])
			{
                NSString* btype = [service type];
                NSRange range = [btype rangeOfString:@"urmus._udp."];
                btype = [btype substringWithRange:NSMakeRange(1, range.location-1)];
				callAllOnNetConnect([ipaddress UTF8String],[btype UTF8String]);
                [remoteIPs addObject:ipaddress];
				[self.services addObject:service];
			}
		}
	}
	[service retain];
}

/*- (void)stopCurrentResolve {
	
	self.needsActivityIndicator = NO;
	self.timer = nil;
	
	[self.currentResolve stop];
	self.currentResolve = nil;
}*/

@end

