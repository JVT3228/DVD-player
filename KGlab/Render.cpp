#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "Texture.h"


#include "ObjLoader.h"


#include "debout.h"

#include <cmath>

//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
extern Camera camera;

// --- Глобальные объекты для подписей кнопок и надписи DVD ---
static GuiTextRectangle btnOpenText;
static GuiTextRectangle btnPlayText;
static GuiTextRectangle btnStopText;
static GuiTextRectangle dvdLabelText;
static bool guiRectsInited = false;

bool texturing = true;
bool lightning = true;
bool alpha = false;

//void DrawPlasmaScreen();
//void DrawButtonHighlight(float x, float y, float z, bool active, bool isPower);

//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
    //конвертируем код клавиши в букву
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

    switch (key)
    {
    case 'L':
        lightning = !lightning;
        break;
    case 'T':
        texturing = !texturing;
        break;
    case 'A':
        alpha = !alpha;
        break;
    }
}

//умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template<typename T, int M1, int N1, int M2, int N2>
void MatrixMultiply(const T* a, const T* b, T* c)
{
    for (int i = 0; i < M1; ++i)
    {
        for (int j = 0; j < N2; ++j)
        {
            c[i * N2 + j] = T(0);
            for (int k = 0; k < N1; ++k)
            {
                c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
            }
        }
    }
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером

ObjModel f;


Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;

Texture stankin_tex, vb_tex, monkey_tex;



void initRender()
{

    cassini_sh.VshaderFileName = "shaders/v.vert";
    cassini_sh.FshaderFileName = "shaders/cassini.frag";
    cassini_sh.LoadShaderFromFile();
    cassini_sh.Compile();

    phong_sh.VshaderFileName = "shaders/v.vert";
    phong_sh.FshaderFileName = "shaders/light.frag";
    phong_sh.LoadShaderFromFile();
    phong_sh.Compile();

    vb_sh.VshaderFileName = "shaders/v.vert";
    vb_sh.FshaderFileName = "shaders/vb.frag";
    vb_sh.LoadShaderFromFile();
    vb_sh.Compile();

    simple_texture_sh.VshaderFileName = "shaders/v.vert";
    simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
    simple_texture_sh.LoadShaderFromFile();
    simple_texture_sh.Compile();

    stankin_tex.LoadTexture("textures/stankin.png");
    vb_tex.LoadTexture("textures/vb.png");
    monkey_tex.LoadTexture("textures/monkey.png");


    f.LoadModel("models//monkey.obj_m");
    //==============НАСТРОЙКА ТЕКСТУР================
    //4 байта на хранение пикселя
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);



    //================НАСТРОЙКА КАМЕРЫ======================
    camera.caclulateCameraPos();

    //привязываем камеру к событиям "движка"
    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
    //==============НАСТРОЙКА СВЕТА===========================
    //привязываем свет к событиям "движка"
    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
    //========================================================
    //====================Прочее==============================
    gl.KeyDownEvent.reaction(switchModes);
    text.setSize(512, 180);
    //========================================================


    camera.setPosition(0, 2, 2);

}
float view_matrix[16];
double full_time = 0;
int location = 0;

// --- Состояние лотка ---
float trayPos = 0.0f; // 0 — лоток внутри, 1 — лоток снаружи
bool trayMoving = false;
float trayTarget = 0.0f;

// === DVD PLAYER STATE ===
enum DVDState { DVD_OFF, DVD_PLAY, DVD_PAUSE, DVD_STOP };
DVDState dvdState = DVD_OFF;
bool powerOn = false;
int currentSlide = 0;

// --- Обработка клавиши 4 (Eject) ---
void HandleTrayKey() {
    if (!trayMoving) {
        trayTarget = (trayPos < 0.05f) ? 0.424f : 0.0f; // если лоток внутри — выдвинуть, если снаружи — задвинуть
        trayMoving = true;
        if (trayPos < 0.05f) {
            dvdState = DVD_STOP;
            currentSlide = 0;
        }
    }
}

// --- Диск (DVD) ---
void DrawDisk(float angle) {
    glPushMatrix();
    // Центрируем диск на площадке лотка
    glTranslatef(0.0f, -0.25f, 0.001f); // чуть выше площадки
    glRotatef(angle, 0, 0, 1); // вращение
    glColor3f(0.85f, 0.85f, 0.9f); // светло-серый
    float R = 0.16f; // радиус диска
    int N = 64;
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= N; ++i) {
        float phi = 2.0f * 3.1415926f * i / N;
        glVertex3f(R * cosf(phi), R * sinf(phi), 0);
    }
    glEnd();
    // Внутреннее отверстие
    glColor3f(0.5f, 0.5f, 0.6f);
    float r = 0.03f;
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0.001f);
    for (int i = 0; i <= N; ++i) {
        float phi = 2.0f * 3.1415926f * i / N;
        glVertex3f(r * cosf(phi), r * sinf(phi), 0.001f);
    }
    glEnd();
    glPopMatrix();
}

// --- Лоток DVD-плеера (вертикальная панель + горизонтальная площадка для диска) ---
void DrawTray(float pos) {
    glPushMatrix();
    // Смещение лотка по +Y (вверх из корпуса)
    glTranslatef(0.0f, 0.38f + 0.6f * pos, 0.3f);
    // Горизонтальная площадка для диска (квадрат)
    glPushMatrix();
    glTranslatef(0.0f, -0.08f, 0.0f);
    glRotatef(90, 1, 0, 0);
    glColor3f(0.5f, 0.5f, 0.55f);
    glScalef(0.4f, 0.1f, 0.01f);
    float h = 0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-h, -h, h); glVertex3f(h, -h, h); glVertex3f(h, h, h); glVertex3f(-h, h, h);
    glNormal3f(0, 0, -1);
    glVertex3f(-h, -h, -h); glVertex3f(-h, h, -h); glVertex3f(h, h, -h); glVertex3f(h, -h, -h);
    glNormal3f(-1, 0, 0);
    glVertex3f(-h, -h, -h); glVertex3f(-h, -h, h); glVertex3f(-h, h, h); glVertex3f(-h, h, -h);
    glNormal3f(1, 0, 0);
    glVertex3f(h, -h, -h); glVertex3f(h, h, -h); glVertex3f(h, h, h); glVertex3f(h, -h, h);
    glNormal3f(0, 1, 0);
    glVertex3f(-h, h, -h); glVertex3f(-h, h, h); glVertex3f(h, h, h); glVertex3f(h, h, -h);
    glNormal3f(0, -1, 0);
    glVertex3f(-h, -h, -h); glVertex3f(h, -h, -h); glVertex3f(h, -h, h); glVertex3f(-h, -h, h);
    glEnd();
    glPopMatrix();
    // --- Диск ---
    extern float diskAngle;
    glPushMatrix();
    DrawDisk((pos < 0.05f) ? diskAngle : 0.0f);
    glPopMatrix();
    // Вертикальная панель лотка (фасад)
    glPushMatrix();
    glTranslatef(0.0f, -0.205f - 0.025f, -0.01f);
    glColor3f(0.4f, 0.4f, 0.45f);
    glScalef(0.39f, 0.305f, 0.02f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-h, -h, h); glVertex3f(h, -h, h); glVertex3f(h, h, h); glVertex3f(-h, h, h);
    glNormal3f(0, 0, -1);
    glVertex3f(-h, -h, -h); glVertex3f(-h, h, -h); glVertex3f(h, h, -h); glVertex3f(h, -h, -h);
    glNormal3f(-1, 0, 0);
    glVertex3f(-h, -h, -h); glVertex3f(-h, -h, h); glVertex3f(-h, h, h); glVertex3f(-h, h, -h);
    glNormal3f(1, 0, 0);
    glVertex3f(h, -h, -h); glVertex3f(h, h, -h); glVertex3f(h, h, h); glVertex3f(h, -h, h);
    glNormal3f(0, 1, 0);
    glVertex3f(-h, h, -h); glVertex3f(-h, h, h); glVertex3f(h, h, h); glVertex3f(h, h, -h);
    glNormal3f(0, -1, 0);
    glVertex3f(-h, -h, -h); glVertex3f(h, -h, -h); glVertex3f(h, -h, h); glVertex3f(-h, -h, h);
    glEnd();
    glPopMatrix();
    glPopMatrix();
}

// --- Константа для смещения подписи над кнопками ---
const float yLabelOffset = 0.012f;

// --- Корпус DVD-плеера с отверстием под лоток снизу ---
void DrawDVDCase() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.3f); // только по Z, без смещения по Y
    glRotatef(-90, 1, 0, 0);
    glColor3f(0.2f, 0.2f, 0.25f);
    glScalef(1.5f, 0.3f, 0.6f);
    float h = 0.5f;
    // --- Боковые и задняя/верхняя/нижняя панели ---
    glBegin(GL_QUADS);
    // Левая
    glNormal3f(-1, 0, 0);
    glVertex3f(-h, -h, -h); glVertex3f(-h, -h, h); glVertex3f(-h, h, h); glVertex3f(-h, h, -h);
    // Правая
    glNormal3f(1, 0, 0);
    glVertex3f(h, -h, -h); glVertex3f(h, h, -h); glVertex3f(h, h, h); glVertex3f(h, -h, h);
    // Верх
    glNormal3f(0, 1, 0);
    glVertex3f(-h, h, -h); glVertex3f(-h, h, h); glVertex3f(h, h, h); glVertex3f(h, h, -h);
    // Низ
    glNormal3f(0, -1, 0);
    glVertex3f(-h, -h, -h); glVertex3f(h, -h, -h); glVertex3f(h, -h, h); glVertex3f(-h, -h, h);
    // Задняя
    glNormal3f(0, 0, -1);
    glVertex3f(-h, -h, -h); glVertex3f(-h, h, -h); glVertex3f(h, h, -h); glVertex3f(h, -h, -h);
    glEnd();
    // --- Передняя панель с отверстием под лоток снизу ---
    glBegin(GL_QUADS);
    // Левая часть передней панели
    glNormal3f(0, 0, 1);
    glVertex3f(-h, -h, h); glVertex3f(-0.13f, -h, h); glVertex3f(-0.13f, h, h); glVertex3f(-h, h, h);
    // Правая часть передней панели
    glNormal3f(0, 0, 1);
    glVertex3f(0.13f, -h, h); glVertex3f(h, -h, h); glVertex3f(h, h, h); glVertex3f(0.13f, h, h);
    // Верхняя перемычка
    glNormal3f(0, 0, 1);
    glVertex3f(-0.13f, 0.08f, h); glVertex3f(0.13f, 0.08f, h); glVertex3f(0.13f, h, h); glVertex3f(-0.13f, h, h);
    // Нижняя перемычка (оставляем больше места для лотка)
    glNormal3f(0, 0, 1);
    glVertex3f(-0.13f, -h, h); glVertex3f(0.13f, -h, h); glVertex3f(0.13f, -0.08f, h); glVertex3f(-0.13f, -0.08f, h);
    glEnd();
    // --- Цветная рамка вокруг отверстия под лоток ---
    glColor3f(0.1f, 0.4f, 0.8f); // синий цвет рамки
    float frameW = 0.01f; // толщина рамки
    float xL = -0.13f, xR = 0.13f, yB = -0.08f, yT = 0.08f;
    // Левая вертикальная рамка
    glBegin(GL_QUADS);
    glVertex3f(xL - frameW, yB - frameW, h + 0.001f);
    glVertex3f(xL, yB - frameW, h + 0.001f);
    glVertex3f(xL, yT + frameW, h + 0.001f);
    glVertex3f(xL - frameW, yT + frameW, h + 0.001f);
    glEnd();
    // Правая вертикальная рамка
    glBegin(GL_QUADS);
    glVertex3f(xR, yB - frameW, h + 0.001f);
    glVertex3f(xR + frameW, yB - frameW, h + 0.001f);
    glVertex3f(xR + frameW, yT + frameW, h + 0.001f);
    glVertex3f(xR, yT + frameW, h + 0.001f);
    glEnd();
    // Верхняя горизонтальная рамка
    glBegin(GL_QUADS);
    glVertex3f(xL - frameW, yT, h + 0.001f);
    glVertex3f(xR + frameW, yT, h + 0.001f);
    glVertex3f(xR + frameW, yT + frameW, h + 0.001f);
    glVertex3f(xL - frameW, yT + frameW, h + 0.001f);
    glEnd();
    // Нижняя горизонтальная рамка
    glBegin(GL_QUADS);
    glVertex3f(xL - frameW, yB - frameW, h + 0.001f);
    glVertex3f(xR + frameW, yB - frameW, h + 0.001f);
    glVertex3f(xR + frameW, yB, h + 0.001f);
    glVertex3f(xL - frameW, yB, h + 0.001f);
    glEnd();
    glColor3f(0.2f, 0.2f, 0.25f); // вернуть цвет корпуса
    // --- КНОПКИ на передней панели (без подписей) ---
    float btnSize = 0.05f;
    float btnZ = h + 0.002f;
    float yBtn = -0.19f;
    float xPanelLeft = -0.48f;
    float btnGap = 0.018f; // чуть больше промежуток
    // OPEN/CLOSE
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(xPanelLeft, yBtn, btnZ);
    glVertex3f(xPanelLeft + btnSize, yBtn, btnZ);
    glVertex3f(xPanelLeft + btnSize, yBtn + btnSize, btnZ);
    glVertex3f(xPanelLeft, yBtn + btnSize, btnZ);
    glEnd();
    // Чёрная рамка
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(xPanelLeft, yBtn, btnZ + 0.0001f);
    glVertex3f(xPanelLeft + btnSize, yBtn, btnZ + 0.0001f);
    glVertex3f(xPanelLeft + btnSize, yBtn + btnSize, btnZ + 0.0001f);
    glVertex3f(xPanelLeft, yBtn + btnSize, btnZ + 0.0001f);
    glEnd();
    // PLAY/PAUSE
    float x2 = xPanelLeft + btnSize + btnGap;
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(x2, yBtn, btnZ);
    glVertex3f(x2 + btnSize, yBtn, btnZ);
    glVertex3f(x2 + btnSize, yBtn + btnSize, btnZ);
    glVertex3f(x2, yBtn + btnSize, btnZ);
    glEnd();
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(x2, yBtn, btnZ + 0.0001f);
    glVertex3f(x2 + btnSize, yBtn, btnZ + 0.0001f);
    glVertex3f(x2 + btnSize, yBtn + btnSize, btnZ + 0.0001f);
    glVertex3f(x2, yBtn + btnSize, btnZ + 0.0001f);
    glEnd();
    // STOP
    float x3 = x2 + btnSize + btnGap;
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(x3, yBtn, btnZ);
    glVertex3f(x3 + btnSize, yBtn, btnZ);
    glVertex3f(x3 + btnSize, yBtn + btnSize, btnZ);
    glVertex3f(x3, yBtn + btnSize, btnZ);
    glEnd();
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(x3, yBtn, btnZ + 0.0001f);
    glVertex3f(x3 + btnSize, yBtn, btnZ + 0.0001f);
    glVertex3f(x3 + btnSize, yBtn + btnSize, btnZ + 0.0001f);
    glVertex3f(x3, yBtn + btnSize, btnZ + 0.0001f);
    glEnd();
    // POWER (красная круглая кнопка)
    float x4 = x3 + btnSize + btnGap + 0.01f;
    float y4 = yBtn + 0.005f;
    float r4 = 0.028f;
    glColor3f(0.8f, 0.1f, 0.1f); // красная
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(x4 + 0.025f, y4 + 0.025f, btnZ + 0.0002f);
    for (int i = 0; i <= 32; ++i) {
        float t = 2.0f * 3.1415926f * i / 32;
        glVertex3f(x4 + 0.025f + r4 * cosf(t), y4 + 0.025f + r4 * sinf(t), btnZ + 0.0002f);
    }
    glEnd();
    glColor3f(0.2f, 0.0f, 0.0f); // тёмная окантовка
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; ++i) {
        float t = 2.0f * 3.1415926f * i / 32;
        glVertex3f(x4 + 0.025f + r4 * cosf(t), y4 + 0.025f + r4 * sinf(t), btnZ + 0.0003f);
    }
    glEnd();
    // --- Подписи к кнопкам (маленькие, над кнопками) ---
    if (!guiRectsInited) {
        btnOpenText.setSize(90, 40); btnOpenText.setText(L"⏏"); // Eject
        btnPlayText.setSize(90, 40); btnPlayText.setText(L"▶/⏸"); // Play/Pause
        btnStopText.setSize(90, 40); btnStopText.setText(L"■"); // Stop
        dvdLabelText.setSize(140, 48); dvdLabelText.setText(L"DVD");
        // POWER
        static GuiTextRectangle btnPowerText;
        btnPowerText.setSize(90, 40); btnPowerText.setText(L"⏻");
    }
    glEnable(GL_TEXTURE_2D);
    // OPEN/CLOSE символ
    glPushMatrix();
    glTranslatef(xPanelLeft + 0.036f, yBtn + btnSize + yLabelOffset, btnZ + 0.001f);
    glRotatef(180, 0, 0, 1);
    glScalef(0.001f, 0.001f, 1.0f);
    btnOpenText.Draw();
    glPopMatrix();
    // PLAY/PAUSE символ
    glPushMatrix();
    glTranslatef(xPanelLeft + btnSize + 0.06f, yBtn + btnSize + yLabelOffset, btnZ + 0.001f);
    glRotatef(180, 0, 0, 1);
    glScalef(0.001f, 0.001f, 1.0f);
    btnPlayText.Draw();
    glPopMatrix();
    // STOP символ
    glPushMatrix();
    glTranslatef(xPanelLeft + 2 * btnSize + 0.066f, yBtn + btnSize + yLabelOffset, btnZ + 0.001f);
    glRotatef(180, 0, 0, 1);
    glScalef(0.001f, 0.001f, 1.0f);
    btnStopText.Draw();
    glPopMatrix();
    // POWER символ (удалён 3D текст)
    // static GuiTextRectangle btnPowerText;
    // glPushMatrix();
    // glTranslatef(x4 + 0.018f, y4 + btnSize + yLabelOffset, btnZ + 0.001f);
    // glRotatef(180, 0, 0, 1);
    // glScalef(0.001f, 0.001f, 1.0f);
    // btnPowerText.Draw();
    // glPopMatrix();
    // DVD декоративная надпись по центру панели
    glPushMatrix();
    glTranslatef(-0.05f, 0.35f, btnZ);
    glRotatef(180, 0, 0, 1);
    glScalef(0.003f, 0.003f, 1.0f);
    dvdLabelText.Draw();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);

    //// --- Индикаторы нажатия кнопок ---
    //DrawButtonHighlight(xPanelLeft, yBtn, btnZ, gl.isKeyPressed('4'), false);
    //DrawButtonHighlight(x2, yBtn, btnZ, gl.isKeyPressed('2'), false);
    //DrawButtonHighlight(x3, yBtn, btnZ, gl.isKeyPressed('3'), false);
    //DrawButtonHighlight(x4, y4, btnZ, gl.isKeyPressed('1'), true); // POWER — красный индикатор
    glPopMatrix();
}

// --- Внутренний чёрный короб (депо для лотка) ---
void DrawTrayDepot() {
    glPushMatrix();
    // Депо чуть меньше корпуса, начинается сразу за отверстием и уходит внутрь
    glTranslatef(0.0f, 0.1f, 0.3f);
    glRotatef(-90, 1, 0, 0);
    glColor3f(0.05f, 0.05f, 0.05f); // почти чёрный
    glScalef(0.4f, 0.05f, 0.4f); // ширина и высота чуть меньше отверстия, глубина — почти вся глубина корпуса
    float h_depot = 0.5f;
    glBegin(GL_QUADS);
    // Левая
    glNormal3f(-1, 0, 0);
    glVertex3f(-h_depot, -h_depot, -h_depot); glVertex3f(-h_depot, -h_depot, h_depot); glVertex3f(-h_depot, h_depot, h_depot); glVertex3f(-h_depot, h_depot, -h_depot);
    // Правая
    glNormal3f(1, 0, 0);
    glVertex3f(h_depot, -h_depot, -h_depot); glVertex3f(h_depot, h_depot, -h_depot); glVertex3f(h_depot, h_depot, h_depot); glVertex3f(h_depot, -h_depot, h_depot);
    // Верх
    glNormal3f(0, 1, 0);
    glVertex3f(-h_depot, h_depot, -h_depot); glVertex3f(-h_depot, h_depot, h_depot); glVertex3f(h_depot, h_depot, h_depot); glVertex3f(h_depot, h_depot, -h_depot);
    // Низ
    glNormal3f(0, -1, 0);
    glVertex3f(-h_depot, -h_depot, -h_depot); glVertex3f(h_depot, -h_depot, -h_depot); glVertex3f(h_depot, -h_depot, h_depot); glVertex3f(-h_depot, -h_depot, h_depot);
    // Задняя
    glNormal3f(0, 0, -1);
    glVertex3f(-h_depot, -h_depot, -h_depot); glVertex3f(-h_depot, h_depot, -h_depot); glVertex3f(h_depot, h_depot, -h_depot); glVertex3f(h_depot, -h_depot, -h_depot);
    // Передняя (не рисуем, чтобы был виден вход)
    glEnd();
    glPopMatrix();
}

float diskAngle = 0.0f;

void Render(double delta_time)
{
    full_time += delta_time;
    if (gl.isKeyPressed('F'))
        light.SetPosition(camera.x(), camera.y(), camera.z());
    if (gl.isKeyPressed('4')) HandleTrayKey();
    // Анимация лотка
    if (trayMoving) {
        float speed = 1.5f * (float)delta_time;
        if (trayPos < trayTarget) {
            trayPos += speed;
            if (trayPos > trayTarget) trayPos = trayTarget;
        }
        else if (trayPos > trayTarget) {
            trayPos -= speed;
            if (trayPos < trayTarget) trayPos = trayTarget;
        }
        if (fabs(trayPos - trayTarget) < 0.01f) {
            trayPos = trayTarget;
            trayMoving = false;
        }
    }
    // --- Анимация вращения диска ---
    if (trayPos < 0.05f) {
        diskAngle += 360.0f * (float)delta_time; // 1 оборот в секунду
        if (diskAngle > 360.0f) diskAngle -= 360.0f;
    }
    camera.SetUpCamera();
    light.SetUpLight();
    DrawDVDCase();
    DrawTrayDepot();
    DrawTray(trayPos);
    //DrawPlasmaScreen(); // рисуем экран



    //рисуем оси
    gl.DrawAxes();



    glBindTexture(GL_TEXTURE_2D, 0);

    //включаем нормализацию нормалей
    //чтобв glScaled не влияли на них.

    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    //включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
    if (lightning)
        glEnable(GL_LIGHTING);
    if (texturing)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
    }

    if (alpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //=============НАСТРОЙКА МАТЕРИАЛА==============


    //настройка материала, все что рисуется ниже будет иметь этот метериал.
    //массивы с настройками материала
    float  amb[] = { 0.2f, 0.2f, 0.1f, 1.0f };
    float dif[] = { 0.4f, 0.65f, 0.5f, 1.0f };
    float spec[] = { 0.9f, 0.8f, 0.3f, 1.0f };
    float sh = 0.2f * 256;

    //фоновая
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    //дифузная
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    //зеркальная
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    //размер блика
    glMaterialf(GL_FRONT, GL_SHININESS, sh);

    //чоб было красиво, без квадратиков (сглаживание освещения)
    glShadeModel(GL_SMOOTH); //закраска по Гуро      
    //(GL_SMOOTH - плоская закраска)

//============ РИСОВАТЬ ТУТ ==============



//квадратик станкина



//рисуем квадратик с овалом Кассини!

    cassini_sh.UseShader();

    location = glGetUniformLocationARB(cassini_sh.program, "Time");
    glUniform1fARB(location, full_time);
    location = glGetUniformLocationARB(cassini_sh.program, "size");
    glUniform2fARB(location, 100, 100);

    glPushMatrix();

    glTranslated(0, -1.2, 0);


    glBegin(GL_QUADS);
    glNormal3d(0, 0, 1);
    glTexCoord2d(1, 1);
    glVertex3d(0.5, 0.5, 0);
    glTexCoord2d(1, 0);
    glVertex3d(0.5, -0.5, 0);
    glTexCoord2d(0, 0);
    glVertex3d(-0.5, -0.5, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-0.5, 0.5, 0);
    glEnd();


    glPopMatrix();







    //Квадратик с освещением
    phong_sh.UseShader();

    float light_pos[4] = { light.x(),light.y(), light.z(), 1 };
    float light_pos_v[4];

    //переносим координаты света в видовые координаты
    MatrixMultiply<float, 1, 4, 4, 4>(light_pos, view_matrix, light_pos_v);


    location = glGetUniformLocationARB(phong_sh.program, "Ia");
    glUniform3fARB(location, 1, 1, 1);
    location = glGetUniformLocationARB(phong_sh.program, "Id");
    glUniform3fARB(location, 1, 1, 1);
    location = glGetUniformLocationARB(phong_sh.program, "Is");
    glUniform3fARB(location, 1, 1, 1);

    location = glGetUniformLocationARB(phong_sh.program, "ma");
    glUniform3fARB(location, 0.1, 0.1, 0.1);
    location = glGetUniformLocationARB(phong_sh.program, "md");
    glUniform3fARB(location, 0.6, 0.6, 0.6);
    location = glGetUniformLocationARB(phong_sh.program, "ms");
    glUniform4fARB(location, 0, 1, 0, 300);


    location = glGetUniformLocationARB(phong_sh.program, "light_pos_v");
    glUniform3fvARB(location, 1, light_pos_v);

    glPushMatrix();

    glTranslated(0, 0, 0);


    glBegin(GL_QUADS);
    glNormal3d(0, 0, 1);
    glTexCoord2d(1, 1);
    glVertex3d(0.5, 0.5, 0);
    glTexCoord2d(1, 0);
    glVertex3d(0.5, -0.5, 0);
    glTexCoord2d(0, 0);
    glVertex3d(-0.5, -0.5, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-0.5, 0.5, 0);
    glEnd();


    glPopMatrix();



    //Квадратник без освещения

    Shader::DontUseShaders();

    glBindTexture(GL_TEXTURE_2D, 0);

    glPushMatrix();

    glTranslated(1.2, 0, 0);


    glBegin(GL_QUADS);
    glNormal3d(0, 0, 1);
    glTexCoord2d(1, 1);
    glVertex3d(0.5, 0.5, 0);
    glTexCoord2d(1, 0);
    glVertex3d(0.5, -0.5, 0);
    glTexCoord2d(0, 0);
    glVertex3d(-0.5, -0.5, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-0.5, 0.5, 0);
    glEnd();


    glPopMatrix();


    //квадратик с ВБ


    vb_sh.UseShader();

    glActiveTexture(GL_TEXTURE0);
    stankin_tex.Bind();
    glActiveTexture(GL_TEXTURE1);
    vb_tex.Bind();

    location = glGetUniformLocationARB(vb_sh.program, "time");
    glUniform1fARB(location, full_time);
    location = glGetUniformLocationARB(vb_sh.program, "tex_stankin");
    glUniform1iARB(location, 0);
    location = glGetUniformLocationARB(vb_sh.program, "tex_vb");
    glUniform1iARB(location, 1);

    glPushMatrix();

    glTranslated(0, 1.2, 0);
    glBegin(GL_QUADS);
    glNormal3d(0, 0, 1);
    glTexCoord2d(1, 1);
    glVertex3d(0.5, 0.5, 0);
    glTexCoord2d(1, 0);
    glVertex3d(0.5, -0.5, 0);
    glTexCoord2d(0, 0);
    glVertex3d(-0.5, -0.5, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-0.5, 0.5, 0);
    glEnd();


    glPopMatrix();


    //обезьянка без шейдеров
    glPushMatrix();
    Shader::DontUseShaders();
    glActiveTexture(GL_TEXTURE0);
    monkey_tex.Bind();
    glShadeModel(GL_SMOOTH);
    glTranslated(-1, 0, 0.5);
    glScaled(0.1, 0.1, 0.1);
    glRotated(180, 0, 0, 1);
    f.Draw();
    glPopMatrix();

    //обезьянка с шейдерами


    simple_texture_sh.UseShader();
    location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
    glUniform1iARB(location, 0);
    glActiveTexture(GL_TEXTURE0);
    monkey_tex.Bind();


    glPushMatrix();
    glTranslated(-1, 1, 0.5);
    glScaled(0.1, 0.1, 0.1);
    glRotated(180, 0, 0, 1);
    f.Draw();
    glPopMatrix();

    //===============================================


    //сбрасываем все трансформации
    glLoadIdentity();
    camera.SetUpCamera();
    Shader::DontUseShaders();
    //рисуем источник света
    light.DrawLightGizmo();

    //================Сообщение в верхнем левом углу=======================
    glActiveTexture(GL_TEXTURE0);
    //переключаемся на матрицу проекции
    glMatrixMode(GL_PROJECTION);
    //сохраняем текущую матрицу проекции с перспективным преобразованием
    glPushMatrix();
    //загружаем единичную матрицу в матрицу проекции
    glLoadIdentity();

    //устанавливаем матрицу паралельной проекции
    glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

    //переключаемся на моделвью матрицу
    glMatrixMode(GL_MODELVIEW);
    //сохраняем матрицу
    glPushMatrix();
    //сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
    glLoadIdentity();

    //отрисованное тут будет визуалзироватся в 2д системе координат
    //нижний левый угол окна - точка (0,0)
    //верхний правый угол (ширина_окна - 1, высота_окна - 1)


    std::wstringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << L"POWER - клавиша 1\n";
    ss << L"PLAY/PAUSE - клавиша 2\n";
    ss << L"STOP - клавиша 3\n";
    ss << L"EJECT (лоток) - клавиша 4\n";
    ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
    ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
    ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
    ss << L"F - Свет из камеры" << std::endl;
    ss << L"G - двигать свет по горизонтали" << std::endl;
    ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
    ss << L"Коорд. света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
    ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
    ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
    ss << L"delta_time: " << std::setprecision(5) << delta_time << std::endl;
    ss << L"full_time: " << std::setprecision(2) << full_time << std::endl;


    text.setPosition(10, gl.getHeight() - 10 - 180);
    text.setText(ss.str().c_str());
    text.Draw();

    //восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

}

//// --- Отрисовка вертикального экрана (плазма) ---
//void DrawPlasmaScreen() {
//    glPushMatrix();
//    // Увеличенный экран: строго над DVD-плеером, квадрат, масштаб x3
//    glTranslatef(0.0f, 0.0f, 1.15f); // по центру, над плеером
//    glRotatef(-90, 1, 0, 0);
//    glScalef(1.5f, 1.0f, 1.0f); // x3 от исходного (0.5 -> 1.5)
//    // Квадратная рамка
//    glColor3f(0.1f, 0.1f, 0.15f);
//    float h = 0.5f, w = 0.5f; // квадрат
//    glBegin(GL_QUADS);
//    glVertex3f(-w, -h, 0.01f); glVertex3f(w, -h, 0.01f); glVertex3f(w, h, 0.01f); glVertex3f(-w, h, 0.01f);
//    glEnd();
//    // Экран
//    glColor3f(1, 1, 1);
//    glEnable(GL_TEXTURE_2D);
//    glBindTexture(GL_TEXTURE_2D, 0);
//    glBegin(GL_QUADS);
//    glTexCoord2f(0, 0); glVertex3f(-w+0.02f, -h+0.02f, 0.02f);
//    glTexCoord2f(1, 0); glVertex3f(w-0.02f, -h+0.02f, 0.02f);
//    glTexCoord2f(1, 1); glVertex3f(w-0.02f, h-0.02f, 0.02f);
//    glTexCoord2f(0, 1); glVertex3f(-w+0.02f, h-0.02f, 0.02f);
//    glEnd();
//    glDisable(GL_TEXTURE_2D);
//    glPopMatrix();
//}
//
//// --- Обработка кнопок управления DVD ---
//void HandleDVDKeys() {
//    static bool prevPower = false, prevPlay = false, prevStop = false;
//    if (gl.isKeyPressed('1')) { // POWER
//        if (!prevPower) { powerOn = !powerOn; dvdState = powerOn ? DVD_STOP : DVD_OFF; }
//        prevPower = true;
//    } else prevPower = false;
//    if (!powerOn) return;
//    if (gl.isKeyPressed('2')) { // PLAY/PAUSE
//        if (!prevPlay) {
//            if (dvdState == DVD_PLAY) dvdState = DVD_PAUSE;
//            else if (dvdState == DVD_PAUSE || dvdState == DVD_STOP) dvdState = DVD_PLAY;
//        }
//        prevPlay = true;
//    } else prevPlay = false;
//    if (gl.isKeyPressed('3')) { // STOP
//        if (!prevStop) { dvdState = DVD_STOP; currentSlide = 0; }
//        prevStop = true;
//    } else prevStop = false;
//}
//
//// --- Визуальная индикация кнопок (цвет) ---
//void DrawButtonHighlight(float x, float y, float z, bool active, bool isPower)
//{
//    if (!active) return;
//    if (isPower) {
//        // Красный индикатор для POWER
//        glColor3f(1.0f, 0.2f, 0.2f);
//        glLineWidth(7.0f);
//        glBegin(GL_LINE_LOOP);
//        for (int i = 0; i < 32; ++i) {
//            float t = 2.0f * 3.1415926f * i / 32;
//            float r = 0.034f;
//            glVertex3f(x + 0.025f + r * cosf(t), y + 0.025f + r * sinf(t), z + 0.005f);
//        }
//        glEnd();
//        glEnable(GL_BLEND);
//        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//        glColor4f(1.0f, 0.2f, 0.2f, 0.28f);
//        glBegin(GL_TRIANGLE_FAN);
//        glVertex3f(x + 0.025f, y + 0.025f, z + 0.0045f);
//        for (int i = 0; i <= 32; ++i) {
//            float t = 2.0f * 3.1415926f * i / 32;
//            float r = 0.027f;
//            glVertex3f(x + 0.025f + r * cosf(t), y + 0.025f + r * sinf(t), z + 0.0045f);
//        }
//        glEnd();
//        glDisable(GL_BLEND);
//    } else {
//        // Зелёный индикатор для остальных
//        glColor3f(0.2f, 1.0f, 0.2f); // ярко-зелёный
//        glLineWidth(6.0f);
//        glBegin(GL_LINE_LOOP);
//        for (int i = 0; i < 32; ++i) {
//            float t = 2.0f * 3.1415926f * i / 32;
//            float r = 0.028f;
//            glVertex3f(x + 0.025f + r * cosf(t), y + 0.025f + r * sinf(t), z + 0.004f);
//        }
//        glEnd();
//        glEnable(GL_BLEND);
//        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//        glColor4f(0.2f, 1.0f, 0.2f, 0.25f);
//        glBegin(GL_TRIANGLE_FAN);
//        glVertex3f(x + 0.025f, y + 0.025f, z + 0.0035f);
//        for (int i = 0; i <= 32; ++i) {
//            float t = 2.0f * 3.1415926f * i / 32;
//            float r = 0.022f;
//            glVertex3f(x + 0.025f + r * cosf(t), y + 0.025f + r * sinf(t), z + 0.0035f);
//        }
//        glEnd();
//        glDisable(GL_BLEND);
//    }
//}