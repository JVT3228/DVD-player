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
#include <cmath>


#include "ObjLoader.h"


#include "debout.h"



//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;

void buildMDV724UBody();
void buildTray(float offset);
void buildScreen(int mode);
void buildButtons();
void buildDisc(float rotationAngle);

// Состояния модели
bool isDiscSpinning = false;
bool isTrayOut = false;
int screenMode = 0; // 0: "00:00", 1: USB
float trayOffset = 0.0f;
float discRotation = 0.0f;

// Текстуры
Texture displayTexDefault, displayTexUSB;

//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL *sender, KeyEventArg arg)
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
	case 'P': 
		isDiscSpinning = !isDiscSpinning; 
		break;
	case 'E':
		isTrayOut = !isTrayOut;
		trayOffset = isTrayOut ? 0.1f : 0.0f;
		break;
	case 'U': 
		screenMode = (screenMode + 1) % 2; 
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
	   
	// Загрузка текстур экрана
	displayTexDefault.LoadTexture("textures/display_default.png"); // "00:00"
	displayTexUSB.LoadTexture("textures/display_usb.png");        // USB иконка

	// Начальная позиция света
	light.SetPosition(1.0f, 2.0f, 1.0f);
}
float view_matrix[16];
double full_time = 0;
int location = 0;
void Render(double delta_time)
{    
	

	full_time += delta_time;
	
	//натройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	//забираем моделвью матрицу сразу после установки камера
	//так как в ней отсуствуют трансформации glRotate...
	//она, фактически, является видовой.
	glGetFloatv(GL_MODELVIEW_MATRIX,view_matrix);

	

	light.SetUpLight();

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
	float  amb[] = { 0.2, 0.2, 0.1, 1. };
	float dif[] = { 0.4, 0.65, 0.5, 1. };
	float spec[] = { 0.9, 0.8, 0.3, 1. };
	float sh = 0.2f * 256;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec); 
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
			   //(GL_SMOOTH - плоская закраска)

	//============ РИСОВАТЬ ТУТ ==============

	

	//квадратик станкина

	

	////рисуем квадратик с овалом Кассини!
	//
	//cassini_sh.UseShader();

	//location = glGetUniformLocationARB(cassini_sh.program, "Time");
	//glUniform1fARB(location, full_time);
	//location = glGetUniformLocationARB(cassini_sh.program, "size");
	//glUniform2fARB(location, 100, 100);
	//
	//glPushMatrix();

	//glTranslated(0, -1.2, 0);

	//
	//glBegin(GL_QUADS);
	//glNormal3d(0, 0, 1);
	//glTexCoord2d(1, 1);
	//glVertex3d(0.5, 0.5, 0);
	//glTexCoord2d(1, 0);
	//glVertex3d(0.5, -0.5, 0);
	//glTexCoord2d(0, 0);
	//glVertex3d(-0.5, -0.5, 0);
	//glTexCoord2d(0, 1);
	//glVertex3d(-0.5, 0.5, 0);
	//glEnd();


	//glPopMatrix();







	////Квадратик с освещением
	//phong_sh.UseShader();

	//float light_pos[4] = { light.x(),light.y(), light.z(), 1 };
	//float light_pos_v[4];
	//
	////переносим координаты света в видовые координаты
	//MatrixMultiply<float, 1, 4, 4, 4>(light_pos, view_matrix, light_pos_v);

	//
	//location = glGetUniformLocationARB(phong_sh.program, "Ia");
	//glUniform3fARB(location, 1, 1, 1);
	//location = glGetUniformLocationARB(phong_sh.program, "Id");
	//glUniform3fARB(location, 1, 1, 1);
	//location = glGetUniformLocationARB(phong_sh.program, "Is");
	//glUniform3fARB(location, 1, 1, 1);

	//location = glGetUniformLocationARB(phong_sh.program, "ma");
	//glUniform3fARB(location, 0.1, 0.1, 0.1);
	//location = glGetUniformLocationARB(phong_sh.program, "md");
	//glUniform3fARB(location, 0.6, 0.6, 0.6);
	//location = glGetUniformLocationARB(phong_sh.program, "ms");
	//glUniform4fARB(location, 0, 1, 0, 300);
	//	
	//
	//location = glGetUniformLocationARB(phong_sh.program, "light_pos_v");
	//glUniform3fvARB(location,1,light_pos_v);
	//
	//glPushMatrix();

	//glTranslated(0, 0, 0);
	//

	//glBegin(GL_QUADS);
	//glNormal3d(0, 0, 1);
	//glTexCoord2d(1, 1);
	//glVertex3d(0.5, 0.5, 0);
	//glTexCoord2d(1, 0);
	//glVertex3d(0.5, -0.5, 0);
	//glTexCoord2d(0, 0);
	//glVertex3d(-0.5, -0.5, 0);
	//glTexCoord2d(0, 1);
	//glVertex3d(-0.5, 0.5, 0);
	//glEnd();


	//glPopMatrix();



	////Квадратик без освещения

	//Shader::DontUseShaders();

	//glBindTexture(GL_TEXTURE_2D, 0);

	//glPushMatrix();

	//glTranslated(1.2, 0, 0);


	//glBegin(GL_QUADS);
	//glNormal3d(0, 0, 1);
	//glTexCoord2d(1, 1);
	//glVertex3d(0.5, 0.5, 0);
	//glTexCoord2d(1, 0);
	//glVertex3d(0.5, -0.5, 0);
	//glTexCoord2d(0, 0);
	//glVertex3d(-0.5, -0.5, 0);
	//glTexCoord2d(0, 1);
	//glVertex3d(-0.5, 0.5, 0);
	//glEnd();


	//glPopMatrix();


	////квадратик с ВБ


	//vb_sh.UseShader();

	//glActiveTexture(GL_TEXTURE0);
	//stankin_tex.Bind();
	//glActiveTexture(GL_TEXTURE1);
	//vb_tex.Bind();

	//location = glGetUniformLocationARB(vb_sh.program, "time");
	//glUniform1fARB(location, full_time);
	//location = glGetUniformLocationARB(vb_sh.program, "tex_stankin");
	//glUniform1iARB(location, 0);
	//location = glGetUniformLocationARB(vb_sh.program, "tex_vb");
	//glUniform1iARB(location, 1);

	//glPushMatrix();

	//glTranslated(0, 1.2, 0);
	//	glBegin(GL_QUADS);
	//	glNormal3d(0, 0, 1);
	//	glTexCoord2d(1, 1);
	//	glVertex3d(0.5, 0.5, 0);
	//	glTexCoord2d(1, 0);
	//	glVertex3d(0.5, -0.5, 0);
	//	glTexCoord2d(0, 0);
	//	glVertex3d(-0.5, -0.5, 0);
	//	glTexCoord2d(0, 1);
	//	glVertex3d(-0.5, 0.5, 0);
	//glEnd();


	//glPopMatrix();


	////обезьянка без шейдеров
	//glPushMatrix();
	//Shader::DontUseShaders();
	//glActiveTexture(GL_TEXTURE0);
	//monkey_tex.Bind();
	//glShadeModel(GL_SMOOTH);
	//glTranslated(-1, 0, 0.5);
	//glScaled(0.1, 0.1, 0.1);
	//glRotated(180, 0, 0, 1);
	//f.Draw();
	//glPopMatrix();

	////обезьянка с шейдерами


	//simple_texture_sh.UseShader();
	//location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
	//glUniform1iARB(location, 0);
	//glActiveTexture(GL_TEXTURE0);
	//monkey_tex.Bind();


	//glPushMatrix();
	//glTranslated(-1, 1, 0.5);
	//glScaled(0.1, 0.1, 0.1);
	//glRotated(180, 0, 0, 1);
	//f.Draw();
	//glPopMatrix();

	// Обновление анимаций
	if (isDiscSpinning) discRotation += 100.0f * delta_time;

    // Отрисовка компонентов
    buildMDV724UBody();
    buildTray(trayOffset);
    // buildScreen(screenMode);
    // buildButtons();
    // buildDisc(discRotation);

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
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) <<  light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5)<< delta_time << std::endl;
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

void drawSolidCube(double size) {
	double s = size / 2.0;
	glBegin(GL_QUADS);
	// Передняя грань
	glNormal3d(0, 0, 1);
	glVertex3d(-s, -s, s);
	glVertex3d(s, -s, s);
	glVertex3d(s, s, s);
	glVertex3d(-s, s, s);
	// Задняя грань
	glNormal3d(0, 0, -1);
	glVertex3d(-s, -s, -s);
	glVertex3d(-s, s, -s);
	glVertex3d(s, s, -s);
	glVertex3d(s, -s, -s);
	// Левая грань
	glNormal3d(-1, 0, 0);
	glVertex3d(-s, -s, -s);
	glVertex3d(-s, -s, s);
	glVertex3d(-s, s, s);
	glVertex3d(-s, s, -s);
	// Правая грань
	glNormal3d(1, 0, 0);
	glVertex3d(s, -s, -s);
	glVertex3d(s, s, -s);
	glVertex3d(s, s, s);
	glVertex3d(s, -s, s);
	// Верхняя грань
	glNormal3d(0, 1, 0);
	glVertex3d(-s, s, -s);
	glVertex3d(-s, s, s);
	glVertex3d(s, s, s);
	glVertex3d(s, s, -s);
	// Нижняя грань
	glNormal3d(0, -1, 0);
	glVertex3d(-s, -s, -s);
	glVertex3d(s, -s, -s);
	glVertex3d(s, -s, s);
	glVertex3d(-s, -s, s);
	glEnd();
}

void drawCylinder(float radius, float height, int slices = 24) {
	glPushMatrix();
	// Восстановление цвета по умолчанию (белый, полностью непрозрачный)
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTranslatef(0, 0, -height / 2.0f);
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= slices; ++i) {
		float theta = 2.0f * 3.1415926f * float(i) / float(slices);
		float x = radius * cosf(theta);
		float y = radius * sinf(theta);
		glNormal3f(x, y, 0);
		glVertex3f(x, y, 0);
		glVertex3f(x, y, height);
	}
	glEnd();
	// Верхняя крышка
	glBegin(GL_TRIANGLE_FAN);
	glNormal3f(0, 0, 1);
	glVertex3f(0, 0, height);
	for (int i = 0; i <= slices; ++i) {
		float theta = 2.0f * 3.1415926f * float(i) / float(slices);
		glVertex3f(radius * cosf(theta), radius * sinf(theta), height);
	}
	glEnd();
	// Нижняя крышка
	glBegin(GL_TRIANGLE_FAN);
	glNormal3f(0, 0, -1);
	glVertex3f(0, 0, 0);
	for (int i = 0; i <= slices; ++i) {
		float theta = 2.0f * 3.1415926f * float(i) / float(slices);
		glVertex3f(radius * cosf(theta), radius * sinf(theta), 0);
	}
	glEnd();
	glPopMatrix();
}

void buildMDV724UBody() {
	// Матовый чёрный пластик (слегка светлее для видимости света)
	float amb[] = { 0.13f, 0.13f, 0.13f, 1.0f };
	float dif[] = { 0.18f, 0.18f, 0.18f, 1.0f };
	float spec[] = { 0.04f, 0.04f, 0.04f, 1.0f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, 8.0f);

	// Размеры корпуса (увеличены в 3 раза)
	float w = 0.75f, h = 0.525f, d = 0.12f;
	float x0 = -w / 2, x1 = w / 2;
	float y0 = -h / 2, y1 = h / 2;
	float z0 = -d / 2, z1 = d / 2;

	// Размеры выреза на передней панели (увеличены в 3 раза)
	float cut_x0 = -0.23f, cut_x1 = -0.09f; // по X
	float cut_z0 = -0.03f, cut_z1 = 0.03f;  // по Z
	float y_front = y1;

    // Включаем текстурирование и привязываем текстуру корпуса
    if (texturing) {
		glEnable(GL_TEXTURE_2D);
		stankin_tex.Bind();
	} else {
		glDisable(GL_TEXTURE_2D);
	}

    // Явный цвет корпуса (матовый чёрный)
    glColor4f(0.18f, 0.18f, 0.18f, 1.0f);
    // Передняя грань с вырезом (рамка из 4-х прямоугольников)
    // Левая часть передней грани
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 1); glVertex3f(x0, y_front, z1);
    glTexCoord2f(1, 1); glVertex3f(cut_x0, y_front, z1);
    glTexCoord2f(1, 0); glVertex3f(cut_x0, y_front, z0);
    glTexCoord2f(0, 0); glVertex3f(x0, y_front, z0);
    glEnd();
    // Правая часть передней грани
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 1); glVertex3f(cut_x1, y_front, z1);
    glTexCoord2f(1, 1); glVertex3f(x1, y_front, z1);
    glTexCoord2f(1, 0); glVertex3f(x1, y_front, z0);
    glTexCoord2f(0, 0); glVertex3f(cut_x1, y_front, z0);
    glEnd();
    // Верхняя часть передней грани
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 1); glVertex3f(cut_x0, y_front, z1);
    glTexCoord2f(1, 1); glVertex3f(cut_x1, y_front, z1);
    glTexCoord2f(1, 0); glVertex3f(cut_x1, y_front, cut_z1);
    glTexCoord2f(0, 0); glVertex3f(cut_x0, y_front, cut_z1);
    glEnd();
    // Нижняя часть передней грани
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 1); glVertex3f(cut_x0, y_front, cut_z0);
    glTexCoord2f(1, 1); glVertex3f(cut_x1, y_front, cut_z0);
    glTexCoord2f(1, 0); glVertex3f(cut_x1, y_front, z0);
    glTexCoord2f(0, 0); glVertex3f(cut_x0, y_front, z0);
    glEnd();

	// Остальные грани корпуса (без изменений, только размеры увеличены)
	// Верхняя грань
    glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
	glTexCoord2f(1, 1); glVertex3f(x1, y1, z1);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z1);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
	glEnd();
	// Нижняя грань
    glBegin(GL_QUADS);
	glNormal3f(0, 0, -1);
	glTexCoord2f(0, 1); glVertex3f(x0, y1, z0);
	glTexCoord2f(1, 1); glVertex3f(x1, y1, z0);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z0);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z0);
	glEnd();
	// Левая грань
    glBegin(GL_QUADS);
	glNormal3f(-1, 0, 0);
	glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
	glTexCoord2f(1, 1); glVertex3f(x0, y1, z0);
	glTexCoord2f(1, 0); glVertex3f(x0, y0, z0);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
	glEnd();
	// Правая грань
    glBegin(GL_QUADS);
	glNormal3f(1, 0, 0);
	glTexCoord2f(0, 1); glVertex3f(x1, y1, z1);
	glTexCoord2f(1, 1); glVertex3f(x1, y1, z0);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z0);
	glTexCoord2f(0, 0); glVertex3f(x1, y0, z1);
	glEnd();
	// Задняя грань
    glBegin(GL_QUADS);
	glNormal3f(0, -1, 0);
	glTexCoord2f(0, 1); glVertex3f(x0, y0, z1);
	glTexCoord2f(1, 1); glVertex3f(x1, y0, z1);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z0);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z0);
	glEnd();

    glDisable(GL_TEXTURE_2D);
	glPopMatrix();
}
void buildTray(float offset) {
    // Размеры выреза на передней панели корпуса (должны совпадать с корпусом)
    float cut_x0 = -0.23f, cut_x1 = -0.09f; // по X
    float cut_z0 = -0.03f, cut_z1 = 0.03f;  // по Z
    float y_front = 0.525f / 2.0f; // y1 корпуса

    // Размеры лотка
    float trayW = cut_x1 - cut_x0 + 0.02f; // чуть шире выреза
    float trayD = 0.18f;                   // глубина лотка (чуть больше корпуса)
    float trayH = 0.012f;                  // толщина платформы
    float rimH  = 0.018f;                  // высота бортиков
    float diskR = 0.06f;                   // радиус отверстия под диск
    float rimT = 0.012f;                   // толщина бортика

    // Позиция лотка: по центру выреза, чуть ниже верхней крышки
    float trayX = (cut_x0 + cut_x1) / 2.0f;
    float trayZ = (cut_z0 + cut_z1) / 2.0f + offset * 3.0f; // теперь Z — по центру выреза
    float trayY = y_front - trayD / 2.0f - 0.002f; // лоток выдвигается вдоль Y (вперёд)

    glPushMatrix();
    glTranslatef(trayX, trayY, trayZ);
    // Разворот на 180 по вертикали: отражаем по оси Y (глубина)
    glScalef(1.0f, -1.0f, 1.0f); // инверсия глубины

    // Цвет лотка
    glColor4f(0.22f, 0.22f, 0.22f, 1.0f);

    // Верхняя платформа лотка (прямоугольник, теперь в X-Y)
    float x0 = -trayW/2, x1 = trayW/2;
    float y0 = 0.0f, y1 = trayD;
    float z = 0.0f;
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z);
    glVertex3f(x1, y0, z);
    glVertex3f(x1, y1, z);
    glVertex3f(x0, y1, z);
    glEnd();

    // Отверстие под диск (в центре платформы)
    glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
    int sectors = 64;
    float diskCenterY = trayD / 2.0f;
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, diskCenterY, z + 0.001f); // центр
    for (int i = 0; i <= sectors; ++i) {
        float angle = 2.0f * 3.1415926f * i / sectors;
        glVertex3f(cos(angle) * diskR, diskCenterY + sin(angle) * diskR, z + 0.001f);
    }
    glEnd();

    // Бортики по периметру (левая, правая, задняя)
    glColor4f(0.18f, 0.18f, 0.18f, 1.0f);
    // Левая
    glPushMatrix();
    glTranslatef(x0 + rimT / 2, trayD / 2.0f, rimH / 2);
    glScalef(rimT, trayD, rimH);
    drawSolidCube(1.0);
    glPopMatrix();
    // Правая
    glPushMatrix();
    glTranslatef(x1 - rimT / 2, trayD / 2.0f, rimH / 2);
    glScalef(rimT, trayD, rimH);
    drawSolidCube(1.0);
    glPopMatrix();
    // Задняя (теперь будет спереди после отражения)
    glPushMatrix();
    glTranslatef(0, trayD - rimT / 2, rimH / 2);
    glScalef(trayW, rimT, rimH);
    drawSolidCube(1.0);
    glPopMatrix();
    // (Передний бортик отсутствует)

    glPopMatrix();
}

void buildScreen(int mode) {
	// Полупрозрачный зелёный
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	float amb[] = { 0.0f, 0.25f, 0.0f, 0.5f };
	float dif[] = { 0.0f, 0.7f, 0.0f, 0.5f };
	float spec[] = { 0.2f, 0.8f, 0.2f, 0.5f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, 30.0f);

	// Явный цвет экрана (полупрозрачный зелёный)
	glColor4f(0.0f, 0.7f, 0.0f, 0.5f);

	glEnable(GL_TEXTURE_2D);
	(mode == 0) ? displayTexDefault.Bind() : displayTexUSB.Bind();

	glPushMatrix();
	glTranslatef(0.21f, 0.03f, 0.105f); // 0.07*3, 0.01*3, 0.035*3
	glBegin(GL_QUADS);
	glTexCoord2f(1, 1); glVertex3f(0.075f, 0.0225f, 0); // 0.025*3, 0.0075*3
	glTexCoord2f(1, 0); glVertex3f(0.075f, -0.0225f, 0);
	glTexCoord2f(0, 0); glVertex3f(-0.075f, -0.0225f, 0);
	glTexCoord2f(0, 1); glVertex3f(-0.075f, 0.0225f, 0);
	glEnd();
	glPopMatrix();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

void buildButtons() {
	// Тёмно-серые глянцевые кнопки
	float amb[] = { 0.13f, 0.13f, 0.13f, 1.0f };
	float dif[] = { 0.22f, 0.22f, 0.22f, 1.0f };
	float spec[] = { 0.7f, 0.7f, 0.7f, 1.0f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, 90.0f);

	// Явный цвет кнопок (тёмно-серый)
	glColor4f(0.22f, 0.22f, 0.22f, 1.0f);

	// Кнопки: Power, Play, Stop, Prev, Next, Eject, USB
	float startX = -0.24f; // -0.08*3
	float stepX = 0.09f;   // 0.03*3
	float y = 0.21f;       // 0.07*3
	float z = 0.066f;      // 0.022*3
	for (int i = 0; i < 7; ++i) {
		glPushMatrix();
		glTranslatef(startX + i * stepX, y, z);
		glScalef(0.036f, 0.036f, 0.036f); // 0.012*3
		drawSolidCube(1.0f);
		glPopMatrix();
	}
	// Индикатор (ярко-зелёный, полупрозрачный)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	float ambG[] = { 0.0f, 0.5f, 0.0f, 0.6f };
	float difG[] = { 0.0f, 1.0f, 0.0f, 0.6f };
	float specG[] = { 0.2f, 1.0f, 0.2f, 0.6f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambG);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, difG);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specG);
	glMaterialf(GL_FRONT, GL_SHININESS, 40.0f);
	// Явный цвет индикатора (ярко-зелёный, полупрозрачный)
	glColor4f(0.0f, 1.0f, 0.0f, 0.6f);
	glPushMatrix();
	glTranslatef(0.33f, 0.036f, 0.066f); // 0.11*3, 0.012*3, 0.022*3
	drawCylinder(0.018f, 0.024f); // 0.006*3, 0.008*3
	glPopMatrix();
	glDisable(GL_BLEND);
}

void buildDisc(float rotationAngle) {
    // Серебристый материал
    float amb[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    float dif[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    float spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 120.0f);

    // Явный цвет диска (серебристый)
    glColor4f(0.8f, 0.8f, 0.8f, 1.0f);

	glPushMatrix();
	glTranslatef(-0.18f, 0.0f, 0.075f + trayOffset * 3.0f); // -0.06*3, 0, 0.025*3 + trayOffset*3
	glRotatef(rotationAngle, 0, 1, 0);
	drawCylinder(0.18f, 0.006f, 48); // 0.06*3, 0.002*3
	glPopMatrix();
}