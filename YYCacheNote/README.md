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



