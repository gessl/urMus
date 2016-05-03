//
//  urMusAppDelegate.h
//  urMus
//
//  Created by Georg Essl on 6/20/09.
//  Copyright Georg Essl 2009. All rights reserved.
//

#import <UIKit/UIKit.h>
#include "config.h"
#include "urAPI.h"
//#import "ExternalDisplayViewController.h"
//#import "BLVideoOut.h"

#ifdef SANDWICH_SUPPORT
#import "SandwichTypes.h"
#endif

@class EAGLView;
//@class ExternalDisplayViewController;

//#ifndef SANDWICH_SUPPORT
@interface urMusAppDelegate : NSObject <UIApplicationDelegate> {
//@interface urMusAppDelegate : NSObject <UIApplicationDelegate,BLVideoOutDelegate> {
//@interface urMusAppDelegate : NSObject <UIApplicationDelegate,UIAlertViewDelegate> {
//#else
//@interface urMusAppDelegate : NSObject <UIApplicationDelegate,SandwichUpdateDelegate> {
//#endif
    UIWindow *window;
    EAGLView *glView;
	EAGLView *glView2;

	UIWindow *externalWindow;
//	UIWindow *window2;
	NSArray *screenModes;
    UIScreen *extScreen;
    UIViewController *viewController;

//	ExternalDisplayViewController *externalVC;
//	NSTimer *repeatingTimer;
}

//@property (nonatomic, retain) IBOutlet UIWindow *deviceWindow;
@property (nonatomic, retain) IBOutlet UIWindow *externalWindow;
@property (nonatomic, retain) EAGLView *glView2;
//@property (nonatomic, retain) UIWindow *window2;
@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet EAGLView *glView;
@property (nonatomic, retain) UIScreen *extScreen;
@property (nonatomic, retain) NSTimer *repeatingTimer;


	
#ifdef SANDWICH_SUPPORT
- (void) rearTouchUpdate: (SandwichEventManager * ) sender;
- (void) pressureUpdate: (SandwichEventManager * ) sender;
#endif
	
#import <UIKit/UIKit.h>
	
	
@end

