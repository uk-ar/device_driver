# device_driver
https://qiita.com/iwatake2222/items/1fdd2e0faaaa868a2db2
https://github.com/yutakakn/Books/tree/master/DeviceDriverIntroduction/chap3
https://resources.oreilly.com/examples/9780596005900
https://github.com/torvalds/linux/blob/master/drivers/rtc/rtc-efi.c
https://linuxjf.osdn.jp/JFdocs/The-Linux-Kernel.html#toc9
https://www.kernel.org/doc/html/v5.17/dev-tools/kunit/start.html#installing-dependencies
https://www.hiroom2.com/2014/02/03/miscdevice%E3%81%A7linux%E3%83%87%E3%83%90%E3%82%A4%E3%82%B9%E3%83%89%E3%83%A9%E3%82%A4%E3%83%90%E3%82%92%E4%BD%9C%E6%88%90%E3%81%99%E3%82%8B/
```
$ sudo insmod hello.ko
$ ls -la /dev/samplehw*
crw-rw-rw- 1 root root 242, 0 Apr 19 08:13 /dev/samplehw0
crw------- 1 root root 242, 1 Apr 19 08:13 /dev/samplehw1
crw------- 1 root root 242, 2 Apr 19 08:13 /dev/samplehw2
$ echo "a" > /dev/samplehw0
$ dmesg
...
[  517.373548] sample_open entered
[  517.373559] sample_write entered
[  517.373561] sample_write entered
[  517.373562] sample_close entered
...
$ cp /dev/zero /dev/samplehw0
$ strace ls -l /dev/samplehw0
$ strace cat main.c > /dev/samplehw0
$ strace cat < /dev/samplehw0
```

```
> dmesg|grep rtc
rtc-efi rtc-efi: registered as rtc0
> sudo cat /proc/iomem
> cat /proc/interrupts
linux-menuconfig arm amba pl031 rtc
```
