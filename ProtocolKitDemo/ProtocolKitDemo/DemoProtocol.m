//
//  DemoProtocol.m
//  ProtocolKitDemo
//
//  Created by LeeWong on 2018/3/29.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "DemoProtocol.h"
#import "PKProtocolExtension.h"
@defs(DemoProtocol)

- (NSString *)demo_RequiredMethod
{
    return @"this is default method";
}

- (void)demo_OptionalMethod
{
    NSLog(@"demo_optionMethod");
}

@end
