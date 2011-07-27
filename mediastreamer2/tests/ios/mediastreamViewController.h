//
//  mediastreamViewController.h
//  mediastream
//
//  Created by jehan on 15/06/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface mediastreamViewController : UIViewController {
     UIView* imageView;
	 UIView* preview;
}
@property (nonatomic, retain) IBOutlet UIView* imageView;
@property (nonatomic, retain) IBOutlet UIView* preview;	
@end
