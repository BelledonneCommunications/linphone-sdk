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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>


extern int _main(int argc, char **argv);

@interface MyApplicationDelegate: NSObject
{
    NSWindow *window;
@public
	int argc;
    char **argv;
}
-(void)applicationWillFinishLaunching: (NSNotification*) aNotification;
-(void)applicationDidFinishLaunching: (NSNotification*) aNotification;
-(BOOL)applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)theApplication;
@end

@implementation MyApplicationDelegate

-(void) runLoop {
	exit(_main(argc,argv));
}

-(void)applicationWillFinishLaunching: (NSNotification*) aNotification
{
    [self performSelectorInBackground:@selector(runLoop) withObject:nil];
}

-(void)applicationDidFinishLaunching: (NSNotification*) aNotification
{
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)theApplication
{
    return YES;
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
