# build_lib(
#     LIBNAME improved-rtt-paper

#     SOURCE_FILES
#         model/improved-rtt-estimator.cc

#     HEADER_FILES
#         model/improved-rtt-estimator.h

#     LIBRARIES_TO_LINK
#         ${libcore}
#         ${libnetwork}
#         ${libinternet}
# )

# build_lib_example(
#     improved-rto-simulation
#     scratch/improved-rto-simulation.cc

#     LIBRARIES_TO_LINK
#         ${libimproved-rtt-paper}
#         ${libcore}
#         ${libnetwork}
#         ${libinternet}
#         ${libpoint-to-point}
#         ${libapplications}
#         ${libflow-monitor}
#         ${libtraffic-control}
# )

# build_lib(
#   LIBNAME improved-rtt-paper
#   SOURCE_FILES
#     model/improved-rtt-estimator.cc
#   HEADER_FILES
#     model/improved-rtt-estimator.h
#   LIBRARIES_TO_LINK
#     ${libinternet}
#     ${libcore}
#     ${libnetwork}
#     ${libstats}
# )

build_lib(
  LIBNAME improved-rto2
  SOURCE_FILES
    model/rtt-improved-estimator.cc
  HEADER_FILES
    model/rtt-improved-estimator.h
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libcore}
    ${libnetwork}
)