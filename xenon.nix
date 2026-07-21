{ stdenv
, cmake
, fetchFromGitHub
, makeWrapper
, lib
, ninja
, pkg-config
, asmjit
, cryptopp
, fmt_11
, glslang
, libxcb
, python3
, roboto
, sdl3
, toml11
, vk-bootstrap
, vulkan-headers
, vulkan-loader
, vulkan-memory-allocator
, withGraphics ? true
}:

let
  imgui = if withGraphics
    then fetchFromGitHub {
      owner = "ocornut";
      repo = "imgui";
      rev = "126d004f9e1eef062bf4b044b3b2faaf58d48c51";
      hash = "sha256-4L37NRR+dlkhdxuDjhLR45kgjyZK2uelKBlGZ1nQzgY=";
    }
    else {};
  sirit = if withGraphics
    then fetchFromGitHub {
      fetchSubmodules = true;
      owner = "shadps4-emu";
      repo = "sirit";
      rev = "282083a595dcca86814dedab2f2b0363ef38f1ec";
      hash = "sha256-/nPJ4gJ48gWtpxJ2Tlz4Az07mdBLrL4w/gdb0Xjq47o= ";
    }
    else {};
  microprofile = fetchFromGitHub {
    fetchSubmodules = true;
    owner = "jonasmr";
    repo = "microprofile";
    rev = "9ecdd59ca514ef56e95e9285c74f6bde4c6e1c97";
    hash = "sha256-/RWgtPLu5GLe3fkLRjI8SURs0hjQa0eleWRSieYYeCo=";
  };
  plusaes = fetchFromGitHub {
    owner = "kkAyataka";
    repo = "plusaes";
    rev = "f9e58596fc67e180d9b6d097226f97bd496af0d8";
    hash = "sha256-z72uaeu4LaO+rEhfLGa/ht49AGW5FgULQv3H8RXmbFM=";
  };
in
stdenv.mkDerivation {
  pname = "xenon";
  version = "0.0.1";

  src = ./.;

  strictDeps = true;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    makeWrapper
  ];

  buildInputs = [
    fmt_11
    toml11
    asmjit
    cryptopp
  ] ++ lib.optionals withGraphics [
    glslang
    libxcb
    sdl3
    vulkan-headers
    vk-bootstrap
    vulkan-memory-allocator
  ];

  cmakeFlags = [
    "-DXENON_USE_SYSTEM_DEPS=ON"
    "-DXENON_ALLOW_BUNDLED_DEPS=OFF"
  ] ++ lib.optionals withGraphics [
    "-DGFX_ENABLED=ON"
  ] ++ lib.optionals (!withGraphics) [
    "-DGFX_ENABLED=OFF"
  ];

  postUnpack = ''
    ${lib.optionalString withGraphics ''
      echo graphics present
      rm -rf $sourceRoot/Deps/ThirdParty/ImGui
      cp -r ${imgui} $sourceRoot/Deps/ThirdParty/ImGui

      rm -rf $sourceRoot/Deps/ThirdParty/Sirit
      cp -r ${sirit} $sourceRoot/Deps/ThirdParty/Sirit
    ''}

    rm -rf $sourceRoot/Deps/ThirdParty/microprofile
    cp -r ${microprofile} $sourceRoot/Deps/ThirdParty/microprofile

    rm -rf $sourceRoot/Deps/ThirdParty/plusaes
    cp -r ${plusaes} $sourceRoot/Deps/ThirdParty/plusaes

    chmod -R +w $sourceRoot
  '';

  installPhase = ''
    ${lib.optionalString withGraphics ''
      mkdir -p $out/share
      ln -sv ${roboto}/share/fonts $out/share/fonts
    ''}
    mkdir -p $out/bin
    cp -v Xenon $out/bin/Xenon
    wrapProgram $out/bin/Xenon \
      --prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ vulkan-loader ]}
  '';
}