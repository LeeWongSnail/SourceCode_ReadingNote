//
//  PrimaryObject.h
//  App
//
//  Created by LeeWong on 2020/8/22.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface PrimaryObject : NSObject

- (void)test;

+ (void)classMethod;

@end


@interface PrimaryObject (Demo)
- (void)test;

+ (void)classMethod;

@end

NS_ASSUME_NONNULL_END
