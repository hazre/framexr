capture_sources = files(
  'Frame.cpp',
  'FrameTransforms.cpp',
)
capture_deps = []
capture_generated = []

if get_option('wayland').allowed()
  wayland_client_dep = dependency('wayland-client', include_type: 'system')
  pipewire_dep = dependency('libpipewire-0.3', include_type: 'system')
  dbus_capture_dep = dependency('dbus-1', include_type: 'system')
  subdir('wayland/protocols')

  capture_sources += files(
    'pipewire/PipewireCapture.cpp',
    'pipewire/ScreenCastPortal.cpp',
    'wayland/DesktopEntry.cpp',
    'wayland/ExtWindowCapture.cpp',
    'wayland/ExtWindowSource.cpp',
    'wayland/KwinWindowSource.cpp',
    'wayland/KwinWindowTracker.cpp',
    'wayland/ShmBuffer.cpp',
    'wayland/WaylandClient.cpp',
    'wayland/WaylandConnection.cpp',
    'wayland/WindowSource.cpp',
    'wayland/WlrDmabufCapture.cpp',
    'wayland/WlrScreencopyCapture.cpp',
  )
  capture_deps += [wayland_client_dep, pipewire_dep, dbus_capture_dep, capture_protocols_dep]
endif

if get_option('x11').allowed()
  capture_sources += files('x11/Xshm.cpp', 'x11/XshmCapture.cpp')
  if not get_option('wayland').allowed()
    capture_sources += files('x11/WindowSource.cpp')
  endif
  capture_deps += [
    dependency('x11', include_type: 'system'),
    dependency('xext', include_type: 'system'),
    dependency('xrandr', include_type: 'system'),
  ]
endif

if is_windows
  capture_sources += files(
    'win32/DxgiCapture.cpp',
    'win32/WindowSource.cpp',
  )
endif
