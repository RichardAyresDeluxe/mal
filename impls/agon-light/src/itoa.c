#include "itoa.h"

#include <math.h>

static void reverse(char *ptr1, int l)
{
    char *ptr = &ptr1[l - 1];
    while(ptr1 < ptr) {
        char tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
}

/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
/* TODO: make this take a max len parameter to guard against overflows */
char* itoa(long value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result;
    long tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
  
    reverse(ptr1, 1+ptr-ptr1);

    return result;
}

static int itoa10(long num, char *str, int d)
{
    int i = 0;
    while (num) {
        if (str)
            str[i] = (num % 10) + '0';
        i++;
        num = num / 10;
    }

    // If number of digits is less, then add leading 0s
    while (i < d) {
        if (str)
            str[i] = '0';
        i++;
    }

    if (str) {
        reverse(str, i);
        str[i] = '\0';
    }

    return i;
}

char* ftoa(double value, char* result, int precision)
{
    long lval;
    char *p = result;

    /* if negative, insert '-' and make positive */
    if (value < 0) {
        *p++ = '-';
        value *= -1;
    }

    /* convert integer part to string */
    lval = (long)value;
    itoa(lval, p, 10);

    /* find end of integer part */
    while (*p != '\0')
        p++;

    if (precision > 0) {
        *p++ = '.';

        value -= lval;
        itoa10(round(value * pow(10, precision)), p, precision);
    }

    return result;
}
