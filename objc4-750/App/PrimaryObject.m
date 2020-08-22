//
//  PrimaryObject.m
//  App
//
//  Created by LeeWong on 2020/8/22.
//

#import "PrimaryObject.h"

@implementation PrimaryObject

- (void)test {
    NSLog(@"PrimaryObject---test");
}

+ (void)classMethod {
    
}

@end



@implementation PrimaryObject (Demo)

- (void)test {
    NSLog(@"PrimaryObject-Demo---test");
}

@end
