/*
 * Copyright (c) 2010-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


/***********************************************************************
* Inlineable parts of NSObject / objc_object implementation
**********************************************************************/

#ifndef _OBJC_OBJCOBJECT_H_
#define _OBJC_OBJCOBJECT_H_

#include "objc-private.h"


enum ReturnDisposition : bool {
    ReturnAtPlus0 = false, ReturnAtPlus1 = true
};

static ALWAYS_INLINE 
bool prepareOptimizedReturn(ReturnDisposition disposition);


#if SUPPORT_TAGGED_POINTERS

extern "C" { 
    extern Class objc_debug_taggedpointer_classes[_OBJC_TAG_SLOT_COUNT];
    extern Class objc_debug_taggedpointer_ext_classes[_OBJC_TAG_EXT_SLOT_COUNT];
}
#define objc_tag_classes objc_debug_taggedpointer_classes
#define objc_tag_ext_classes objc_debug_taggedpointer_ext_classes

#endif

#if SUPPORT_INDEXED_ISA

ALWAYS_INLINE Class &
classForIndex(uintptr_t index) {
    ASSERT(index > 0);
    ASSERT(index < (uintptr_t)objc_indexed_classes_count);
    return objc_indexed_classes[index];
}

#endif


inline bool
objc_object::isClass()
{
    if (isTaggedPointer()) return false;
    return ISA()->isMetaClass();
}


#if SUPPORT_TAGGED_POINTERS

inline Class 
objc_object::getIsa() 
{
    if (fastpath(!isTaggedPointer())) return ISA();

    extern objc_class OBJC_CLASS_$___NSUnrecognizedTaggedPointer;
    uintptr_t slot, ptr = (uintptr_t)this;
    Class cls;

    slot = (ptr >> _OBJC_TAG_SLOT_SHIFT) & _OBJC_TAG_SLOT_MASK;
    cls = objc_tag_classes[slot];
    if (slowpath(cls == (Class)&OBJC_CLASS_$___NSUnrecognizedTaggedPointer)) {
        slot = (ptr >> _OBJC_TAG_EXT_SLOT_SHIFT) & _OBJC_TAG_EXT_SLOT_MASK;
        cls = objc_tag_ext_classes[slot];
    }
    return cls;
}

inline uintptr_t
objc_object::isaBits() const
{
    return isa.bits;
}

inline bool 
objc_object::isTaggedPointer() 
{
    return _objc_isTaggedPointer(this);
}

inline bool 
objc_object::isBasicTaggedPointer() 
{
    return isTaggedPointer()  &&  !isExtTaggedPointer();
}

inline bool 
objc_object::isExtTaggedPointer() 
{
    uintptr_t ptr = _objc_decodeTaggedPointer(this);
    return (ptr & _OBJC_TAG_EXT_MASK) == _OBJC_TAG_EXT_MASK;
}


// SUPPORT_TAGGED_POINTERS
#else
// not SUPPORT_TAGGED_POINTERS


inline Class 
objc_object::getIsa() 
{
    return ISA();
}

inline uintptr_t
objc_object::isaBits() const
{
    return isa.bits;
}


inline bool 
objc_object::isTaggedPointer() 
{
    return false;
}

inline bool 
objc_object::isBasicTaggedPointer() 
{
    return false;
}

inline bool 
objc_object::isExtTaggedPointer() 
{
    return false;
}


// not SUPPORT_TAGGED_POINTERS
#endif


#if SUPPORT_NONPOINTER_ISA

inline Class 
objc_object::ISA() 
{
    ASSERT(!isTaggedPointer()); 
#if SUPPORT_INDEXED_ISA
    if (isa.nonpointer) {
        uintptr_t slot = isa.indexcls;
        return classForIndex((unsigned)slot);
    }
    return (Class)isa.bits;
#else
    return (Class)(isa.bits & ISA_MASK);
#endif
}

inline Class
objc_object::rawISA()
{
    ASSERT(!isTaggedPointer() && !isa.nonpointer);
    return (Class)isa.bits;
}

inline bool 
objc_object::hasNonpointerIsa()
{
    return isa.nonpointer;
}


inline void 
objc_object::initIsa(Class cls)
{
    initIsa(cls, false, false);
}

inline void 
objc_object::initClassIsa(Class cls)
{
    if (DisableNonpointerIsa  ||  cls->instancesRequireRawIsa()) {
        initIsa(cls, false/*not nonpointer*/, false);
    } else {
        initIsa(cls, true/*nonpointer*/, false);
    }
}

inline void
objc_object::initProtocolIsa(Class cls)
{
    return initClassIsa(cls);
}

inline void 
objc_object::initInstanceIsa(Class cls, bool hasCxxDtor)
{
    ASSERT(!cls->instancesRequireRawIsa());
    ASSERT(hasCxxDtor == cls->hasCxxDtor());

    initIsa(cls, true, hasCxxDtor);
}

inline void 
objc_object::initIsa(Class cls, bool nonpointer, bool hasCxxDtor) 
{ 
    ASSERT(!isTaggedPointer()); 
    // 如果是非isa_t类型 直接返回 这个cls即可 因为isa中只有一个指针指向isa
    if (!nonpointer) {
        isa = isa_t((uintptr_t)cls);
    } else {
        ASSERT(!DisableNonpointerIsa);
        ASSERT(!cls->instancesRequireRawIsa());
        // 创建 isa_t 类型临时变量
        isa_t newisa(0);

#if SUPPORT_INDEXED_ISA
        ASSERT(cls->classArrayIndex() > 0);
        newisa.bits = ISA_INDEX_MAGIC_VALUE;
        // isa.magic is part of ISA_MAGIC_VALUE
        // isa.nonpointer is part of ISA_MAGIC_VALUE
        newisa.has_cxx_dtor = hasCxxDtor;
        newisa.indexcls = (uintptr_t)cls->classArrayIndex();
#else
        // 配置 magic 表示当前对象已经创建
        // 配置 nonpointer 表示当前对象的 isa 为 isa_t 类型的
        newisa.bits = ISA_MAGIC_VALUE;
        // isa.magic is part of ISA_MAGIC_VALUE
        // isa.nonpointer is part of ISA_MAGIC_VALUE
        // 配置 has_cxx_dtor 表示当前对象是否有 C++ 的析构器
        newisa.has_cxx_dtor = hasCxxDtor;
        // 配置 shiftcls 指向类对象，右移了 3 位是因为类的指针是按照字节（8bits）对齐的，
        // 其指针后三位都是没有意义的 0，因此可以右移 3 位进行消除，以减小无意义的内存占用。
        newisa.shiftcls = (uintptr_t)cls >> 3;
#endif
        // 将临时变量赋值给结构体成员
        isa = newisa;
    }
}


inline Class 
objc_object::changeIsa(Class newCls)
{
    // This is almost always true but there are 
    // enough edge cases that we can't assert it.
    // assert(newCls->isFuture()  || 
    //        newCls->isInitializing()  ||  newCls->isInitialized());

    ASSERT(!isTaggedPointer()); 

    isa_t oldisa;
    isa_t newisa;

    bool sideTableLocked = false;
    bool transcribeToSideTable = false;

    do {
        transcribeToSideTable = false;
        oldisa = LoadExclusive(&isa.bits);
        if ((oldisa.bits == 0  ||  oldisa.nonpointer)  &&
            !newCls->isFuture()  &&  newCls->canAllocNonpointer())
        {
            // 0 -> nonpointer
            // nonpointer -> nonpointer
#if SUPPORT_INDEXED_ISA
            if (oldisa.bits == 0) newisa.bits = ISA_INDEX_MAGIC_VALUE;
            else newisa = oldisa;
            // isa.magic is part of ISA_MAGIC_VALUE
            // isa.nonpointer is part of ISA_MAGIC_VALUE
            newisa.has_cxx_dtor = newCls->hasCxxDtor();
            ASSERT(newCls->classArrayIndex() > 0);
            newisa.indexcls = (uintptr_t)newCls->classArrayIndex();
#else
            if (oldisa.bits == 0) newisa.bits = ISA_MAGIC_VALUE;
            else newisa = oldisa;
            // isa.magic is part of ISA_MAGIC_VALUE
            // isa.nonpointer is part of ISA_MAGIC_VALUE
            newisa.has_cxx_dtor = newCls->hasCxxDtor();
            newisa.shiftcls = (uintptr_t)newCls >> 3;
#endif
        }
        else if (oldisa.nonpointer) {
            // nonpointer -> raw pointer
            // Need to copy retain count et al to side table.
            // Acquire side table lock before setting isa to 
            // prevent races such as concurrent -release.
            if (!sideTableLocked) sidetable_lock();
            sideTableLocked = true;
            transcribeToSideTable = true;
            newisa.cls = newCls;
        }
        else {
            // raw pointer -> raw pointer
            newisa.cls = newCls;
        }
    } while (!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits));

    if (transcribeToSideTable) {
        // Copy oldisa's retain count et al to side table.
        // oldisa.has_assoc: nothing to do
        // oldisa.has_cxx_dtor: nothing to do
        sidetable_moveExtraRC_nolock(oldisa.extra_rc, 
                                     oldisa.deallocating, 
                                     oldisa.weakly_referenced);
    }

    if (sideTableLocked) sidetable_unlock();

    if (oldisa.nonpointer) {
#if SUPPORT_INDEXED_ISA
        return classForIndex(oldisa.indexcls);
#else
        return (Class)((uintptr_t)oldisa.shiftcls << 3);
#endif
    }
    else {
        return oldisa.cls;
    }
}


inline bool
objc_object::hasAssociatedObjects()
{
    if (isTaggedPointer()) return true;
    if (isa.nonpointer) return isa.has_assoc;
    return true;
}


inline void
objc_object::setHasAssociatedObjects()
{
    if (isTaggedPointer()) return;

 retry:
    isa_t oldisa = LoadExclusive(&isa.bits);
    isa_t newisa = oldisa;
    if (!newisa.nonpointer  ||  newisa.has_assoc) {
        ClearExclusive(&isa.bits);
        return;
    }
    newisa.has_assoc = true;
    if (!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits)) goto retry;
}


inline bool
objc_object::isWeaklyReferenced()
{
    ASSERT(!isTaggedPointer());
    if (isa.nonpointer) return isa.weakly_referenced;
    else return sidetable_isWeaklyReferenced();
}


inline void
objc_object::setWeaklyReferenced_nolock()
{
 retry:
    isa_t oldisa = LoadExclusive(&isa.bits);
    isa_t newisa = oldisa;
    if (slowpath(!newisa.nonpointer)) {
        ClearExclusive(&isa.bits);
        sidetable_setWeaklyReferenced_nolock();
        return;
    }
    if (newisa.weakly_referenced) {
        ClearExclusive(&isa.bits);
        return;
    }
    newisa.weakly_referenced = true;
    if (!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits)) goto retry;
}


inline bool
objc_object::hasCxxDtor()
{
    ASSERT(!isTaggedPointer());
    if (isa.nonpointer) return isa.has_cxx_dtor;
    else return isa.cls->hasCxxDtor();
}



inline bool 
objc_object::rootIsDeallocating()
{
    if (isTaggedPointer()) return false;
    if (isa.nonpointer) return isa.deallocating;
    return sidetable_isDeallocating();
}


inline void 
objc_object::clearDeallocating()
{
    // 如果是没有isa优化
    if (slowpath(!isa.nonpointer)) {
        // Slow path for raw pointer isa.
        // sidetable移除
        sidetable_clearDeallocating();
    }
    else if (slowpath(isa.weakly_referenced  ||  isa.has_sidetable_rc)) {
        // Slow path for non-pointer isa with weak refs and/or side table data.
        clearDeallocating_slow();
    }

    assert(!sidetable_present());
}


inline void
objc_object::rootDealloc()
{
    if (isTaggedPointer()) return;  // fixme necessary?

    if (fastpath(isa.nonpointer  &&  
                 !isa.weakly_referenced  &&  
                 !isa.has_assoc  &&  
                 !isa.has_cxx_dtor  &&  
                 !isa.has_sidetable_rc))
    {
        assert(!sidetable_present());
        free(this);
    } 
    else {
        object_dispose((id)this);
    }
}


// Equivalent to calling [this retain], with shortcuts if there is no override
// 等价于直接使用对象调用retain方法
inline id 
objc_object::retain()
{
    // 如果是TaggedPointer类型 不涉及引用计数
    ASSERT(!isTaggedPointer());
    // fastpath 表示if中的条件是一个大概率事件
    // 如果当前对象没有自定义（override）retain 方法
    if (fastpath(!ISA()->hasCustomRR())) {
        return rootRetain();
    }
    // 如果有自定义的retain方法
    // 通过发消息的方式调用自定义的 retain 方法
    return ((id(*)(objc_object *, SEL))objc_msgSend)(this, @selector(retain));
}


// Base retain implementation, ignoring overrides.
// This does not check isa.fast_rr; if there is an RR override then 
// it was already called and it chose to call [super retain].
//
// tryRetain=true is the -_tryRetain path.
// handleOverflow=false is the frameless fast path.
// handleOverflow=true is the framed slow path including overflow to side table
// The code is structured this way to prevent duplication.

ALWAYS_INLINE id 
objc_object::rootRetain()
{
    return rootRetain(false, false);
}

ALWAYS_INLINE bool 
objc_object::rootTryRetain()
{
    return rootRetain(true, false) ? true : false;
}

ALWAYS_INLINE id 
objc_object::rootRetain(bool tryRetain, bool handleOverflow)
{
    // 如果如果是taggedPointer直接返回不需要引用计数
    if (isTaggedPointer()) return (id)this;
    // 默认不使用sideTable
    bool sideTableLocked = false;
    // 是否需要将引用计数转到sidetable
    bool transcribeToSideTable = false;

    // 记录新旧两个isa指针
    isa_t oldisa;
    isa_t newisa;

    do {
        transcribeToSideTable = false;
        //// 通过 LoadExclusive 方法加载 isa 的值，加锁
        oldisa = LoadExclusive(&isa.bits);
        // 此时 newisa = oldisa
        newisa = oldisa;
        // slowpath表示if中的条件是小概率事件
        // 如果newisa(此时和oldisa相等) 如果没有采用isa优化
        if (slowpath(!newisa.nonpointer)) {
            // 解锁
            ClearExclusive(&isa.bits);
            //rawISA() = (Class)isa.bits
            // 如果当前对象的 isa 指向的类对象是元类（也就是说当前对象不是实例对象，而是类对象），直接返回
            if (rawISA()->isMetaClass()) return (id)this;
            // 如果不需要retain对象(引用计数+1) 且sideTable是锁上的
            if (!tryRetain && sideTableLocked)
                // sidetable解锁
                sidetable_unlock();
            if (tryRetain)
                // sidetable_tryRetain 尝试对引用计数器进行+1的操作 返回+1操作是否成功
                return sidetable_tryRetain() ? (id)this : nil;
            else
                // 将sidetable中保存的引用计数+1同时返回引用计数
                return sidetable_retain();
        }
        // 如果需要尝试 +1 但是当前对象正在销毁中
        if (slowpath(tryRetain && newisa.deallocating)) {
            // 解锁
            ClearExclusive(&isa.bits);
            // 如果不需要去尝试 +1 并且 SideTables 表锁住了，就将其解锁
            // 这里的条件 应该永远都不会被满足
            if (!tryRetain && sideTableLocked)
                sidetable_unlock();
            // 如果对象正在被释放 执行retain是无效的
            return nil;
        }
        // 引用计数是否溢出标志位
        uintptr_t carry;
        //为 isa 中的 extra_rc 位 +1 ，并保存引用计数
        newisa.bits = addc(newisa.bits, RC_ONE, 0, &carry);  // extra_rc++
        // 如果 isa中的extra_rc 溢出
        if (slowpath(carry)) {
            // newisa.extra_rc++ 溢出
            // 是否需要处理溢出 这个变量是rootRetain函数外部传入的参数 是否需要处理溢出时的情况
            if (!handleOverflow) {
                //解锁
                ClearExclusive(&isa.bits);
                // rootRetain_overflow 方法实际上就是递归调用了当前方法只是将handleOverflow
                // 置为yes
                return rootRetain_overflow(tryRetain);
            }
            // 保留isa中extra_rc一半的值 将另一半转移到sidetable中
            // 如果不需要尝试 +1 并且 sidetable 表未加锁，就将其加锁
            if (!tryRetain && !sideTableLocked) sidetable_lock();
            // sidetable加锁
            sideTableLocked = true;
            // 需要将引用计数转移到sidetable
            transcribeToSideTable = true;
            // 将newisa中的引用计数置为之前的一半 # define RC_HALF  (1ULL<<18)
            newisa.extra_rc = RC_HALF;
            // isa中是否使用sidetable存储retiancount的标志位置为1
            newisa.has_sidetable_rc = true;
        }
        //while循环开始 直到 isa.bits 中的值被成功更新成 newisa.bits
        // StoreExclusive(uintptr_t *dst, uintptr_t oldvalue, uintptr_t value)
        // 将更新后的newisa的值更新到isabit中
    } while (slowpath(!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits)));

    // 如果需要转移引用计数到sidetable中
    if (slowpath(transcribeToSideTable)) {
        // 将溢出的引用计数加到 sidetable 中
        sidetable_addExtraRC_nolock(RC_HALF);
    }
    // 如果不需要去尝试 +1 并且 SideTables 表锁住了，就将其解锁
    if (slowpath(!tryRetain && sideTableLocked)) sidetable_unlock();
    // 返回当前对象 引用计数已完成+1操作
    return (id)this;
}


// Equivalent to calling [this release], with shortcuts if there is no override
// 等价于直接使用对象调用release方法
inline void
objc_object::release()
{
    ASSERT(!isTaggedPointer());
    // 如果没有自定义的release方法 就直接调用rootRelease
    if (fastpath(!ISA()->hasCustomRR())) {
        rootRelease();
        return;
    }
    // 如果有自定义的release方法那么调用对象的release方法
    ((void(*)(objc_object *, SEL))objc_msgSend)(this, @selector(release));
}


// Base release implementation, ignoring overrides.
// Does not call -dealloc.
// Returns true if the object should now be deallocated.
// This does not check isa.fast_rr; if there is an RR override then 
// it was already called and it chose to call [super release].
// 
// handleUnderflow=false is the frameless fast path.
// handleUnderflow=true is the framed slow path including side table borrow
// The code is structured this way to prevent duplication.
//
ALWAYS_INLINE bool 
objc_object::rootRelease()
{
    return rootRelease(true, false);
}

ALWAYS_INLINE bool 
objc_object::rootReleaseShouldDealloc()
{
    return rootRelease(false, false);
}

// 真正的release方法
// 两个参数分别是 是否需要调用dealloc函数，是否需要处理 向下溢出的问题
ALWAYS_INLINE bool 
objc_object::rootRelease(bool performDealloc, bool handleUnderflow)
{
    // 如果是TaggedPointer 不需要进行release操作
    if (isTaggedPointer()) return false;
    // 局部变量sideTable是否上锁 默认false
    bool sideTableLocked = false;

    // 两个局部变量用来记录这个对象的isa指针
    isa_t oldisa;
    isa_t newisa;

 retry:
    do {
        // 加载这个isa指针
        oldisa = LoadExclusive(&isa.bits);
        newisa = oldisa;
        // 如果没有进行nonpointer优化
        if (slowpath(!newisa.nonpointer)) {
            ClearExclusive(&isa.bits);
            // 如果是类对象直接返回false 不需要释放
            if (rawISA()->isMetaClass()) return false;
            // 如果sideTableLocked 则解锁 这里默认是false
            if (sideTableLocked)
                sidetable_unlock();
            // 调用sidetable_release 进行引用计数-1操作
            return sidetable_release(performDealloc);
        }

        // 溢出标记位
        uintptr_t carry;
        // newisa 对象的extra_rc 进行-1操作
        newisa.bits = subc(newisa.bits, RC_ONE, 0, &carry);  // extra_rc--
        // 如果-1操作后 向下溢出了 结果为负数
        if (slowpath(carry)) {
            // don't ClearExclusive()
            // 调用underflow 进行向下溢出的处理
            goto underflow;
        }
        //  开启循环，直到 isa.bits 中的值被成功更新成 newisa.bits
    } while (slowpath(!StoreReleaseExclusive(&isa.bits, 
                                             oldisa.bits, newisa.bits)));

    //走到这说明引用计数的 -1 操作已完成
    if (slowpath(sideTableLocked)) sidetable_unlock();
    return false;

 underflow:
    //newisa的extra_rc在执行-1操作后导致了向下溢出
    // 放弃对newisa的修改 使用之前的oldisa
    newisa = oldisa;

    // 如果 isa 的 has_sidetable_rc 标志位标识引用计数已溢出
    // has_sidetable_rc 用于标识是否当前的引用计数过大，无法在isa中存储，
    // 而需要借用sidetable来存储。（这种情况大多不会发生）
    if (slowpath(newisa.has_sidetable_rc)) {
        // 是否需要处理下溢
        if (!handleUnderflow) {
            // 清除原 isa 中的数据的原子独占
            ClearExclusive(&isa.bits);
            // 如果不需要处理下溢 直接调用 rootRelease_underflow方法
            return rootRelease_underflow(performDealloc);
        }

        // 如果sidetable是上锁状态
        if (!sideTableLocked) {
            // 解除清除原 isa 中的数据的原子独占
            ClearExclusive(&isa.bits);
            // sidetable 上锁
            sidetable_lock();
            sideTableLocked = true;
            // 跳转到 retry 重新开始，避免 isa 从 nonpointer 类型转换成原始类型导致的问题
            goto retry;
        }

        // sidetable_subExtraRC_nolock 返回要从sidetable移动到isa的extra_rc的值
        // 默认是获取extra_rc可存储的长度一半的值
        size_t borrowed = sidetable_subExtraRC_nolock(RC_HALF);

        // To avoid races, has_sidetable_rc must remain set 
        // even if the side table count is now zero.
        //  为了避免冲突 has_sidetable_rc 标志位必须保留1的状态 即使sidetable中的个数为0
        if (borrowed > 0) {
            // 将newisa中引用计数值extra_rc 设置为borrowed - 1
            // -1 是因为 本身这次是release操作
            newisa.extra_rc = borrowed - 1;
            // 然后将修改同步到isa中
            bool stored = StoreReleaseExclusive(&isa.bits, 
                                                oldisa.bits, newisa.bits);
            // 如果保存失败
            if (!stored) {
                // Inline update failed. 
                // Try it again right now. This prevents livelock on LL/SC 
                // architectures where the side table access itself may have 
                // dropped the reservation.
                // 从新装载isa
                isa_t oldisa2 = LoadExclusive(&isa.bits);
                isa_t newisa2 = oldisa2;
                // 如果newisa2是nonpointer类型
                if (newisa2.nonpointer) {
                    // 下溢出标志位
                    uintptr_t overflow;
                    // 将从 SideTables 表中获取的引用计数保存到 newisa2 的 extra_rc 标志位中
                    newisa2.bits = 
                        addc(newisa2.bits, RC_ONE * (borrowed-1), 0, &overflow);
                    //
                    if (!overflow) {
                        // 如果没有溢出再次将 isa.bits 中的值更新为 newisa2.bits
                        stored = StoreReleaseExclusive(&isa.bits, oldisa2.bits, 
                                                       newisa2.bits);
                    }
                }
            }

            // 如果重试之后依然失败
            if (!stored) {
                // 将从sidetable中取出的引用计数borrowed 重新加到sidetable中
                sidetable_addExtraRC_nolock(borrowed);
                // 重新尝试
                goto retry;
            }

            // Decrement successful after borrowing from side table.
            // This decrement cannot be the deallocating decrement - the side 
            // table lock and has_sidetable_rc bit ensure that if everyone 
            // else tried to -release while we worked, the last one would block.
            // 完成对 SideTables 表中数据的操作后，为其解锁
            sidetable_unlock();
            return false;
        }
        else {
            // 在从Side table拿出一部分引用计数之后 Side table为空
            // Side table is empty after all. Fall-through to the dealloc path.
        }
    }

    // 如果当前的对象正在被释放
    if (slowpath(newisa.deallocating)) {
        ClearExclusive(&isa.bits);
        // 如果sideTableLocked被锁 那么解锁
        if (sideTableLocked) sidetable_unlock();
        // 兑现被过度释放
        return overrelease_error();
        // does not actually return
    }
    // 将对象被释放的标志位置为true
    newisa.deallocating = true;
    // 将newisa同步到isa中 如果失败 进行重试
    if (!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits))
        goto retry;

    // 如果sideTableLocked= true
    if (slowpath(sideTableLocked))
        // Side table解锁
        sidetable_unlock();

    __c11_atomic_thread_fence(__ATOMIC_ACQUIRE);

    // 如果需要执行dealloc方法 那么调用该对象的dealloc方法
    if (performDealloc) {
        ((void(*)(objc_object *, SEL))objc_msgSend)(this, @selector(dealloc));
    }
    return true;
}


// Equivalent to [this autorelease], with shortcuts if there is no override
inline id 
objc_object::autorelease()
{
    ASSERT(!isTaggedPointer());
    if (fastpath(!ISA()->hasCustomRR())) {
        return rootAutorelease();
    }

    return ((id(*)(objc_object *, SEL))objc_msgSend)(this, @selector(autorelease));
}


// Base autorelease implementation, ignoring overrides.
inline id 
objc_object::rootAutorelease()
{
    if (isTaggedPointer()) return (id)this;
    if (prepareOptimizedReturn(ReturnAtPlus1)) return (id)this;

    return rootAutorelease2();
}

// 获取引用高技术
inline uintptr_t 
objc_object::rootRetainCount()
{
    //isTaggedPointer 不需要引用计数
    if (isTaggedPointer()) return (uintptr_t)this;

    sidetable_lock();
    isa_t bits = LoadExclusive(&isa.bits);
    ClearExclusive(&isa.bits);
    // 如果是nonpointer
    if (bits.nonpointer) {
        // 先从extra_rc取出部分引用计数
        uintptr_t rc = 1 + bits.extra_rc;
        // sidetable中是否有额外的引用计数
        if (bits.has_sidetable_rc) {
            // 从sidetable中获取引用计数
            rc += sidetable_getExtraRC_nolock();
        }
        sidetable_unlock();
        return rc;
    }

    sidetable_unlock();
    // 如果不是nonpointer 直接从sidetable中获取引用计数
    return sidetable_retainCount();
}


// SUPPORT_NONPOINTER_ISA
#else
// not SUPPORT_NONPOINTER_ISA


inline Class 
objc_object::ISA() 
{
    ASSERT(!isTaggedPointer()); 
    return isa.cls;
}

inline Class
objc_object::rawISA()
{
    return ISA();
}

inline bool 
objc_object::hasNonpointerIsa()
{
    return false;
}


inline void 
objc_object::initIsa(Class cls)
{
    ASSERT(!isTaggedPointer()); 
    isa = (uintptr_t)cls; 
}


inline void 
objc_object::initClassIsa(Class cls)
{
    initIsa(cls);
}


inline void 
objc_object::initProtocolIsa(Class cls)
{
    initIsa(cls);
}


inline void 
objc_object::initInstanceIsa(Class cls, bool)
{
    initIsa(cls);
}


inline void 
objc_object::initIsa(Class cls, bool, bool)
{ 
    initIsa(cls);
}


inline Class 
objc_object::changeIsa(Class cls)
{
    // This is almost always rue but there are 
    // enough edge cases that we can't assert it.
    // assert(cls->isFuture()  ||  
    //        cls->isInitializing()  ||  cls->isInitialized());

    ASSERT(!isTaggedPointer()); 
    
    isa_t oldisa, newisa;
    newisa.cls = cls;
    do {
        oldisa = LoadExclusive(&isa.bits);
    } while (!StoreExclusive(&isa.bits, oldisa.bits, newisa.bits));
    
    if (oldisa.cls  &&  oldisa.cls->instancesHaveAssociatedObjects()) {
        cls->setInstancesHaveAssociatedObjects();
    }
    
    return oldisa.cls;
}


inline bool
objc_object::hasAssociatedObjects()
{
    return getIsa()->instancesHaveAssociatedObjects();
}


inline void
objc_object::setHasAssociatedObjects()
{
    getIsa()->setInstancesHaveAssociatedObjects();
}


inline bool
objc_object::isWeaklyReferenced()
{
    ASSERT(!isTaggedPointer());

    return sidetable_isWeaklyReferenced();
}


inline void 
objc_object::setWeaklyReferenced_nolock()
{
    ASSERT(!isTaggedPointer());

    sidetable_setWeaklyReferenced_nolock();
}


inline bool
objc_object::hasCxxDtor()
{
    ASSERT(!isTaggedPointer());
    return isa.cls->hasCxxDtor();
}


inline bool 
objc_object::rootIsDeallocating()
{
    if (isTaggedPointer()) return false;
    return sidetable_isDeallocating();
}


inline void 
objc_object::clearDeallocating()
{
    sidetable_clearDeallocating();
}


inline void
objc_object::rootDealloc()
{
    if (isTaggedPointer()) return;
    object_dispose((id)this);
}


// Equivalent to calling [this retain], with shortcuts if there is no override
inline id 
objc_object::retain()
{
    ASSERT(!isTaggedPointer());

    if (fastpath(!ISA()->hasCustomRR())) {
        return sidetable_retain();
    }

    return ((id(*)(objc_object *, SEL))objc_msgSend)(this, @selector(retain));
}


// Base retain implementation, ignoring overrides.
// This does not check isa.fast_rr; if there is an RR override then 
// it was already called and it chose to call [super retain].
inline id 
objc_object::rootRetain()
{
    if (isTaggedPointer()) return (id)this;
    return sidetable_retain();
}


// Equivalent to calling [this release], with shortcuts if there is no override
inline void
objc_object::release()
{
    ASSERT(!isTaggedPointer());

    if (fastpath(!ISA()->hasCustomRR())) {
        sidetable_release();
        return;
    }

    ((void(*)(objc_object *, SEL))objc_msgSend)(this, @selector(release));
}


// Base release implementation, ignoring overrides.
// Does not call -dealloc.
// Returns true if the object should now be deallocated.
// This does not check isa.fast_rr; if there is an RR override then 
// it was already called and it chose to call [super release].

inline bool 
objc_object::rootRelease()
{
    if (isTaggedPointer()) return false;
    return sidetable_release(true);
}

inline bool 
objc_object::rootReleaseShouldDealloc()
{
    if (isTaggedPointer()) return false;
    return sidetable_release(false);
}


// Equivalent to [this autorelease], with shortcuts if there is no override
inline id 
objc_object::autorelease()
{
    if (isTaggedPointer()) return (id)this;
    if (fastpath(!ISA()->hasCustomRR())) return rootAutorelease();

    return ((id(*)(objc_object *, SEL))objc_msgSend)(this, @selector(autorelease));
}


// Base autorelease implementation, ignoring overrides.
inline id 
objc_object::rootAutorelease()
{
    if (isTaggedPointer()) return (id)this;
    if (prepareOptimizedReturn(ReturnAtPlus1)) return (id)this;

    return rootAutorelease2();
}


// Base tryRetain implementation, ignoring overrides.
// This does not check isa.fast_rr; if there is an RR override then 
// it was already called and it chose to call [super _tryRetain].
inline bool 
objc_object::rootTryRetain()
{
    if (isTaggedPointer()) return true;
    return sidetable_tryRetain();
}


inline uintptr_t 
objc_object::rootRetainCount()
{
    if (isTaggedPointer()) return (uintptr_t)this;
    return sidetable_retainCount();
}


// not SUPPORT_NONPOINTER_ISA
#endif


#if SUPPORT_RETURN_AUTORELEASE

/***********************************************************************
  Fast handling of return through Cocoa's +0 autoreleasing convention.
  The caller and callee cooperate to keep the returned object 
  out of the autorelease pool and eliminate redundant retain/release pairs.

  An optimized callee looks at the caller's instructions following the 
  return. If the caller's instructions are also optimized then the callee 
  skips all retain count operations: no autorelease, no retain/autorelease.
  Instead it saves the result's current retain count (+0 or +1) in 
  thread-local storage. If the caller does not look optimized then 
  the callee performs autorelease or retain/autorelease as usual.

  An optimized caller looks at the thread-local storage. If the result 
  is set then it performs any retain or release needed to change the 
  result from the retain count left by the callee to the retain count 
  desired by the caller. Otherwise the caller assumes the result is 
  currently at +0 from an unoptimized callee and performs any retain 
  needed for that case.

  There are two optimized callees:
    objc_autoreleaseReturnValue
      result is currently +1. The unoptimized path autoreleases it.
    objc_retainAutoreleaseReturnValue
      result is currently +0. The unoptimized path retains and autoreleases it.

  There are two optimized callers:
    objc_retainAutoreleasedReturnValue
      caller wants the value at +1. The unoptimized path retains it.
    objc_unsafeClaimAutoreleasedReturnValue
      caller wants the value at +0 unsafely. The unoptimized path does nothing.

  Example:

    Callee:
      // compute ret at +1
      return objc_autoreleaseReturnValue(ret);
    
    Caller:
      ret = callee();
      ret = objc_retainAutoreleasedReturnValue(ret);
      // use ret at +1 here

    Callee sees the optimized caller, sets TLS, and leaves the result at +1.
    Caller sees the TLS, clears it, and accepts the result at +1 as-is.

  The callee's recognition of the optimized caller is architecture-dependent.
  x86_64: Callee looks for `mov rax, rdi` followed by a call or 
    jump instruction to objc_retainAutoreleasedReturnValue or 
    objc_unsafeClaimAutoreleasedReturnValue. 
  i386:  Callee looks for a magic nop `movl %ebp, %ebp` (frame pointer register)
  armv7: Callee looks for a magic nop `mov r7, r7` (frame pointer register). 
  arm64: Callee looks for a magic nop `mov x29, x29` (frame pointer register). 

  Tagged pointer objects do participate in the optimized return scheme, 
  because it saves message sends. They are not entered in the autorelease 
  pool in the unoptimized case.
**********************************************************************/

# if __x86_64__

static ALWAYS_INLINE bool 
callerAcceptsOptimizedReturn(const void * const ra0)
{
    const uint8_t *ra1 = (const uint8_t *)ra0;
    const unaligned_uint16_t *ra2;
    const unaligned_uint32_t *ra4 = (const unaligned_uint32_t *)ra1;
    const void **sym;

#define PREFER_GOTPCREL 0
#if PREFER_GOTPCREL
    // 48 89 c7    movq  %rax,%rdi
    // ff 15       callq *symbol@GOTPCREL(%rip)
    if (*ra4 != 0xffc78948) {
        return false;
    }
    if (ra1[4] != 0x15) {
        return false;
    }
    ra1 += 3;
#else
    // 48 89 c7    movq  %rax,%rdi
    // e8          callq symbol
    if (*ra4 != 0xe8c78948) {
        return false;
    }
    ra1 += (long)*(const unaligned_int32_t *)(ra1 + 4) + 8l;
    ra2 = (const unaligned_uint16_t *)ra1;
    // ff 25       jmpq *symbol@DYLDMAGIC(%rip)
    if (*ra2 != 0x25ff) {
        return false;
    }
#endif
    ra1 += 6l + (long)*(const unaligned_int32_t *)(ra1 + 2);
    sym = (const void **)ra1;
    if (*sym != objc_retainAutoreleasedReturnValue  &&  
        *sym != objc_unsafeClaimAutoreleasedReturnValue) 
    {
        return false;
    }

    return true;
}

// __x86_64__
# elif __arm__

static ALWAYS_INLINE bool 
callerAcceptsOptimizedReturn(const void *ra)
{
    // if the low bit is set, we're returning to thumb mode
    if ((uintptr_t)ra & 1) {
        // 3f 46          mov r7, r7
        // we mask off the low bit via subtraction
        // 16-bit instructions are well-aligned
        if (*(uint16_t *)((uint8_t *)ra - 1) == 0x463f) {
            return true;
        }
    } else {
        // 07 70 a0 e1    mov r7, r7
        // 32-bit instructions may be only 16-bit aligned
        if (*(unaligned_uint32_t *)ra == 0xe1a07007) {
            return true;
        }
    }
    return false;
}

// __arm__
# elif __arm64__

static ALWAYS_INLINE bool 
callerAcceptsOptimizedReturn(const void *ra)
{
    // fd 03 1d aa    mov fp, fp
    // arm64 instructions are well-aligned
    if (*(uint32_t *)ra == 0xaa1d03fd) {
        return true;
    }
    return false;
}

// __arm64__
# elif __i386__

static ALWAYS_INLINE bool 
callerAcceptsOptimizedReturn(const void *ra)
{
    // 89 ed    movl %ebp, %ebp
    if (*(unaligned_uint16_t *)ra == 0xed89) {
        return true;
    }
    return false;
}

// __i386__
# else

#warning unknown architecture

static ALWAYS_INLINE bool 
callerAcceptsOptimizedReturn(const void *ra)
{
    return false;
}

// unknown architecture
# endif


static ALWAYS_INLINE ReturnDisposition 
getReturnDisposition()
{
    return (ReturnDisposition)(uintptr_t)tls_get_direct(RETURN_DISPOSITION_KEY);
}


static ALWAYS_INLINE void 
setReturnDisposition(ReturnDisposition disposition)
{
    tls_set_direct(RETURN_DISPOSITION_KEY, (void*)(uintptr_t)disposition);
}


// Try to prepare for optimized return with the given disposition (+0 or +1).
// Returns true if the optimized path is successful.
// Otherwise the return value must be retained and/or autoreleased as usual.
static ALWAYS_INLINE bool 
prepareOptimizedReturn(ReturnDisposition disposition)
{
    ASSERT(getReturnDisposition() == ReturnAtPlus0);

    if (callerAcceptsOptimizedReturn(__builtin_return_address(0))) {
        if (disposition) setReturnDisposition(disposition);
        return true;
    }

    return false;
}


// Try to accept an optimized return.
// Returns the disposition of the returned object (+0 or +1).
// An un-optimized return is +0.
static ALWAYS_INLINE ReturnDisposition 
acceptOptimizedReturn()
{
    ReturnDisposition disposition = getReturnDisposition();
    setReturnDisposition(ReturnAtPlus0);  // reset to the unoptimized state
    return disposition;
}


// SUPPORT_RETURN_AUTORELEASE
#else
// not SUPPORT_RETURN_AUTORELEASE


static ALWAYS_INLINE bool
prepareOptimizedReturn(ReturnDisposition disposition __unused)
{
    return false;
}


static ALWAYS_INLINE ReturnDisposition 
acceptOptimizedReturn()
{
    return ReturnAtPlus0;
}


// not SUPPORT_RETURN_AUTORELEASE
#endif


// _OBJC_OBJECT_H_
#endif
