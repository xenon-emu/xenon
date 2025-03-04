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
  imgui = fetchFromGitHub {
    owner = "ocornut";
    repo = "imgui";
    rev = "15b96fd051731254f4ed0ef78c183f3466bf9e1f";
    hash = "sha256-VYNqqpE1bo4bjzVsPQhNlOVKemWOZeQg0JaryuAu/Tk=";
  };
in
stdenv.mkDerivation {
  name = "xenon";
  allowSubstitutes = false;
  src = ./.;
  nativeBuildInputs = [ cmake pkg-config ninja ];
  buildInputs = [ sdl3 fmt toml11 ];
  postUnpack = ''
    cp -r ${imgui} $sourceRoot/third_party/ImGui
    chmod -R +w $sourceRoot
  '';

  installPhase = ''
    mkdir -p $out/bin $out/share
    cp -v Xenon $out/bin/Xenon
    ln -sv ${roboto}/share/fonts $out/share/fonts
  '';
}
