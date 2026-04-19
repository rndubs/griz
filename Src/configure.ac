# $Id$
#	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#	+                                                               +
#	+                       Copyright (c) 2004                      +
#	+          The Regents of the University of California          +
#	+                      All Rights Reserved                      +
#	+                                                               +
#	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
#	*****************************************************************
#	*								*
#	*	     Griz4 configure - File "configure.ac"    		*
#	*								*
#	*****************************************************************
#
#	Process this file with "autoconf" to produce "configure".
#
#	-----------------------------------------------------------------
#	Revision History:
#
#	20-Feb-04 IRC: Created
#
#	05-Nov-04 trt: Renamed to configure.ac as per GNU convensions
#	24-Nov-04 trt: Added Fortran compiler/library support
#	30-Nov-04 trt: Added useful variables.
#	02-Oct-07 IRC: Added Configure for KlocWorks
#			SCR #492.
#	-----------------------------------------------------------------
#
#
# The Regents of the University of California.  
# All rights reserved.
# 
# This work was produced at the University of California, 
# Lawrence Livermore National Laboratory (UC LLNL) under 
# contract no. W-7405-ENG-48 between the U.S. Department of 
# Energy (DOE) and The Regents of the University of California
# (University) for   the operation of UC LLNL. Copyright is 
# reserved to the University for purposes of controlled dis-
# semination, commercialization through formal licensing, or 
# other disposition under terms of Contract 48; DOE policies, 
# regulations and orders; and U.S. statutes.
# 
# DISCLAIMER OF LIABILITY
# 
# This document was prepared as an account of work sponsored 
# by an agency of the United States Government.  Neither the 
# United States Government nor the University of California 
# nor any of their employees, makes any warranty, express
# or implied, including the warranties of merchantability
# and fitness for a particular purpose, or assumes any legal
# liability or responsibility for the accuracy, completeness,
# or usefulness of any information, apparatus, product, or 
# process disclosed, or represents that its use would not 
# infringe privately-owned rights.
# 
# DISCLAIMER OF ENDORSEMENT
# 
# Reference herein to any specific commercial products,
# process, or service by trade name, trademark, manufacturer,
# or otherwise, does not necessarily constitute or imply its
# endorsement, recommendation, or favoring by the United States
# Government or the University of California.  The views and 
# opinions of authors expressed herein do not necessarily state
# or reflect those of the United States Government or the 
# University of California, and shall not be used for advertising
# or product endorsement purposes.
#
#############################################################################
#
#	Initialize AutoConf.
#
#############################################################################

m4_define(GRIZ_NAME_M4, Griz4)
m4_define(GRIZ_VERSION_M4, V23_01_04)
m4_define(GRIZ_MAJOR_M4, 23)
m4_define(GRIZ_MINOR_M4, 1)
m4_define(GRIZ_BUG_M4, 4)
m4_define(MILI_VERSION_M4, V23_01)
m4_define(GRIZ_DATE_M4, 12/13/2022)
m4_define(GRIZ_TIME_M4, 07:00:00)
m4_define(GRIZ_DATETIME_M4, [GRIZ_DATE_M4 GRIZ_TIME_M4])
m4_define(GRIZ_BUGS_M4,[Kevin Durrenberger  ext: 4-3015 email: durrenberger1@llnl.gov])
m4_define(GRIZ_TOP_SRC_DIR_M4, `pwd`)

AC_INIT(GRIZ_NAME_M4, GRIZ_VERSION_M4, GRIZ_BUGS_M4,GRIZ_MAJOR_VERSION_M4,GRIZ_MINOR_VERSION_M4,GRIZ_BUG_VERSION_M4)

AC_CONFIG_AUX_DIR(scripts)

AC_CANONICAL_HOST

GRIZ_VERSION=GRIZ_VERSION_M4
MILI_VERSION=MILI_VERSION_M4
AC_SUBST([GRIZ_VERSION])
AC_SUBST([MILI_VERSION])

AC_REVISION($Revision: 1.7 $)
#TARGET_DNS_DOMAIN="`(dnsdomainname) 2> /dev/null`"
TARGET_DNS_DOMAIN="llnl.gov"
AC_PREREQ(2.59)
AC_DEFINE_UNQUOTED(CONFIG_CMD, "$0 $@", [Configure command])
CONFIG_CMD="configure $@"
AC_SUBST([CONFIG_CMD])

AC_COPYRIGHT([This work was produced at the University of California, 
Lawrence Livermore National Laboratory (UC LLNL) under 
contract no. W-7405-ENG-48 between the U.S. Department of 
Energy (DOE) and The Regents of the University of California
(University) for   the operation of UC LLNL. Copyright is 
reserved to the University for purposes of controlled dis-
semination, commercialization through formal licensing, or 
other disposition under terms of Contract 48; DOE policies, 
regulations and orders; and U.S. statutes.])

# Variable Initialization
AC_DEFINE([PACKAGE_NAME], ["GRIZ_NAME_M4"], [Name of release])
AC_DEFINE([PACKAGE_DATE], ["GRIZ_DATE_M4"], [Date of release])
AC_DEFINE([PACKAGE_TIME], ["GRIZ_TIME_M4"], [Time of release])
AC_DEFINE([GRIZ_MAJOR], ["GRIZ_MAJOR_M4"], [Major version of the release])
AC_DEFINE([GRIZ_MINOR], ["GRIZ_MINOR_M4"], [Minor version of the release])
AC_DEFINE([GRIZ_BUG], ["GRIZ_BUG_M4"], [Bug version of the release])
AC_DEFINE([PACKAGE_DATETIME], ["GRIZ_DATETIME_M4"], [Date and time of release])
AC_DEFINE([PACKAGE_BUGREPORT_ALT],[""],[Alternate contact for bugs])
#AC_DEFINE([PACKAGE_BUGREPORT_ALT],["Kevin Durrenberger ext:4-3015 email:durrenberger1@llnl.gov"],[Alternate contact for bugs])
#PACKAGE_DATETIME="GRIZ_DATE_M4 GRIZ_TIME_M4"
#AC_SUBST([PACKAGE_DATETIME])

AC_PROG_CC([icc xlc gcc cc])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS(gethostname regcomp strstr isnan)

AC_CHECK_SIZEOF([long])

#############################################################################
#
#	Create the AutoConf environment variables for export.
#
#############################################################################
AC_SUBST(ARFLAGS)

# CC Options
#AC_SUBST(CC)
AC_SUBST(CC_INCLUDE_PATHS)
AC_SUBST(CC_FLAGS_DEBUG)
AC_SUBST(CC_FLAGS_OPT)
AC_SUBST(CC_DEFINES)
AC_SUBST(CC_DEPEND)

AC_SUBST(LOADER)
AC_SUBST(LD_FLAGS_DEBUG)
AC_SUBST(LD_FLAGS_OPT)

AC_SUBST(EXE_SUFFIX)
AC_SUBST(LIB_SUFFIX)

AC_SUBST(LIBRARY_PATHS)
AC_SUBST(LIBS)
AC_SUBST(OTHER_LIBS)

AC_SUBST(GRIZ_VERBOSE)

# AR variables
#AC_SUBST(AR)
#AC_SUBST(AR_FLAGS)


###################################
# GRIZ specific variables
###################################

# Mili Library Options
AC_SUBST(MILI_HOME)
AC_SUBST(MILI_INCLUDE_PATHS)
AC_SUBST(MILI_LIBRARY_PATHS)
AC_SUBST(MILI_DEFINES)
AC_SUBST(MILI_LIBRARY)
AC_SUBST(ROOT_DIR)
AC_SUBST(GRIZLIBS)

CONFIGURE_64bit

CONFIGURE_INIT

CONFIGURE_VERSION

CONFIGURE_OS

CONFIGURE_HOST

CONFIGURE_MILI

CONFIGURE_DIRS

CONFIGURE_BUILDDIRS

CONFIGURE_GRIZ_LIBRARIES

AC_CONFIG_HEADERS([$PATH_NAME/src/griz_config.h:scripts/config.h.in])

JPEG_ENABLE="True"  
CONFIG_JPEG="True"
AC_ARG_ENABLE([nojpeg],       
   AC_HELP_STRING([--enable-nojpeg],[Turn off JPEG support]),
	[
		JPEG_ENABLE="False" 
		CONFIG_JPEG="False"
		AC_MSG_RESULT(---->Disabling: JPEG)
	],
   [
      CONFIGURE_JPEG
   ]
)

PNG_ENABLE="True"	 
AC_ARG_ENABLE([nopng],
   AC_HELP_STRING([--enable-nopng],[Disable PNG Support]),
   [
	   PNG_ENABLE="False"  
	   AC_MSG_RESULT(---->Disabling: PNG)
   ],
   [
      CONFIGURE_PNG
   ]
) 
EXTERNAL_CONFIGURATIONS
#  AC_MSG_RESULT([$TARGET_DNS_DOMAIN])
MY_LIBS=$LIBS
case "$TARGET_DNS_DOMAIN" in 
   *llnl.gov)
      AC_MSG_RESULT([Configuring for LLNL])
      MILI_HOME="/usr/apps/mdg"
      
      CONFIGURE_COMPILER

      ############################################################
      #
      # Determine if 'gmake' is available for building the code.	
      #
      ############################################################
      CONFIGURE_GMAKE
      CONFIGURE_TASKS

      ############################################################
      # 
      # Configure library options
      #
      ############################################################

      
      if test "$OS_NAME" = "IRIX64" -o "$OS_NAME" = "IRIX"; then
         CONFIGURE_SGI_GRAPHICS
      else
         CONFIGURE_OSMESA
         # gui.c unconditionally includes X11/Motif headers, so even
         # SERIAL_BATCH builds must link against X11/Xt/Xm.
         NO_MESA_X11_LIBS="-lX11 -lXt -lXm"
         AC_SUBST(NO_MESA_X11_LIBS)
      fi
      
      CONFIGURE_KLOCWORK

      ############################################################
      #
      #	Set the directory names and paths.
      #
      ############################################################
      
      
      CONFIGURE_INSTALL

      ############################################################
      #
      #	Configure the Compiler Options
      #
      ############################################################
      CONFIGURE_COMPILER_FLAGS
      CONFIGURE_COMPILER_GRIZ
      CONFIGURE_GRIZ_LIBRARIES
      

      ############################################################
      # 
      #	Create the exported environment variables.
      # 
      ############################################################

      CFLAGS_DEBUG="$DEBUG_CFLAG_OPTIONS $GPROF_FLAGS"
      CFLAGS_OPT="$OPT_CFLAG_OPTIONS $GPROF_FLAGS"

      # Look for scripts in ./scripts

      #############################################################################
      #
      #	Set the names of the 'Makefile's to be created.  Don't forget to
      #       include the Makefile.Common file on all but the Driver.
      #
      #############################################################################
      

      ;;
   *)
      AC_PATH_X
      
      AC_SEARCH_LIBS([OSMesaGetCurrentContext],[OSMesa],
         [
            OSMESA_LIBRARY="$LIBS"
            LIBS=$MY_LIBS
				BATCH_ENABLE="True"
				BATCH_DEFINES="-DSERIAL_BATCH"
         ],
         [
            CONFIGURE_OSMESA
         ]
         
      )
      
      AC_SEARCH_LIBS([XDestroyWindow],[X11],
         [
         ],
         [
            AC_MSG_ERROR([No X11 Library available])
         ]
      
      ) 
      
       AC_SEARCH_LIBS([XtAppInitialize],[Xt],
         [
         ],
         [
            AC_MSG_ERROR([No Xt Library available])
         ]
      
      )
      
      AC_SEARCH_LIBS([XmStringFree],[Xm],
         [
         ],
         [
            AC_MSG_ERROR([No Xm Library available])
         ]
      
      ) 
	   
     
	   
      AC_SEARCH_LIBS([glXChooseVisual],[GL],
         [
         ],
         [
            AC_MSG_ERROR([No GL Library available])
         ]
      
      ) 
	   
      
      AC_SEARCH_LIBS([gluNewQuadric],[GLU],
         [
         ],
         [
            AC_MSG_ERROR([No GLU Library available])
         ]
      
      ) 
      
      AC_SEARCH_LIBS([glwDrawingAreaClassRec],[GLw],
         [
         ],
         [
            AC_MSG_ERROR([No Glw Library available])
         ]
      
      )
      AC_SEARCH_LIBS([glutInit],[glut],
         [
         ],
         [
            AC_MSG_ERROR([No glut Library available])
         ]
      
      ) 
      
      
      GRIZ_EXE="griz.""$build_os""$EXE_SUFFIX"
		
      case $CC in
         *icc)
				CC_FLAGS_DEBUG="-g"
				CC_FLAGS_OPT=" -O3"
		
				AR="ar"
				RANLIB="ranlib"
				AR_FLAGS="-r";;
				
         *xlc)
				CC_FLAGS_EXTRA=" -bnoobjreorder -qfullpath -DAIX -qMAXMEM=8192 -bmaxdata:0x40000000"
				CC_FLAGS_DEBUG="-g -DAIX "
				CC_FLAGS_OPT="-g -O3 -DAIX "
				
				AR="ar"
				RANLIB="ranlib"
				AR_FLAGS="-r"

            AC_MSG_WARN([xlc compiler]);;
         *gcc)
            AC_MSG_WARN([gcc compiler]);;
			cc)
            AC_MSG_WARN([cc compiler]);;
         *)
            AC_MSG_WARN([Unknown compiler]);;
      esac
      ;;
esac

CONFIGURE_IMAGE
CONFIGURE_COMPILER_IMAGELIB

CONFIGURE_HERSHEY
CONFIGURE_COMPILER_HERSHEYLIB
 
MDGLIBS="$HERSHEY_LIBRARY_PATHS $HERSHEY_LIBRARY"
MDGLIBS="$MDGLIBS $IMAGE_LIBRARY_PATHS $IMAGE_LIBRARY" 
GRIZLIBS="$GRIZLIBS $MDGLIBS"

MAKEFILES="$PATH_NAME/Makefile $PATH_NAME/src/Makefile $PATH_NAME/ImageLib/Makefile $PATH_NAME/HersheyLib/src/Makefile $MAKEFILES"
    
#############################################################################
#
#	Specify the commands to run at the end of 'configure'.
#
#############################################################################
AC_CONFIG_COMMANDS([cleanup],[[rm -f config.cache config.log config.status ]])
#############################################################################
#
#	Create the 'Makefile's from the template 'Makefile.in'
#	by exporting the AutoConf environment variables.
#
#############################################################################
AC_CONFIG_FILES($MAKEFILES)
AC_OUTPUT

# End of configure.ac

