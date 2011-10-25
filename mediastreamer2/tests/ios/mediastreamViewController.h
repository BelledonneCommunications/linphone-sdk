//
//  mediastreamViewController.h
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
<<<<<<< HEAD
@class VideoStream;
@interface mediastreamViewController : UIViewController {
    UIView* portraitImageView;
	UIView* portraitPreview;
	UIView* landscapeImageView;
	UIView* landscapePreview;
	UIView* portrait;
	UIView* landscape;
	}


@property (nonatomic, retain) IBOutlet UIView* portraitImageView;
@property (nonatomic, retain) IBOutlet UIView* portraitPreview;	
@property (nonatomic, retain) IBOutlet UIView* landscapeImageView;
@property (nonatomic, retain) IBOutlet UIView* landscapePreview;	
@property (nonatomic, retain) IBOutlet UIView* portrait;
@property (nonatomic, retain) IBOutlet UIView* landscape;	
=======

@interface mediastreamViewController : UIViewController {
     UIView* imageView;
	 UIView* preview;
}
@property (nonatomic, retain) IBOutlet UIView* imageView;
@property (nonatomic, retain) IBOutlet UIView* preview;	
>>>>>>> public/master
@end
