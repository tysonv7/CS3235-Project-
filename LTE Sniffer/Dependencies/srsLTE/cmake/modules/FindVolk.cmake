INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_VOLK volk QUIET)

FIND_PATH(
    VOLK_INCLUDE_DIRS
    NAMES volk/volk.h
    HINTS $ENV{VOLK_DIR}/include
          ${CMAKE_INSTALL_PREFIX}/include
          ${PC_VOLK_INCLUDE_DIR}
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    VOLK_LIBRARIES
    NAMES volk
    HINTS $ENV{VOLK_DIR}/lib
          ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          ${PC_VOLK_LIBDIR}
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

# Some functions are not defined in old volk versions
SET(CMAKE_REQUIRED_LIBRARIES volk m)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_index_max_16u HAVE_VOLK_MAX_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_accumulator_s32f HAVE_VOLK_ACC_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_s32fc_multiply_32fc HAVE_VOLK_MULT_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_conjugate_32fc HAVE_VOLK_CONJ_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_multiply_32fc HAVE_VOLK_MULT2_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_multiply_conjugate_32fc HAVE_VOLK_MULT2_CONJ_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_32f_multiply_32fc HAVE_VOLK_MULT_REAL_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_s32f_multiply_32f HAVE_VOLK_MULT_FLOAT_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_multiply_32f HAVE_VOLK_MULT_REAL2_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_magnitude_32f HAVE_VOLK_MAG_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_divide_32f HAVE_VOLK_DIVIDE_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_32f_dot_prod_32fc HAVE_VOLK_DOTPROD_FC_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_conjugate_dot_prod_32fc HAVE_VOLK_DOTPROD_CONJ_FC_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_dot_prod_32f HAVE_VOLK_DOTPROD_F_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_s32f_atan2_32f HAVE_VOLK_ATAN_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_s32f_convert_16i HAVE_VOLK_CONVERT_FI_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_deinterleave_32f_x2 HAVE_VOLK_DEINTERLEAVE_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_subtract_32f HAVE_VOLK_SUB_FLOAT_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_square_dist_32f HAVE_VOLK_SQUARE_DIST_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_deinterleave_real_32f HAVE_VOLK_DEINTERLEAVE_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_32fc_index_max_16u HAVE_VOLK_MAX_ABS_FUNCTION)
CHECK_FUNCTION_EXISTS_MATH(volk_16i_s32f_convert_32f HAVE_VOLK_CONVERT_IF_FUNCTION)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VOLK DEFAULT_MSG VOLK_LIBRARIES VOLK_INCLUDE_DIRS)
MARK_AS_ADVANCED(VOLK_LIBRARIES VOLK_INCLUDE_DIRS VOLK_DEFINITIONS)

IF(VOLK_FOUND)
  SET(CMAKE_REQUIRED_LIBRARIES ${VOLK_LIBRARIES} m)
  CHECK_FUNCTION_EXISTS_MATH(volk_16i_s32f_convert_32f HAVE_VOLK_CONVERT_IF_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_index_max_16u HAVE_VOLK_MAX_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_max_32f HAVE_VOLK_MAX_VEC_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_accumulator_s32f HAVE_VOLK_ACC_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_s32fc_multiply_32fc HAVE_VOLK_MULT_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_conjugate_32fc HAVE_VOLK_CONJ_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_multiply_32fc HAVE_VOLK_MULT2_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_multiply_conjugate_32fc HAVE_VOLK_MULT2_CONJ_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_32f_multiply_32fc HAVE_VOLK_MULT_REAL_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_s32f_multiply_32f HAVE_VOLK_MULT_FLOAT_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_magnitude_32f HAVE_VOLK_MAG_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_magnitude_squared_32f HAVE_VOLK_MAG_SQUARE_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_divide_32f HAVE_VOLK_DIVIDE_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_dot_prod_32fc HAVE_VOLK_DOTPROD_FC_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_32f_dot_prod_32fc HAVE_VOLK_DOTPROD_CFC_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_conjugate_dot_prod_32fc HAVE_VOLK_DOTPROD_CONJ_FC_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_dot_prod_32f HAVE_VOLK_DOTPROD_F_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_s32f_atan2_32f HAVE_VOLK_ATAN_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_s32f_convert_16i HAVE_VOLK_CONVERT_FI_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_deinterleave_32f_x2 HAVE_VOLK_DEINTERLEAVE_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_interleave_32fc HAVE_VOLK_INTERLEAVE_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_subtract_32f HAVE_VOLK_SUB_FLOAT_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_add_32f HAVE_VOLK_ADD_FLOAT_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_x2_square_dist_32f HAVE_VOLK_SQUARE_DIST_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_deinterleave_real_32f HAVE_VOLK_DEINTERLEAVE_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32fc_index_max_16u HAVE_VOLK_MAX_ABS_FUNCTION)
  CHECK_FUNCTION_EXISTS_MATH(volk_32f_x2_multiply_32f HAVE_VOLK_MULT_REAL2_FUNCTION)



  SET(VOLK_DEFINITIONS "HAVE_VOLK")
  IF(${HAVE_VOLK_MAX_ABS_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MAX_ABS_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MAX_VEC_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MAX_VEC_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DOTPROD_CONJ_FC_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DOTPROD_CONJ_FC_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MAG_SQUARE_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MAG_SQUARE_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_SQUARE_DIST_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_SQUARE_DIST_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DEINTERLEAVE_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DEINTERLEAVE_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_INTERLEAVE_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_INTERLEAVE_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_SUB_FLOAT_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_SUB_FLOAT_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_ADD_FLOAT_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_ADD_FLOAT_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MULT2_CONJ_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MULT2_CONJ_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DEINTERLEAVE_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DEINTERLEAVE_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_CONVERT_FI_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_CONVERT_FI_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MAX_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MAX_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_ACC_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_ACC_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MULT_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MULT_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_CONJ_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_CONJ_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MULT2_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MULT2_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MULT_FLOAT_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MULT_FLOAT_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MULT_REAL_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MULT_REAL_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_MAG_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_MAG_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DIVIDE_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DIVIDE_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DOTPROD_FC_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DOTPROD_FC_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_DOTPROD_F_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_DOTPROD_F_FUNCTION")
  ENDIF()
  IF(${HAVE_VOLK_ATAN_FUNCTION})
    SET(VOLK_DEFINITIONS "${VOLK_DEFINITIONS}; HAVE_VOLK_ATAN_FUNCTION")
  ENDIF()
ENDIF(VOLK_FOUND)
