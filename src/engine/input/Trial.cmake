# Opt-in switch shared with the existing composed-runtime owner.
option(WXL_PASSIVE_HOVER_TRIAL "Immediate-first 100 ms passive world-hover Beta trial" OFF)
if(WXL_PASSIVE_HOVER_TRIAL)
    if(CLIENT_PATH)
        message(FATAL_ERROR "Passive hover trial builds stay offline; CLIENT_PATH must be empty")
    endif()
    target_compile_definitions(WarcraftXL PRIVATE WXL_PASSIVE_HOVER_TRIAL=1)
endif()
