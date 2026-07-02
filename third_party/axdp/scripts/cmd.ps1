# # am10p测试，
# # -v  vid参数，10 or 16进制
# # -p  pid参数，10 or 16进制
# # -f  ota升级固件文件路径，英文，最好直接放本地
# # -s  升级策略，A20 填2， AMX100填7
# #-d  dst参数，表示移位数，14表示 1 << 14
# #-u  subindex，用一个字符串表示，程序内会直接用这个ascii码字符串来按位取数据
# #需要修改vid和pid参数，dfu升级固件文件路径，-s升级策略，-u subindex的值
#.\axdp_demo.exe -v 8137 -p 33387 -f a20.bin -s 7 -d 14 -u abcdef1234

# #For A20 test
.\axdp_demo.exe -v 8137 -p 33387 -f a20.bin -s 2 -d 2