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
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") // подключаем winmm


#include "ObjLoader.h"


#include "debout.h"

#include <vector>
#include <string>
#include <algorithm>

using namespace std;


bool isPlaying = false;
bool isPaused = false;
DWORD soundStartTime = 0;

bool isPowerOff = false;
int visualMode = 0; // 0 - видео, 1 - DVD логотип
float dvdX = 0.0f, dvdY = 0.0f;
float dvdVX = 0.25f, dvdVY = 0.15f; // скорость (м/с)
float dvdSize = 0.15f;
float dvdR = 1.0f, dvdG = 1.0f, dvdB = 1.0f;
Texture dvdLogoTex;

vector<string> framePaths;
int currentFrame = 0;
float timeSinceLastFrame = 0.0f;
float frameDuration = 1.0f / 30.0f; // 30 FPS
Texture videoFrame;

void initFramePaths() {
	for (int i = 1; i <= 1754; ++i) {
		stringstream ss;
		ss << "textures/frames/frame_"
			<< setw(4) << setfill('0') << i << ".jpg";
		framePaths.push_back(ss.str());
	}
}

void PlayAudio() {
	if (!isPlaying) {
		PlaySound(L"sounds/Тайпан_жестокая-змея.wav", NULL, SND_FILENAME | SND_ASYNC);
		soundStartTime = timeGetTime();
		isPlaying = true;
		isPaused = false;
	}
}

void PauseAudio() {
	if (isPlaying && !isPaused) {
		PlaySound(NULL, 0, 0); // остановить
		isPaused = true;
	}
}

void ResumeAudio() {
	if (isPaused) {
		PlaySound(L"sounds/Тайпан_жестокая-змея.wav", NULL, SND_FILENAME | SND_ASYNC);
		isPaused = false;
		// нет точного resume, будет играть заново (ограничение PlaySound)
	}
}

#include <map>
#include <chrono>

map<char, chrono::steady_clock::time_point> pressedButtons;
float pressAnimationDuration = 0.15f; // в секундах

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
void buildScreen();
void buildButtons();
void buildDisc(float rotationAngle);

void randomizeColor();

// Состояния модели
bool isDiscSpinning = false;
bool isTrayOut = false;
//int screenMode = 0; // 0: "00:00", 1: USB
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
		pressedButtons[key] = std::chrono::steady_clock::now();
		if (!isPowerOff) {
			isDiscSpinning = !isDiscSpinning;
			if (!isPlaying) {
				PlayAudio();
			}
			else if (isPaused) {
				ResumeAudio();
			}
			else {
				PauseAudio();
			}
		}
		break;
	case 'E':
		pressedButtons[key] = std::chrono::steady_clock::now();
		if (!isPowerOff) {
			isTrayOut = !isTrayOut;
			// trayOffset = isTrayOut ? 1.0f : 0.0f;
		}
		break;
	//case 'U': 
	//	screenMode = (screenMode + 1) % 2; 
	//	break;
	case 'O': // Power
		isPowerOff = !isPowerOff;
		if (!isPowerOff) {
			currentFrame = 0;
			videoFrame.LoadTexture(framePaths[currentFrame]);
			ResumeAudio();
			visualMode = 0;
			isDiscSpinning = true;
		}
		else {
			PauseAudio();
			videoFrame.LoadTexture("textures/black.png");
			isDiscSpinning = false;
		}
		break;
	case 'J':
		pressedButtons[key] = std::chrono::steady_clock::now();
		if (!isPowerOff) {
			visualMode = (visualMode == 0) ? 1 : 0;
			if (visualMode == 1) {
				PauseAudio();
				dvdX = -0.1f;
				dvdY = -0.1f;
				dvdVX = 0.25f;
				dvdVY = 0.15f;
				randomizeColor();
			}
			else {
				ResumeAudio();
				currentFrame = 0;
				videoFrame.LoadTexture(framePaths[currentFrame]);
			}
		}
		break;
	case 'K':
		pressedButtons[key] = std::chrono::steady_clock::now();
		if (!isPowerOff) {
			visualMode = (visualMode == 0) ? 1 : 0;
			if (visualMode == 1) {
				PauseAudio();
				dvdX = -0.1f;
				dvdY = -0.1f;
				dvdVX = 0.25f;
				dvdVY = 0.15f;
				randomizeColor();
			}
			else {
				ResumeAudio();
				currentFrame = 0;
				videoFrame.LoadTexture(framePaths[currentFrame]);
			}
		}
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

Texture stankin_tex, vb_tex, monkey_tex, disk_tex, tkan_tex, logo_tex;



void initRender()
{
	randomizeColor();

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
	disk_tex.LoadTexture("textures/disk.png");
	tkan_tex.LoadTexture("textures/tkan.png");
	logo_tex.LoadTexture("textures/logo.png");


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
	//displayTexDefault.LoadTexture("textures/display_default.png"); // "00:00"
	//displayTexUSB.LoadTexture("textures/display_usb.png");        // USB иконка

	// Начальная позиция света
	light.SetPosition(1.0f, 2.0f, 1.0f);
	
	dvdLogoTex.LoadTexture("textures/DVD.png");
	initFramePaths();
	videoFrame.LoadTexture(framePaths[0]);
	bool isPowerOff = true;
	videoFrame.LoadTexture("textures/black.png");
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

    // Плавная анимация выдвигания и задвигания лотка
	static float trayTarget = 0.0f;
	static bool prevTrayOut = isTrayOut;
	float traySpeed = 1.0f;

	if (isTrayOut != prevTrayOut) {
		trayTarget = isTrayOut ? 1.0f : 0.0f;
		prevTrayOut = isTrayOut;
	}

	if (fabs(trayOffset - trayTarget) > 0.001f) {
        if (trayOffset < trayTarget) {
            trayOffset += delta_time * traySpeed;
            if (trayOffset > trayTarget) trayOffset = trayTarget;
        } else {
            trayOffset -= delta_time * traySpeed;
            if (trayOffset < trayTarget) trayOffset = trayTarget;
        }
    }

    // Отрисовка компонентов
    buildMDV724UBody();
    buildTray(trayOffset);

	if (!isPowerOff) {
		switch (visualMode) {
		case 0: // обычное видео
			if (isPlaying && !isPaused) {
				timeSinceLastFrame += static_cast<float>(delta_time);
				if (timeSinceLastFrame >= frameDuration) {
					timeSinceLastFrame = 0.0f;
					if (currentFrame < framePaths.size() - 1) {
						currentFrame++;
						videoFrame.LoadTexture(framePaths[currentFrame]);
					}
				}
			}
			break;
		case 1: // DVD логотип
			dvdX += dvdVX * delta_time;
			dvdY += dvdVY * delta_time;

			// Границы (в условных координатах)
			float minX = -0.21f, maxX = 0.21f - dvdSize;
			float minY = -0.2025f, maxY = 0.2025f - dvdSize;

			if (dvdX < minX || dvdX > maxX) {
				dvdVX *= -1;
				dvdX = clamp(dvdX, minX, maxX);
				randomizeColor();
			}
			if (dvdY < minY || dvdY > maxY) {
				dvdVY *= -1;
				dvdY = clamp(dvdY, minY, maxY);
				randomizeColor();
			}
			break;
		}
	}
	else {
		videoFrame.LoadTexture("textures/black.png");
	}

    buildScreen();
    buildButtons();
    buildDisc(discRotation);

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

	
	wstringstream ss;
	ss << fixed << setprecision(3);
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << endl;
	ss << "E - " << (isTrayOut ? L"[открыт]закрыть  " : L"открыть[закрыт] ") << L"лоток" << endl;
	ss << "P - " << (isDiscSpinning ? L"[вращается]стоп  " : L"вращать[стоп] ") << L"диск" << endl;
	//ss << "U - " << (screenMode == 0 ? L"[00:00]USB  " : L"00:00[USB] ") << L"экран" << endl; 
	ss << "O - Power: " << (isPowerOff ? L"вкл[выкл]  " : L" [вкл]выкл ") << endl;
	ss << "J - Prev  |  K - Next" << L"  (Режим: ";
	ss << (visualMode == 0 ? L"Видео" : L"DVD логотип") << L")" << endl;
	ss << L"F - Свет из камеры" << endl;
	ss << L"G - двигать свет по горизонтали" << endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << endl;
	ss << L"Коорд. света: (" << setw(7) <<  light.x() << "," << setw(7) << light.y() << "," << setw(7) << light.z() << ")" << endl;
	ss << L"Коорд. камеры: (" << setw(7) << camera.x() << "," << setw(7) << camera.y() << "," << setw(7) << camera.z() << ")" << endl;
	ss << L"Параметры камеры: R=" << setw(7) << camera.distance() << ",fi1=" << setw(7) << camera.fi1() << ",fi2=" << setw(7) << camera.fi2() << endl;
	ss << L"delta_time: " << setprecision(5)<< delta_time << endl;
	ss << L"full_time: " << setprecision(2) << full_time << endl;

	
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
	float amb[] = { 0.25f, 0.25f, 0.25f, 1.0f };
	float dif[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	float spec[] = { 0.3f, 0.3f, 0.3f, 1.0f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, 20.0f);

	// Размеры корпуса (увеличены в 3 раза)
	float w = 0.75f, h = 0.525f, d = 0.12f;
	float x0 = -w / 2, x1 = w / 2;
	float y0 = -h / 2, y1 = h / 2;
	float z0 = -d / 2, z1 = d / 2;

	// Размеры выреза на передней панели (увеличены в 3 раза)
	float cut_x0 = -0.298f, cut_x1 = -0.093f; // по X
	float cut_z0 = 0.0f, cut_z1 = 0.018f;  // по Z
	float y_front = y1;

    // Включаем текстурирование и привязываем текстуру корпуса
    if (texturing) {
		glEnable(GL_TEXTURE_2D);
		tkan_tex.Bind();
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
	// Верхняя грань
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
	glTexCoord2f(1, 1); glVertex3f(x1, y1, z1);
	glTexCoord2f(1, 0); glVertex3f(x1, y0, z1);
	glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
	glEnd();

	// --- ЛОГОТИП поверх крышки ---
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    logo_tex.Bind();

    // Размер и положение логотипа (по центру крышки, 60% от размера)
    float logoW = (x1 - x0) * 0.6f;
    float logoH = (y1 - y0) * 0.6f;
    float logoX0 = (x0 + x1) / 2 - logoW / 2;
    float logoX1 = logoX0 + logoW;
    float logoY0 = (y0 + y1) / 2 - logoH / 2;
    float logoY1 = logoY0 + logoH;

    glColor4f(1, 1, 1, 0.85f); // прозрачность логотипа
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glTexCoord2f(1, 0); glVertex3f(logoX0, logoY1, z1 + 0.0001f);
    glTexCoord2f(0, 0); glVertex3f(logoX1, logoY1, z1 + 0.0001f);
    glTexCoord2f(0, 1); glVertex3f(logoX1, logoY0, z1 + 0.0001f);
    glTexCoord2f(1, 1); glVertex3f(logoX0, logoY0, z1 + 0.0001f);
    glEnd();

    glDisable(GL_BLEND);

    glDisable(GL_TEXTURE_2D);
	glPopMatrix();
}
void buildTray(float offset) {
	float trayAmb[] = { 0.22f, 0.22f, 0.22f, 1.0f };
	float trayDif[] = { 0.6f, 0.6f, 0.6f, 1.0f };
	float traySpec[] = { 0.4f, 0.4f, 0.4f, 1.0f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, trayAmb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, trayDif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, traySpec);
	glMaterialf(GL_FRONT, GL_SHININESS, 40.0f);

    // Размеры выреза на передней панели корпуса (должны совпадать с корпусом)
    float cut_x0 = -0.3f, cut_x1 = -0.09f; // по X (как в корпусе)
    float cut_z0 = -0.01f, cut_z1 = 0.01f;  // по Z (как в корпусе)
    float y_front = 0.525f / 2.0f; // y1 корпуса

    // Размеры лотка
    float trayW = (cut_x1 - cut_x0) - 0.004f; // чуть уже выреза (по 2 мм с каждой стороны)
    float trayD = 0.18f;                      // глубина лотка (можно оставить прежней)
    float trayH = 0.012f;                     // толщина платформы
    float rimH  = 0.018f;                     // высота бортиков
    float diskR = 0.075f;                      // радиус отверстия под диск
    float rimT = 0.012f;                      // толщина бортика

	float cut_height = cut_z1 - cut_z0;
	float trayX = (cut_x0 + cut_x1) / 2.0f;
	float trayZ = (cut_z0 + cut_z1); // по центру выреза по Z
	float trayY = y_front - trayD + offset * trayD; // trayH вверх, чтобы верхняя грань совпала с нижней гранью выреза

    // trayOffset теперь реально управляет выдвижением (0 — полностью задвинут, 1 — полностью выдвинут)
    float trayMove = offset * trayD; // trayD положительно — движение вперёд (наружу корпуса)

    glPushMatrix();
    glTranslatef(trayX, trayY, trayZ);

    // Цвет лотка
    glColor4f(0.22f, 0.22f, 0.22f, 1.0f);

    // Верхняя платформа лотка (прямоугольник, теперь в X-Y)
    float x0 = -trayW/2, x1 = trayW/2;
    float y0 = 0.0f, y1 = trayD;
    float z = 0.0f;
	// Объёмная платформа как прямоугольный куб
	glPushMatrix();
	glTranslatef(0, trayD / 2.0f, trayH / 4.0f - 0.004f);
	glScalef(trayW, trayD, trayH / 4.0f);
	drawSolidCube(1.0);
	glPopMatrix();

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
    // Задняя 
    glPushMatrix();
    glTranslatef(0, trayD - rimT / 2, rimH / 2);
    glScalef(trayW, rimT, rimH);
    drawSolidCube(1.0);
    glPopMatrix();
    // (Передний бортик отсутствует)
	glPushMatrix();
	glTranslatef(0, rimT / 10.0f, rimH / 2.0f);
	glScalef(trayW, rimT / 10.0f, rimH);
	drawSolidCube(1.0);
	glPopMatrix();

    glPopMatrix();
}

void buildScreen() {
	// Полупрозрачный зелёный
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//float amb[] = { 0.0f, 0.25f, 0.0f, 0.5f };
	//float dif[] = { 0.0f, 0.7f, 0.0f, 0.5f };
	//float spec[] = { 0.2f, 0.8f, 0.2f, 0.5f };
	//glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	//glMaterialf(GL_FRONT, GL_SHININESS, 30.0f);

	//// Явный цвет экрана (полупрозрачный зелёный)
	//glColor4f(0.0f, 0.7f, 0.0f, 0.5f);

	glEnable(GL_TEXTURE_2D);
	//(mode == 0) ? displayTexDefault.Bind() : displayTexUSB.Bind();

	videoFrame.Bind();

	glPushMatrix();
	glTranslatef(0.0f, 0.05f, 0.24f); // 0.07*3, 0.01*3, 0.035*3

	glDisable(GL_LIGHTING);

	if (visualMode == 1) {
		dvdLogoTex.Bind();
		glColor3f(dvdR, dvdG, dvdB);
		glRotatef(90, 1, 0, 0);
		glBegin(GL_QUADS);
		glTexCoord2f(1, 1); glVertex3f(dvdX + dvdSize, dvdY + dvdSize, 0.2f);
		glTexCoord2f(1, 0); glVertex3f(dvdX + dvdSize, dvdY, -0.15f);
		glTexCoord2f(0, 0); glVertex3f(dvdX, dvdY, -0.15f);
		glTexCoord2f(0, 1); glVertex3f(dvdX, dvdY + dvdSize, 0.2f);
		glEnd();
	}
	else {
		glColor4f(1.2f, 1.2f, 1.2f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(1, 1); glVertex3f(-0.21f, -0.2625f, 0.2f);
		glTexCoord2f(1, 0); glVertex3f(-0.21f, 0.2025f, -0.15f);
		glTexCoord2f(0, 0); glVertex3f(0.21f, 0.2025f, -0.15f);
		glTexCoord2f(0, 1); glVertex3f(0.21f, -0.2625f, 0.2f);
		glEnd();

		// Чёрная рамка для основного экрана
		glDisable(GL_TEXTURE_2D);
		glColor3f(0.0f, 0.0f, 0.0f);
		glLineWidth(2.0f);
		glBegin(GL_LINE_LOOP);
		glVertex3f(-0.21f, -0.2625f, 0.2f);
		glVertex3f(-0.21f, 0.2025f, -0.15f);
		glVertex3f(0.21f, 0.2025f, -0.15f);
		glVertex3f(0.21f, -0.2625f, 0.2f);
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}

	glEnable(GL_LIGHTING);

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

	// Кнопки: Power, Play\Stop, Eject, Prev, Next
	const char buttonKeys[7] = { '-', '-', 'O', 'P', 'E', 'J', 'K'};
	float startX = -0.24f;
	float stepX = 0.09f;
	float y = 0.2625f;
	float z = 0.0f;

	auto now = std::chrono::steady_clock::now();

	for (int i = 3; i < 7; ++i) {
		float pressOffset = 0.0f;

		if (i > 2 && pressedButtons.count(buttonKeys[i])) {
			auto elapsed = std::chrono::duration<float>(
				now - pressedButtons[buttonKeys[i]]).count();

			if (elapsed < pressAnimationDuration) {
				float t = 1.0f - (elapsed / pressAnimationDuration);
				pressOffset = 0.005f * t;
			}
		}

		glPushMatrix();
		glTranslatef(startX + i * stepX, y - pressOffset, z - 0.025f);
		glRotatef(90, 1, 0, 0);
		glScalef(0.036f, 0.036f, 0.012f);
		drawSolidCube(1.0f);
		glPopMatrix();
	}
	// Индикатор (ярко-зелёный, полупрозрачный)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	float ambG[] = { isPowerOff ? 0.5f : 0.0f, isPowerOff ? 0.0f : 0.5f, 0.0f, 0.6f };
	float difG[] = { isPowerOff ? 1.0f : 0.0f, isPowerOff ? 0.0f : 1.0f, 0.0f, 0.6f };
	float specG[] = { 0.2f, 1.0f, 0.2f, 0.6f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, ambG);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, difG);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specG);
	glMaterialf(GL_FRONT, GL_SHININESS, 40.0f);
	// Явный цвет индикатора (ярко-зелёный, полупрозрачный)
	glColor4f(0.0f, 1.0f, 0.0f, 0.6f);
	glPushMatrix();
	glTranslatef(0.33f, y, z + 0.03f); // 0.11*3, 0.012*3, 0.022*3
	glRotatef(90, 1, 0, 0);
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

    // Цвет диска (серебристый)
    glColor4f(0.8f, 0.8f, 0.8f, 0.5f);

	glPushMatrix();
	glTranslatef(-0.1945f, 0.173f + trayOffset * 0.18f, 0.0015f);
	glRotatef(rotationAngle, 0, 0, 1);

	// Включаем текстурирование
	if (texturing) {
		glEnable(GL_TEXTURE_2D);
		disk_tex.Bind();
	}
	else {
		glDisable(GL_TEXTURE_2D);
	}

    // Рисуем диск с текстурой
    float radius = 0.075f;
    int sectors = 64;
    float z = 0.0f;
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 0, 1);
    glTexCoord2f(0.5f, 0.5f); glVertex3f(0, 0, z); // центр
    for (int i = 0; i <= sectors; ++i) {
        float angle = 2.0f * 3.1415926f * i / sectors;
        float x = cos(angle) * radius;
        float y = sin(angle) * radius;
        glTexCoord2f(0.5f + 0.5f * cos(angle), 0.5f + 0.5f * sin(angle));
        glVertex3f(x, y, z);
    }
    glEnd();

	glPopMatrix();
}

void randomizeColor() {
	dvdR = static_cast<float>(rand()) / RAND_MAX * 0.8f + 0.2f;
	dvdG = static_cast<float>(rand()) / RAND_MAX * 0.8f + 0.2f;
	dvdB = static_cast<float>(rand()) / RAND_MAX * 0.8f + 0.2f;
}