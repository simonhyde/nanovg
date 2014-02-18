#include "perf.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef NANOVG_GLEW
#  include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>
#include "nanovg.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#else
#include <iconv.h>
#endif

// timer query support
#ifndef GL_ARB_timer_query
#define GL_TIME_ELAPSED                   0x88BF
typedef void (APIENTRY *pfnGLGETQUERYOBJECTUI64V)(GLuint id, GLenum pname, GLuint64* params);
pfnGLGETQUERYOBJECTUI64V glGetQueryObjectui64v = 0;
#endif

void initGPUTimer(struct GPUtimer* timer)
{
	memset(timer, 0, sizeof(*timer));

	timer->supported = glfwExtensionSupported("GL_ARB_timer_query");
	if (timer->supported) {
#ifndef GL_ARB_timer_query
		glGetQueryObjectui64v = (pfnGLGETQUERYOBJECTUI64V)glfwGetProcAddress("glGetQueryObjectui64v");
		if (!glGetQueryObjectui64v) {
			timer->supported = GL_FALSE;
			return;
		}
#endif
		glGenQueries(GPU_QUERY_COUNT, timer->queries);
	}
}

void startGPUTimer(struct GPUtimer* timer)
{
	if (!timer->supported)
		return;
	glBeginQuery(GL_TIME_ELAPSED, timer->queries[timer->cur % GPU_QUERY_COUNT] );
	timer->cur++;
}

int stopGPUTimer(struct GPUtimer* timer, float* times, int maxTimes)
{
	GLint available = 1;
	int n = 0;
	if (!timer->supported)
		return 0;

	glEndQuery(GL_TIME_ELAPSED);
	while (available && timer->ret <= timer->cur) {
		// check for results if there are any
		glGetQueryObjectiv(timer->queries[timer->ret % GPU_QUERY_COUNT], GL_QUERY_RESULT_AVAILABLE, &available);
		if (available) {
			GLuint64 timeElapsed = 0;
			glGetQueryObjectui64v(timer->queries[timer->ret % GPU_QUERY_COUNT], GL_QUERY_RESULT, &timeElapsed);
			timer->ret++;
			if (n < maxTimes) {
				times[n] = (float)((double)timeElapsed * 1e-9);
				n++;
			}
		}
	}
	return n;
}


void initFPS(struct FPScounter* fps, int style, const char* name)
{
	memset(fps, 0, sizeof(struct FPScounter));
	fps->style = style;
	strncpy(fps->name, name, sizeof(fps->name));
	fps->name[sizeof(fps->name)-1] = '\0';
}

void updateFPS(struct FPScounter* fps, float frameTime)
{
	fps->head = (fps->head+1) % FPS_HISTORY_COUNT;
	fps->values[fps->head] = frameTime;
}

#define AVG_SIZE 20

static float getAvg(struct FPScounter* fps)
{
	int i, head = fps->head;
	float avg = 0;
	for (i = 0; i < AVG_SIZE; i++) {
		avg += fps->values[head];
		head = (head+FPS_HISTORY_COUNT-1) % FPS_HISTORY_COUNT;
	}
	return avg / (float)AVG_SIZE;
}

void renderFPS(struct NVGcontext* vg, float x, float y, struct FPScounter* fps)
{
	int i;
	float avg, w, h;
	char str[64];

	avg = getAvg(fps);

	w = 200;
	h = 35;

	nvgBeginPath(vg);
	nvgRect(vg, x,y, w,h);
	nvgFillColor(vg, nvgRGBA(0,0,0,128));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y+h);
	if (fps->style == FPS_RENDER_FPS) {
		for (i = 0; i < FPS_HISTORY_COUNT; i++) {
			float v = 1.0f / (0.00001f + fps->values[(fps->head+i) % FPS_HISTORY_COUNT]);
			if (v > 80.0f) v = 80.0f;
			float vx = x + ((float)i/(FPS_HISTORY_COUNT-1)) * w;
			float vy = y + h - ((v / 80.0f) * h);
			nvgLineTo(vg, vx, vy);
		}
	} else {
		for (i = 0; i < FPS_HISTORY_COUNT; i++) {
			float v = fps->values[(fps->head+i) % FPS_HISTORY_COUNT] * 1000.0f;
			if (v > 20.0f) v = 20.0f;
			float vx = x + ((float)i/(FPS_HISTORY_COUNT-1)) * w;
			float vy = y + h - ((v / 20.0f) * h);
			nvgLineTo(vg, vx, vy);
		}
	}
	nvgLineTo(vg, x+w, y+h);
	nvgFillColor(vg, nvgRGBA(255,192,0,128));
	nvgFill(vg);

	nvgFontFace(vg, "sans");

	if (fps->name[0] != '\0') {
		nvgFontSize(vg, 14.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
		nvgFillColor(vg, nvgRGBA(240,240,240,192));
		nvgText(vg, x+3,y+1, fps->name, NULL);
	}

	if (fps->style == FPS_RENDER_FPS) {
		nvgFontSize(vg, 18.0f);
		nvgTextAlign(vg,NVG_ALIGN_RIGHT|NVG_ALIGN_TOP);
		nvgFillColor(vg, nvgRGBA(240,240,240,255));
		sprintf(str, "%.2f FPS", 1.0f / avg);
		nvgText(vg, x+w-3,y+1, str, NULL);
		
		nvgFontSize(vg, 15.0f);
		nvgTextAlign(vg,NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
		nvgFillColor(vg, nvgRGBA(240,240,240,160));
		sprintf(str, "%.2f ms", avg * 1000.0f);
		nvgText(vg, x+w-3,y+h-1, str, NULL);
	} else {
		nvgFontSize(vg, 18.0f);
		nvgTextAlign(vg,NVG_ALIGN_RIGHT|NVG_ALIGN_TOP);
		nvgFillColor(vg, nvgRGBA(240,240,240,255));
		sprintf(str, "%.2f ms", avg * 1000.0f);
		nvgText(vg, x+w-3,y+1, str, NULL);
	}
}
