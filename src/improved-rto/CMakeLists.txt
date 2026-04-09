build_lib(
  LIBNAME improved-rto
  SOURCE_FILES
    model/rtt-improved-paper.cc
    model/rtt-proposed.cc
  HEADER_FILES
    model/rtt-improved-paper.h
    model/rtt-proposed.h
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libcore}
    ${libnetwork}
)