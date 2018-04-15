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
#import <objc/runtime.h>

// For a magic reserved keyword color, use @defs(your_protocol_name)
//defs 只是 _pk_extension 的别名，为了提供一个更加合适的名字作为接口 使用的时候用@defs()
#define defs _pk_extension

// 我们使用的时候用@defs(Protocol)实际上是调用_pk_extension_imp方法
#define _pk_extension($protocol) _pk_extension_imp($protocol, _pk_get_container_class($protocol))

// 这个宏实际上是创建了一个隐藏类(container_class) 这个类遵守protocol这个协议
// 然后在load方法中调用内部实现的load(load方法也是在main函数执行之前执行的)
#define _pk_extension_imp($protocol, $container_class) \
    protocol $protocol; \
    @interface $container_class : NSObject <$protocol> @end \
    @implementation $container_class \
    + (void)load { \
        _pk_extension_load(@protocol($protocol), $container_class.class); \
    } \

// _pk_get_container_class 实际上就是生成一个类名
// 可以看到这个类名实际上是由雷鸣+协议名+count(一个计数器每次调用会自动加一)
#define _pk_get_container_class($protocol) _pk_get_container_class_imp($protocol, __COUNTER__)
#define _pk_get_container_class_imp($protocol, $counter) _pk_get_container_class_imp_concat(__PKContainer_, $protocol, $counter)
#define _pk_get_container_class_imp_concat($a, $b, $c) $a ## $b ## _ ## $c

void _pk_extension_load(Protocol *protocol, Class containerClass);

