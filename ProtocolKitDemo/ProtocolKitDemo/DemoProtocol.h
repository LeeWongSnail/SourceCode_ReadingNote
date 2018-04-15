//
//  DemoProtocol.h
//  ProtocolKitDemo
//
//  Created by LeeWong on 2018/3/29.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol DemoProtocol <NSObject>

@optional

- (void)demo_OptionalMethod;

@required

- (NSString *)demo_RequiredMethod;
@end
