#include <malloc.h>
#include <stdlib.h>
#include <esp8266.h>

#if 1
void * _malloc_r(struct _reent * r, size_t sz) 
{
    return os_malloc(sz);
}

void *_realloc_r(struct _reent * r, void * x, size_t sz)
{
    return os_realloc(x, sz);
}

_VOID _free_r _PARAMS ((struct _reent * r, _PTR x))
//ivoid * _free_r(struct _reent * r, void * x)
{
    return os_free(x);
}
#endif
