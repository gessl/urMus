//
//  main.m
//  urMus
//
//  Created by Georg Essl on 6/20/09.
//  Copyright Georg Essl 2009. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "urMusAppDelegate.h"

int main(int argc, char *argv[]) {
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
//    int retVal = UIApplicationMain(argc, argv, nil, nil);
//    int retVal = UIApplicationMain(argc, argv, @"UIApplication", NSStringFromClass([urMusAppDelegate class]));
    int retVal = UIApplicationMain(argc, argv, @"UIApplication", @"urMusAppDelegate");
    [pool release];
    return retVal;
}
