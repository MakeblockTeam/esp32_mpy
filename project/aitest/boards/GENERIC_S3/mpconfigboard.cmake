set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base_s3
    boards/sdkconfig.240mhz_s3
    boards/sdkconfig.spiram_s3
    boards/sdkconfig.usb
    boards/sdkconfig.freertos
)

if(NOT MICROPY_FROZEN_MANIFEST)
    set(MICROPY_FROZEN_MANIFEST ${MICROPY_PORT_DIR}/boards/manifest.py)
endif()
