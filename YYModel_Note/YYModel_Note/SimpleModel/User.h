//
//  User.h
//  YYModel_Note
//
//  Created by LeeWong on 2018/4/10.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface User : NSObject
@property (nonatomic, assign) UInt64 uid;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, strong) NSDate *created;
@end

@interface LeeUser : NSObject
@property (nonatomic, assign) UInt64 uid;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, strong) NSDate *created;
@end
