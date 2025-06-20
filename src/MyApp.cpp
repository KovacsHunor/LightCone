#include "../include/framework.h"
#include <GLFW/glfw3.h>
#include <iostream>

const char* vert_source = R"(
	#version 330				
    uniform mat4 MVP;
	layout(location = 0) in vec2 vertexPosition;

	void main() {
		gl_Position = MVP * vec4(vertexPosition, 0, 1);
	}
)";

const char* fragSource = R"(
	#version 330
    uniform vec4 color;
	out vec4 fragmentColor;
	
	void main() {
		fragmentColor = color;
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
	unsigned int vao{}, vbo[1];
	vec4 color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
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
	void draw(GPUProgram* prog, int type, Camera& camera, std::vector<T> vec, vec4 color) {
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
	void draw(GPUProgram* prog, int type, Camera& camera, std::vector<T> vec) {
		draw(prog, type, camera, vec, color);
	}
};

float Rad(float deg) {
	return deg / 360.0f * 2 * M_PI;
}

float Deg(float rad) {
	return rad / (2 * M_PI) * 360.0f;
}

class Cone : public Object<vec2> {
	vec2 p;
	float M;

	float const fid = 100;
	float length = 0.5f;
	std::vector<std::vector<vec2>> vtx;
	std::vector<vec2> triangle_vtx;
	Camera* cam;
	bool relative = false;

   public:
	Cone(float M, vec2 p, Camera& cam, bool relative = false) : M(M), p(p), cam(&cam), relative(relative){
		color = vec4(1.0f, 1.0f, 0.0f, 1.0f);
		for (int i = 0; i < 4; i++) {
			vtx.push_back(std::vector<vec2>());
		}
		update(relative);
	}


	void update(bool relative) {
		this->relative = relative;
		clear();
		if (floatCmp(2 * M, p.x)) {
			create_horizon_segment();
		} else {
			create_plus_segment();
			create_minus_segment();
		}
	}
	void create_horizon_segment() {
		vtx[0].push_back(vec2(p.x, p.y - get_len()));
		vtx[0].push_back(vec2(p.x, p.y + get_len()));
	}

	void create_plus_segment() {
		vec2 c = p;
		vec2 dir;
		int i;
		for (i = 0; i < fid; i++) {
			if (c.x < 0) break;
			vtx[0].push_back(vec2(c.x, c.y));
			dir = ((ddt(M, c.x) < 1) ? -1.0f : 1.0f) * normalize(vec2(1.0f, ddt(M, c.x)));
			c = c + (dir * get_len() / fid);
		}
		if (i == fid) {
			dir *= get_arrow_size();
			triangle_vtx.push_back(vec2(c.x, c.y) + dir);
			triangle_vtx.push_back(vec2(c.x, c.y) + vec2(dir.y, -dir.x));
			triangle_vtx.push_back(vec2(c.x, c.y) + vec2(-dir.y, dir.x));
		}

		c = p;
		for (i = 0; i < fid; i++) {
			if (c.x < 0) break;
			vtx[1].push_back(vec2(c.x, c.y));
			dir = ((ddt(M, c.x) < 1) ? -1.0f : 1.0f) * normalize(vec2(1.0f, ddt(M, c.x)));
			c = c - (dir * get_len() / fid);
		}
	}

	void create_minus_segment() {
		vec2 c = p;
		vec2 dir;
		int i;
		for (i = 0; i < fid; i++) {
			if (c.x < 0) break;
			vtx[2].push_back(vec2(c.x, c.y));
			dir = ((ddt(M, c.x) < 1) ? 1.0f : 1.0f) * normalize(vec2(-1.0f, ddt(M, c.x)));
			c = c + (dir * get_len() / fid);
		}
		if (i == fid) {
			dir *= get_arrow_size();
			triangle_vtx.push_back(vec2(c.x, c.y) + dir);
			triangle_vtx.push_back(vec2(c.x, c.y) + vec2(dir.y, -dir.x));
			triangle_vtx.push_back(vec2(c.x, c.y) + vec2(-dir.y, dir.x));
		}

		c = p;
		for (i = 0; i < fid; i++) {
			if (c.x < 0) break;
			vtx[3].push_back(vec2(c.x, c.y));
			dir = ((ddt(M, c.x) < 1) ? 1.0f : 1.0f) * normalize(vec2(-1.0f, ddt(M, c.x)));
			c = c - (dir * get_len() / fid);
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
		draw(gpuProgram, GL_TRIANGLES, camera, triangle_vtx);
	}

private:
	void clear() {
		for (auto& varr : vtx) {
		 	varr.clear();
		}
		triangle_vtx.clear();
	}

	float get_len() {
		if (relative) {
			return length*cam->get_size()*0.1f;
		}
		return length;
	}

	float get_arrow_size() {
		return get_len()*0.2f;
	}
};

void print_vec(vec2 v) {
	printf("%f %f\n", v.x, v.y);
}

size_t get_size_t(std::string& s) {
	return s.size();
}

template <class T>
void swap(T& a, T& b) {
	T temp = a;
	a = b;
	b = temp;
}

class Grid : Object<vec2> {
	std::vector<vec2> vtx_whole;
	std::vector<vec2> vtx_fractional;

   public:
	Grid() {
		color = vec4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	void update(Camera& camera) {
		vtx_whole.clear();
		vtx_fractional.clear();

		vec2 a = camera.convert(0, 0);
		vec2 b = camera.convert(winWidth, winHeight);
		int x1 = a.x < 0 ? 0 : a.x;
		int x2 = b.x < 0 ? 0 : b.x;
		int y1 = a.y;
		int y2 = b.y;
		if (x1 > x2) swap(x1, x2);
		if (y1 > y2) swap(y1, y2);

		for (int i = x1; i <= x2 + 1; i++) {
			vtx_whole.push_back(vec2(i, a.y));
			vtx_whole.push_back(vec2(i, b.y));
		}
		for (float i = x1; i <= x2 + 1; i += 0.5f) {
			vtx_fractional.push_back(vec2(i, a.y));
			vtx_fractional.push_back(vec2(i, b.y));
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
		draw(gpuProgram, GL_LINES, camera, vtx_fractional, vec4(0.1f, 0.1f, 0.1f, 0.0f));
		glLineWidth(1);
		draw(gpuProgram, GL_LINES, camera, vtx_whole, vec4(0.5f, 0.5f, 0.5f, 1.0f));
	}
};

class Singularity : Object<vec2> {
	std::vector<vec2> vtx;

   public:
	Singularity() {
		color = vec4(0.25f, 0.5f, 1.0f, 1.0f);
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
	float top = 0.0f;
	float bottom = 0.0f;

   public:
	Horizon() {
		color = vec4(0.25f, 1.0f, 0.5f, 1.0f);
	}
	void update(Camera& camera, float M) {
		vtx.clear();
		vec2 bottom_left = camera.convert(0, winHeight);
		vec2 top_right = camera.convert(winWidth, 0);
		float diff = 0.1 * camera.get_size();

		while (bottom >= bottom_left.y - 1.0f){
			bottom -= diff;
		}
		while (top <= top_right.y + 1.0f){
			top += diff;
		}

		for (float i = bottom; i < top; i += diff) {
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

enum MODE {
	PUT,
	FOLLOW
};

class Scene {
	std::vector<Cone> cones;
	GPUProgram* gpuProgram;
	Camera* camera;
	Grid grid;
	Singularity singularity;
	Horizon hor;
	float M = 1;
	vec2 mouse_pos;
	bool is_cone_size_dynamic = false;

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
		cones.push_back(Cone(M, p, *camera));
	}
	void draw_cones() {
		for (auto& i : cones) {
			i.draw(gpuProgram, *camera);
		}
	}
	void draw_cone() {
		Cone c = Cone(M, mouse_pos, *camera, is_cone_size_dynamic);
		c.draw(gpuProgram, *camera);
	}
	void draw(MODE mode) {

		grid.update(*camera);
		singularity.update(*camera);
		hor.update(*camera, M);
		for (auto& cone : cones) {
			cone.update(is_cone_size_dynamic);
		}

		grid.draw(gpuProgram, *camera);
		singularity.draw(gpuProgram, *camera);
		hor.draw(gpuProgram, *camera);
		draw_cones();
		if (mode == FOLLOW) {
			draw_cone();
		}
	}
	void set_mouse_pos(vec2 p) {
		mouse_pos = p;
	}
	void clear() {
		cones.clear();
	}

	void switch_dynamic() {
		is_cone_size_dynamic = !is_cone_size_dynamic;
	}
};

class MyApp : public glApp {
	const float FPS = 60.0f;
	GPUProgram* gpuProgram;
	Scene* scene;
	float lastTime = 0.0f;
	bool pressed = false;
	vec2 pressedPos;
	enum MODE mode = PUT;
	vec2 size = vec2(10, 10);
	Camera camera = Camera(vec2(size.x / 2.0f, 0), size);

   public:
	MyApp() : glApp("") {}

	void onInitialization() override {
		gpuProgram = new GPUProgram(vert_source, fragSource);
		scene = new Scene(gpuProgram, &camera);
	}

	void onDisplay() override {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// multipliers to make it work properly on 2560*1440 screens
		glViewport(0, 0, winWidth * 2, winHeight * 2);
		scene->draw(mode);
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
		scene->set_mouse_pos(camera.convert(pX, pY));
		if (pressed) {
			vec2 p = camera.convert(pX, pY);
			vec2 v = pressedPos - p;
			camera.addOrigo(v);
		}
		if (pressed || mode == FOLLOW) {
			refreshScreen();
		}
	}

	void onMouseScroll(float amount, int pX, int pY) {
		vec2 p = camera.convert(pX, pY);

		camera.zoom(p, 1.0f - amount * 0.1f);
		refreshScreen();
	}

	void onKeyboard(int key) override {
		switch (key) {
			case 'm':
				mode = (mode == PUT) ? FOLLOW : PUT;
				break;
			case 't':
				scene->task();
				break;
			case 'c':
				scene->clear();
				break;
			case 'r':
				scene->switch_dynamic();
			default:
				break;
		}
		refreshScreen();
	}

	~MyApp() {}
};

MyApp app;