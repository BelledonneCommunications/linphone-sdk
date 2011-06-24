//
//  mediastreamViewController.m
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "mediastreamViewController.h"
#include "mediastream.h"
static VideoStream* sVideoStream=0;

void ms_set_video_stream(VideoStream* video) {
	sVideoStream = video;
}
@implementation mediastreamViewController
@synthesize imageView;

- (void)dealloc
{
    [super dealloc];
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle


// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    [super viewDidLoad];
	ms_filter_call_method(sVideoStream->output,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,imageView);
}


- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

@end
