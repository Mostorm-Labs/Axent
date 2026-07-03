#生成签名证数
makecert -sv axdp.pvk -r axdp.cer -n "CN=AUDITORYWORKS"
#创建发行者证书
cert2spc axdp.cer axdp.spc
#从pvk文件中导出pfx文件
pvk2pfx -pvk axdp.pvk -pi Disc1234 -spc axdp.spc -pfx axdp.pfx -f
#签名
signtool sign /f axdp.pfx /p Disc1234 axdp.dll