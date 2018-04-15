//
//  ViewController.m
//  YYModel_Note
//
//  Created by LeeWong on 2018/4/10.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "ViewController.h"
#import "YYModel.h"
#import "User.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)userTest
{
    NSString *path = [[NSBundle mainBundle] pathForResource:@"user.json" ofType:nil];
    NSError *aError = nil;
    id json = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:NSJSONReadingMutableContainers error:&aError];
    
    User *user = [User yy_modelWithJSON:json];
    NSLog(@"%@",user);
}






- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    [self userTest];
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
