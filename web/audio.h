#ifndef FE8_WEB_AUDIO_H
#define FE8_WEB_AUDIO_H
void fe8_audio_reset(void);
void fe8_audio_bgm(int song,int speed);
void fe8_audio_se(int song);
void fe8_audio_fadein(int song,int speed);
void fe8_audio_fadeout(int speed);
void fe8_audio_override(int song);
void fe8_audio_restore(int speed);
void fe8_audio_volume(int from,int to,int frames);
int fe8_audio_map_song(void);
void fe8_audio_map_start(void);
int fe8_audio_count(void);
int fe8_audio_read(int field);
void fe8_audio_pop(void);
int fe8_audio_info(int field);
#endif
