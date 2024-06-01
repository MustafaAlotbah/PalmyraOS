#include "libs/ctype.h"


int isalnum(int c){
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalpha(int c){
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isblank(int c){
	return c == ' ' || c == '\t';
}

int iscntrl(int c){
	return (c >= 0 && c <= 0x1F) || c == 0x7F;
}

int isdigit(int c){
	return c >= '0' && c <= '9';
}

int isgraph(int c){
	return c > ' ' && c <= '~';
}

int islower(int c){
	return c >= 'a' && c <= 'z';
}

int isprint(int c){
	return c >= ' ' && c <= '~';
}

int ispunct(int c){
	return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}

int isspace(int c){
	return c == ' ' || c == '\n' || c == '\t' || c == '\v' || c == '\f' || c == '\r';
}

int isupper(int c){
	return c >= 'A' && c <= 'Z';
}

int isxdigit(int c){
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int tolower(int c){
	if (isupper(c)) return c + 32;
	return c;
}

int toupper(int c){
	if (islower(c)) return c - 32;
	return c;
}
