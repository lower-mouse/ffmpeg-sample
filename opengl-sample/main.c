#include <GL/glut.h>
#include <stdio.h>
void init();
void display();

int width = 896;
int height = 480;
unsigned char* buffer = NULL;

void timeDisplayFunc(int value){
    display();
    glutTimerFunc(40, timeDisplayFunc, 0);

}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowPosition(0, 0); // 用于设置窗口的位置。可以指定x，y坐标
    glutInitWindowSize(300, 300); //用于设置窗口的大小。可以设置窗口的宽，高。
    
    glutCreateWindow("OpenGL 3D View"); //创建窗口
    
    init();
    glutDisplayFunc(display); //设置绘制函数

    glutTimerFunc(40, timeDisplayFunc, 0);
    
    glutMainLoop(); //glutMainLoop()将会进入GLUT事件处理循环
    return 0;
}

void init()
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    buffer = (unsigned char*)malloc(width * height * 3);
    FILE* f = fopen("out1", "rb");
    if(f == NULL){
        printf("out1 open failed\n");
        return ;
    }

    int ret = fread(buffer, 1, width*height*3, f);
    if(ret != width * height * 3){
        printf("fread ret:%d\n", ret);
    }

    fclose(f);
    // glMatrixMode(GL_PROJECTION);
    // glOrtho(-5, 5, -5, 5, 5, 15);
    // glMatrixMode(GL_MODELVIEW);
    // gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
        
    glColor3f(1.0, 0, 0);
    glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);


    glFlush();
}