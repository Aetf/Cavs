FILE(GLOB test_srcs *.cc)

MESSAGE(STATUS ${test_srcs} "[TEST]")
FOREACH(f ${test_srcs})
  MESSAGE(STATUS ${f} "[For Each CXX]")
  GET_FILENAME_COMPONENT(f_name ${f} NAME_WE)
  ADD_EXECUTABLE(${f_name} "${f}")
  MESSAGE(STATUS cavs_cxx "[TEST]")
  TARGET_LINK_LIBRARIES(${f_name} "-Wl,--whole-archive" cavs_cxx "-Wl,--no-whole-archive" ${EXTERNAL_LIBS})
ENDFOREACH()
