/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __TECHFILE_H__
#define __TECHFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

  /* prefix = prepend to all paramters in case the configuration
     variables must be read into a separate namespace 
     Default should be NULL

     configuration files will be in the "layout" directory.
  */
void tech_init (const char *prefix, const char *techfilename);


#ifdef __cplusplus
}
#endif


#endif /* __TECHFILE_H__ */
