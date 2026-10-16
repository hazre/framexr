{ lib, stdenv, meson, ninja, pkg-config, glslang, wayland-scanner, wayland-protocols, git,
  vulkan-headers, vulkan-loader, freetype, fontconfig, dbus, libxkbcommon, wayland, pipewire,
  openxr-loader, libdrm, mesa, libglvnd, libX11, libXext, libXrandr, libxcb,
  eigen, nlohmann_json, yaml-cpp, openvr, gtest,
  useSystemDeps ? false, src }:

stdenv.mkDerivation (finalAttrs: {
  pname = "framexr";

  # Never touch on release bumps; tracks meson.build project(version:).
  version =
    let
      flat = builtins.replaceStrings [ "\n" ] [ " " ] (builtins.readFile ../meson.build);
      m = builtins.match ".*version:[ ]*'([0-9][0-9.]*)'.*" flat;
    in
    if m == null then throw "nix/package.nix: version not found in meson.build" else builtins.head m;

  inherit src;

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    glslang
    wayland-scanner
    wayland-protocols
    git
  ];
  buildInputs = [
    vulkan-headers vulkan-loader freetype fontconfig dbus libxkbcommon
    wayland wayland-protocols pipewire openxr-loader
    libdrm mesa libglvnd libX11 libXext libXrandr libxcb
  ] ++ lib.optionals useSystemDeps [ eigen nlohmann_json yaml-cpp openvr ];
  checkInputs = lib.optionals useSystemDeps [ gtest ];

  # Flake sources strip .git; contents are present via ?submodules=1,
  # so satisfy meson.build's submodule guard without touching it.
  postPatch = ''
    if [ ! -f external/miniaudio/miniaudio.h ]; then
      echo "framexr: submodule contents missing; flake consumers must add ?submodules=1 to the input URL" >&2
      exit 1
    fi
    for d in external/*/; do mkdir -p "$d/.git"; done
  '';

  # Upstream defaults to werror=true; Nix hardening trips it.
  mesonFlags = [
    "-Dwerror=false"
    (lib.mesonEnable "system-deps" useSystemDeps)
  ];
  doCheck = true;
  # style: clang-format version drift; integration: needs GPU + desktop session.
  mesonCheckFlags = [ "--no-suite" "style" "--no-suite" "integration" ];

  meta = with lib; {
    description = "Modular VR overlay for SteamVR and Monado";
    homepage = "https://github.com/wayvr-org/wayvr";
    license = licenses.gpl3Only;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
    mainProgram = "framexr";
  };
})
