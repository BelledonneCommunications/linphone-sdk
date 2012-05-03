//
//  mediastreamViewController.h
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

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
@end
