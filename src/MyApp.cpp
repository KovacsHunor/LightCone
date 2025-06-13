#include "framework.h"
#include <GLFW/glfw3.h>

const char* vertSource = R"(
	#version 330				
    uniform mat4 MVP;
	layout(location = 0) in vec2 vertexPosition;

	void main() {
		gl_Position = MVP * vec4(vertexPosition, 0, 1);
	}
)";

const char* fragSource = R"(
	#version 330
    uniform vec3 color;
	out vec4 fragmentColor;
	
	void main() {
		fragmentColor = vec4(color, 1.0f);
	}
)";

const float R = 40000;
const int winWidth = 600, winHeight = 600;

bool floatCmp(float a, float b) {
	float epsilon = 0.00001;
	return fabs(a - b) < epsilon;
}

class Camera {
   protected:
	vec2 pos;
	vec2 size;

   public:
	Camera(vec2 pos, vec2 size) : pos(pos), size(size) {}

	mat4 View() {
		return translate(vec3(-pos.x, -pos.y, 0));
	}
	mat4 Projection() {
		return scale(vec3(2 / size.x, 2 / size.y, 1));
	}
	mat4 ViewInv() {
		return translate(vec3(pos.x, pos.y, 0));
	}
	mat4 ProjectionInv() {
		return scale(vec3(size.x / 2, size.y / 2, 1));
	}

	float get_size() {
		return length(size) / sqrt(2);
	}

	void addOrigo(vec2 v) {
		pos += v;
	}

	vec2 convert(int pX, int pY) {
		float x = size.x * ((float)pX / winWidth - 0.5f) + pos.x;
		float y = size.y * (0.5f - (float)pY / winHeight) + pos.y;
		return vec2(x, y);
	}

	void zoom(vec2 p, float s) {
		pos += (p - pos) * (1.0f - s);
		size.x *= s;
		size.y *= s;
	}
};

template <class T>
class Object {
   protected:
	unsigned int vao, vbo[1];
	vec3 color = vec3(1.0f, 0.0f, 0.0f);
	float phi = 0;
	vec3 scaling = vec3(1, 1, 0), pos = vec3(0, 0, 0);

   public:
	Object() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	void updateGPU(std::vector<T> vec) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, vec.size() * sizeof(T), vec.data(), GL_STATIC_DRAW);
	}
	void Bind() {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	}
	void draw(GPUProgram* prog, int type, Camera& camera, std::vector<T> vec) {
		if (vec.size() > 0) {
			updateGPU(vec);	 // not here
			mat4 M = translate(pos) * rotate(phi, vec3(0, 0, 1)) * scale(scaling);
			mat4 MVP = camera.Projection() * camera.View() * M;
			prog->Use();
			prog->setUniform(MVP, "MVP");
			prog->setUniform(color, "color");
			glBindVertexArray(vao);
			glDrawArrays(type, 0, (int)vec.size());
		}
	}
};

float Rad(float deg) {
	return deg / 360.0f * 2 * M_PI;
}

float Deg(float rad) {
	return rad / (2 * M_PI) * 360.0f;
}

class Spline : public Object<vec2> {
	vec2 p;
	float M;

	float const fid = 1000;
	float const len = 1.0f / 2.0f * 1.0f;
	std::vector<std::vector<vec2>> vtx;

   public:
	Spline(float M, vec2 p) : M(M), p(p) {
		color = vec3(1.0f, 1.0f, 0.0f);
		for (int i = 0; i < 4; i++) {
			vtx.push_back(std::vector<vec2>());
		}

		update();
	}

	void update() {
		for (int i = 0; i < vtx.size(); i++) {
			vtx[i].clear();
			create_segment(vtx[i], i);
		}
	}

	void create_segment(std::vector<vec2>& vtx, int ind) {
		vec2 c = p;
		for (int i = 0; i < fid; i++) {
			if (c.x < 0) return;
			vtx.push_back(vec2(c.x, c.y));
			vec2 dir = normalize(vec2(1.0f, (ind % 2 * 2 - 1) * ddt(M, c.x)));
			c = c + float(ind / 2 % 2 * 2 - 1) * (dir * len / fid);
		}
	}

	float dt(float M, float r, float ra) {
		return r - ra + 2 * M * log(abs((r / M - 2) / (ra / M - 2)));
	}

	float ddt(float M, float r) {
		return r / (r - 2 * M);
	}

	using Object::draw;
	void draw(GPUProgram* gpuProgram, Camera& camera) {
		// update();
		glLineWidth(3);
		for (int i = 0; i < vtx.size(); i++) {
			draw(gpuProgram, GL_LINE_STRIP, camera, vtx[i]);
		}
	}
};

void print_vec(vec2 v) {
	printf("%f %f\n", v.x, v.y);
}

template <class T>
void swap(T& a, T& b) {
	T temp = a;
	a = b;
	b = temp;
}

class Grid : Object<vec2> {
	std::vector<vec2> vtx;

   public:
	Grid() {
		color = vec3(0.5f, 0.5f, 0.5f);
	}
	void update(Camera& camera) {
		vtx.clear();

		vec2 a = camera.convert(0, 0);
		vec2 b = camera.convert(winWidth, winHeight);
		int x1 = a.x < 0 ? 0 : a.x;
		int x2 = b.x < 0 ? 0 : b.x;
		int y1 = a.y;
		int y2 = b.y;
		if (x1 > x2) swap(x1, x2);
		if (y1 > y2) swap(y1, y2);

		for (int i = x1; i <= x2 + 1; i++) {
			vtx.push_back(vec2(i, a.y));
			vtx.push_back(vec2(i, b.y));
		}
		/* t-coordinates
		for (int i = y1; i <= y2 + 1; i++) {
			vtx.push_back(vec2(x1, i));
			vtx.push_back(vec2(x2 + 1, i));
		}*/
	}

	using Object::draw;
	void draw(GPUProgram* gpuProgram, Camera& camera) {
		update(camera);
		glLineWidth(1);
		draw(gpuProgram, GL_LINES, camera, vtx);
	}
};

class Singularity : Object<vec2> {
	std::vector<vec2> vtx;

   public:
	Singularity() {
		color = vec3(0.25f, 0.5f, 1.0f);
	}
	void update(Camera& camera) {
		vtx.clear();
		vec2 a = camera.convert(0, 0);
		vec2 b = camera.convert(winWidth, winHeight);

		vtx.push_back(vec2(0, a.y));
		vtx.push_back(vec2(0, b.y));
	}

	using Object::draw;
	void draw(GPUProgram* gpuProgram, Camera& camera) {
		glLineWidth(10);
		draw(gpuProgram, GL_LINES, camera, vtx);
	}
};

class Horizon : Object<vec2> {
	std::vector<vec2> vtx;

   public:
	Horizon() {
		color = vec3(0.25f, 1.0f, 0.5f);
	}
	void update(Camera& camera, float M) {
		vtx.clear();
		vec2 a = camera.convert(0, winHeight);
		vec2 b = camera.convert(winWidth, 0);
		float diff = 0.1 * camera.get_size();
		for (float i = floor(diff * floor(a.y / diff)); i < floor((diff + sign(b.y)) * floor(b.y / diff));
			 i += diff) {
			vtx.push_back(vec2(2 * M, i));
			vtx.push_back(vec2(2 * M, i + diff / 2.0f));
		}
	}

	using Object::draw;
	void draw(GPUProgram* gpuProgram, Camera& camera) {
		glLineWidth(2);
		draw(gpuProgram, GL_LINES, camera, vtx);
	}
};

class Scene {
	std::vector<Spline> splines;
	GPUProgram* gpuProgram;
	Camera* camera;
	Grid grid;
	Singularity sing;
	Horizon hor;
	float M = 1;

   public:
	Scene(GPUProgram* GPUProgram, Camera* camera) : gpuProgram(GPUProgram), camera(camera) {
		task();
	}
	void task() {
		add_spline(vec2(0.5 * M, 0.0f));
		add_spline(vec2(1 * M, 0.0f));
		add_spline(vec2(1.5 * M, 0.0f));
		add_spline(vec2(2 * M, 0.0f));
		add_spline(vec2(2.5 * M, 0.0f));
		add_spline(vec2(3 * M, 0.0f));
		add_spline(vec2(3.5 * M, 0.0f));
		add_spline(vec2(4 * M, 0.0f));
	}
	void add_spline(vec2 p) {
		splines.push_back(Spline(M, p));
	}
	void draw_cones() {
		for (auto& i : splines) {
			i.draw(gpuProgram, *camera);
		}
	}
	void draw() {
		grid.update(*camera);
		sing.update(*camera);
		hor.update(*camera, M);

		grid.draw(gpuProgram, *camera);
		sing.draw(gpuProgram, *camera);
		hor.draw(gpuProgram, *camera);
		draw_cones();
	}
};

class MyApp : public glApp {
	const float FPS = 60.0f;
	GPUProgram* gpuProgram;
	Scene* scene;
	float lastTime = 0.0f;
	bool pressed = false;
	vec2 pressedPos;

	vec2 size = vec2(10, 10);
	Camera camera = Camera(vec2(size.x / 2.0f, 0), size);

   public:
	MyApp() : glApp("") {}

	void onInitialization() override {
		gpuProgram = new GPUProgram(vertSource, fragSource);
		scene = new Scene(gpuProgram, &camera);
	}

	void onDisplay() override {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// multipliers in order to make it work properly on 2560*1440 screen
		glViewport(0, 0, winWidth * 2, winHeight * 2);
		scene->draw();
	}
	void onTimeElapsed(float startTime, float endTime) {
		// refreshScreen();
	}

	void onMousePressed(MouseButton but, int pX, int pY) override {
		vec2 p = camera.convert(pX, pY);

		if (but == MOUSE_LEFT) {
			if (p.x >= 0) {
				scene->add_spline(p);
			}
		} else {
			pressed = true;
			pressedPos = p;
		}
		refreshScreen();
	}

	void onMouseReleased(MouseButton but, int pX, int pY) {
		pressed = false;
	}

	void onMouseMotion(int pX, int pY) {
		if (pressed) {
			vec2 p = camera.convert(pX, pY);
			vec2 v = pressedPos - p;
			camera.addOrigo(v);
			refreshScreen();
		}
	}

	void onMouseScroll(float amount, int pX, int pY) {
		vec2 p = camera.convert(pX, pY);

		camera.zoom(p, 1.0f - amount * 0.1f);
		refreshScreen();
	}

	virtual void onKeyboard(int key) override {
		switch (key) {
			case 'x':
				break;
			case 'c':
				break;
			case 'n':
				break;
			default:
				break;
		}
		refreshScreen();
	}

	~MyApp() {}
};

MyApp app;