import os

path = "/home/intellif/work/ffmpeg-sample/2023-04-13"

if not os.path.exists(path):
    print("please intput directory")
    exit

for f in os.listdir(path):
    if(f.split('.')[-1] == "mp4"):
        print(f)
        src = os.path.join(path, f)
        dst = os.path.join(path, f.split('.')[0] + ".h264")
        if os.path.exists(dst):
            continue
        
        cmd=f"./mp4-display/build/mp4-player -o {dst} {src} -k hand_trash_bag -k trashbox"
        print("cmd:", cmd)
        res = os.system(cmd)
        print(res)
    