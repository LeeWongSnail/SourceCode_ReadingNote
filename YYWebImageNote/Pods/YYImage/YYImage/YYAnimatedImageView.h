//
//  YYAnimatedImageView.h
//  YYImage <https://github.com/ibireme/YYImage>
//
//  Created by ibireme on 14/10/19.
//  Copyright (c) 2015 ibireme.
//
//  This source code is licensed under the MIT-style license found in the
//  LICENSE file in the root directory of this source tree.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/**
 
 一个用来显示动图的ImageView
 
 @discussion
 与UIImageView的子类完全兼容
 如果image或者highlightedImage属性遵守了YYAnimatedImage协议,那么他可以用来播放多帧动画。
 动画也可以通过UIImageView的`-startAnimating`, `-stopAnimating`  `-isAnimating` 三个方法控制
 
 
 当设备有足够的空闲内存时，这个view会及时的请求帧数据。为了降低CPU利用率,这个view可能会在内部的buffer中缓存部分或者所有的帧。
 buffer的大小是根据当前设备内存状态进行动态哦调整
 
 Sample Code:
 
     // ani@3x.gif
     YYImage *image = [YYImage imageNamed:@"ani"];
     YYAnimatedImageView *imageView = [YYAnimatedImageView alloc] initWithImage:image];
     [view addSubView:imageView];
 */
@interface YYAnimatedImageView : UIImageView

/**
 如果这个图片包含不止一帧,当这个属性被设置为YES的时候，那么当这个view可见的时候会自动开始不可见的时候自动停止
 
 默认是YES
 */
@property (nonatomic) BOOL autoPlayAnimatedImage;

/**
 当前展示的是第几帧
 
 设置这个属性会使YYAnimatedImageView立刻展示新的帧图片 如果新值非法这个操作没有影响
 
 你可以通过添加一个监听者来观察播放的状态
 */
@property (nonatomic) NSUInteger currentAnimatedImageIndex;

/**
 YYAnimatedImageView当前是否正在执行动画
 
  你可以通过添加一个监听者来观察播放的状态
 */
@property (nonatomic, readonly) BOOL currentIsPlayingAnimation;

/**
 这个动画定时器的runloopMode 默认是NSRunLoopCommonModes
 
 如果设置成NSDefaultRunLoopMode 那么当UIScrollView 滚动的时候YYAnimatedImageView动画会停止
 */
@property (nonatomic, copy) NSString *runloopMode;

/**
 inner frame buffer 的最大值 默认是0
 当设备有足够的空闲内存时,YYAnimatedImageView会请求解码部分或者所有的帧图到buffer中。如果这个值设置的是0
 那么buffersize的最大值会根据当前设备空闲内存的状态动态调整。否则buffer size会根据这个设定的值做限制
 

 当应用进入后台或者受到内存警告时,这个buffer会立刻被释放,然后在合适的时间恢复

 */
@property (nonatomic) NSUInteger maxBufferSize;

@end



/**
 YYAnimatedImage协议声明了使用YYAnimatedImageView展示动图的一些必要方法

 
 UIImage的子类实现了这个协议,那么这个子类的对象就可以通过设置YYAnimatedImageView.image或者
 YYAnimatedImageView.highlightedImage来展示动画


 See `YYImage` and `YYFrameImage` for example.
 */
@protocol YYAnimatedImage <NSObject>
@required
/// Total animated frame count.
/// It the frame count is less than 1, then the methods below will be ignored.
- (NSUInteger)animatedImageFrameCount;

/// Animation loop count, 0 means infinite looping.
- (NSUInteger)animatedImageLoopCount;

/// Bytes per frame (in memory). It may used to optimize memory buffer size.
- (NSUInteger)animatedImageBytesPerFrame;

/// Returns the frame image from a specified index.
/// This method may be called on background thread.
/// @param index  Frame index (zero based).
- (nullable UIImage *)animatedImageFrameAtIndex:(NSUInteger)index;

/// Returns the frames's duration from a specified index.
/// @param index  Frame index (zero based).
- (NSTimeInterval)animatedImageDurationAtIndex:(NSUInteger)index;

@optional
/// A rectangle in image coordinates defining the subrectangle of the image that
/// will be displayed. The rectangle should not outside the image's bounds.
/// It may used to display sprite animation with a single image (sprite sheet).
- (CGRect)animatedImageContentsRectAtIndex:(NSUInteger)index;
@end

NS_ASSUME_NONNULL_END
