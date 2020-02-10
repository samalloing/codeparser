
#
# Setup a script to work-around non-0 exit codes when running WolframScript
#
# Seen with v11.1 on Windows
# error MSB6006: "cmd.exe" exited with code -1073741819. [C:\Jenkins\workspace\Component.AST.Windows-x86-64.prototype\ast\build\paclet.vcxproj]
#
# The -1073741819 is really just 0xc0000005 and means a crash happened.
#
# This might be some strange confluence of Windows, WolframScript, and Java
#
# Related bugs: ?
#

execute_process(
  COMMAND
    ${WOLFRAMKERNEL} -noinit -noprompt -nopaclet -script ${GENERATEEXPRESSIONLIBRARY_WL_SCRIPT} -buildDir ${BUILDDIR}
  TIMEOUT
    # make this much longer than 10 seconds, duh
    600
  RESULT_VARIABLE
    GENERATEEXPRESSIONLIBRARY_RESULT
)

if(NOT ${GENERATEEXPRESSIONLIBRARY_RESULT} EQUAL "0")
  message(WARNING "Bad exit code from GenerateExpressionLibrary script: ${GENERATEEXPRESSIONLIBRARY_RESULT}; Continuing")
endif()
