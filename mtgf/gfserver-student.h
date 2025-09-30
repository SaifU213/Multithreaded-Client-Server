/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "gfserver.h"
#include "content.h"


void init_threads(size_t numthreads);
void cleanup_threads();

struct handler_item {
  gfcontext_t *ctx;
  char *path;
};


#endif // __GF_SERVER_STUDENT_H__
