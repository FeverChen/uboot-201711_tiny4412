# uboot-201711_tiny4412

tiny4412 1312 SDK
the usb gadget enabled
dwmmc and sdhc are both enabled

build instructions:
make tiny4412_defconfig
makeboot

the final output file put at sd_fuse/tiny4412/bootimg
you can use superboot or other tool burn the file

ps. the dwmmc access speed still need to tune

