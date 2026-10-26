/* SPDX-License-Identifier: GPL-3.0-or-later
 * This first PXA port preserves upstream gameplay but deliberately has no
 * SDL_mixer audio backend. Original audio source and assets remain upstream.
 */
void InitAudio(void) {}
void MusicUpdate(void) {}
void TitleScreenMusic(void) {}
void SND_CircuitRelease(int strength) {(void)strength;}
void SND_Pos(char *name,int volume,int distance) {(void)name;(void)volume;(void)distance;}
