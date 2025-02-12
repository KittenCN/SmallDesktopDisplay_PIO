# SmallDesktopDisplay
# 基于[chuxin520922作品](https://github.com/chuxin520922/SmallDesktopDisplay)的修改，感谢原作者的无私奉献 
# 沙雕系列-SD -太空人天气时钟显示

**视频介绍：** https://www.bilibili.com/video/BV1V64y1e72M/

## MOD介绍：
1. 增加天干地支显示（需要注册天聚数行，并获取免费的key供模块使用）www.tianapi.com
2. 增加阴历显示
3. 修改AQI显示内容，调整AQI等级（参照美国标准）
4. 增加HTTPS链接模块
5. 增加Miku图片
6. 增加对于AQI为0，以及时间为00：00：00情况的处理
7. 调整天气，天干地支，时间获取的频率
8. 修改wifi使用的规则

## 1. 硬件打样说明

**PCB打样的话暂时没发现有啥需要特别注意的，注意附带的文件下单提示。** PCB文件可以直接拿去工厂打样，两层板很便宜，器件BOM的话也都是比较常用的。

`Hardware`文件内目前包含两个版本的PCB电路：

* **SD2-推荐桌面摆件** ：基于下面的版本轻微修改，将按键删去，无需按键，一键直达编译下载;新增了触摸扩展板，<code>IO口标识需要根据实际连接的改</code>
* **SD3-电脑主机显示** ：即视频中出现的版本，修改了PCB形状以适配新的外壳


**外壳加工** 根据自己喜欢的版本选择，`3D Model`文件夹目前包含2个版本的外壳文件：一 一 对 应文件即可


## 2. 固件编译说明

固件框架主要基于Arduino开发完成，玩过Arduino的基本没有上手难度了，把Firmware/Libraries里面的库安装到Arduino库目录（如果你用的是Arduino IDE的话）即可。

> 由于后续发现，使用的是VS code上面的PlatfromIO插件进行Arduino开发，因为VS code+PlatfromIO编译速度起飞，强烈推荐使用VS code+PlatfromIO来编译，大家可以选择自己喜欢的IDE就好了。
> 


**然后这里需要官方库文件才能正常使用：**

肯定得安装ESP83266的Arduino<code>**支持包2.6.3**</code>（百度也有海量教程，本文件已经附带安装包，直接安装即可）


**代码优化版本：VS code+PlatfromIO** https://github.com/SmallDesktopDisplay-team/SmallDesktopDisplay

## 其他的后续再补充，有用的话记得点星星~

