
macro(CheckPacletInfo)

  if(NOT EXISTS ${WOLFRAMKERNEL})
  message(FATAL_ERROR "WOLFRAMKERNEL does not exist. WOLFRAMKERNEL: ${WOLFRAMKERNEL}")
  endif()

  if(LOCAL_BUILD)
    set(PACLET_VERSION 999)
  else()
    #
    # if not local build, then get Version from PacletInfo.m
    #
    execute_process(
      COMMAND
        ${WOLFRAMKERNEL} -noinit -noprompt -nopaclet -runfirst Pause[${BUG349779_PAUSE}]\;Print[OutputForm[Version\ /.\ List\ @@\ Get["${PACLETINFO_SOURCE}"]]]\;Exit[]
      OUTPUT_VARIABLE
        PACLET_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY
        ${PROJECT_SOURCE_DIR}
      TIMEOUT
        10
      RESULT_VARIABLE
        PACLETINFO_RESULT
    )

    if(NOT ${PACLETINFO_RESULT} EQUAL "0")
      message(WARNING "Bad exit code from PacletInfo script: ${PACLETINFO_RESULT}; Continuing")
    endif()

  endif(LOCAL_BUILD)

  message(STATUS "PACLET_VERSION: ${PACLET_VERSION}")

endmacro(CheckPacletInfo)
