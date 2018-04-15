// The MIT License (MIT)
//
// Copyright (c) 2015-2016 forkingdog ( https://github.com/forkingdog )
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#import <Foundation/Foundation.h>
#import "PKProtocolExtension.h"
#import <pthread.h>

typedef struct {
    Protocol *__unsafe_unretained protocol;
    Method *instanceMethods;    //实例方法
    unsigned instanceMethodCount;   //实例方法的个数
    Method *classMethods;   //类方法
    unsigned classMethodCount; //类方法的个数
} PKExtendedProtocol;

//结构体的一个实例 用来管理所有的协议方法
static PKExtendedProtocol *allExtendedProtocols = NULL;
//全局的一个多线程互斥锁
static pthread_mutex_t protocolsLoadingLock = PTHREAD_MUTEX_INITIALIZER;
//拓展协议的个数 以及拓展协议的容量
static size_t extendedProtcolCount = 0, extendedProtcolCapacity = 0;


/**
 合并当前现存的和要添加的方法

 @param existMethods 现存的方法
 @param existMethodCount 现存的方法的个数
 @param appendingMethods 需要增加的方法
 @param appendingMethodCount 需要增加的方法的个数
 @return 合并后的方法集合
 
 /// An opaque type that represents a method in a class definition.
 typedef struct objc_method *Method;
 
 struct objc_method {
 SEL method_name;        // 方法名称
 char *method_typesE;    // 参数和返回类型的描述字串
 IMP method_imp;         // 方法的具体的实现的指针
 }
 */
Method *_pk_extension_create_merged(Method *existMethods, unsigned existMethodCount, Method *appendingMethods, unsigned appendingMethodCount) {
    //如果当前现存的方法数为0个直接返回需要添加的方法 不需要拼接
    if (existMethodCount == 0) {
        return appendingMethods;
    }
    //先求出合并后所有的方法数
    unsigned mergedMethodCount = existMethodCount + appendingMethodCount;
    //malloc() 函数用来动态地分配内存空间
    //void* malloc (size_t size);
    //size 为需要分配的内存空间的大小，以字节（Byte）计。分配成功返回指向该内存的地址，失败则返回 NULL。
    Method *mergedMethods = malloc(mergedMethodCount * sizeof(Method));
    //void *memcpy(void*dest, const void *src, size_t n);
    //由src指向地址为起始地址的连续n个字节的数据复制到以destin指向地址为起始地址的空间内。
    //函数返回一个指向dest的指针。
    //将这两个要拼接的方法拼接到同一块内存区域
    memcpy(mergedMethods, existMethods, existMethodCount * sizeof(Method));
    memcpy(mergedMethods + existMethodCount, appendingMethods, appendingMethodCount * sizeof(Method));
    
    return mergedMethods;
}

/**
 将containerClass中的类方法和实例方法合并到extendedProtocol中

 @param extendedProtocol 拓展协议
 @param containerClass 要被合并的方法所属的类
 */
void _pk_extension_merge(PKExtendedProtocol *extendedProtocol, Class containerClass) {
    
    // 实例方法
    unsigned appendingInstanceMethodCount = 0;
    //运行时获取到这个类中的实例方法以及实例方法的个数
    Method *appendingInstanceMethods = class_copyMethodList(containerClass, &appendingInstanceMethodCount);
    //将这类中的实例方法与拓展协议中的实例方法合并
    Method *mergedInstanceMethods = _pk_extension_create_merged(extendedProtocol->instanceMethods,
                                                                extendedProtocol->instanceMethodCount,
                                                                appendingInstanceMethods,
                                                                appendingInstanceMethodCount);
    //释放拓展协议的实例方法
    free(extendedProtocol->instanceMethods);
    
    //更新拓展协议中的实例方法以及实例方法的个数
    extendedProtocol->instanceMethods = mergedInstanceMethods;
    extendedProtocol->instanceMethodCount += appendingInstanceMethodCount;
    
    // 类方法
    unsigned appendingClassMethodCount = 0;
    //运行时获取类中的类方法以及类方法的个数
    Method *appendingClassMethods = class_copyMethodList(object_getClass(containerClass), &appendingClassMethodCount);
    //将拓展协议中的类方法和类中的类方法合并
    Method *mergedClassMethods = _pk_extension_create_merged(extendedProtocol->classMethods,
                                                             extendedProtocol->classMethodCount,
                                                             appendingClassMethods,
                                                             appendingClassMethodCount);
    //释放拓展协议的类方法
    free(extendedProtocol->classMethods);
    
    //更新拓展协议中的类方法和类方法的数量
    extendedProtocol->classMethods = mergedClassMethods;
    extendedProtocol->classMethodCount += appendingClassMethodCount;
}


/**
 将protocol加入到allExtendedProtocols数组中(如果存在则更新)
 可能有多各类要遵守这个协议 那么同一个协议在allExtendedProtocols中只有一项 所以实现方法是合并在一起的
 @param protocol 某个协议
 @param containerClass 某各类
 */
void _pk_extension_load(Protocol *protocol, Class containerClass) {
    
    //多线程互斥锁 锁住下面的操作
    pthread_mutex_lock(&protocolsLoadingLock);
    
    //如果拓展协议的个数大于拓展协议内存的容量 那么增加拓展协议内存的容量
    if (extendedProtcolCount >= extendedProtcolCapacity) {
        size_t newCapacity = 0; //需要新增的空间
        
        //如果拓展协议的控件是0
        if (extendedProtcolCapacity == 0) {
            //新增一个
            newCapacity = 1;
        } else {
            //新增为之前的两倍(也就是说只是增加了一倍的容量)
            newCapacity = extendedProtcolCapacity << 1;
        }
        //void* realloc (void* ptr, size_t size);
        //ptr 为需要重新分配的内存空间指针，size 为新的内存空间的大小。
        //返回值:分配成功返回新的内存地址，可能与 ptr 相同，也可能不同；失败则返回 NULL。
        allExtendedProtocols = realloc(allExtendedProtocols, sizeof(*allExtendedProtocols) * newCapacity);
        //更新拓展协议的内存容量
        extendedProtcolCapacity = newCapacity;
    }
    
    //在32位架构中被普遍定义为：typedef   unsigned int size_t;
    //而在64位架构中被定义为：typedef  unsigned long size_t;
    size_t resultIndex = SIZE_T_MAX;
    //在allExtendedProtocols找到对应的协议 resultIndex记录下标
    for (size_t index = 0; index < extendedProtcolCount; ++index) {
        if (allExtendedProtocols[index].protocol == protocol) {
            resultIndex = index;
            break;
        }
    }
    //如果没有找到 那么就新建
    if (resultIndex == SIZE_T_MAX) {
//       下标是extendedProtcolCount处插入这个协议对应的内容 初始化的时候实例方法和类方法均为null
        allExtendedProtocols[extendedProtcolCount] = (PKExtendedProtocol){
            .protocol = protocol,
            .instanceMethods = NULL,
            .instanceMethodCount = 0,
            .classMethods = NULL,
            .classMethodCount = 0,
        };
        //新建之后 将下表重新赋值
        resultIndex = extendedProtcolCount;
        //拓展协议的个数加一
        extendedProtcolCount++;
    }
    
    //将这个类的方法拷贝到allExtendedProtocols中protocol协议对应项中
    //相当于为新建的项设置类方法和实例方法
    _pk_extension_merge(&(allExtendedProtocols[resultIndex]), containerClass);

    //解开这个多线程互斥锁
    pthread_mutex_unlock(&protocolsLoadingLock);
}

/**
 将extendedProtocol中的方法注入到targetClass中
 如果一个类遵守了一个协议但是却没有实现这个协议中的方法 那么就把这个协议的默认实现添加到这个类中class_addMethod

 @param targetClass 遵守了某个协议的一个类
 @param extendedProtocol 拓展协议
 */
static void _pk_extension_inject_class(Class targetClass, PKExtendedProtocol extendedProtocol) {
    //依次遍历extendedProtocol中的所有实例方法
    for (unsigned methodIndex = 0; methodIndex < extendedProtocol.instanceMethodCount; ++methodIndex) {
        //取出每一个实例方法
        Method method = extendedProtocol.instanceMethods[methodIndex];
        //获取方法的名字
        SEL selector = method_getName(method);
        
        //判断目标类是否实现了这个方法 如果实现了 跳过这次循环 继续下一次循环
        if (class_getInstanceMethod(targetClass, selector)) {
            continue;
        }
        
        //如果目标类中没有实现这个方法
        IMP imp = method_getImplementation(method);
        //将这个方法添加到这个类中 相当于给这个类增加了一个方法 方法的实现为extendedProtocol中的实现
        const char *types = method_getTypeEncoding(method);
        class_addMethod(targetClass, selector, imp, types);
    }
    
    //获取这个类的元类 类的类方法存在于元类中
    Class targetMetaClass = object_getClass(targetClass);
    
    //依次遍历这些类方法
    for (unsigned methodIndex = 0; methodIndex < extendedProtocol.classMethodCount; ++methodIndex) {
        //获取某各类方法
        Method method = extendedProtocol.classMethods[methodIndex];
        SEL selector = method_getName(method);
        //如果这个类方法是load或者initialize 那么跳过这次循环 继续下一次循环
        if (selector == @selector(load) || selector == @selector(initialize)) {
            continue;
        }
        
        //获取这个元类的所有对象方法,元类的对象方法就是类的实例方法
        //(这个地方有点绕 类是元类的一个实例因此可以把元类的实例方法理解为类的类方法)
        if (class_getInstanceMethod(targetMetaClass, selector)) {
            continue;
        }
        
        //如果目标类中没有实现对应的类方法 那么就为这个类添加一个方法
        IMP imp = method_getImplementation(method);
        const char *types = method_getTypeEncoding(method);
        class_addMethod(targetMetaClass, selector, imp, types);
    }
}

//__attribute__((constructor))
//参考https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Common-Function-Attributes.html#Common-Function-Attributes

/**
 ProtocolKit的入口 这个方法将在main函数执行之前执行

 constructor 固定参数 表示要在main还是之前执行 同时可以指定为destructor表示main函数执行完成或者exit()被调用的时候执行
 */
__attribute__((constructor)) static void _pk_extension_inject_entry(void) {
    //多线程互斥🔐
    pthread_mutex_lock(&protocolsLoadingLock);

    unsigned classCount = 0;
    //获取所有已注册的类
    Class *allClasses = objc_copyClassList(&classCount);
    
    //这里手动加了一个自动释放池,是为了让产生的数据能够尽快的释放
    //因为只有在最外层的for循环才有新创建对象 里层的for循环并没有新建对象 因此将释放池写在外面即可
    @autoreleasepool {
        // 遍历每一个拓展协议
        for (unsigned protocolIndex = 0; protocolIndex < extendedProtcolCount; ++protocolIndex) {
            PKExtendedProtocol extendedProtcol = allExtendedProtocols[protocolIndex];
            //遍历每一个类 观察这个类是否遵守了某一个协议
            for (unsigned classIndex = 0; classIndex < classCount; ++classIndex) {
                Class class = allClasses[classIndex];
                //判断该类是否遵守了某个拓展协议
                if (!class_conformsToProtocol(class, extendedProtcol.protocol)) {
                    continue;
                }
                //如果这个类遵守了这个协议 那么就将拓展协议中的方法注入到这个类中
                // 注入的时候回做判断 因此如果类中已经实现了协议的方法不会覆盖
                _pk_extension_inject_class(class, extendedProtcol);
            }
        }
    }
    
    //解除互斥锁
    pthread_mutex_unlock(&protocolsLoadingLock);
    
    free(allClasses);
    free(allExtendedProtocols);
    extendedProtcolCount = 0;
    extendedProtcolCapacity = 0;
}
