//
//  ViewController.m
//  ProtocolKitDemo
//
//  Created by LeeWong on 2018/3/29.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "ViewController.h"
#import "DemoObject.h"
#import <objc/runtime.h>
@interface ViewController ()<DemoProtocol>

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    DemoObject *obj = [[DemoObject alloc] init];
    obj.delegate = self;
    
//    NSLog(@"%@",[obj test_DemoDelegate]) ;
    
    unsigned int outCount;
    Class *classes = objc_copyClassList(&outCount);
    for (int i = 0; i < outCount; i++) {
        NSLog(@"%s", class_getName(classes[i]));
    }
    free(classes);
    
    NSLog(@"%d",outCount);
    
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
