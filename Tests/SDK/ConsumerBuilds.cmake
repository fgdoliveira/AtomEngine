# v0.0.20 (M127): the engine builds under a game's own CMake. Copies
# Consumer/ to a scratch folder (with a space in its path), configures it
# against the engine, builds it in each configuration - where a C++ runtime
# mismatch fails the link (/MTd in Debug, /MT in Release, M133) - and runs
# it from its own folder. No window, no GPU: CI runs it too (M133).
#
#   cmake -DENGINE=<engine source> -DWORK=<scratch dir> [-DCONFIGS=Debug;Release] -P ConsumerBuilds.cmake
foreach(var ENGINE WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()
if(NOT DEFINED CONFIGS)
    set(CONFIGS Debug Release)
endif()

file(REMOVE_RECURSE "${WORK}")
set(source "${WORK}/my game")
set(build "${WORK}/my game/build")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/Consumer/" DESTINATION "${source}")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${build}" "-DATOM_ENGINE_DIR=${ENGINE}"
                RESULT_VARIABLE configured OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "the consumer didn't configure (${configured}):\n${out}\n${err}")
endif()

foreach(config IN LISTS CONFIGS)
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}" --config ${config} --target SdkConsumer --parallel
                    RESULT_VARIABLE built OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT built EQUAL 0)
        string(REGEX MATCHALL "[^\n]*(error|LNK[0-9]+)[^\n]*" problems "${out}${err}")
        list(JOIN problems "\n" problems)
        message(FATAL_ERROR "the consumer didn't build in ${config} (${built}):\n${problems}")
    endif()

    set(payload "${build}/bin/${config}/SdkConsumer")
    if(NOT EXISTS "${payload}/SDL3.dll")
        message(FATAL_ERROR "the consumer's folder has no SDL3.dll: ${payload}")
    endif()
    execute_process(COMMAND "${payload}/SdkConsumer.exe" WORKING_DIRECTORY "${payload}"
                    RESULT_VARIABLE ran OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT ran EQUAL 0 OR NOT out MATCHES "SDK consumer: linked AtomEngine and AtomFramework.*own found, shared Kit found")
        message(FATAL_ERROR "the consumer didn't run in ${config} (${ran}):\n${out}\n${err}")
    endif()
    if(EXISTS "${build}/atom/Shaders" AND NOT EXISTS "${payload}/shaders")
        message(FATAL_ERROR "the engine compiled its shaders but the consumer's ${config} folder has none")
    endif()
    message(STATUS "PASS (${config}): ${out}")
endforeach()
file(REMOVE_RECURSE "${WORK}")
