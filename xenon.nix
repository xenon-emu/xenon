{ stdenv
, cmake
, fetchFromGitHub
, fmt
, ninja
, pkg-config
, roboto
, sdl3
, toml11
}:

let
  withGraphics = false;
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
  
  buildInputs = if withGraphics
    then [ sdl3 fmt toml11 ]
    else [ fmt toml11 ];

  cmakeFlags = if withGraphics
    then [ "-DGFX_ENABLED=True" ]
    else [ "-DGFX_ENABLED=False" ];

  postUnpack = if withGraphics
    then ''
      echo graphics present
      rm -rf $sourceRoot/third_party/ImGui
      cp -r ${imgui} $sourceRoot/third_party/ImGui
      chmod -R +w $sourceRoot
    ''
    else ''
      chmod -R +w $sourceRoot
    '';

  installPhase = if withGraphics
    then ''
      echo graphics present
      mkdir -p $out/bin $out/share
      cp -v Xenon $out/bin/Xenon
      ln -sv ${roboto}/share/fonts $out/share/fonts
    ''
    else ''
      mkdir -p $out/bin $out/share
      cp -v Xenon $out/bin/Xenon
    '';
}