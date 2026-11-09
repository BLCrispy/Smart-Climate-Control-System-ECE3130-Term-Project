# groups.cmake

# group CubeMX
add_library(Group_CubeMX OBJECT
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/MDK-ARM/startup_stm32l476xx.s"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Src/main.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Src/stm32l4xx_it.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Src/stm32l4xx_hal_msp.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Src/system_stm32l4xx.c"
)
target_include_directories(Group_CubeMX PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(Group_CubeMX PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_COMPILE_DEFINITIONS>
)
add_library(Group_CubeMX_ABSTRACTIONS INTERFACE)
target_link_libraries(Group_CubeMX_ABSTRACTIONS INTERFACE
  ${CONTEXT}_ABSTRACTIONS
)
target_compile_options(Group_CubeMX PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_COMPILE_OPTIONS>
)
target_link_libraries(Group_CubeMX PUBLIC
  Group_CubeMX_ABSTRACTIONS
)
set(COMPILE_DEFINITIONS
  STM32L476xx
  _RTE_
)
cbuild_set_defines(AS_ARM COMPILE_DEFINITIONS)
set_source_files_properties("${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/MDK-ARM/startup_stm32l476xx.s" PROPERTIES
  COMPILE_FLAGS "${COMPILE_DEFINITIONS}"
)

# group STM32 HAL Driver
add_library(Group_STM32_HAL_Driver OBJECT
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc_ex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ramfunc.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_gpio.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c_ex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma_ex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr_ex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_cortex.c"
  "${SOLUTION_ROOT}/STM32CubeMX/Target_1/STM32CubeMX/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_exti.c"
)
target_include_directories(Group_STM32_HAL_Driver PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(Group_STM32_HAL_Driver PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_COMPILE_DEFINITIONS>
)
add_library(Group_STM32_HAL_Driver_ABSTRACTIONS INTERFACE)
target_link_libraries(Group_STM32_HAL_Driver_ABSTRACTIONS INTERFACE
  ${CONTEXT}_ABSTRACTIONS
)
target_compile_options(Group_STM32_HAL_Driver PUBLIC
  $<TARGET_PROPERTY:${CONTEXT},INTERFACE_COMPILE_OPTIONS>
)
target_link_libraries(Group_STM32_HAL_Driver PUBLIC
  Group_STM32_HAL_Driver_ABSTRACTIONS
)
