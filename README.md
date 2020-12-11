# rkmedia_zbar_scanner



## 概述

rkmedia_zbar_scanner 是一个基于 rkmedia 和 zbar 构建的二维码/条形码扫描程序。目前在 rv1109/rv1126 平台测试 OK。它使用 rkmedia 从摄像头获取NV12格式的图片，并传给 zbar 处理，最终完成二维码/条形码的解析。



## 编译

可在 Rockchip RV1109/RV1126 SDK 的 buildroot 编译此程序。

1. 把程序源码放置到 SDK 的 external/ 目录。
2. 在 SDK 的 buildroot/package/rockchip 目录下创建 rkmedia_zbar_scanner目录。
3. 在 buildroot/package/rockchip/rkmedia_zbar_scanner 添加文件 Config.in。

```SHELL
config BR2_PACKAGE_RKMEDIA_ZBAR_SCANNER
	bool "rkmedia_zbar_scanner: qrcode scanner"
	select BR2_PACKAGE_RKMEDIA
	select BR2_PACKAGE_CAMERA_ENGINE_RKAIQ
	select BR2_PACKAGE_ISP2_IPC
	select BR2_PACKAGE_ZBAR
```

4. 在 buildroot/package/rockchip/rkmedia_zbar_scanner 添加文件 rkmedia_zbar_scanner.mk。

```makefile
RKMEDIA_ZBAR_SCANNER_SITE = $(TOPDIR)/../external/rkmedia_zbar_scanner
RKMEDIA_ZBAR_SCANNER_SITE_METHOD = local

RKMEDIA_ZBAR_SCANNER_DEPENDENCIES = rkmedia camera_engine_rkaiq zbar

$(eval $(cmake-package))
```

5. 修改 buildroot/package/rockchip/Config.in，添加：

```shell
source "package/rockchip/rkmedia_zbar_scanner/Config.in"
```

6. 在 buildroot 目录执行 `make menuconfig` 将此程序选上；或者直接在 defconfig 文件（ 如 buildroot/configs/rockchip_rv1126_rv1109_defconfig）将此 package 选上：

```shell
BR2_PACKAGE_RKMEDIA_ZBAR_SCANNER=y
```

7. 在 buildroot 目录执行 `make rkmedia_zbar_scanner` 编译程序，最终可执行文件被安装到 buildroot/output 的目标目录下。
8. 如果在 external/rkmedia_zbar_scanner 中修改完代码需要重新编译，可执行：

```shell
source envsetup.sh
cd buildroot
make rkmedia_zbar_scanner-dirclean && make rkmedia_zbar_scanner
```



## 运行

可执行文件最终被安装到系统的 /usr/bin 目录，重新打包根文件系统固件并烧录可在设备运行程序。

也可以直接从 buildroot/output 中将可执行文件复制出来，使用 adb 或者其他方法把 rkmedia_zbar_scanner 放置到设备上。

在运行程序之前可能需要先加上执行权限，如：

```shell
chmod +x rkmedia_zbar_scanner
```

参考以下命令执行程序，-a 选项指定 tunning file 的目录（通常摄像头sensor工作在HDR模式需要此选项）；-w, -h 选项分别指定缩放之后传给zbar的图像的宽和高，程序中默认为 640x360。

```shell
rkmedia_zbar_scanner -a "/oem/etc/iqfiles" -w 640 -h 360
```

如果不使用 -a 指定 iqfiles 目录，运行 rkmedia_zbar_scanner 需要将 ispserver 先运行起来， 可执行：

```shell
ispserver -no-sync-db &
```

