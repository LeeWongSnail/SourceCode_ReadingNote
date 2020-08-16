//
//  ViewController.m
//  YYWebImageNote
//
//  Created by LeeWong on 2018/6/2.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "ViewController.h"
#import "ProxyObject.h"
#import "NormalObject.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    NSString *str = [NSString stringWithFormat:@"this is a word"];
    ProxyObject *proxyA = [[ProxyObject alloc] initWithObject:str];
    NormalObject *proxyB = [[NormalObject alloc] initWithObject:str];
    
    
//    NSLog(@"%d", [proxyA respondsToSelector:@selector(length)]);
//    NSLog(@"%d", [proxyB respondsToSelector:@selector(length)]);
//
//    NSLog(@"%d", [proxyA isKindOfClass:[NSString class]]);
//    NSLog(@"%d", [proxyB isKindOfClass:[NSString class]]);
    
    [proxyA isEqual:str];
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
