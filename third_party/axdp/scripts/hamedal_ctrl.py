import os
import time



#获取a20设备信息
#A20的 设备数量、单/多台心跳状态、版本检测、sn码均通过该命令获取
#A20的 speaker状态、mic状态暂不支持通过该sdk获取
cmd_get_a20_info = "./hamedal_ctrl -info a20"

#获取v20设备信息
#V20的 设备数量、单台心跳状态、智能模式状态、mic使能状态、sn码均通过该命令获取
#V20的 画面校正、刷新率设置暂不支持通过该sdk获取
cmd_get_v20_info = "./hamedal_ctrl -info v20"

#关闭v20设备mic，返回值 0表示成功 -1表示失败
cmd_set_v20_mic_off = "./hamedal_ctrl -mic 0"

#关闭v20设备自动追踪功能，返回值 0表示成功 -1表示失败
cmd_set_v20_smart_video_off = "./hamedal_ctrl -video 0"

#开启v20设备自动追踪功能，返回值 0表示成功 -1表示失败
cmd_set_v20_smart_video_on = "./hameda_ctrl -video 1"

#重启a20设备 ，返回值 0表示成功 -1表示失败
cmd_reboot_a20 = "./hamedal_ctrl -reboot a20"

#重启v20设备 ，返回值 0表示成功，-1表示失败
cmd_reboot_v20 = "./hamedal_ctrl -reboot v20"

while True:
    f= os.popen(cmd_get_a20_info)
    a20_data = f.readlines()
    print(a20_data)
    f= os.popen(cmd_get_v20_info)
    v20_data = f.readlines()
    print(v20_data)
    time.sleep(3)



