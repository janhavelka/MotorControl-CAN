# Apply an ESP-IDF application's capacity Kconfig (Kconfig.capacity) to the one component that
# compiles the adapter and example sources, so every unit of the image sees one capacity set
# (Capacity.h). Never apply it to a single source or to only some of those components.
function(motorcontrolcan_capacity_definitions target)
    target_compile_definitions(${target} PRIVATE
        MOTORCONTROLCAN_SDO_CHANNELS=${CONFIG_MOTORCONTROLCAN_SDO_CHANNELS}
        MOTORCONTROLCAN_PDO_ROUTES=${CONFIG_MOTORCONTROLCAN_PDO_ROUTES})
    foreach(setting OWNER_REQUESTS OWNER_PRODUCERS DISCOVERY_TARGETS)
        if(CONFIG_MOTORCONTROLCAN_${setting}) # Zero leaves the derived default.
            target_compile_definitions(${target} PRIVATE
                MOTORCONTROLCAN_${setting}=${CONFIG_MOTORCONTROLCAN_${setting}})
        endif()
    endforeach()
endfunction()
