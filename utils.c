//

void strxcpy(char *src, char *dst, int len)
{
    // for alignment (?) reasons strcpy and strncpy bus-err on string access
    int i = 0;
    for (; i < len && *(src+i) != 0; i++) *(dst+i) = *(src+i);
    for (; i < len; i++) *(dst+i) = 0;
}

// eof