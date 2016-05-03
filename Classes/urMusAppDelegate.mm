//
//  urMusAppDelegate.m
//  urMus
//
//  Created by Georg Essl on 6/20/09.
//  Copyright Georg Essl 2009. All rights reserved. See LICENSE.txt for license conditions.
//

#import "urMusAppDelegate.h"
#import "RootViewController.h"
#import "EAGLView.h"
#include "urAPI.h"
#include "RIOAudioUnitLayer.h"
#include "lfs.h"
#include "httpServer.h"
#import <QuartzCore/QuartzCore.h>

#ifdef SANDWICH_SUPPORT
#import "SandwichUpdateListener.h"
#endif

#define NOXIB

@implementation urMusAppDelegate

@synthesize window;
@synthesize glView;
@synthesize repeatingTimer;
//@synthesize window2;
@synthesize glView2;
@synthesize externalWindow;
@synthesize extScreen;

// Make EAGLview global so lua interface can grab it without breaking a leg over IMP
EAGLView* g_glView;

#define EARLY_LAUNCH
extern std::string errorstr;
extern bool newerror;

extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

#define GSEVENT_TYPE 2
#define GSEVENT_FLAGS 12
#define GSEVENTKEY_KEYCODE 15
#define GSEVENT_TYPE_KEYUP 11

NSString *const GSEventKeyUpNotification = @"GSEventKeyUpHackNotification";

#ifdef HANDLEEXTERNALDISPLAYS
- (void)screenDidChange:(NSNotification *)notification
{
    
	NSArray			*screens;
	
	// Log the current screens and display modes
	screens = [UIScreen screens];
	
 //   NSLog(@"1");
	NSUInteger screenCount = [screens count];
	
	if (screenCount > 1) {
        
		// Select first external screen
		self.extScreen = [screens objectAtIndex:1];
        
        if (externalWindow == nil || !CGRectEqualToRect(externalWindow.bounds, [extScreen bounds])) {
            
            externalWindow = [[UIWindow alloc] initWithFrame:[extScreen bounds]];
            
            externalWindow.screen = extScreen;
            
            // Control the size of the display here
            //glView2 = [[EAGLView alloc] initWithFrame:[externalWindow frame] andContextSharegroup:glView.context];
            
            float extScreenWidth = externalWindow.frame.size.width;
            float extScreenHeight = externalWindow.frame.size.height;
            float deviceScreenRatio = [[screens objectAtIndex:0] bounds].size.width/[[screens objectAtIndex:0] bounds].size.height;
            CGRect finalExtFrame;
            
            if (extScreenHeight < extScreenWidth) {
                // Height is limiting factor
                
                finalExtFrame = CGRectMake(0, 0, extScreenHeight*deviceScreenRatio, extScreenHeight);
            } else {
                
                finalExtFrame = CGRectMake(0, 0, extScreenWidth, extScreenWidth/deviceScreenRatio);
            }
            
//            finalExtFrame = CGRectMake(0,0, [[screens objectAtIndex:0] bounds].size.width, [[screens objectAtIndex:0] bounds].size.height);
            
            glView2 = [[EAGLView alloc] initWithFrame:finalExtFrame andContextSharegroup:glView.context];
            [externalWindow addSubview:glView2];
            
            
#ifdef CAPTUREMANAGER
            glView.captureManager.delegateTwo = glView2;
            [glView.captureManager informViewsOfCameraTexture];
#endif
            
            [glView2 startAnimation];
            [glView2 drawView];
            
            [externalWindow makeKeyAndVisible];
            
        }
        
    } else {
        
		// Release external screen and window
		self.extScreen = nil;
		
		self.externalWindow = nil;
        
#ifdef CAPTUREMANAGER
        glView.captureManager.delegateTwo = nil;
#endif
        
    }
}
#endif

extern std::string g_fontPath;
extern std::string g_storagePath;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [self applicationDidFinishLaunching:application];
    return YES;
}

#define HANDLEEXTERNALKEYBOARDS
#ifdef HANDLEEXTERNALKEYBOARDS
-(void) keyPressed: (NSNotification*) notification
{
    NSLog([[notification object]text]);
}
#endif

-(void)applicationDidFinishLaunching:(UIApplication *)application {

    [[UIApplication sharedApplication] setStatusBarOrientation:UIDeviceOrientationPortrait animated:NO];
//    [[UIApplication sharedApplication] setStatusBarOrientation:UIDeviceOrientationLandscapeLeft animated:NO];
//- (void)applicationDidFinishLaunching:(UIApplication *)application {
    
#ifdef NOXIB
    // Override point for customization after application launch.
    
//    [window release];
//	[glView release];

    window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    window.backgroundColor = [UIColor whiteColor];
//    [self.window makeKeyAndVisible];
    
    glView = [EAGLView alloc];
    [glView awakeFromNib]; // Fake nib initialization
	[glView initWithFrame:[window bounds]];//]
										   //pixelFormat:kEAGLColorFormatRGBA8
										   //depthFormat:GL_DEPTH_COMPONENT24_OES
									//preserveBackbuffer:NO];

    
    // make the OpenGLView a child of the main window
    // Use RootViewController manage EAGLView
    viewController = [[RootViewController alloc] initWithNibName:nil bundle:nil];
    viewController.wantsFullScreenLayout = YES;
    viewController.view = glView;

    
    g_glView = glView;
    /* Declare a Lua State, open the Lua State and load the libraries (see above). */
    lua = lua_open();
    luaL_openlibs(lua);
    luaopen_lfs (lua); // Added external luafilesystem, runs under lua's open license
    l_setupAPI(lua);
    
    
    // Set RootViewController to window
    NSString *reqSysVer = @"6.0";
    NSString *currSysVer = [[UIDevice currentDevice] systemVersion];
    if ([currSysVer compare:reqSysVer options:NSNumericSearch] != NSOrderedAscending) {
        [window setRootViewController:viewController]; //iOS 6
    } else {
        [window addSubview: viewController.view]; //iOS 5 or less
        //        [window addSubview:glView];
    }
	// make main window visible
	[window makeKeyAndVisible];
#endif
    
    



#ifdef SANDWICH_SUPPORT
	// init SandwichUpdateListener
//	NSLog(@"Delegate: Starting Listener...");
	[SandwichUpdateListener initializeWithServerPort:4555 andDelegate:glView];
	[SandwichUpdateListener addDelegate: self];
	
	if([SandwichUpdateListener startListening])
	{
//		NSLog(@"Success!");  
	}
	else
	{
//		NSLog(@"Fail :(");  
	};
#endif

#ifndef UISTRINGS
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	// path to the default font
    g_storagePath = [resourcePath UTF8String];
	g_fontPath=g_storagePath+"/arial.ttf";
#endif
    
	[glView startAnimation];
	[glView drawView];
    
#ifdef HANDLEEXTERNALDISPLAYS
    // Catches a launch with two screens and sets reasonable default values otherwise
    [self screenDidChange:nil];
#endif
    
#ifdef EARLY_LAUNCH
#ifdef UISTRINGS
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
#endif
#ifndef SLEEPER
//    NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urFinalDancer-modulation.lua"];
	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urMus.lua"];
//    NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urConcertLaunch15.lua"];
//    NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urFinalDancer-slave.lua"];
//    NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urNetTest.lua"];
//	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urBlank.lua"];
//	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urVen2.lua"];
#else
	NSString *filePath = [resourcePath stringByAppendingPathComponent:@"urSleeperLaunch.lua"];
#endif
    
	NSArray *paths;
	paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath;
	if ([paths count] > 0)
		documentPath = [paths objectAtIndex:0];
    else {
        assert(false);
    }
		
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
		errorstr = error; // DPrinting errors for now
		newerror = true;
	}
#endif	

}

#ifdef SANDWICH_SUPPORT
- (void) rearTouchUpdate: (SandwichEventManager * ) sender;
{
	
}
- (void) pressureUpdate: (SandwichEventManager * ) sender;
{
	NSLog(@"Appdelegate - PressureUpate");
	int pressure[4];
	
	pressure[0] = sender.pressureValues[0];
	pressure[1] = sender.pressureValues[1];
	pressure[2] = sender.pressureValues[2];
	pressure[3] = sender.pressureValues[3];
}
#endif

extern MoNet myoscnet;

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [glView stopAnimation];
    [glView stopAudio];
    [glView stopOSCListener];
}
- (void)applicationWillEnterForeground:(UIApplication *)application
{
    [glView startAnimation];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
}

- (void)dealloc {
	/* Remember to destroy the Lua State */
	http_stop();
    if(lua != NULL) 
        lua_close(lua);
	
	[window release];
	[glView release];
	[super dealloc];

}


@end
