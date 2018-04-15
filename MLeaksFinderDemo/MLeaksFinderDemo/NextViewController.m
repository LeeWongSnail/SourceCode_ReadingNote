//
//  NextViewController.m
//  MLeaksFinderDemo
//
//  Created by LeeWong on 2018/3/31.
//  Copyright © 2018年 LeeWong. All rights reserved.
//

#import "NextViewController.h"

@interface NextViewController ()
@property (nonatomic, strong) NSTimer *timer;
@end

@implementation NextViewController

- (void)test
{
    __weak id weakSelf = self;
    //2s中之后调用assertNotDealloc
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        __strong id strongSelf = weakSelf;
        [strongSelf popTest];
    });
}

- (IBAction)popDidClick:(UIButton *)sender {
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    [self test];
    
}

- (void)popTest
{
    NSLog(@"popTest coming");
}

- (void)dealloc
{
    NSLog(@"%s",__func__);
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
