#ifndef MINIMAL_STRING_H
#define MINIMAL_STRING_H

// strlen: get length of null-terminated string
static inline unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (s[len]) len++;
    return len;
}

// strcpy: copy null-terminated string src into dest (assumes dest is large enough)
static inline char* strcpy(char *dest, const char *src) {
    char *orig = dest;
    while ((*dest++ = *src++));
    return orig;
}

// strncpy: copy at most n chars from src into dest, pads with '\0' if shorter
static inline char* strncpy(char *dest, const char *src, unsigned int n) {
    char *orig = dest;
    unsigned int i = 0;
    for (; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return orig;
}

// strcmp: compare two strings; returns 0 if equal, <0 if s1 < s2, >0 if s1 > s2
static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)(*s1) - (unsigned char)(*s2);
}

// strncmp: compare at most n chars of two strings
static inline int strncmp(const char *s1, const char *s2, unsigned int n) {
    for (unsigned int i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0' || s2[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

#endif // MINIMAL_STRING_H
