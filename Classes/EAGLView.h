//
//  EAGLView.h
//  urMus
//
//  Created by Georg Essl on 6/20/09.
//  Copyright Georg Essl 2009. All rights reserved. See LICENSE.txt for license details.
//


#include "config.h"
#import <UIKit/UIKit.h>
#ifdef OPENGLES2
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import "esUtil.h"
#else
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#endif

#ifdef USEMUMOAUDIO
#import "mo_audio.h"
#define SRATE 48000
#define FRAMESIZE 256
#define NUMCHANNELS 2
#else
#include "RIOAudioUnitLayer.h"
#endif

#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>
#import "CaptureSessionManager.h"
#include <string>

#undef SANDWICH_SUPPORT

#ifdef SANDWICH_SUPPORT
#import "UdpServerSocket.h"
#import "SandwichTypes.h"
#import "SandwichUpdateListener.h"
#endif

#import "ExternalKeyboardReaderView.h"

/*
This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
The view content is basically an EAGL surface you render your OpenGL scene into.
Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
*/

#import <Foundation/NSNetServices.h>
#import <Foundation/Foundation.h>

#define MAX_FINGERS 10

#ifdef GPUIMAGE
#import "GPUImage.h"

typedef enum {
    GPUIMAGE_NONE,
    GPUIMAGE_SATURATION,
    GPUIMAGE_CONTRAST,
    GPUIMAGE_BRIGHTNESS,
    GPUIMAGE_EXPOSURE,
    GPUIMAGE_RGB,
    GPUIMAGE_SHARPEN,
    GPUIMAGE_UNSHARPMASK,
    GPUIMAGE_TRANSFORM,
    GPUIMAGE_TRANSFORM3D,
    GPUIMAGE_CROP,
//	GPUIMAGE_MASK,
    GPUIMAGE_GAMMA,
    GPUIMAGE_TONECURVE,
    GPUIMAGE_HAZE,
    GPUIMAGE_SEPIA,
    GPUIMAGE_COLORINVERT,
    GPUIMAGE_GRAYSCALE,
    GPUIMAGE_THRESHOLD,
    GPUIMAGE_ADAPTIVETHRESHOLD,
    GPUIMAGE_PIXELLATE,
    GPUIMAGE_POLARPIXELLATE,
    GPUIMAGE_CROSSHATCH,
    GPUIMAGE_SOBELEDGEDETECTION,
    GPUIMAGE_PREWITTEDGEDETECTION,
    GPUIMAGE_CANNYEDGEDETECTION,
    GPUIMAGE_XYGRADIENT,
/*    GPUIMAGE_HARRISCORNERDETECTION,
    GPUIMAGE_NOBLECORNERDETECTION,
    GPUIMAGE_SHITOMASIFEATUREDETECTION,*/
    GPUIMAGE_SKETCH,
    GPUIMAGE_TOON,
    GPUIMAGE_SMOOTHTOON,
    GPUIMAGE_TILTSHIFT,
    GPUIMAGE_CGA,
    GPUIMAGE_POSTERIZE,
//    GPUIMAGE_CONVOLUTION,
    GPUIMAGE_EMBOSS,
/*    GPUIMAGE_KUWAHARA, */
    GPUIMAGE_VIGNETTE,
    GPUIMAGE_GAUSSIAN,
    GPUIMAGE_GAUSSIAN_SELECTIVE,
    GPUIMAGE_FASTBLUR,
    GPUIMAGE_BOXBLUR,
    GPUIMAGE_MEDIAN,
    GPUIMAGE_BILATERAL,
    GPUIMAGE_SWIRL,
    GPUIMAGE_BULGE,
    GPUIMAGE_PINCH,
    GPUIMAGE_STRETCH,
    GPUIMAGE_DILATION,
    GPUIMAGE_EROSION,
    GPUIMAGE_OPENING,
    GPUIMAGE_CLOSING,
//    GPUIMAGE_PERLINNOISE,
/*    GPUIMAGE_VORONI, */
    GPUIMAGE_MOSAIC,
/*    GPUIMAGE_DISSOLVE,
    GPUIMAGE_CHROMAKEY,
    GPUIMAGE_MULTIPLY,
    GPUIMAGE_OVERLAY,
    GPUIMAGE_LIGHTEN,
    GPUIMAGE_DARKEN,
    GPUIMAGE_COLORBURN,
    GPUIMAGE_COLORDODGE,
    GPUIMAGE_SCREENBLEND,
    GPUIMAGE_DIFFERENCEBLEND,
	GPUIMAGE_SUBTRACTBLEND,
    GPUIMAGE_EXCLUSIONBLEND,
    GPUIMAGE_HARDLIGHTBLEND,
    GPUIMAGE_SOFTLIGHTBLEND,*/
/*    GPUIMAGE_CUSTOM, */
    GPUIMAGE_FILTERGROUP,
    GPUIMAGE_POLKADOT,
    GPUIMAGE_HALFTONE,
    GPUIMAGE_LEVELS,
    GPUIMAGE_MONOCHROME,
    GPUIMAGE_HUE,
    GPUIMAGE_WHITEBALANCE,
/*    GPUIMAGE_LOWPASS,
    GPUIMAGE_HIGHPASS, 
    GPUIMAGE_MOTIONDETECTOR, These crash when you remove them from video target, are cool otherwise */
//    GPUIMAGE_THRESHOLDSKETCH,
    GPUIMAGE_SPHEREREFRACTION,
    GPUIMAGE_GLASSSPHERE,
//    GPUIMAGE_HIGHLIGHTSHADOW,
    GPUIMAGE_LOCALBINARYPATTERN,
    GPUIMAGE_NUMFILTERS,
    maxFilterMode
} GPUImageFilterType; 

#endif

#ifdef OPENGLES2
extern GLint _positionSlot;
extern GLint _texcoordSlot;
extern GLuint _textureUniform;

typedef struct urAPI_Region urAPI_Region_t;

#ifdef GPUIMAGE
@interface urRegionMovie: NSObject <GPUImageTextureOutputDelegate>
{
@public
    urAPI_Region_t* region;
}
@end

@interface urFilterHandler: NSObject <GPUImageTextureOutputDelegate>
{
@public
    urAPI_Region_t* region;
}
@end

#endif
#endif

@interface urNetServiceDiscovery : NSObject <NSNetServiceBrowserDelegate>
{
@public
    NSMutableArray *remoteIPs;
	NSNetService *netService;
	NSMutableArray *_services;
	NSNetServiceBrowser *_netServiceBrowser;    
}
@end

enum recordsource { SOURCE_TEXTURE, SOURCE_CAMERA, SOURCE_MOVIE };

#ifdef SANDWICH_SUPPORT
@interface EAGLView : UIView <UIAccelerometerDelegate,CLLocationManagerDelegate,SandwichUpdateDelegate, CaptureSessionManagerDelegate, NSNetServiceDelegate, NSNetServiceBrowserDelegate>
#else
#ifdef GPUIMAGE
@interface EAGLView : UIView <UIAccelerometerDelegate,CLLocationManagerDelegate, CaptureSessionManagerDelegate, NSNetServiceDelegate, NSNetServiceBrowserDelegate, GPUImageTextureOutputDelegate, ExternalKeyboardEventDelegate>
#else
@interface EAGLView : UIView <UIAccelerometerDelegate,CLLocationManagerDelegate, CaptureSessionManagerDelegate, NSNetServiceDelegate, NSNetServiceBrowserDelegate, ExternalKeyboardEventDelegate>
#endif
#endif
{
@public
	NSNetService *netService;
    NSMutableArray *remoteIPs;
	CaptureSessionManager *captureManager;
@private
    bool isRendering_PATCH_VARIABLE;
    /* The pixel dimensions of the backbuffer */
    GLint backingWidth;
    GLint backingHeight;
    GLint extbackingWidth;
    GLint extbackingHeight;
    
    EAGLContext *context;
    
#ifdef OPENGLES2
    GLuint shaderProgram;
    GLuint _modelviewprojUniform;
	GLuint uniformMvp;
    GLuint uniformColour;
    GLuint whiteTexture;
	
	ESMatrix modelView;
	ESMatrix projection;
	ESMatrix mvp;

#ifdef GPUIMAGE
    GPUImageMovie *movieFile;
    GPUImageOutput<GPUImageInput> *filter;
    GPUImageMovieWriter *movieWriter;
#endif
#endif
    
    /* OpenGL names for the renderbuffer and framebuffers used to render to this view */
    GLuint viewRenderbuffer, viewFramebuffer;
    
    /* OpenGL name for the depth buffer that is attached to viewFramebuffer, if it exists (0 if it does not exist) */
    GLuint depthRenderbuffer;
    
    NSTimer *animationTimer;
    NSTimeInterval animationInterval;
	CLLocationManager *locationManager;
	CMMotionManager *motionManager;
//	NSOperationQueue *opQ;
//	NSInputStream		*_inStream;
//	NSOutputStream		*_outStream;
//	BOOL				_inReady;
//	BOOL				_outReady;
	
	int	max_displays;
	int current_display;
	AVAssetWriter *videoWriter;
	AVAssetWriterInput* writerInput;
	AVAssetWriterInputPixelBufferAdaptor *adaptor;
	
	int displaynumber;

#ifdef GPUIMAGE
@public
    GPUImageVideoCamera *videoCamera;
@private
    GPUImageOutput<GPUImageInput> *inputFilter, *outputFilter;
    GPUImageAverageColor *averageFilter;
    GPUImageTextureOutput *textureOutput;
    GPUImageFilterType currentfiltertype;
    CGSize sourcesize;
    GPUImageTextureInput *textureInput;
    GPUImageTextureInput *textureFilterInput;
    GPUImageCropFilter* cropfilter;
    GPUImageTransformFilter* rotateFilter;
    enum recordsource recordfrom;
    bool latelaunchmovie;
#endif
    // New
    BOOL animating;
    BOOL displayLinkSupported;
    NSInteger animationFrameInterval;
    /*
	 Use of the CADisplayLink class is the preferred method for controlling your animation timing.
	 CADisplayLink will link to the main display and fire every vsync when added to a given run-loop.
	 The NSTimer object is used only as fallback when running on a pre-3.1 device where CADisplayLink isn't available.
	 */
    id displayLink;

@private
//	id<EAGLViewDelegate> _delegate;
	NSString *_searchingForServicesString;
	NSString *_ownName;
	NSNetService *_ownEntry;
	BOOL _showDisclosureIndicators;
	NSMutableArray *_services;
	NSNetServiceBrowser *_netServiceBrowser;
    NSMutableDictionary *searchtype;
	NSNetService *_currentResolve;
	NSTimer *_timer;
	BOOL _needsActivityIndicator;
	BOOL _initialWaitOver;
	GLuint	_cameraTexture;
    long int framecnt;
    double totalelapsedtime;
}

@property (nonatomic, strong) CLLocationManager *locationManager;
@property (nonatomic, retain) EAGLContext *context;
@property NSTimeInterval animationInterval;
@property(nonatomic,retain) NSNetService* netService;
@property(retain) CaptureSessionManager *captureManager;
@property (nonatomic, strong) NSMutableArray *accessibleElements;

//- (id)initWithFrame:(CGRect)frame andContextSharegroup:(EAGLSharegroup*)passedSharegroup;
- (id)initWithFrame:(CGRect)frame andContextSharegroup:(EAGLContext*)passedContext;
- (void)startAnimation;
- (void)stopAnimation;
- (void)drawView;
//- (void)setFramePointer;
//- (void)processPixelBuffer: (CVImageBufferRef)pixelBuffer;
//- (void)newFrame:(GLuint)frame;
- (void)newCameraTextureForDisplay:(GLuint)frame;

- (void)writeMovie:(NSString*)filename ofSize:(CGSize)size withCrop:(CGRect)crop fromTexture:(GLuint)textureID;
- (void)writeMovieFromTexture:(GLuint)textureID ofSize:(CGSize)size withCrop:(CGRect)crop;
- (void)finishMovie;

-(void) saveScreenToFile:(const char*)fname;
-(void) startMovieWriter:(const char*)fname;
-(void) writeScreenshotToMovie:(float)duration;
-(void) closeMovieWriter;

- (void) advertiseService:(NSString *)name withID:(NSString *)nsid atPort:(int)port;
- (BOOL)searchForServicesOfType:(NSString *)type inDomain:(NSString *)domain;
- (void)stopCurrentResolve;

#ifdef GPUIMAGE
- (void)setCameraFilterParameter:(double)value;
- (void)setCameraFilter:(GPUImageFilterType)filterType;
- (void)setFilterParameter:(double)value forFilter:(GPUImageOutput<GPUImageInput> *)inputFilter withType:(GPUImageFilterType)currentfiltertype;
- (GPUImageOutput<GPUImageInput> *)createFilter:(GPUImageFilterType)filterType;
#endif

#ifdef SANDWICH_SUPPORT
// sandwich update Delegate functions
- (void) rearTouchUpdate: (SandwichEventManager * ) sender;
- (void) pressureUpdate: (SandwichEventManager * ) sender;
#endif

- (void) setupNetConnects;

void Net_Advertise(const char* nsid, int port);
void Net_Find(const char* nsid);
void Stop_Net_Advertise(const char* nsid);
void Stop_Net_Find(const char* nsid);

void incCameraUse();
void decCameraUse();
void decCameraUseBy(int dec);

void urLoadIdentity();
void urPopMatrix();
void urTranslatef(GLfloat x, GLfloat y, GLfloat z);
void urPushMatrix();

const char* accessiblePathSystemFirst(const char* fn);

@end

