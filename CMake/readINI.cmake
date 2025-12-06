function(read_ini_file INI_FILE PREFIX)

    file(STRINGS "${INI_FILE}" INI_LINES) # Read entire file
    set(CURRENT_SECTION "")
    
    foreach(LINE ${INI_LINES})
        string(STRIP "${LINE}" LINE) # Remove whitespace
        
        if(LINE STREQUAL "" OR LINE MATCHES "^[#;]") # Skip empty lines and comments
            continue()
        endif()
        
        # Check for section header: [section]
        if(LINE MATCHES "^\\[(.+)\\]$")
            set(CURRENT_SECTION "${CMAKE_MATCH_1}")
            continue()
        endif()
        
        # Parse key = value
        if(LINE MATCHES "^([^=]+)=(.+)$")
            string(STRIP "${CMAKE_MATCH_1}" KEY)
            string(STRIP "${CMAKE_MATCH_2}" VALUE)
            
            # Create variable: PREFIX_SECTION_KEY
            string(TOUPPER "${CURRENT_SECTION}" SECTION_UPPER)
            string(TOUPPER "${KEY}" KEY_UPPER)
            string(REPLACE " " "_" SECTION_UPPER "${SECTION_UPPER}")
            string(REPLACE " " "_" KEY_UPPER "${KEY_UPPER}")
            
            set(VAR_NAME "${PREFIX}_${SECTION_UPPER}_${KEY_UPPER}")
            
            # Set in parent scope
            set(${VAR_NAME} "${VALUE}" PARENT_SCOPE)
            
            message(DEBUG "  ${VAR_NAME} = ${VALUE}")
        endif()
    endforeach()
endfunction()
