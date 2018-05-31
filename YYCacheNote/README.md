### YYCache源码解读

先奉上YY自己写的对于YYCache的[解读](https://blog.ibireme.com/2015/10/26/yycache/)

这篇文章主要介绍了YYCache在内存缓存和磁盘缓存一个一些锁的使用上的选择以及选择的原因:

##### 内存缓存

相比较于`NSCache`、`TMCache`、`TMMemoryCache`、`PINMemoryCache`。`YYCache`相对于 `PINMemoryCache` 来说,去掉了异步访问的接口，尽量优化了同步访问的性能，用 `OSSpinLock` 来保证线程安全。另外，缓存内部用双向链表和 NSDictionary 实现了 LRU 淘汰算法.

比较结果:

![cachememory](https://blog.ibireme.com/wp-content/uploads/2015/10/memory_cache_bench_result.png)

##### 磁盘缓存

磁盘缓存一般实现选项有: 基于文件读写、基于 mmap 文件内存映射、基于数据库。

TMDiskCache, PINDiskCache, SDWebImage 等缓存，都是基于文件系统的，即一个 Value 对应一个文件，通过文件读写来缓存数据，缺点:不方便扩展、没有元数据、难以实现较好的淘汰算法、数据统计缓慢。

FastImageCache 采用的是 mmap 将文件映射到内存.缺陷：热数据的文件不要超过物理内存大小，不然 mmap 会导致内存交换严重降低性能；另外内存中的数据是定时 flush 到文件的，如果数据还未同步时程序挂掉，就会导致数据错误。


NSURLCache、FBDiskCache 都是基于 SQLite 数据库的，SQLite 写入性能比直接写文件要高，但读取性能取决于数据大小：当单条数据小于 20K 时，数据越小 SQLite 读取性能越高；单条数据大于 20K 时，直接写为文件速度会更快一些。

YYDiskCache 也是采用的 SQLite 配合文件的存储方式。

![diskcache](https://blog.ibireme.com/wp-content/uploads/2015/10/disk_cache_bench_result.png)


##### 关于锁

OSSpinLock 自旋锁，性能最高的锁。原理很简单，就是一直 do while 忙等。它的缺点是当等待时会消耗大量 CPU 资源，所以它不适用于较长时间的任务。对于内存缓存的存取来说，它非常合适。

dispatch_semaphore 是信号量，但当信号总量设为 1 时也可以当作锁来。在没有等待情况出现时，它的性能比 pthread_mutex 还要高，但一旦有等待情况出现时，性能就会下降许多。相对于 OSSpinLock 来说，它的优势在于等待时不会消耗 CPU 资源。对磁盘缓存来说，它比较合适。

#### 我自己的理解

##### YYCache的整体框架

![frame](http://og0h689k8.bkt.clouddn.com/18-5-31/90052508.jpg)

##### YYCache

我们通过下面两个方法一个读一个写来看YYCache是如何来管理内存缓存和磁盘缓存的


读:

```objc
// 获取key对应的对象 以block的形式返回给外部
- (void)objectForKey:(NSString *)key withBlock:(void (^)(NSString *key, id<NSCoding> object))block {
    if (!block) return;
    //先看一下内存中是否存在
    id<NSCoding> object = [_memoryCache objectForKey:key];
    
    if (object) { //如果内存中存在这个key对应的对象 直接返回给外部
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            block(key, object);
        });
    } else {    // 如果内存中没有这个key对应的对象 那么在磁盘缓存中去查找
        [_diskCache objectForKey:key withBlock:^(NSString *key, id<NSCoding> object) {
            //如果在磁盘中找到了这个key对应的对象但是内存中没有 那么将这个对象放到内存缓存中
            if (object && ![_memoryCache objectForKey:key]) {
                [_memoryCache setObject:object forKey:key];
            }
            //将找到的数据返回给外部 这里也有可能返回空
            block(key, object);
        }];
    }
}
```

写:

```objc
// 保存key对应的object
- (void)setObject:(id<NSCoding>)object forKey:(NSString *)key {
    //同时在内存和磁盘中保存这个对象 这样不存在内存缓存中存在磁盘缓存中不存在的情况了
    [_memoryCache setObject:object forKey:key];
    [_diskCache setObject:object forKey:key];
}
```

##### YYMemoryCache

![属性列表](http://og0h689k8.bkt.clouddn.com/18-5-31/58485569.jpg)


![双向链表的实现](http://og0h689k8.bkt.clouddn.com/18-5-31/32998159.jpg)


使用双向链表的形式来管理每一个缓存的对象。下面列几个方法来更好的理解


###### 增加一条缓存

```objc
//设置这个object
- (void)setObject:(id)object forKey:(id)key withCost:(NSUInteger)cost {
    if (!key) return;
    //如果object是nil那么如果有这个key相当于移除这个<key value>
    if (!object) {
        [self removeObjectForKey:key];
        return;
    }
    
    //这个锁有啥好处?
    pthread_mutex_lock(&_lock);
    //从Map中取出key对应的node
    _YYLinkedMapNode *node = CFDictionaryGetValue(_lru->_dic, (__bridge const void *)(key));
    
    NSTimeInterval now = CACurrentMediaTime();
    //如果node已经存在那么修改 每次修改一个节点都要把这个节点放在双向链表的头结点的位置
    if (node) {
        _lru->_totalCost -= node->_cost;
        _lru->_totalCost += cost;
        node->_cost = cost;
        node->_time = now;
        node->_value = object;
        [_lru bringNodeToHead:node];
    } else {
        //如果这个节点不存在那么直接在头部插入这个节点
        node = [_YYLinkedMapNode new];
        node->_cost = cost;
        node->_time = now;
        node->_key = key;
        node->_value = object;
        [_lru insertNodeAtHead:node];
    }
    
    //每次插入的时候都要检测一下 是否超过了限制
    
    // 大小的限制
    if (_lru->_totalCost > _costLimit) {
        dispatch_async(_queue, ^{
            [self trimToCost:_costLimit];
        });
    }
    
    //个数的限制
    if (_lru->_totalCount > _countLimit) {
        //移除尾部的节点
        _YYLinkedMapNode *node = [_lru removeTailNode];
        
        if (_lru->_releaseAsynchronously) {
            dispatch_queue_t queue = _lru->_releaseOnMainThread ? dispatch_get_main_queue() : YYMemoryCacheGetReleaseQueue();
            dispatch_async(queue, ^{
                [node class]; //hold and release in queue
            });
        } else if (_lru->_releaseOnMainThread && !pthread_main_np()) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [node class]; //hold and release in queue
            });
        }
    }
    pthread_mutex_unlock(&_lock);
}
```

###### 删除一条缓存

```objc

//移除map中的一个节点
- (void)removeObjectForKey:(id)key {
    if (!key) return;
    pthread_mutex_lock(&_lock);
    _YYLinkedMapNode *node = CFDictionaryGetValue(_lru->_dic, (__bridge const void *)(key));
    
    if (node) {
        [_lru removeNode:node];
        if (_lru->_releaseAsynchronously) {
            dispatch_queue_t queue = _lru->_releaseOnMainThread ? dispatch_get_main_queue() : YYMemoryCacheGetReleaseQueue();
            dispatch_async(queue, ^{
                [node class]; //hold and release in queue
            });
        } else if (_lru->_releaseOnMainThread && !pthread_main_np()) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [node class]; //hold and release in queue
            });
        }
    }
    pthread_mutex_unlock(&_lock);
}
```

###### 获取一条缓存数据

```objc
- (BOOL)containsObjectForKey:(id)key {
    if (!key) return NO;
    pthread_mutex_lock(&_lock);
    BOOL contains = CFDictionaryContainsKey(_lru->_dic, (__bridge const void *)(key));
    pthread_mutex_unlock(&_lock);
    return contains;
}
```

再来看一下LRU算法的使用:

先看一下如果内存空间满了,移除的顺序:

```objc
// 注意这里的顺序 总容量->总数量->时间
- (void)_trimInBackground {
    dispatch_async(_queue, ^{
        [self _trimToCost:self->_costLimit];
        [self _trimToCount:self->_countLimit];
        [self _trimToAge:self->_ageLimit];
    });
}

```


看看_trimToAge这个方法的具体实现

```objc
// 按照最久未使用的顺序移除
- (void)_trimToAge:(NSTimeInterval)ageLimit {
    BOOL finish = NO;
    NSTimeInterval now = CACurrentMediaTime();
    pthread_mutex_lock(&_lock);
    if (ageLimit <= 0) {
        [_lru removeAll];
        finish = YES;
    } else if (!_lru->_tail || (now - _lru->_tail->_time) <= ageLimit) {
        finish = YES;
    }
    pthread_mutex_unlock(&_lock);
    if (finish) return;
    
    NSMutableArray *holder = [NSMutableArray new];
    while (!finish) {
        if (pthread_mutex_trylock(&_lock) == 0) {
            if (_lru->_tail && (now - _lru->_tail->_time) > ageLimit) {
                _YYLinkedMapNode *node = [_lru removeTailNode];
                if (node) [holder addObject:node];
            } else {
                finish = YES;
            }
            pthread_mutex_unlock(&_lock);
        } else {
            usleep(10 * 1000); //10 ms
        }
    }
    
    /**
     对象的销毁虽然消耗资源不多，但累积起来也是不容忽视的。通常当容器类持有大量对象时，其销毁时的资源消耗就非常明显。
     同样的，如果对象可以放到后台线程去释放，那就挪到后台线程去。这里有个小 Tip：把对象捕获到 block 中，
     然后扔到后台队列去随便发送个消息以避免编译器警告，就可以让对象在后台线程销毁了。
     */
    if (holder.count) {
        dispatch_queue_t queue = _lru->_releaseOnMainThread ? dispatch_get_main_queue() : YYMemoryCacheGetReleaseQueue();
        dispatch_async(queue, ^{
            [holder count]; // release in queue
        });
    }
}
```


下面我们在来看一下对于这个双向链表的操作,列举一下方法名大概就可以知道操作了

```objc
/// Insert a node at head and update the total cost.
/// Node and node.key should not be nil.
- (void)insertNodeAtHead:(_YYLinkedMapNode *)node;

/// Bring a inner node to header.
/// Node should already inside the dic.
- (void)bringNodeToHead:(_YYLinkedMapNode *)node;

/// Remove a inner node and update the total cost.
/// Node should already inside the dic.
- (void)removeNode:(_YYLinkedMapNode *)node;

/// Remove tail node if exist.
- (_YYLinkedMapNode *)removeTailNode;

/// Remove all node in background queue.
- (void)removeAll;
```

##### YYDiskCache

我们在最开始介绍DiskCache的时候,介绍到了:`当单条数据小于 20K 时，数据越小 SQLite 读取性能越高；单条数据大于 20K 时，直接写为文件速度会更快一些` 因此 在YYCache中也是做了判断。下面我们来具体的看一下DiskCache的实现。

先看几个比较重要的属性

![](http://og0h689k8.bkt.clouddn.com/18-5-31/65125255.jpg)

属性 | 作用
------------ | -------------
inlineThreshold |  这个属性意思是 对象的二进制大小是否大于inlineThreshold如果大于这个值那么将会被以文件的方式存储如果不大于这个值 那么将会以sqlite的形式存储
countLimit | 磁盘缓存可以保存的最大文件个数
costLimit  | 磁盘缓存可以保存文件的最大容量
ageLimit   | 磁盘缓存可以保存文件最长的时间
freeDiskSpaceLimit | 设置磁盘空间最小的空间阈值 如果剩余的磁盘空间小于这个值 那么会自动释放这个应用的磁盘空间
autoTrimInterval | 默认60单位s 递归检测磁盘缓存的时间间隔

