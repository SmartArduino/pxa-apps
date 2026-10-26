/*
	C-Dogs SDL
	A port of the legendary (and fun) action/arcade cdogs.
	Copyright (c) 2014-2015, 2017-2019, 2021, 2024, 2026 Cong Xu
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
#pragma once

#include "character.h"
#include "particle_class.h"

typedef struct
{
	const ParticleClass *Class;
	union {
		CPic Pic;
		char *Text;
		const Character *Char;
	} u;
	int ActorUID;
	struct vec2 Pos;
	float Z;
	double Angle;
	float DZ;
	double Spin;
	int Count;
	int Range;
	Thing thing;
	bool isAttached;
	bool isInUse;
} Particle;
extern CArray gParticles; // of Particle

typedef struct
{
	const ParticleClass *Class;
	int ActorUID;
	struct vec2 Pos;
	float Z;
	struct vec2 Vel;
	double Angle;
	float DZ;
	double Spin;
	struct vec2 DrawScale;
	color_t Mask;
	char Text[128];
	bool IsAttached;
} AddParticle;

void ParticlesInit(CArray *particles);
void ParticlesTerminate(CArray *particles);
void ParticlesUpdate(CArray *particles, const int ticks);

int ParticleAdd(CArray *particles, const AddParticle add);
void ParticleDestroy(CArray *particles, const int id);
