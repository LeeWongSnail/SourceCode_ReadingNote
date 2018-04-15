//
//  DemoObject.h
//  ProtocolKitDemo
//
//  Created by LeeWong on 2018/3/29.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "DemoProtocol.h"

@interface DemoObject : NSObject 
@property (nonatomic, weak) id <DemoProtocol> delegate;


- (NSString *)test_DemoDelegate;
@end
