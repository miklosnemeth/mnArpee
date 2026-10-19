# Run by the mnArpeeInstaller target (cmake -P): compiles the Windows installer with Inno Setup.
# Expects -DCONFIG -DISCC -DVERSION -DBUNDLE (the built mnArpee.vst3) -DOUTPUT -DISS.
if(NOT CONFIG STREQUAL "Release")
    message(STATUS "Installer: skipped for the ${CONFIG} configuration (it is built for Release)")
    return()
endif()

file(TO_NATIVE_PATH "${BUNDLE}" bundle_native)
file(TO_NATIVE_PATH "${OUTPUT}" output_native)
execute_process(
    COMMAND "${ISCC}" "/DMyAppVersion=${VERSION}" "/DVst3Bundle=${bundle_native}"
            "/DInstallerOutputDir=${output_native}" "${ISS}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Inno Setup failed (${result})")
endif()
message(STATUS "Installer: ${output_native}\\mnArpee-${VERSION}-win-x64-setup.exe")
