//
//  PrimaryObject.h
//  App
//
//  Created by LeeWong on 2020/8/22.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface PrimaryObject : NSObject

- (void)primaryObjectMethod;

@end


@interface PrimaryObject (Demo)

@property (nonatomic, strong) NSArray *demoCategoryArray;

- (void)test;

+ (void)classMethod;

@end

NS_ASSUME_NONNULL_END
