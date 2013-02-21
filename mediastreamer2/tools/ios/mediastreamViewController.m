//
//  mediastreamViewController.m
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "mediastreamViewController.h"
#include "mediastream.h"

VideoStream* videoStream;
mediastreamViewController* instance;

@implementation mediastreamViewController
@synthesize portraitImageView;
@synthesize portraitPreview;
@synthesize landscapeImageView;
@synthesize landscapePreview;
@synthesize landscape;
@synthesize portrait;




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
	instance=self;
	[landscape removeFromSuperview];
	[portrait removeFromSuperview];
}

-(void) configureOrientation:(UIInterfaceOrientation) oritentation  {
	if (oritentation == UIInterfaceOrientationPortrait ) {
		[self.view addSubview:portrait];
		video_stream_set_native_window_id(videoStream,(unsigned long)portraitImageView);	
		video_stream_set_native_preview_window_id(videoStream,(unsigned long)portraitPreview);
		video_stream_set_device_rotation(videoStream, 0);
		
	} else if (oritentation == UIInterfaceOrientationLandscapeRight ) {
		[self.view addSubview:landscape];
		video_stream_set_native_window_id(videoStream,(unsigned long)landscapeImageView);	
		video_stream_set_native_preview_window_id(videoStream,(unsigned long)landscapePreview);
		video_stream_set_device_rotation(videoStream, 270);
	} else if (oritentation == UIInterfaceOrientationLandscapeLeft ) {
		[self.view addSubview:landscape];
		video_stream_set_native_window_id(videoStream,(unsigned long)landscapeImageView);	
		video_stream_set_native_preview_window_id(videoStream,(unsigned long)landscapePreview);
		video_stream_set_device_rotation(videoStream, 90);	
	}
}
- (void)viewDidUnload
{
    [super viewDidUnload];
	
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

-(void) viewWillAppear:(BOOL)animated {
	[super viewWillAppear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return		interfaceOrientation == UIInterfaceOrientationPortrait 
			||	interfaceOrientation == UIInterfaceOrientationLandscapeRight
			||	interfaceOrientation == UIInterfaceOrientationLandscapeLeft;
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation {
	if (videoStream == nil) {
		ms_warning("no video stream yet");
		return;
	}
	[self configureOrientation:self.interfaceOrientation];
	if (fromInterfaceOrientation !=self.interfaceOrientation) {
		video_stream_update_video_params(videoStream);
	} 
}
- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration {
	[landscape removeFromSuperview];
	[portrait removeFromSuperview];
}

-(void) configureOrientation {
	[self configureOrientation:self.interfaceOrientation]; 
}

-(void) setVideoStream:(VideoStream*) stream {
	videoStream = stream;
	[self performSelectorOnMainThread:@selector(configureOrientation)
						   withObject:nil 
						waitUntilDone:YES];
	video_stream_update_video_params(videoStream);
}
void ms_set_video_stream(VideoStream* video) {
	[instance setVideoStream:video];
}

@end
