{ stdenv
, cmake
, fetchFromGitHub
, makeWrapper
, lib
, ninja
, pkg-config
, roboto
, libxcb
, sdl3
, fmt_11
, toml11
, vulkan-headers
, vulkan-loader
}:

let
  sirit = fetchFromGitHub {
    fetchSubmodules = true;
    owner = "shadps4-emu";
    repo = "sirit";
    rev = "282083a595dcca86814dedab2f2b0363ef38f1ec";
    hash = "sha256-/nPJ4gJ48gWtpxJ2Tlz4Az07mdBLrL4w/gdb0Xjq47o= ";
  };
in
stdenv.mkDerivation {
  name = "xenon-tools";
  allowSubstitutes = false;
  src = ./.;
  nativeBuildInputs = [ makeWrapper cmake pkg-config ninja ];

  buildInputs = [
    fmt_11
    libxcb
    sdl3
    toml11
    vulkan-headers
  ];

  cmakeDir = "../Tools";

  postUnpack = ''
    rm -rf $sourceRoot/Deps/ThirdParty/Sirit
    cp -r ${sirit} $sourceRoot/Deps/ThirdParty/Sirit
    chmod -R +w $sourceRoot
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp -v Byteswap $out/bin/Byteswap
    cp -v GetIndex $out/bin/GetIndex
    cp -v GetOpcode $out/bin/GetOpcode
    cp -v VPUTests $out/bin/VPUTests
    cp -v xenos-disasm $out/bin/xenos-disasm
    wrapProgram $out/bin/xenos-disasm \
      --prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ vulkan-loader ]}
  '';
}
