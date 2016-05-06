# Pachila IoT Fireware

<a href="http://www.pachila.cn"><img src="https://github.com/pachila-org/pachila-iot-mobile/blob/master/www/images/icon.png" align="left" hspace="4" vspace="4"></a>

Pachila-iot-fireware是基于乐鑫SDK扩展的针对ESP8266 wifi模组(ESP01~ESP12)的固件。该固件支持基于ESP8266wifi模组的灯泡，按钮，插座与温度传感器的演示设备。使用ESP8266wifi模组与本固件，再配合pachila mobile/PAAS/inBound的demo环境可以在很短的时间里面(甚至不到1个小时)搭建出手机APP通过云平台远程监视控制家电设备的演示环境。

##Quick Start
* 方式1：直接使用bin文件(针对希望快速搭建物联网应用，远程操控设备的需求)

  A. 根据[Getting Started Guide](http://www.espressif.com/support/explore/get-started/esp8266/getting-started-guide)的“3. Download Binaries to Flash”里面的说明把bin文件烧录进ESP8266 wifi模组

  B. 连接wifi模组与硬件设备的电路

  C. 通过[pachila-iot-mobile](https://github.com/pachila-org/pachila-iot-mobile)提供的手机APP设定设备需要连接的wifi网络的用户名/密码

  D. 通过[Pachila-iot-paas](http://120.27.4.46/iotpass/admin.php)"设备监控"->"新增MAC地址"页面把设备MAC与SN等级PAAS平台(用于安全验证)

  E. 使用通过[Pachila-iot-mobile](https://github.com/pachila-org/pachila-iot-mobile)的手机APP远程操纵物理设备(手机APP需要用StepD的用户名和密码)

*平台2：根据产品(设备)的功能定义，扩展固件源程序，重新编译固件源程序(针对熟悉C语言和linux希望开发新产品的需求)


## Supports

* [Homepage](http://www.pachila.cn)
* [Community](http://www.pachila.cn/)
* [User Guide](http://www.pachila.cn/)
* [Deployment Guide](http://www.pachila.cn/)
* [Development Guide](http://www.pachila.cn/)
* [Downloads]((http://www.pachila.cn/))
* [Mailing List](sicon@pachila.cn)
* [Issues](https://github.com/pachila-org/pachila-iot-mobile/issues)

##Contributors

* [@Pachilatopgun](https://github.com/pachilatopgun)
* [@sicon](https://github.com/sicon)
* [@boboking](https://github.com/boboking)
* [@howard](https://github.com/howard)
* [@microlyu](https://github.com/microlyu)
* [@ryle](https://github.com/ryle)

##Author

[Pachila org](https://github.com/pachila-org)

## License

Apache License Version 2.0

