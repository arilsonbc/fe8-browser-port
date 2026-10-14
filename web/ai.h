#ifndef FE8_AI_H
#define FE8_AI_H
int fe8_ai_script_probe(int index,int kind,int startPc);
int fe8_ai_script_table_probe(int index,int kind,int id,int pc);
int fe8_ai_script_info(int field);
void fe8_ai_reset(void);
int fe8_ai_prepare(void);
int fe8_ai_info(int field);
int fe8_ai_danger_probe(int index);
int fe8_ai_danger_at(int x,int y);
int fe8_ai_tick(void);
int fe8_ai_offensive_probe(int index,int weight,int stay);
int fe8_ai_offensive_info(int field);
#endif
