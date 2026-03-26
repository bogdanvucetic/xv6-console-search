#ifndef SEARCH_H
#define SEARCH_H

#define PROMPT "Search:"
#define PROMPT_LEN 7

#define SEARCH_COLOR 0xF000
#define HIGHLIGHT_COLOR 0xDF00

int search_mode_active = 0;
int old_pos;
short prev_content[2000];

char query[80 - PROMPT_LEN];
int qlen = 0;
int query_capacity = 80 - PROMPT_LEN - 2;

int matches[1920];
int mnum = 0;
int curr_highlighted = -1;

#endif
