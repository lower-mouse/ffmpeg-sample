### 依赖动态库
jsoncpp, opencv*, ffmpavformat avcodec swresample swscale avutil

### 安装动态库
jsoncpp: 
```
    sudo apt-get install libjsoncpp-dev
```

ffmepg 相关库
```
    sudo apt install libavformat-dev
    sudo apt install libavcodec-dev
    sudo apt install libswresample-dev
    sudo apt install libswscale-dev
    sudo apt install libavutil-dev
```

opencv
```
    export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:`pwd`/lib
```
### 运行
./mp4-player [filename]