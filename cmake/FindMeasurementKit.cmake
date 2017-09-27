if (NOT MEASUREMENTKIT_FOUND)
    set(MEASUREMENTKIT_ROOT
        "${MEASUREMENTKIT_ROOT}"
		CACHE
		PATH
		"Directory to search")

    message(STATUS "SEARCHING IN ${MEASUREMENTKIT_ROOT}")

    find_package(Threads REQUIRED)
    list(APPEND MK_LIBS GeoIP crypto ssl event event_openssl event_pthreads Threads::Threads)

    # Find the library
    find_library(MEASUREMENTKIT_LIBRARY
        NAMES
        libmeasurement_kit.a
        HINTS
        "${MEASUREMENTKIT_ROOT}"
        PATH_SUFFIXES
		"${_LIBSUFFIXES}"
    )

    # Find the headers
    find_path(MEASUREMENTKIT_INCLUDE_DIR
        NAMES
            measurement_kit/common/aaa_base.hpp
            measurement_kit/common/callback.hpp
            measurement_kit/common/continuation.hpp
            measurement_kit/common/detail/citrus_ctype.h
            measurement_kit/common/detail/delegate.hpp
            measurement_kit/common/detail/encoding.hpp
            measurement_kit/common/detail/every.hpp
            measurement_kit/common/detail/fapply.hpp
            measurement_kit/common/detail/fcar.hpp
            measurement_kit/common/detail/fcdr.hpp
            measurement_kit/common/detail/fcompose.hpp
            measurement_kit/common/detail/fmap.hpp
            measurement_kit/common/detail/freverse.hpp
            measurement_kit/common/detail/locked.hpp
            measurement_kit/common/detail/maybe.hpp
            measurement_kit/common/detail/mock.hpp
            measurement_kit/common/detail/parallel.hpp
            measurement_kit/common/detail/range.hpp
            measurement_kit/common/detail/utils.hpp
            measurement_kit/common/detail/worker.hpp
            measurement_kit/common/error.hpp
            measurement_kit/common/error_or.hpp
            measurement_kit/common/git_version.hpp
            measurement_kit/common/json.hpp
            measurement_kit/common/lexical_cast.hpp
            measurement_kit/common/libevent/reactor.hpp
            measurement_kit/common/logger.hpp
            measurement_kit/common/nlohmann/json.hpp
            measurement_kit/common/non_copyable.hpp
            measurement_kit/common/non_movable.hpp
            measurement_kit/common/platform.h
            measurement_kit/common/portable/arpa
            measurement_kit/common/portable/arpa/inet.h
            measurement_kit/common/portable/netdb.h
            measurement_kit/common/portable/netinet
            measurement_kit/common/portable/netinet/in.h
            measurement_kit/common/portable/netinet/tcp.h
            measurement_kit/common/portable/stdlib.h
            measurement_kit/common/portable/strings.h
            measurement_kit/common/portable/sys
            measurement_kit/common/portable/sys/socket.h
            measurement_kit/common/portable/sys/time.h
            measurement_kit/common/portable/sys/types.h
            measurement_kit/common/portable/time.h
            measurement_kit/common/reactor.hpp
            measurement_kit/common/scalar.hpp
            measurement_kit/common/settings.hpp
            measurement_kit/common/shared_ptr.hpp
            measurement_kit/common/socket.hpp
            measurement_kit/common/version.h
            measurement_kit/common.hpp
            measurement_kit/dns/error.hpp
            measurement_kit/dns/qctht_.hpp
            measurement_kit/dns/query.hpp
            measurement_kit/dns/query_class.hpp
            measurement_kit/dns/query_type.hpp
            measurement_kit/dns/resolve_hostname.hpp
            measurement_kit/dns.hpp
            measurement_kit/ext/sole.hpp
            measurement_kit/ext.hpp
            measurement_kit/http/http.hpp
            measurement_kit/http.hpp
            measurement_kit/mlabns/mlabns.hpp
            measurement_kit/mlabns.hpp
            measurement_kit/ndt/error.hpp
            measurement_kit/ndt/run.hpp
            measurement_kit/ndt.hpp
            measurement_kit/net/buffer.hpp
            measurement_kit/net/connect.hpp
            measurement_kit/net/error.hpp
            measurement_kit/net/transport.hpp
            measurement_kit/net/utils.hpp
            measurement_kit/net.hpp
            measurement_kit/nettests/nettests.hpp
            measurement_kit/nettests.hpp
            measurement_kit/neubot/dash.hpp
            measurement_kit/neubot.hpp
            measurement_kit/ooni/bouncer.hpp
            measurement_kit/ooni/collector_client.hpp
            measurement_kit/ooni/error.hpp
            measurement_kit/ooni/nettests.hpp
            measurement_kit/ooni/orchestrate.hpp
            measurement_kit/ooni/resources.hpp
            measurement_kit/ooni/templates.hpp
            measurement_kit/ooni/utils.hpp
            measurement_kit/ooni.hpp
            measurement_kit/report/base_reporter.hpp
            measurement_kit/report/entry.hpp
            measurement_kit/report/error.hpp
            measurement_kit/report/file_reporter.hpp
            measurement_kit/report/ooni_reporter.hpp
            measurement_kit/report/report.hpp
            measurement_kit/report.hpp
            measurement_kit/traceroute/android.hpp
            measurement_kit/traceroute/error.hpp
            measurement_kit/traceroute/interface.hpp
            measurement_kit/traceroute.hpp
        HINTS
        "${_libdir}/.."
        PATHS
        "${MEASUREMENTKIT_ROOT}"
        PATH_SUFFIXES
        include
        )

    message(STATUS "DONE ${MEASUREMENTKIT_INCLUDE_DIR}")
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        MeasurementKit
        DEFAULT_MSG
        MEASUREMENTKIT_LIBRARY
        MEASUREMENTKIT_INCLUDE_DIR
    )

endif (NOT MEASUREMENTKIT_FOUND)

if(MEASUREMENTKIT_FOUND)
    set(MEASUREMENTKIT_INCLUDE_DIRS ${MEASUREMENTKIT_INCLUDE_DIR})
    set(MEASUREMENTKIT_LIBRARIES "${MEASUREMENTKIT_LIBRARY};${MK_LIBS}")
    mark_as_advanced(MEASUREMENTKIT_ROOT)
endif()

mark_as_advanced(MEASUREMENTKIT_INCLUDE_DIR,
    MEASUREMENTKIT_LIBRARIES)
