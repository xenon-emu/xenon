{ stdenv
, cmake
, fetchFromGitHub
, fmt
, lib
, ninja
, pkg-config
, roboto
, sdl3
, toml11
, withGraphics ? true
}:

let
  imgui = if withGraphics
    then fetchFromGitHub {
      owner = "ocornut";
      repo = "imgui";
      rev = "15b96fd051731254f4ed0ef78c183f3466bf9e1f";
      hash = "sha256-VYNqqpE1bo4bjzVsPQhNlOVKemWOZeQg0JaryuAu/Tk=";
    }
    else {};
in
stdenv.mkDerivation {
  name = "xenon";
  allowSubstitutes = false;
  src = ./.;
  nativeBuildInputs = [ cmake pkg-config ninja ];

  buildInputs = [
    fmt toml11
  ] ++ lib.optional withGraphics sdl3;

  cmakeFlags = if withGraphics
    then [ "-DGFX_ENABLED=True" ]
    else [ "-DGFX_ENABLED=False" ];

  postUnpack = ''
    ${lib.optionalString withGraphics ''
      echo graphics present
      rm -rf $sourceRoot/third_party/ImGui
      cp -r ${imgui} $sourceRoot/third_party/ImGui
    ''}
    chmod -R +w $sourceRoot
  '';

  installPhase = ''
    ${lib.optionalString withGraphics ''
      echo graphics present
      mkdir -p $out/share
      ln -sv ${roboto}/share/fonts $out/share/fonts
    ''}
    mkdir -p $out/bin
    cp -v Xenon $out/bin/Xenon
  '';
}
