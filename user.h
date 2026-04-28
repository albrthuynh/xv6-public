#pragma once
#include "types.h"
struct stat;
struct rtcdate;
struct win_event;
struct window;
struct dirent;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(uint64);
int sleep(int);
int uptime(void);
int win_create(int, int, int, int);
int win_destroy(int);
int win_poll(int, struct win_event *);
int win_focus(int);
int win_get_focus(void);
int win_move(int, int, int);
int win_snapshot(struct window *, int);
int readdir(int, struct dirent *);
int draw_pixel(int, int, int);
int draw_rect(int, int, int, int, int);
int win_set_compositor(int);
int win_post_event(int, struct win_event *);
int read_pixel(int, int);
int draw_bitmap(int, int, int, int, void*);
int win_get_compositor(void);
int halt(void);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
