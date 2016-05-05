# pachila-iot-fireware

Pachila-iot-fireware是基于乐鑫SDK扩展的针对ESP8266 wifi模组(ESP01~ESPxx)的固件。该固件支持基于ESP8266wifi模组的灯泡，按钮，插座与温度传感器的演示设备。使用ESP8266wifi模组与本固件，再配合pachila mobile/PAAS/inBound的demo环境可以在很短的时间里面(甚至不到1个小时)搭建出手机APP通过云平台远程监视控制家电设备的演示环境。

##Quick Start
* 方式1：直接使用bin文件
  
根据xxx guide的xx里面的guide把bin文件烧录进ESP8266 wifi模组。

 把wifi模组与硬件设备的电路连接好，

** 通过手机APP把设备连接的wifi网络的用户名/密码设定好，

** 设备mac加入平台？

** 就可以用手机APP远程操纵物理设备了。

*平台2：根据产品(设备)的功能定义，扩展固件源程序，重新编译固件源程序
