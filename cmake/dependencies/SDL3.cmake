include_guard(GLOBAL)
include(CPM)

# SDL3 is used only to read gamepads. The game keeps its own window, audio,
# rendering and keyboard/mouse input, so every other subsystem is disabled.
function(cox_add_sdl3)
	set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
	CPMAddPackage(
		NAME SDL3
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG f00dc919ca35b9756316e86c4777392de442f9b2 # release-3.4.20
		OPTIONS
			"SDL_SHARED OFF"
			"SDL_STATIC ON"
			"SDL_TEST_LIBRARY OFF"
			"SDL_EXAMPLES OFF"
			"SDL_TESTS OFF"
			"SDL_INSTALL OFF"
			"SDL_AUDIO OFF"
			"SDL_VIDEO OFF"
			"SDL_RENDER OFF"
			"SDL_GPU OFF"
			"SDL_CAMERA OFF"
			"SDL_DIALOG OFF"
			"SDL_TRAY OFF"
			"SDL_POWER OFF"
			"SDL_SENSOR OFF"
	)

	set_target_properties(SDL3-static PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endfunction()

cox_add_sdl3()
