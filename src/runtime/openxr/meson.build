openxr_sources = []
openxr_deps = []
openxr_generated = []

if get_option('openxr').allowed()
  openxr_deps += dependency('openxr')
  oxr_sdk_inc = sysdeps ? [] : [include_directories('../../../external/OpenXR-SDK/include', is_system: true)]
  openxr_deps += declare_dependency(
    include_directories: [include_directories('.')] + oxr_sdk_inc,
    compile_args: ['-DXR_USE_GRAPHICS_API_VULKAN'],
  )

  openxr_sources += files(
    'Monado.cpp',
    'OpenXrBlocker.cpp',
    'OpenXrClickAction.cpp',
    'OpenXrHand.cpp',
    'OpenXrInput.cpp',
    'OpenXrInstance.cpp',
    'OpenXrLines.cpp',
    'OpenXrRuntime.cpp',
    'OpenXrMultiClick.cpp',
    'OpenXrOverlay.cpp',
    'OpenXrPlayspace.cpp',
    'OpenXrPose.cpp',
    'OpenXrSession.cpp',
    'OpenXrSkybox.cpp',
    'OpenXrSwapchain.cpp',
  )
endif
