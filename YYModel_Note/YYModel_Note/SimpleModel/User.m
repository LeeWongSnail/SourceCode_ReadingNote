//
//  User.m
//  YYModel_Note
//
//  Created by LeeWong on 2018/4/10.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "User.h"
#import "YYModel.h"

@implementation User

//modelCustomClassForDictionary

+ (nullable Class)modelCustomClassForDictionary:(NSDictionary *)dictionary
{
    return [LeeUser class];
}


- (void)setName:(NSString *)name
{
    _name = name;
    NSLog(@"%s",__func__);
}

- (void)setUid:(UInt64)uid
{
    _uid = uid;
    NSLog(@"%s",__func__);

}

- (void)setCreated:(NSDate *)created
{
    _created = created;
    NSLog(@"%s",__func__);

}

@end


@implementation LeeUser
- (void)setName:(NSString *)name
{
    _name = name;
    NSLog(@"%s",__func__);
}

- (void)setUid:(UInt64)uid
{
    _uid = uid;
    NSLog(@"%s",__func__);
    
}

- (void)setCreated:(NSDate *)created
{
    _created = created;
    NSLog(@"%s",__func__);
    
}
@end
