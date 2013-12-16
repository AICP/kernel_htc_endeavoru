--Please follow below command to download the official android toolchain: (arm-eabi-4.6)
        
                git clone https://android.googlesource.com/platform/prebuilt

                NOTE: the tool ¡¥git¡¦ will need to be installed first; for example, on Ubuntu, the installation command would be: apt-get install git

--Modify the .bashrc to add the toolchain path, like bellowing example:

								PATH=/usr/local/share/toolchain-eabi-4.6/bin:$PATH 

--Start 
$make ARCH=arm CROSS_COMPILE=/home/ssdpqm3/arm-eabi-4.6/bin/arm-eabi- ap33_android_defconfig
$make ARCH=arm CROSS_COMPILE=/home/ssdpqm3/arm-eabi-4.6/bin/arm-eabi- -j12
$make ARCH=arm CROSS_COMPILE=/home/ssdpqm3/arm-eabi-4.6/bin/arm-eabi- -C drivers/net/wireless/compat-wireless_R5.SP2.03 KLIB=`pwd` KLIB_BUILD=`pwd` clean -j20
$make ARCH=arm CROSS_COMPILE=/home/ssdpqm3/arm-eabi-4.6/bin/arm-eabi- -C drivers/net/wireless/compat-wireless_R5.SP2.03 KLIB=`pwd` KLIB_BUILD=`pwd` -j20

$TOP is an absolute path to android JB code base

 



--Clean
								$make clean

--Files path
> arch/arm/boot/zImage

> Please remember to update kernel modules also by the following commands before full function testing
adb remount
adb push ./crypto/tcrypt.ko system/lib/modules/
adb push ./drivers/misc/ti-st/gps_drv.ko system/lib/modules/
adb push ./drivers/misc/ti-st/st_drv.ko system/lib/modules/
adb push ./drivers/misc/ti-st/ti_hci_drv.ko system/lib/modules/
adb push ./drivers/scsi/scsi_wait_scan.ko system/lib/modules/
adb push ./drivers/bluetooth/btwilink.ko system/lib/modules/
adb push ./drivers/bluetooth/hci_uart.ko system/lib/modules/
adb push ./drivers/usb/serial/baseband_usb_chr.ko system/lib/modules/
adb push ./drivers/usb/class/cdc-acm.ko system/lib/modules/
adb push ./drivers/staging/ti-st/fm_drv.ko system/lib/modules/
adb push ./drivers/hid/hid-magicmouse.ko system/lib/modules/
adb push ./drivers/net/usb/raw_ip_net.ko system/lib/modules/
adb push ./drivers/net/wireless/compat-wireless_R5.SP2.03/compat/compat.ko system/lib/modules/
adb push ./drivers/net/wireless/compat-wireless_R5.SP2.03/drivers/net/wireless/wl12xx/wl12xx.ko system/lib/modules/
adb push ./drivers/net/wireless/compat-wireless_R5.SP2.03/drivers/net/wireless/wl12xx/wl12xx_sdio.ko system/lib/modules/
adb push ./drivers/net/wireless/compat-wireless_R5.SP2.03/net/mac80211/mac80211.ko system/lib/modules/
adb push ./drivers/net/wireless/compat-wireless_R5.SP2.03/net/wireless/cfg80211.ko system/lib/modules/
adb push ./drivers/net/kineto_gan.ko system/lib/modules/
adb push ./net/bluetooth/bnep/bnep.ko system/lib/modules/
adb push ./net/bluetooth/rfcomm/rfcomm.ko system/lib/modules/
adb push ./net/bluetooth/bluetooth.ko system/lib/modules/
adb push ./net/bluetooth/hidp/hidp.ko system/lib/modules/
adb push ./net/wireless/lib80211.ko system/lib/modules/
adb push ./arch/arm/mach-tegra/baseband-xmm-power2.ko system/lib/modules/
adb shell chmod 755 /system/lib/modules
adb shell "chmod 644 /system/lib/modules/*"
adb reboot