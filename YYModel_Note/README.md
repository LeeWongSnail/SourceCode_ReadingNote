# YYModel 源码解析

## YYModel 类结构

![YYModel类](http://og0h689k8.bkt.clouddn.com/18-4-11/38434041.jpg)


## YYModel 字典转模型过程

![字典转模型过程](http://og0h689k8.bkt.clouddn.com/18-4-11/66856717.jpg)

### 将外部传入的id类型转为NSDictionary

```objc
/**
 将外部传进来的数据转换为字典

 @param json 外部传入的数据
 @return 返回给外部的字典
 */
+ (NSDictionary *)_yy_dictionaryWithJSON:(id)json {
//    kCFNull: NSNull的单例
    if (!json || json == (id)kCFNull) return nil;
    
    NSDictionary *dic = nil;
    NSData *jsonData = nil;
    //判断类型 是字典还是字符串还是NSData
    if ([json isKindOfClass:[NSDictionary class]]) {
        dic = json;
    } else if ([json isKindOfClass:[NSString class]]) {
        jsonData = [(NSString *)json dataUsingEncoding : NSUTF8StringEncoding];
    } else if ([json isKindOfClass:[NSData class]]) {
        jsonData = json;
    }
    if (jsonData) {
        dic = [NSJSONSerialization JSONObjectWithData:jsonData options:kNilOptions error:NULL];
        if (![dic isKindOfClass:[NSDictionary class]]) dic = nil;
    }
    return dic;
}

```

### _YYModelMeta

根据类来创建一个_YYModelMeta 模型中存放了 做模型转换所需要的类的所有信息

```
_YYModelMeta *modelMeta = [_YYModelMeta metaWithClass:cls];
```


### 创建模型 开始设置模型

```objc
    NSObject *one = [cls new];
    if ([one yy_modelSetWithDictionary:dictionary]) return one;
```

### ModelSetContext 准备 字典转模型

```objc
ModelSetContext context = {0};
context.modelMeta = (__bridge void *)(modelMeta);
context.model = (__bridge void *)(self);
context.dictionary = (__bridge void *)(dic);
    
//开始转模型
//模型中key的个数 大于字典中key的个数
if (modelMeta->_keyMappedCount >= CFDictionaryGetCount((CFDictionaryRef)dic)) {
        //对字典中的每个元素都执行ModelSetWithDictionaryFunction 方法
        CFDictionaryApplyFunction((CFDictionaryRef)dic, ModelSetWithDictionaryFunction, &context);
        //如果这中间存在_keyPathPropertyMetas 那么执行对应的方法
        if (modelMeta->_keyPathPropertyMetas) {
            CFArrayApplyFunction((CFArrayRef)modelMeta->_keyPathPropertyMetas,
                                 CFRangeMake(0, CFArrayGetCount((CFArrayRef)modelMeta->_keyPathPropertyMetas)),
                                 ModelSetWithPropertyMetaArrayFunction,
                                 &context);
        }
        //如果存在一对多的情况
        if (modelMeta->_multiKeysPropertyMetas) {
            CFArrayApplyFunction((CFArrayRef)modelMeta->_multiKeysPropertyMetas,
                                 CFRangeMake(0, CFArrayGetCount((CFArrayRef)modelMeta->_multiKeysPropertyMetas)),
                                 ModelSetWithPropertyMetaArrayFunction,
                                 &context);
        }
    } else {
        //直接转 内部有判断_multiKeysPropertyMetas和_keyPathPropertyMetas
        CFArrayApplyFunction((CFArrayRef)modelMeta->_allPropertyMetas,
                             CFRangeMake(0, modelMeta->_keyMappedCount),
                             ModelSetWithPropertyMetaArrayFunction,
                             &context);
    }
```

### 内部实现(ModelSetValueForProperty)

#### 如果是C中的数字类型

判断条件
```c
static force_inline BOOL YYEncodingTypeIsCNumber(YYEncodingType type) {
    switch (type & YYEncodingTypeMask) {
        case YYEncodingTypeBool:
        case YYEncodingTypeInt8:
        case YYEncodingTypeUInt8:
        case YYEncodingTypeInt16:
        case YYEncodingTypeUInt16:
        case YYEncodingTypeInt32:
        case YYEncodingTypeUInt32:
        case YYEncodingTypeInt64:
        case YYEncodingTypeUInt64:
        case YYEncodingTypeFloat:
        case YYEncodingTypeDouble:
        case YYEncodingTypeLongDouble: return YES;
        default: return NO;
    }
}
```

赋值

```c
 case YYEncodingTypeFloat: {
            float f = num.floatValue;
            if (isnan(f) || isinf(f)) f = 0;
            ((void (*)(id, SEL, float))(void *) objc_msgSend)((id)model, meta->_setter, f);
        } break;
```

#### NSType(系统提供的类型)

以NSString和NSMutableString 为例：

```c
 case YYEncodingTypeNSString:
                case YYEncodingTypeNSMutableString: {
                    if ([value isKindOfClass:[NSString class]]) {
                        if (meta->_nsType == YYEncodingTypeNSString) {
                            ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, value);
                        } else {
                            ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, ((NSString *)value).mutableCopy);
                        }
                    } else if ([value isKindOfClass:[NSNumber class]]) {
                        ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model,
                                                                       meta->_setter,
                                                                       (meta->_nsType == YYEncodingTypeNSString) ?
                                                                       ((NSNumber *)value).stringValue :
                                                                       ((NSNumber *)value).stringValue.mutableCopy);
                    } else if ([value isKindOfClass:[NSData class]]) {
                        NSMutableString *string = [[NSMutableString alloc] initWithData:value encoding:NSUTF8StringEncoding];
                        ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, string);
                    } else if ([value isKindOfClass:[NSURL class]]) {
                        ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model,
                                                                       meta->_setter,
                                                                       (meta->_nsType == YYEncodingTypeNSString) ?
                                                                       ((NSURL *)value).absoluteString :
                                                                       ((NSURL *)value).absoluteString.mutableCopy);
                    } else if ([value isKindOfClass:[NSAttributedString class]]) {
                        ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model,
                                                                       meta->_setter,
                                                                       (meta->_nsType == YYEncodingTypeNSString) ?
                                                                       ((NSAttributedString *)value).string :
                                                                       ((NSAttributedString *)value).string.mutableCopy);
                    }
                } break;
```

#### CustomType(自定义的类型)

以NSObject为例

```c
case YYEncodingTypeObject: {
                Class cls = meta->_genericCls ?: meta->_cls;
                if (isNull) {
                    ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, (id)nil);
                } else if ([value isKindOfClass:cls] || !cls) {
                    ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, (id)value);
                } else if ([value isKindOfClass:[NSDictionary class]]) {
                    NSObject *one = nil;
                    if (meta->_getter) {
                        one = ((id (*)(id, SEL))(void *) objc_msgSend)((id)model, meta->_getter);
                    }
                    if (one) {
                        [one yy_modelSetWithDictionary:value];
                    } else {
                        if (meta->_hasCustomClassFromDictionary) {
                            cls = [cls modelCustomClassForDictionary:value] ?: cls;
                        }
                        one = [cls new];
                        [one yy_modelSetWithDictionary:value];
                        ((void (*)(id, SEL, id))(void *) objc_msgSend)((id)model, meta->_setter, (id)one);
                    }
                }
            } break;
```

## 读写安全

YYModel中也用到了一些全局的缓存,如何保证每次对这个缓存的写操作或者取操作的安全性？

```objc
    static CFMutableDictionaryRef classCache;
    static CFMutableDictionaryRef metaCache;
```

信号量

看下面这段代码

```objc
 //这个方法每个类只会走一次
    dispatch_once(&onceToken, ^{
        classCache = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        metaCache = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        lock = dispatch_semaphore_create(1);
    });
    
    //这里也是采用信号量的机制来保证操作的安全
    dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);
    //class_isMetaClass 判断是否是元类 从缓存中获取classInfo
    YYClassInfo *info = CFDictionaryGetValue(class_isMetaClass(cls) ? metaCache : classCache, (__bridge const void *)(cls));
    //如果渠道信息 且信息需要更新 那么更新信息
    if (info && info->_needUpdate) {
        [info _update];
    }
    dispatch_semaphore_signal(lock);
    
    // 如果缓存中没有类的信息 那么新建
    if (!info) {
        info = [[YYClassInfo alloc] initWithClass:cls];
        if (info) {
            //这里也是利用信号量来保证写操作的安全
            dispatch_semaphore_wait(lock, DISPATCH_TIME_FOREVER);
            CFDictionarySetValue(info.isMeta ? metaCache : classCache, (__bridge const void *)(cls), (__bridge const void *)(info));
            dispatch_semaphore_signal(lock);
        }
    }
```


## 运行时方法

方法 | 作用
------------ | -------------
class_getSuperclass | 获取父类 
class_isMetaClass   | 是否为元类
objc_getMetaClass  | 获取元类
objc_getClass | 获取对象的类
class_copyPropertyList | 获取类的属性列表
class_copyIvarList | 获取实例变量的列表
class_copyMethodList | 获取方法列表
method_getName | 获取方法名
method_getImplementation | 获取方法实现
sel_getName | 获取方法名称
method_getTypeEncoding | 获取方法编码方式
method_copyReturnType | 获取方法返回值的类型
method_getNumberOfArguments | 获取方法参数个数
method_copyArgumentType(method, i) | 获取方法的第几个参数
property_getName | 获取属性名称
property_copyAttributeList | 获取属性的详细信息(下面详解)
objc_msgSend(target,selector,argus) | 调用方法


## property_copyAttributeList

![](http://og0h689k8.bkt.clouddn.com/18-4-11/97101382.jpg)

```
属性类型  name值：T  value：变化
编码类型  name值：C(copy) &(strong) W(weak) 空(assign) 等 value：无
非/原子性 name值：空(atomic) N(Nonatomic)  value：无
变量名称  name值：V  value：变化
```



