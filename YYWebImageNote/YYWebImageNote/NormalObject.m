//
//  NormalObject.m
//  YYWebImageNote
//
//  Created by LeeWong on 2018/6/2.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "NormalObject.h"

@interface NormalObject()
@property (nonatomic, strong) id object;

@end

@implementation NormalObject

- (instancetype)initWithObject:(id)object
{
    if ([super init]) {
        _object = object;
    }
    return self;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    return [self.object methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation {
    [invocation invokeWithTarget:self.object];
}

@end
