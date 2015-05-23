#ifndef _RAII_H
#define _RAII_H

#define RAII(dtor) __attribute__((cleanup(dtor)))
#define RAII_SDS RAII(__sds_dtor)

static __attribute__((__unused__)) void __sds_dtor(sds* pstr) {
    if (*pstr) {
        sdsfree(*pstr);
    }
}

#endif
