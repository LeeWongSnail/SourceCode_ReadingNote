//
//  PrimaryObject.m
//  App
//
//  Created by LeeWong on 2020/8/22.
//

#import "PrimaryObject.h"

@implementation PrimaryObject

+ (void)load {
    NSLog(@"PrimaryObject---load");
}

- (void)primaryObjectMethod {
    NSLog(@"PrimaryObject---primaryObjectMethod");
}


@end



@implementation PrimaryObject (Demo)

+ (void)load {
    NSLog(@"PrimaryObject--Category---load");
}

+ (void)classMethod {
    NSLog(@"PrimaryObject--Category--classMethod");
}
- (void)test {
    NSLog(@"PrimaryObject--Category---test");
}

@end
