/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>


extern int apple_main(int argc, char **argv);

@interface MyApplicationDelegate: NSObject
{
    NSWindow *window;
@public
	int argc;
    char **argv;
    id activity;
}


-(void)applicationWillFinishLaunching: (NSNotification*) aNotification;
-(void)applicationDidFinishLaunching: (NSNotification*) aNotification;
-(BOOL)applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)theApplication;
@end


@implementation MyApplicationDelegate



-(void) runLoop {
	exit(apple_main(argc,argv));
}

-(void)applicationWillFinishLaunching: (NSNotification*) aNotification
{
    if( [[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)] ){
        //NSActivityOptions options = NSActivityAutomaticTerminationDisabled & NSActivityIdleSystemSleepDisabled;
        // NSLog(@"Disabling App nap for tester");
        self->activity = [[[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:@"No app nap for ms2 tester"] retain];
    }
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0), ^{
        [self runLoop];
    });
}

-(void)applicationDidFinishLaunching: (NSNotification*) aNotification
{
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
	NSLog(@"applicationWillTerminate");
	if( self->activity ){
		[[NSProcessInfo processInfo] endActivity:self->activity];
		[self->activity release];
		self->activity = nil;
	}
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)theApplication
{
    return NO;
}

-(void)dealloc
{
    [window release];
    [super dealloc];
}
@end

int main(int argc, char **argv)
{
    static const ProcessSerialNumber thePSN = { 0, kCurrentProcess };
    TransformProcessType(&thePSN, kProcessTransformToForegroundApplication);
    SetFrontProcess(&thePSN);
    NSAutoreleasePool *aPool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    MyApplicationDelegate *aMyApplicationDelegate = [[MyApplicationDelegate alloc] init];
    aMyApplicationDelegate->argc = argc;
    aMyApplicationDelegate->argv = argv;
    [NSApp setDelegate: aMyApplicationDelegate];
    [aPool release];
    [NSApp run];
    return 0;
}
