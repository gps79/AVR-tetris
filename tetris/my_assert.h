/*
 * my_assert.h
 *
 * Created: 2016.12.04 17:48:24
 *  Author: Grzegorz
 */ 


#ifndef MY_ASSERT_H_
#define MY_ASSERT_H_

#  if defined(NDEBUG)
#    define assert(e)	((void)0)
#  else /* !NDEBUG */
#    if defined(__ASSERT_USE_STDERR)
#      define assert(e)	((e) ? (void)0 : \
__assert(__FILE__, __LINE__))
#    else /* !__ASSERT_USE_STDERR */
#      define assert(e)	((e) ? (void)0 : abort())
#    endif /* __ASSERT_USE_STDERR */
#  endif /* NDEBUG */

extern void __assert(const char *__file, int __lineno);

#endif /* MY_ASSERT_H_ */