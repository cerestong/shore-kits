
/** @file trace_print_pthread.cpp
 *
 *  @brief Implements trace_print_pthread(). Unfortunately, POSIX does
 *  not specify the representation of a pthread_t. This file provides
 *  a portable way to print a pthread_t instance.
 *
 *  @bug None known.
 */
#include "trace_print_pthread.h" /* for prototypes */




/* definitions of exported functions */


/**
 *  @brief Print the specified pthread_t to the specified stream. This
 *  function is reentrant. It does not flush the specified stream when
 *  complete.
 *
 *  @param out_stream The stream to print the pthread_t to.
 *
 *  @param thread The pthread_t to print.
 *
 *  @return void
 */
void trace_print_pthread(FILE* out_stream, pthread_t thread)
{

  /* operating system specific */

  /* GNU Linux */
#if defined(linux) || defined(__linux)
#ifndef __USE_GNU
#define __USE_GNU
#endif

  /* detected GNU Linux */
  /* A pthread_t is an unsigned long int. */
  fprintf(out_stream, "%lu", thread);
  return;

#endif


  /* Sun Solaris */
#if defined(sun) || defined(__sun)
#if defined(__SVR4) || defined(__svr4__)

  /* detected Sun Solaris */
  /* A pthread_t is an unsigned int. */
  fprintf(out_stream, "%u", thread);
  return;

#endif
#endif

}
