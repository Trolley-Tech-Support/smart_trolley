set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_PRIV_INCLUDEDIRS "include")
set(COMPONENT_SRCS "lcd.c")

# Add the necessary component dependencies, such as "driver"
set(COMPONENT_REQUIRES "driver")

# Register the component
register_component()
