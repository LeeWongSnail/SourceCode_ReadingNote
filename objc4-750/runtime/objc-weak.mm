/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
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

#include "objc-private.h"

#include "objc-weak.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <libkern/OSAtomic.h>

#define TABLE_SIZE(entry) (entry->mask ? entry->mask + 1 : 0)

static void append_referrer(weak_entry_t *entry, objc_object **new_referrer);

BREAKPOINT_FUNCTION(
    void objc_weak_error(void)
);

static void bad_weak_table(weak_entry_t *entries)
{
    _objc_fatal("bad weak table at %p. This may be a runtime bug or a "
                "memory error somewhere else.", entries);
}

/** 
 * Unique hash function for object pointers only.
 * 
 * @param key The object pointer
 * 
 * @return Size unrestricted hash of pointer.
 */
static inline uintptr_t hash_pointer(objc_object *key) {
    return ptr_hash((uintptr_t)key);
}

/** 
 * Unique hash function for weak object pointers only.
 * 
 * @param key The weak object pointer. 
 * 
 * @return Size unrestricted hash of pointer.
 */
static inline uintptr_t w_hash_pointer(objc_object **key) {
    return ptr_hash((uintptr_t)key);
}

/** 
 * Grow the entry's hash table of referrers. Rehashes each
 * of the referrers.
 * 
 * @param entry Weak pointer hash set for a particular object.
 */
// entry 中存放弱引用指针数组 扩容
// weak_entry_t 要扩容的对象
// new_referrer 要插入的指向entry->referent弱引用指针
__attribute__((noinline, used))
static void grow_refs_and_insert(weak_entry_t *entry, 
                                 objc_object **new_referrer)
{
    assert(entry->out_of_line());
    // 获取entry当前的大小
    size_t old_size = TABLE_SIZE(entry);
    // 新的大小为旧的大小的2倍
    size_t new_size = old_size ? old_size * 2 : 8;

    // 获取weak_entry_t中存储的弱引用指针个数
    size_t num_refs = entry->num_refs;
    //获取entry中旧的引用数组
    weak_referrer_t *old_refs = entry->referrers;
    // 更新entry->mask 这里是为了后续申请内存空间使用
    entry->mask = new_size - 1;

    // 创建一个新的entry->referrers数组
    // #define TABLE_SIZE(entry) (entry->mask ? entry->mask + 1 : 0)
    // TABLE_SIZE 获取的数组大小是 mask+1 = new_size
    entry->referrers = (weak_referrer_t *)
        calloc(TABLE_SIZE(entry), sizeof(weak_referrer_t));
    // 重置num_refs和max_hash_displacement
    entry->num_refs = 0;
    entry->max_hash_displacement = 0;

    // 将old_refs中的数据重新插入到新创建entry->referrers中
    for (size_t i = 0; i < old_size && num_refs > 0; i++) {
        if (old_refs[i] != nil) {
            append_referrer(entry, old_refs[i]);
            num_refs--;
        }
    }
    // 将new_referrer插入到扩容后的entry中
    append_referrer(entry, new_referrer);
    if (old_refs) free(old_refs);
}

/** 
 * Add the given referrer to set of weak pointers in this entry.
 * Does not perform duplicate checking (b/c weak pointers are never
 * added to a set twice). 
 *
 * @param entry The entry holding the set of weak pointers. 
 * @param new_referrer The new weak pointer to be added.
 */
// 在entry对象的弱引用数组中追加一个新的弱引用指针new_referrer
// entry 被弱引用的对象
// new_referrer 弱引用entry的指针
static void append_referrer(weak_entry_t *entry, objc_object **new_referrer)
{
    // 如果entry中弱引用指针没有超过了4个 表示弱引用指针存放在inline_referrers中
    // weak_entry 尚未使用动态数组
    if (! entry->out_of_line()) {
        // 遍历inline_referrers数组找到第一个为空的位置 将目标指针插入 尾部追加
        for (size_t i = 0; i < WEAK_INLINE_COUNT; i++) {
            if (entry->inline_referrers[i] == nil) {
                entry->inline_referrers[i] = new_referrer;
                return;
            }
        }

        // 如果entry中弱引用指针==4个
        // 新创建一个weak_referrer_t数组 大小为4(WEAK_INLINE_COUNT)
        // 如果inline_referrers的位置已经存满了，则要转型为referrers，做动态数组。
        weak_referrer_t *new_referrers = (weak_referrer_t *)
            calloc(WEAK_INLINE_COUNT, sizeof(weak_referrer_t));
        // 遍历inline_referrers 将数据放在新创建的临时数组中
        for (size_t i = 0; i < WEAK_INLINE_COUNT; i++) {
            new_referrers[i] = entry->inline_referrers[i];
        }
        // 弱引用指针的存储改为存放到entry->referrers(entry->inline_referrers -> entry->referrers)
        entry->referrers = new_referrers;
        // 更新弱引用个数
        entry->num_refs = WEAK_INLINE_COUNT;
        //更新是否使用动态数组标记位
        entry->out_of_line_ness = REFERRERS_OUT_OF_LINE;
        // 更新mask和最大哈希偏移值
        entry->mask = WEAK_INLINE_COUNT-1;
        entry->max_hash_displacement = 0;
    }

    assert(entry->out_of_line());

    // 如果只想entry的弱引用个数大于4
    // 弱引用个数是否已超过数组容量的3/4
    if (entry->num_refs >= TABLE_SIZE(entry) * 3/4) {
        // 如果已超过 那么先扩容在插入
        return grow_refs_and_insert(entry, new_referrer);
    }

    // 如果不需要扩容，直接插入到weak_entry中
    // 注意，weak_entry是一个哈希表，key：w_hash_pointer(new_referrer) value: new_referrer
    size_t begin = w_hash_pointer(new_referrer) & (entry->mask);
    size_t index = begin;
    size_t hash_displacement = 0;
    // 由低到高遍历entry->referrers 找到第一个空位置
    while (entry->referrers[index] != nil) {
        hash_displacement++;
        index = (index+1) & entry->mask;
        // 如果遍历了所有元素后都没有找到 那么报错
        if (index == begin)
            bad_weak_table(entry);
    }
    // 更新最大哈希偏移值
    if (hash_displacement > entry->max_hash_displacement) {
        entry->max_hash_displacement = hash_displacement;
    }
    // 将new_referrer插入到数组的第index个位置
    weak_referrer_t &ref = entry->referrers[index];
    ref = new_referrer;
    // 弱引用计个数+1
    entry->num_refs++;
}

/** 
 * Remove old_referrer from set of referrers, if it's present.
 * Does not remove duplicates, because duplicates should not exist. 
 * 
 * @todo this is slow if old_referrer is not present. Is this ever the case? 
 *
 * @param entry The entry holding the referrers.
 * @param old_referrer The referrer to remove. 
 */
// 删除old_referrer集合中的referrers
// 参数 entry 被弱引用对象
// 参数 old_referrer 要删除的弱引用指针
static void remove_referrer(weak_entry_t *entry, objc_object **old_referrer)
{
    // 指向entry的弱引用指针不超过4个
    if (! entry->out_of_line()) {
        for (size_t i = 0; i < WEAK_INLINE_COUNT; i++) {
            // 遍历inline_referrers数组如果找到直接置空
            if (entry->inline_referrers[i] == old_referrer) {
                entry->inline_referrers[i] = nil;
                return;
            }
        }
        // 如果没有找到 则报错 弱引用指针小于4个且在inline_referrers中没有找到
        _objc_inform("Attempted to unregister unknown __weak variable "
                     "at %p. This is probably incorrect use of "
                     "objc_storeWeak() and objc_loadWeak(). "
                     "Break on objc_weak_error to debug.\n", 
                     old_referrer);
        objc_weak_error();
        return;
    }

    // 哈希函数 判断这个旧的弱引用指针存放的位置
    size_t begin = w_hash_pointer(old_referrer) & (entry->mask);
    size_t index = begin;
    size_t hash_displacement = 0;
    // 遍历entry->referrers数组查找old_referrer
    while (entry->referrers[index] != old_referrer) {
        // 如果没有在指定index找到 那么取下一个位置的值比较
        index = (index+1) & entry->mask;
        // 如果找了一圈仍然没有找到 那么报错
        if (index == begin)
            bad_weak_table(entry);
        // 更新最大哈希偏移值
        hash_displacement++;
        // 如果最大哈希偏移值 超过了预定的限制 那么报错
        if (hash_displacement > entry->max_hash_displacement) {
            _objc_inform("Attempted to unregister unknown __weak variable "
                         "at %p. This is probably incorrect use of "
                         "objc_storeWeak() and objc_loadWeak(). "
                         "Break on objc_weak_error to debug.\n", 
                         old_referrer);
            objc_weak_error();
            return;
        }
    }

    // 走到这一步说明在entry->referrers中的index位置找到了值为old_referrer的引用
    // 将数组的这个位置置空
    entry->referrers[index] = nil;
    // 弱引用个数-1
    entry->num_refs--;
}

/** 
 * Add new_entry to the object's table of weak references.
 * Does not check whether the referent is already in the table.
 */
// 向指定的weak_table_t中插入某个对象
// weak_table_t 目标 table
// new_entry 被弱引用的对象
static void weak_entry_insert(weak_table_t *weak_table, weak_entry_t *new_entry)
{
    // 取出weak_table中所有弱引用的对象
    weak_entry_t *weak_entries = weak_table->weak_entries;
    assert(weak_entries != nil);

    // 根据new_entry中被弱引用对象地址通过哈希算法 算出 弱引用new_entry->referent的对象存放的index
    size_t begin = hash_pointer(new_entry->referent) & (weak_table->mask);
    size_t index = begin;
    size_t hash_displacement = 0;
    // weak_entries[index].referent 如果不为空 表示已经有
    while (weak_entries[index].referent != nil) {
        // 计算下一个要遍历的index
        index = (index+1) & weak_table->mask;
        // 遍历了所有元素发现weak_entries[index].referent 都不为nil
        if (index == begin)
            // 直接报错
            bad_weak_table(weak_entries);
        // 哈希冲突次数++
        hash_displacement++;
    }

    // 如果走到这里 表明index位置的元素referent=nil
    // 直接插入
    weak_entries[index] = *new_entry;
    // 实体个数++
    weak_table->num_entries++;

    // 如果哈希冲突次数大于最大允许的次数 但是实际上
    if (hash_displacement > weak_table->max_hash_displacement) {
        // 修改最大允许的哈希冲突次数
        weak_table->max_hash_displacement = hash_displacement;
    }
}


//weak_table_t扩容
// 参数 weak_table 要扩容的table new_size 目标大小
static void weak_resize(weak_table_t *weak_table, size_t new_size)
{
    //weak_table的容量
    size_t old_size = TABLE_SIZE(weak_table);

    // 取出weak_table中存放的所有实体
    weak_entry_t *old_entries = weak_table->weak_entries;
    // 新创建一个weak_entry_t类型的数组
    // 数组的大小是new_size * sizeof(weak_entry_t)
    weak_entry_t *new_entries = (weak_entry_t *)
        calloc(new_size, sizeof(weak_entry_t));

    // 重置weak_table的mask的值
    weak_table->mask = new_size - 1;
    // 将weak_table->weak_entries指向新创建的内存区域 注意 此时weak_table中没有任何数据
    weak_table->weak_entries = new_entries;
    // 允许最大哈希冲突次数重置为0
    weak_table->max_hash_displacement = 0;
    //weak_table 中存储实体个数为0
    weak_table->num_entries = 0;  // restored by weak_entry_insert below

    // 旧数据的搬迁
    if (old_entries) {
        weak_entry_t *entry;
        //old_entries看做数组中第一个元素的地址 由于数组是连续的存储空间 那么old_entries + old_size = 数组最后一个元素的地址
        weak_entry_t *end = old_entries + old_size;
        // 遍历这些旧数据
        for (entry = old_entries; entry < end; entry++) {
            //weak_entry_t的referent(referent是指被弱引用的对象)
            if (entry->referent) {
                // 将旧数据搬移到新的结构中
                weak_entry_insert(weak_table, entry);
            }
        }
        // 释放所有的旧数据
        free(old_entries);
    }
}

// Grow the given zone's table of weak references if it is full.
// 存放弱引用目标对象的对象的weak表扩容
// 参数weak_table 目标扩容表
static void weak_grow_maybe(weak_table_t *weak_table)
{
    // 获取旧的table的大小
    size_t old_size = TABLE_SIZE(weak_table);

    // weak_table中实体个数大于容量的3/4 则进行扩容 否则不扩容
    if (weak_table->num_entries >= old_size * 3 / 4) {
        // 将weak_table的容量扩展为原来的2倍
        weak_resize(weak_table, old_size ? old_size*2 : 64);
    }
}

// weak_table 缩容
static void weak_compact_maybe(weak_table_t *weak_table)
{
    // 取出当前weak_table的容量
    size_t old_size = TABLE_SIZE(weak_table);

    // 如果 当前容量大于1024 且 当前容量的使用率不足0.06
    if (old_size >= 1024  && old_size / 16 >= weak_table->num_entries) {
        // 将weak_table容量缩小为之前的1/8
        weak_resize(weak_table, old_size / 8);
        // leaves new table no more than 1/2 full
    }
}


//从weak_table中移除entry (指向entry的弱引用指针数为0)
static void weak_entry_remove(weak_table_t *weak_table, weak_entry_t *entry)
{
    // 如果弱引用指针超过4个(弱引用指针存放在entry->referrers中)
    if (entry->out_of_line())
        // 释放entry->referrers中所有数据
        free(entry->referrers);
    bzero(entry, sizeof(*entry));
    //num_entries-1
    weak_table->num_entries--;
    //weak_table是否需要锁绒
    weak_compact_maybe(weak_table);
}


/** 
 * Return the weak reference table entry for the given referent. 
 * If there is no entry for referent, return NULL. 
 * Performs a lookup.
 *
 * @param weak_table 
 * @param referent The object. Must not be nil.
 * 
 * @return The table of weak referrers to this object. 
 */

// 在weak_table中查找所有弱引用referent的对象
static weak_entry_t *
weak_entry_for_referent(weak_table_t *weak_table, objc_object *referent)
{
    assert(referent);
    //获取这个weak_table_t中所有的弱引用对象
    weak_entry_t *weak_entries = weak_table->weak_entries;

    if (!weak_entries) return nil;
    //hash_pointer 哈希函数 传入的是 objc_object *key
    // weak_table->mask = weaktable的容量-1
    size_t begin = hash_pointer(referent) & weak_table->mask;
    size_t index = begin;
    // 哈希冲突次数
    size_t hash_displacement = 0;
    // 判断根据index获取到的弱引用对象数组中对应的weak_entry_t的弱引用对象是否为
    // 外部传入的对象
    while (weak_table->weak_entries[index].referent != referent) {
        // 开放地址法解决哈希冲突
        // & weak_table->mask 是为了在下一个地址仍然没有找到外部传入对象时回到第一个对比的位置
        index = (index+1) & weak_table->mask;
        if (index == begin)
            // 对比了所有数据 仍没有找到 直接报错
            bad_weak_table(weak_table->weak_entries);
        // 哈希冲突次数++
        hash_displacement++;
        // 如果哈希冲突次数大于哈希表所允许的最大哈希冲突次数
        // 没有找到那么直接返回nil
        if (hash_displacement > weak_table->max_hash_displacement) {
            return nil;
        }
    }
    // 直接返回被弱引用的对象
    return &weak_table->weak_entries[index];
}

/** 
 * Unregister an already-registered weak reference.
 * This is used when referrer's storage is about to go away, but referent
 * isn't dead yet. (Otherwise, zeroing referrer later would be a
 * bad memory access.)
 * Does nothing if referent/referrer is not a currently active weak reference.
 * Does not zero referrer.
 * 
 * FIXME currently requires old referent value to be passed in (lame)
 * FIXME unregistration should be automatic if referrer is collected
 * 
 * @param weak_table The global weak table.
 * @param referent The object.
 * @param referrer The weak reference.
 */
// 解除已注册的 对象-弱引用指针 对
// 参数weak_table 全局弱引用表
// referent_id 弱引用所指向的对象
// referrer_id 弱引用指针地址
void
weak_unregister_no_lock(weak_table_t *weak_table, id referent_id, 
                        id *referrer_id)
{
    // 被弱引用的对象
    objc_object *referent = (objc_object *)referent_id;
    // 指向被弱引用对象的指针的地址
    objc_object **referrer = (objc_object **)referrer_id;

    weak_entry_t *entry;

    if (!referent) return;

    // 找到weak_table中指向被弱引用对象的所有指针 类型为 weak_entry_t
    if ((entry = weak_entry_for_referent(weak_table, referent))) {
        // 从数组中删除当前这个弱引用指针
        remove_referrer(entry, referrer);
        bool empty = true;
        // 弱引用referent对象的弱引用指针是否为空
        if (entry->out_of_line()  &&  entry->num_refs != 0) {
            empty = false;
        }
        else {
            // 如果referrer数组中为空 那么判断inline_referrers中是否为空 如果为空empty=true
            for (size_t i = 0; i < WEAK_INLINE_COUNT; i++) {
                if (entry->inline_referrers[i]) {
                    empty = false; 
                    break;
                }
            }
        }

        // 如果为空 则证明没有其他指针指向这个被所引用的对象
        if (empty) {
            // 将这个实体从weak_table中移除
            weak_entry_remove(weak_table, entry);
        }
    }

    // Do not set *referrer = nil. objc_storeWeak() requires that the 
    // value not change.
}

/** 
 * Registers a new (object, weak pointer) pair. Creates a new weak
 * object entry if it does not exist.
 * 
 * @param weak_table The global weak table.
 * @param referent The object pointed to by the weak reference.
 * @param referrer The weak pointer address.
 */
// 添加对某个对象的新的弱引用指针
// weak_table 目标被弱引用对象所存储的表
// referent_id 被所引用的对象
// referrer_id 要被添加的弱引用指针
// crashIfDeallocating 如果对象正在被释放时是否崩溃
id 
weak_register_no_lock(weak_table_t *weak_table, id referent_id, 
                      id *referrer_id, bool crashIfDeallocating)
{
    // 被弱引用的对象
    objc_object *referent = (objc_object *)referent_id;
    // 要添加的指向弱引用指针的对象
    objc_object **referrer = (objc_object **)referrer_id;

    // 如果referent为nil 或 referent 采用了TaggedPointer计数方式，直接返回，不做任何操作
    if (!referent  ||  referent->isTaggedPointer()) return referent_id;

    // 确保被引用的对象可用（没有在析构，同时应该支持weak引用）
    bool deallocating;
    // referent 是否有自定义的释放方法
    if (!referent->ISA()->hasCustomRR()) {
        deallocating = referent->rootIsDeallocating();
    }
    else {
        // referent是否支持weak引用
        BOOL (*allowsWeakReference)(objc_object *, SEL) = 
            (BOOL(*)(objc_object *, SEL))
            object_getMethodImplementation((id)referent, 
                                           SEL_allowsWeakReference);
        // 如果referent不能够被weak引用，则直接返回nil
        if ((IMP)allowsWeakReference == _objc_msgForward) {
            return nil;
        }
        // 调用referent的SEL_allowsWeakReference方法来判断是否正在被释放
        deallocating =
            ! (*allowsWeakReference)(referent, SEL_allowsWeakReference);
    }

    // 正在析构的对象，不能够被弱引用
    if (deallocating) {
        // 判断是否需要崩溃 如果需要则崩溃
        if (crashIfDeallocating) {
            _objc_fatal("Cannot form weak reference to instance (%p) of "
                        "class %s. It is possible that this object was "
                        "over-released, or is in the process of deallocation.",
                        (void*)referent, object_getClassName((id)referent));
        } else {
            return nil;
        }
    }

    // 对象没有被正在释放
    weak_entry_t *entry;
    // 在 weak_table中找到referent对应的weak_entry,并将referrer加入到weak_entry中
    // 如果能找到weak_entry,则讲referrer插入到weak_entry中
    if ((entry = weak_entry_for_referent(weak_table, referent))) {
        // 将referrer插入到weak_entry_t的引用数组中
        append_referrer(entry, referrer);
    } 
    else {
        // 创建一个新的weak_entry_t ，并将referrer插入到weak_entry_t的引用数组中
        weak_entry_t new_entry(referent, referrer);
        // weak_table的weak_entry_t 数组是否需要动态增长，若需要，则会扩容一倍
        weak_grow_maybe(weak_table);
        // 将weak_entry_t插入到weak_table中
        weak_entry_insert(weak_table, &new_entry);
    }

    // Do not set *referrer. objc_storeWeak() requires that the 
    // value not change.

    return referent_id;
}


#if DEBUG
bool
weak_is_registered_no_lock(weak_table_t *weak_table, id referent_id) 
{
    return weak_entry_for_referent(weak_table, (objc_object *)referent_id);
}
#endif


/** 
 * Called by dealloc; nils out all weak pointers that point to the 
 * provided object so that they can no longer be used.
 * 
 * @param weak_table 
 * @param referent The object being deallocated. 
 */
//清理weak_table，同时将所有weak引用该对象的ptr置为nil
void 
weak_clear_no_lock(weak_table_t *weak_table, id referent_id) 
{
    objc_object *referent = (objc_object *)referent_id;

    // 找到referent在weak_table中对应的weak_entry_t
    weak_entry_t *entry = weak_entry_for_referent(weak_table, referent);

    if (entry == nil) {
        /// XXX shouldn't happen, but does with mismatched CF/objc
        //printf("XXX no entry for clear deallocating %p\n", referent);
        return;
    }

    // 找出weak引用referent的weak 指针地址数组以及数组长度
    weak_referrer_t *referrers;
    size_t count;
    // 是否使用动态数组
    if (entry->out_of_line()) {
        referrers = entry->referrers;
        count = TABLE_SIZE(entry);
    } 
    else {
        referrers = entry->inline_referrers;
        count = WEAK_INLINE_COUNT;
    }

    // 遍历所有的所引用weak指针
    for (size_t i = 0; i < count; ++i) {
        // 取出每个weak ptr的地址
        objc_object **referrer = referrers[i];

        if (referrer) {
            // 如果weak ptr确实weak引用了referent，则将weak ptr设置为nil，这也就是为什么weak 指针会自动设置为nil的原因
            if (*referrer == referent) {
                *referrer = nil;
            }
            else if (*referrer) {
                // 如果所存储的weak ptr没有weak 引用referent，这可能是由于runtime代码的逻辑错误引起的，报错
                _objc_inform("__weak variable at %p holds %p instead of %p. "
                             "This is probably incorrect use of "
                             "objc_storeWeak() and objc_loadWeak(). "
                             "Break on objc_weak_error to debug.\n", 
                             referrer, (void*)*referrer, (void*)referent);
                objc_weak_error();
            }
        }
    }
    // 由于referent要被释放了，因此referent的weak_entry_t也要移除出weak_table
    weak_entry_remove(weak_table, entry);
}

