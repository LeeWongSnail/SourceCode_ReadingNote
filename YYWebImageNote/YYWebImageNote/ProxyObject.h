//
//  ProxyObject.h
//  YYWebImageNote
//
//  Created by LeeWong on 2018/6/2.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ProxyObject : NSProxy
- (instancetype)initWithObject:(id)object;
@end
