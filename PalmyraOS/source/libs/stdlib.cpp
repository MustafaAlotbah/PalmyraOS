#include "libs/stdlib.h"
#include "libs/ctype.h"

long strtol(const char* nptr, char** endptr, int base)
{
	long result = 0;
	int sign = 1;

	// Skip whitespace
	while (isspace(*nptr)) nptr++;

	// Handle sign
	if (*nptr=='-' || *nptr=='+') {
		sign = (*nptr++=='-') ? -1 : 1;
	}

	// Validate base
	if (base==0 || base==16) {
		if (*nptr=='0' && (*(nptr+1)=='x' || *(nptr+1)=='X')) {
			base = 16;
			nptr += 2;
		}
	}
	if (base==0) {
		if (*nptr=='0') {
			base = 8;
			nptr++;
		}
		else {
			base = 10;
		}
	}

	// Convert number
	unsigned long digit;
	while (*nptr) {
		if (isdigit(*nptr)) {
			digit = *nptr-'0';
		}
		else if (isalpha(*nptr)) {
			digit = toupper(*nptr)-'A'+10;
		}
		else {
			break;
		}

		if (digit>=(unsigned)base) {
			break;
		}

		result = result*base+digit;
		nptr++;
	}

	// Set endptr
	if (endptr!=nullptr) {
		*endptr = (char*)nptr;
	}

	return result*sign;
}

unsigned long strtoul(const char* nptr, char** endptr, int base)
{
	unsigned long result = 0;

	// Skip whitespace
	while (isspace(*nptr)) nptr++;

	// Validate base
	if (base==0 || base==16) {
		if (*nptr=='0' && (*(nptr+1)=='x' || *(nptr+1)=='X')) {
			base = 16;
			nptr += 2;
		}
	}
	if (base==0) {
		if (*nptr=='0') {
			base = 8;
			nptr++;
		}
		else {
			base = 10;
		}
	}

	// Convert number
	unsigned long digit;
	while (*nptr) {
		if (isdigit(*nptr)) {
			digit = *nptr-'0';
		}
		else if (isalpha(*nptr)) {
			digit = toupper(*nptr)-'A'+10;
		}
		else {
			break;
		}

		if (digit>=(unsigned)base) {
			break;
		}

		result = result*base+digit;
		nptr++;
	}

	// Set endptr
	if (endptr!=nullptr) {
		*endptr = (char*)nptr;
	}

	return result;
}

int atoi(const char* str)
{
	return (int) strtol(str, nullptr, 10);
}

void itoa(int num, char* str, int base, bool upper_case)
{
	int i = 0;
	bool isNegative = false;

	if (num == 0) {
		str[i++] = '0';
		str[i] = '\0';
		return;
	}

	if (num < 0 && base == 10) {
		isNegative = true;
		num = -num;
	}

	// Process individual digits
	while (num != 0) {
		int rem = num % base;
		if (rem > 9) {
			char offset = upper_case ? 'A' - 10 : 'a' - 10;
			str[i++] = rem + offset;
		} else {
			str[i++] = rem + '0';
		}
		num = num / base;
	}

	// If number is negative, append '-'
	if (isNegative && base == 10) {
		str[i++] = '-';
	}

	str[i] = '\0'; // Null-terminate string

	// Reverse the string
	reverse(str, i);
}

void reverse(char str[], int length)
{
	int start = 0;
	int end = length-1;
	while (start<end) {
		char temp = str[start];
		str[start] = str[end];
		str[end] = temp;
		start++;
		end--;
	}
}