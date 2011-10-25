//
//  mediastreamViewController.m
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "mediastreamViewController.h"
#include "mediastream.h"
<<<<<<< HEAD
#include "iosdisplay.h"

VideoStream* videoStream;
mediastreamViewController* instance;

@implementation mediastreamViewController
@synthesize portraitImageView;
@synthesize portraitPreview;
@synthesize landscapeImageView;
@synthesize landscapePreview;
@synthesize landscape;
@synthesize portrait;



=======
#ifdef VIDEO_ENABLE
#include "iosdisplay.h"
#endif
static UIView* sImageView=0;
static UIView* spreview=0;


@implementation mediastreamViewController
@synthesize imageView;
@synthesize preview;

void ms_set_video_stream(VideoStream* video) {
#ifdef VIDEO_ENABLED
	ms_filter_call_method(video->output,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&sImageView);
	ms_filter_call_method(video->source,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&spreview);
#endif
}
>>>>>>> public/master

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
<<<<<<< HEAD
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
	
=======
	sImageView=imageView;
	spreview=preview;
}

- (void)viewDidUnload
{
    [super viewDidUnload];
>>>>>>> public/master
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

<<<<<<< HEAD
-(void) viewWillAppear:(BOOL)animated {
	[super viewWillAppear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return		interfaceOrientation == UIInterfaceOrientationPortrait 
			||	interfaceOrientation == UIInterfaceOrientationLandscapeRight
			/*||	interfaceOrientation == UIInterfaceOrientationLandscapeLeft*/;
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation {
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
=======
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
>>>>>>> public/master
}

@end
