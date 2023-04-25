import os

# 输入：
# g_path： 需要导出GPS数据的MP4文件目录
# g_exec_path： extract_position 可执行文件路径

# 输出：
# gps 文件会输出到 g_path 目录下
g_path = '/media/intellif/Seagate Backup Plus Drive/video_2023_04_17'
g_exec_path = './build/extract_position'

def handle_dir(path):
    if not os.path.exists(path):
        print("please intput directory")
        exit

    for f in os.listdir(path):
        if(f.split('.')[-1] == "mp4"):
            print(f)
            src = os.path.join(path, f)
            # dst = os.path.join(path, f.split('.')[0] + "_event.txt")
            gps_dst = os.path.join(path, f.split('.')[0] + "_gps.txt")
            event_dst = os.path.join(path, f.split('.')[0] + "_event.txt")
            cmd=f"{g_exec_path} '{src}' -g '{gps_dst}' -e  '{event_dst}'"
            print("cmd:", cmd)
            res = os.system(cmd)
            print(res)
    

for f in os.listdir(g_path):
    path = os.path.join(g_path, f)
    if os.path.isdir(path):
        handle_dir(path)