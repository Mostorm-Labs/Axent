###获取a20设备信息
- A20的 设备数量、单/多台心跳状态、版本检测、sn码均通过该命令获取
- A20的 speaker状态、mic状态暂不支持通过该sdk获取
```shell script
./hamedal_ctrl -info a20
```
- 示例
```json
{
    "A20":{
        "count":2,
        "list":[        
            {
                "index":0,
                "serial_number":"AW02YCA98G9D009864",
                "version":"1.3.0.2"
            },
            {
                "index":1,
                "serial_number":"MJ02A8A88BB8101158",
                "version":"1.11.2.2"
            }
        ]
    }
}
```

###获取v20设备信息
- V20的 设备数量、单台心跳状态、智能模式状态、mic使能状态、sn码均通过该命令获取
- V20的 画面校正、刷新率设置暂不支持通过该sdk获取
```shell script
./hamedal_ctrl -info v20
```
- 示例
```json
{
    "V20":{
        "count":1,
        "list":[
            {
                "index":0,
                "mic_enabled":0,    #智能模式 0表示已关闭状态，1表示开启状态，其他值表示获取该值出现错误
                "serial_number":"1234567890tsyatern",
                "smart_track_video_enabled":0,  #V20 mic开关，0表示已关闭状态，1表示开启状态，其他值表示获取该值出现错误
                "version":"V1.100.1.3.R.211008"
            }
        ]
    }
}
```

###关闭v20设备mic，返回值 0表示成功 -1表示失败
```shell script
./hamedal_ctrl -mic 0
```

###关闭v20设备自动追踪功能，返回值 0表示成功 -1表示失败
```shell script
./hamedal_ctrl -video 0
```

###开启v20设备自动追踪功能，返回值 0表示成功 -1表示失败
```shell script
./hameda_ctrl -video 1
```

###重启a20设备 ，返回值 0表示成功 -1表示失败
```shell script
./hamedal_ctrl -reboot a20
```

###重启v20设备 ，返回值 0表示成功，-1表示失败
```shell script
./hamedal_ctrl -reboot v20
```
