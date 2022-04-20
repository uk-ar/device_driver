# device_driver
https://qiita.com/iwatake2222/items/1fdd2e0faaaa868a2db2
https://github.com/yutakakn/Books/tree/master/DeviceDriverIntroduction/chap3

add sample.h
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
```
