# simple headless terminal

- 一个简易的无头终端，从标准输入读取数据，并在读取到 `\n` 时输出当前屏幕内容
- 标准输入关闭时，程序会结束
- 渲染屏幕内容借助了 [libvterm](https://www.leonerd.org.uk/code/libvterm/)
- 输出的屏幕内容会移除行尾空格

### 构建

```shell
git clone https://github.com/BreakTheMyth/simple-headless-terminal.git
cd simple-headless-terminal/
make
```

### 安装

```shell
sudo make install
```

### 卸载

```shell
sudo make uninstall
```

### 使用

```shell
# 使用命名管道连接程序标准输入
# mkfifo /tmp/myfifo
# sht < /tmp/myfifo
# exec 3> /tmp/myfifo
# 或直接
exec 3> >(sht -s 80x25 -o screen.txt --json iwctl)
#              |        |              |    |
#              |        |              |    启动 iwctl
#              |        |              +--- 对 \n \r 等字符进行转义，人类使用不要加
#              |        +------------------ 最新的内容会覆盖写入 screen.txt
#              +--------------------------- 终端尺寸 80 列 25 行

echo -n "device list\r" >&3 # 输入命令
sleep 1                     # 等待执行
echo >&3                    # 刷新屏幕
cat screen.txt              # 查看结果

echo -n "station wlan0 scan\r" >&3
echo -n "station wlan0 get-networks\r" >&3
sleep 1
echo >&3
cat screen.txt

echo -n "station wlan0 connect <SSID>\r" >&3
sleep 1
echo -n "<password>\r" >&3
sleep 1
echo >&3
cat screen.txt

# 输入 \u0004 (Ctrl + D) 或 exit\r 退出 iwctl, sht 结束
printf "%s%04x" '\u' $(expr $(printf "%d" "'D") - 64) >&3
# 或关闭输入, sht 结束
exec 3>&-
# 或直接 kill
ps -ef | grep sht | grep -v grep | awk '{print $2}' | xargs kill
# 或 killall
killall sht
```
