#ifndef SEARCH_H
#define SEARCH_H

#define PROMPT "Search:"
#define PROMPT_LEN 7

#define SEARCH_COLOR 0xF000
#define HIGHLIGHT_COLOR 0xD700

int search_mode_active = 0;
int old_pos;

char query[80 - PROMPT_LEN];
int qlen = 0;
int query_capacity = 80 - PROMPT_LEN - 2;

int matches[1920];
int mnum = 0;

#endif
