//
//  DemoObject.m
//  ProtocolKitDemo
//
//  Created by LeeWong on 2018/3/29.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "DemoObject.h"

@implementation DemoObject

- (NSString *)test_DemoDelegate
{
    [self.delegate demo_OptionalMethod];
   return  [self.delegate demo_RequiredMethod];
}

@end
