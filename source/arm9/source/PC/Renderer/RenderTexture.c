#include "RenderTexture.h"
#include <glew.h>
#include "Window.h"

RenderTexture* currRenderTexture = NULL;

void UseRenderTexture(RenderTexture* rt) {
	if (rt == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, NULL);
		glViewport(0, 0, GetWindowWidth(), GetWindowHeight());
	}
	else {
		glBindFramebuffer(GL_FRAMEBUFFER, rt->frameBuffer);
		glViewport(0, 0, rt->texture[0].width, rt->texture[0].height);
	}
	currRenderTexture = rt;
}

void DestroyRenderTexture(RenderTexture* rt) {
	for (int i = 0; i < rt->textureCount; ++i) {
		DeleteTexture(&rt->texture[i]);
	}
	glDeleteRenderbuffers(1, &rt->depthBuffer);
	glDeleteFramebuffers(1, &rt->frameBuffer);
}

RenderTexture* CreateRenderTexture(int width, int height, int type, bool cube, int MSAA, int count) {
	{
		RenderTexture* rt = (RenderTexture*)calloc(1, sizeof(RenderTexture));
		rt->textureCount = count;
		glGenFramebuffers(1, &rt->frameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->frameBuffer);
		for (int i = 0; i < count; ++i) {
			glGenTextures(1, &rt->texture[i].texRef);
			glBindTexture(cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, rt->texture[i].texRef);
			if (cube) {
				for (int i2 = 0; i2 < 6; ++i2) {
					if (MSAA == 0)
						glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i2, 0, GL_RGBA, width, height, 0, GL_RGBA, type, 0);
					else
						glTexImage2DMultisample(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i2, MSAA, GL_RGBA, width, height, false);
				}
			}
			else {
				if (MSAA == 0)
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, type, 0);
				else
					glTexImage2DMultisample(GL_TEXTURE_2D, MSAA, GL_RGBA, width, height, false);
			}
			rt->texture[i].width = width;
			rt->texture[i].height = height;
			// ensure no filtering
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		// depth buffer
		glGenRenderbuffers(1, &rt->depthBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, rt->depthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depthBuffer);
		// set render texture as our color attachment?
		for (int i = 0; i < count; ++i) {
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, rt->texture[i].texRef, 0);
		}
		// Set the list of draw buffers.
		GLenum DrawBuffers[8] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
		glDrawBuffers(count, DrawBuffers); // "1" is the size of DrawBuffers
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			// whoops, guess something's gone wrong
			DestroyRenderTexture(rt);
			free(rt);
			UseRenderTexture(currRenderTexture);
			return NULL;
		}
		// ffs hack
		RenderTexture* tmp = currRenderTexture;
		UseRenderTexture(rt);
		glDepthMask(GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		currRenderTexture = tmp;
		UseRenderTexture(currRenderTexture);
		return rt;
	}
}