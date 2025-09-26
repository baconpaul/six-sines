# A basic installer setup.
#
# Modified for products building clap first

function(add_clapfirst_installer)
    set(oneValueArgs
            INSTALLER_TARGET
            ASSET_OUTPUT_DIRECTORY
            PRODUCT_NAME
            INSTALLER_PREFIX
    )
    set(multiValueArgs
            TARGETS  # A list of plugin formats, "CLAP" "VST3" "AUV2"
    )
    cmake_parse_arguments(CIN "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

    if ("${CIN_INSTALLER_PREFIX}" STREQUAL "")
        message(FATAL_ERROR "You must set INSTALLER_PREFIX in add_clapfirst_installer")
    endif()

    if ("${CIN_PRODUCT_NAME}" STREQUAL "")
        message(FATAL_ERROR "You must set PRODUCT_NAME in add_clapfirst_installer")
    endif()

    if ("${CIN_ASSET_OUTPUT_DIRECTORY}" STREQUAL "")
        message(FATAL_ERROR "You must set ASSET_OUTPUT_DIRECTORY in add_clapfirst_installer")
    endif()

    set(TGT ${CIN_INSTALLER_TARGET})
    add_custom_target(${TGT})
    foreach (INST ${CIN_TARGETS})
        if (TARGET ${INST})
            message(STATUS "Adding ${INST} to ${TGT} target deps")
            add_dependencies(${TGT} ${INST})
        endif()
    endforeach()


    string(TIMESTAMP INST_DATE "%Y-%m-%d")
    set(INST_ZIP ${CIN_INSTALLER_PREFIX}-${CMAKE_SYSTEM_NAME}${INST_EXTRA_ZIP_NAME}-${INST_DATE}-${GIT_COMMIT_HASH}.zip)
    message(STATUS "Zip File Name is ${INST_ZIP}")

    message(STATUS "PRODUCT NAME is ${CIN_PRODUCT_NAME}")

    if (APPLE)
        message(STATUS "Configuring for mac installer")
        add_custom_command(
                TARGET ${TGT}
                POST_BUILD
                USES_TERMINAL
                VERBATIM
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory installer
                COMMAND echo ${CMAKE_SOURCE_DIR}/libs/sst/sst-plugininfra/scripts/installer_mac/make_installer.sh ${CIN_PRODUCT_NAME} ${CIN_ASSET_OUTPUT_DIR} ${CMAKE_SOURCE_DIR}/resources/installer_mac ${CMAKE_BINARY_DIR}/installer "${INST_DATE}-${GIT_COMMIT_HASH}"
                COMMAND ${CMAKE_SOURCE_DIR}/libs/sst/sst-plugininfra/scripts/installer_mac/make_installer.sh ${CIN_PRODUCT_NAME} ${CIN_ASSET_OUTPUT_DIRECTORY} ${CMAKE_SOURCE_DIR}/resources/installer_mac ${CMAKE_BINARY_DIR}/installer "${INST_DATE}-${GIT_COMMIT_HASH}"
        )
    elseif (WIN32)
        message(STATUS "Configuring for win installer")
        cmake_path(REMOVE_EXTENSION INST_ZIP OUTPUT_VARIABLE WIN_INSTALLER)
        set(WINCOL ${CIN_ASSET_OUTPUT_DIRECTORY}/installer_copy)
        file(MAKE_DIRECTORY ${WINCOL})

        add_custom_target(${TGT}_wincollect)
        foreach (INST ${CIN_TARGETS})
            if (TARGET ${INST})
                message(STATUS "Copying ${INST} installer copy")
                add_dependencies(${TGT}_wincollect ${INST})
                add_custom_command(TARGET ${TGT}_wincollect
                        POST_BUILD
                        USES_TERMINAL
                        COMMAND cmake -E echo "Staging " $<TARGET_FILE:${INST}> " to " ${WINCOL}
                        COMMAND cmake -E copy "$<TARGET_FILE:${INST}>" "${WINCOL}"
                )
            endif()
        endforeach()

        add_dependencies(${TGT} ${TGT}_wincollect)

        add_custom_command(
            TARGET ${TGT}
            POST_BUILD
            USES_TERMINAL
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E echo Building zip for windoes
            COMMAND ${CMAKE_COMMAND} -E make_directory installer
            COMMAND 7z a -r installer/${INST_ZIP} ${WINCOL}
        )

        if (TARGET innosetup::compiler)
            message(STATUS "Ejecting innosetup installer rules")
            add_dependencies(${TGT} innosetup::compiler)

            add_custom_command(
                    TARGET ${TGT}
                    POST_BUILD
                    USES_TERMINAL
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMAND ${CMAKE_COMMAND} -E echo Building exe installer for windows with $<TARGET_PROPERTY:innosetup::compiler,IMPORTED_LOCATION> compiler
                    COMMAND ${CMAKE_COMMAND} -E make_directory installer
                    COMMAND innosetup::compiler
                        /O"${CMAKE_BINARY_DIR}/installer"
                        /F"${WIN_INSTALLER}"
                        /DName="${PRODUCT_NAME}"
                        /DNameCondensed="${PRODUCT_NAME}"
                        /DVersion="${GIT_COMMIT_HASH}"
                        /DID="2cfbe819-3947-4c10-93cc-03a2896fdfec"
                        /DPublisher="BaconPaul"
                        /DCLAP /DVST3 /DVST3_IS_SINGLE_FILE /DSA
                        /DIcon="${CMAKE_SOURCE_DIR}/resources/SideQuestIcon.ico"
                        /DBanner="${CMAKE_SOURCE_DIR}/resources/SideQuestBanner.png"
                        /DArch="x64compatible"
                        /DLicense="${CMAKE_SOURCE_DIR}/resources/LICENSE_GPL3"
                        /DStagedAssets="${WINCOL}"
                        "$<TARGET_PROPERTY:innosetup::compiler,INSTALL_SCRIPT>"
            )
        endif()

    else ()
        message(STATUS "Basic Installer: Target is installer/${OBXF_ZIP}")
        add_custom_command(
                TARGET ${TGT}
                POST_BUILD
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory installer
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/resources/LICENSE_GPL3 ${CIN_ASSET_OUTPUT_DIRECTORY}
                COMMAND ${CMAKE_COMMAND} -E tar cvf installer/${INST_ZIP} --format=zip ${CIN_ASSET_OUTPUT_DIRECTORY}/
                COMMAND ${CMAKE_COMMAND} -E echo "Installer in: installer/${INST_ZIP}")

        #add_custom_command(
        #        TARGET obxf-installer
        #        POST_BUILD
        #        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        #        USES_TERMINAL
        #        COMMAND scripts/installer_linux/make_deb.sh ${OBXF_PRODUCT_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/installer "${OBXF_DATE}-${GIT_COMMIT_HASH}"
        #)

    endif ()
endfunction()
