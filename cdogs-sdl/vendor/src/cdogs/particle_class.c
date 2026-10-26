/*
	C-Dogs SDL
	A port of the legendary (and fun) action/arcade cdogs.
	Copyright (c) 2014-2017, 2019-2021, 2024, 2026 Cong Xu
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.
	Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include "particle_class.h"

#include "json_utils.h"
#include "log.h"
#include "pic_manager.h"
#include "sys_config.h"

ParticleClasses gParticleClasses;

#define VERSION 3

ParticleType StrParticleType(const char *s)
{
	S2T(PARTICLE_PIC, "Pic");
	S2T(PARTICLE_TEXT, "Text");
	S2T(PARTICLE_CHAR_SPRITE, "CharSprite");
	return PARTICLE_PIC;
}

static void LoadParticleClass(
	ParticleClass *c, json_t *node, const int version);
void ParticleClassesInit(ParticleClasses *classes, const char *filename)
{
	CArrayInit(&classes->Classes, sizeof(ParticleClass));
	CArrayInit(&classes->CustomClasses, sizeof(ParticleClass));

	char buf[CDOGS_PATH_MAX];
	GetDataFilePath(buf, filename);
	FILE *f = fopen(buf, "r");
	json_t *root = NULL;
	if (f == NULL)
	{
		LOG(LM_MAIN, LL_ERROR, "Error: cannot load particles file %s", buf);
		goto bail;
	}
	enum json_error e = json_stream_parse(f, &root);
	if (e != JSON_OK)
	{
		LOG(LM_MAIN, LL_ERROR, "Error parsing particles file %s", buf);
		goto bail;
	}
	ParticleClassesLoadJSON(&classes->Classes, root);

bail:
	if (f != NULL)
	{
		fclose(f);
	}
	json_free_value(&root);
}
void ParticleClassesLoadJSON(CArray *classes, json_t *root)
{
	int version;
	LoadInt(&version, root, "Version");
	if (version > VERSION || version <= 0)
	{
		CASSERT(false, "cannot read particles file version");
		return;
	}

	json_t *particlesNode = json_find_first_label(root, "Particles")->child;
	for (json_t *child = particlesNode->child; child; child = child->next)
	{
		ParticleClass c;
		LoadParticleClass(&c, child, version);
		CArrayPushBack(classes, &c);
	}
}
void ParticleClassesTerminate(ParticleClasses *classes)
{
	ParticleClassesClear(&classes->Classes);
	CArrayTerminate(&classes->Classes);
	ParticleClassesClear(&classes->CustomClasses);
	CArrayTerminate(&classes->CustomClasses);
}
void ParticleClassesClear(CArray *classes)
{
	for (int i = 0; i < (int)classes->size; i++)
	{
		ParticleClass *c = CArrayGet(classes, i);
		CFREE(c->Name);
		switch (c->Type)
		{
		case PARTICLE_TEXT:
			CFREE(c->u.Text.Value);
			break;
		case PARTICLE_CHAR_SPRITE:
			CFREE(c->u.CharSprite);
			break;
		default:
			break;
		}
	}
	CArrayClear(classes);
}
static void LoadParticleClass(
	ParticleClass *c, json_t *node, const int version)
{
	memset(c, 0, sizeof *c);
	char *tmp;

	c->Name = GetString(node, "Name");

	c->Type = PARTICLE_PIC;
	if (version < 2)
	{
		c->u.Pic.Mask = colorWhite;
		if (json_find_first_label(node, "Sprites"))
		{
			tmp = GetString(node, "Sprites");
			c->u.Pic.Type = PICTYPE_DIRECTIONAL;
			c->u.Pic.u.Sprites =
				&PicManagerGetSprites(&gPicManager, tmp)->pics;
			CFREE(tmp);
		}
		else
		{
			LoadPic(&c->u.Pic.u.Pic, node, "Pic");
		}
		if (json_find_first_label(node, "Mask"))
		{
			tmp = GetString(node, "Mask");
			c->u.Pic.Mask = StrColor(tmp);
			CFREE(tmp);
		}
		int ticksPerFrame = 0;
		LoadInt(&ticksPerFrame, node, "TicksPerFrame");
		if (ticksPerFrame > 0)
		{
			c->u.Pic.Type = PICTYPE_ANIMATED;
			c->u.Pic.u.Animated.Sprites = c->u.Pic.u.Sprites;
			c->u.Pic.u.Animated.TicksPerFrame = ticksPerFrame;
		}
	}
	else
	{
		tmp = NULL;
		LoadStr(&tmp, node, "Type");
		if (tmp != NULL)
		{
			c->Type = StrParticleType(tmp);
			CFREE(tmp);
		}
		switch (c->Type)
		{
		case PARTICLE_PIC:
			CPicLoadJSON(&c->u.Pic, json_find_first_label(node, "Pic")->child);
			break;
		case PARTICLE_TEXT:
			c->u.Text.Mask = colorWhite;
			if (version <= 2)
			{
				tmp = NULL;
				LoadStr(&tmp, node, "TextMask");
				if (tmp != NULL)
				{
					c->u.Text.Mask = StrColor(tmp);
				}
				CFREE(tmp);
			}
			else
			{
				if (json_find_first_label(node, "Text"))
				{
					json_t *text = json_find_first_label(node, "Text")->child;
					tmp = NULL;
					LoadStr(&tmp, text, "Mask");
					if (tmp != NULL)
					{
						c->u.Text.Mask = StrColor(tmp);
					}
					CFREE(tmp);

					LoadStr(&c->u.Text.Value, text, "Value");
				}
			}
			break;
		case PARTICLE_CHAR_SPRITE:
			LoadStr(&c->u.CharSprite, node, "CharSprite");
			break;
		default:
			break;
		}
	}

	if (json_find_first_label(node, "Range"))
	{
		LoadInt(&c->RangeLow, node, "Range");
		c->RangeHigh = c->RangeLow;
	}
	if (json_find_first_label(node, "RangeLow"))
	{
		LoadInt(&c->RangeLow, node, "RangeLow");
	}
	if (json_find_first_label(node, "RangeHigh"))
	{
		LoadInt(&c->RangeHigh, node, "RangeHigh");
	}
	c->RangeLow = MIN(c->RangeLow, c->RangeHigh);
	c->RangeHigh = MAX(c->RangeLow, c->RangeHigh);
	LoadFloat(&c->GravityFactor, node, "GravityFactor");
	LoadBool(&c->HitsWalls, node, "HitsWalls");
	c->Bounces = true;
	LoadBool(&c->Bounces, node, "Bounces");
	LoadFloat(&c->BounceFriction, node, "BounceFriction");
	c->WallBounces = true;
	LoadBool(&c->WallBounces, node, "WallBounces");
	LoadBool(&c->ZDarken, node, "ZDarken");
	LoadBool(&c->DrawBelow, node, "DrawBelow");
	LoadBool(&c->DrawAbove, node, "DrawAbove");
}

const ParticleClass *StrParticleClass(
	const ParticleClasses *classes, const char *name)
{
	if (name == NULL || strlen(name) == 0)
	{
		return NULL;
	}
	CA_FOREACH(const ParticleClass, c, classes->CustomClasses)
	if (strcmp(c->Name, name) == 0)
	{
		return c;
	}
	CA_FOREACH_END()
	CA_FOREACH(const ParticleClass, c, classes->Classes)
	if (strcmp(c->Name, name) == 0)
	{
		return c;
	}
	CA_FOREACH_END()
	LOG(LM_MAIN, LL_ERROR, "Cannot find particle class %s", name);
	return NULL;
}
