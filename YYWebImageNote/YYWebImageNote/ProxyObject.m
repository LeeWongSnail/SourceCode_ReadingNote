//
//  ProxyObject.m
//  YYWebImageNote
//
//  Created by LeeWong on 2018/6/2.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "ProxyObject.h"

@interface ProxyObject ()
@property (nonatomic, strong) id object;
@end

@implementation ProxyObject


//- (instancetype)init{
//    
//    return self;
//}

- (instancetype)initWithObject:(id)object
{
    self.object = object;
    return self;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    return [self.object methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation {
    [invocation invokeWithTarget:self.object];
}

@end
