//
//  main.m
//  objc-test
//
//  Created by GongCF on 2018/12/16.
//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <dlfcn.h>
#import <mach-o/ldsyms.h>
#import <objc/>
#import "Person.h"
//#include "ffi.h"


void checkClassKindAndMember(void);
static void register_Block(SEL _sel);


int main(int argc, const char * argv[]) {
    @autoreleasepool {
//        register_Block(@selector(say));
//        register_Block(@selector(hello));
//        Person *p =  [[Person new] init];
//        Class aclass = Person.class;
//        SEL didload = @selector(say);
//        IMP imp= class_getMethodImplementation(aclass, didload);
//        NSLog(@"IMP:%p",imp);
//
//        [p say];
//        [p hello];
        
//        hookC();
//        Person *p = [[Person alloc]init];
//        [Person test];

//        Class cls = NSObject.class;
//        Class pcls = Person.class;
//        //isKindOfClass cls->isa 和cls/cls->superclass相等吗
//        //元类对象和类对象不相等，但是最后一个元类的isa->superclass是指向
//        BOOL res1 =[cls isKindOfClass:cls];
//        //cls->isa 和cls相等吗？ 不相等 cls->isa是元类对象,cls是类对象，不可能相等。
//        BOOL res2 =[cls isMemberOfClass:cls];
//        
//        BOOL res3 =[pcls isKindOfClass:pcls];
//        BOOL res4 =[pcls isMemberOfClass:pcls];
//        NSLog(@"%d %d %d %d",res1,res2,res3,res4);
    }
    return 0;
}

static void register_Block(SEL _sel){
    SEL didload = _sel;
    Class aclass = Person.class;
    Method md = class_getInstanceMethod(aclass, didload);
    
    if (md) {
        //获取didload函数的的IMP
        IMP load = method_getImplementation(md);
//        NSLog(@"load:%p",load);
        void(*loadFunc)(id,SEL) = (void *)load;
        __block typeof(Class) __blockClass = aclass;
        __block typeof(loadFunc)__blockFunc = loadFunc;
        __block typeof(didload) __sel = didload;
        void (^block)(id _self) = ^(id _self){
            //统计时间
            CFAbsoluteTime time1 = CFAbsoluteTimeGetCurrent();
            //执行ViewDidLoad IMP
            __blockFunc(__blockClass,__sel);
            NSLog(@"1:%s cost: %.2fs",class_getName(__blockClass),CFAbsoluteTimeGetCurrent() - time1);
        };
        //将 block block 转化成 IMP 存储到SEL ViewDidload 中
        
        void(*func)(id,SEL) =(void*)imp_implementationWithBlock(block);
        class_replaceMethod(aclass, didload, (IMP)func, method_getTypeEncoding(md));
        
        static int count = 0;

        void (^block2)(id _self) = ^(id _self){
            count ++;
            
            //统计时间
            CFAbsoluteTime time1 = CFAbsoluteTimeGetCurrent();
            //执行ViewDidLoad IMP
            __blockFunc(__blockClass,__sel);
            NSLog(@"2:%s cost: %.2fs",class_getName(__blockClass),CFAbsoluteTimeGetCurrent() - time1);
        };
//        IMP new1 = class_getMethodImplementation(aclass, didload);
//        NSLog(@"new1IMP:%p",new1);
        IMP blockIMP = imp_implementationWithBlock(block2);
        void(*func2)(id,SEL) =(void*)blockIMP;
//        sel_registerName(<#const char * _Nonnull str#>)
        class_replaceMethod(aclass, didload, (IMP)func2, method_getTypeEncoding(md));

        IMP new2 = class_getMethodImplementation(aclass, didload);
        NSLog(@"new2IMP:%p func:%p block2:%p blockIMP:%ld",new2,func2,block2,sizeof(blockIMP));
    }
}
