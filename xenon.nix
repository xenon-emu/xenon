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
  asmjit = fetchFromGitHub {
    owner = "asmjit";
    repo = "asmjit";
    rev = "356dddbc5508dd65f466098da26a2e47584eafdb";
    hash = "sha256-j/Ft9hVU1bXPM70jOC5uyirvs7aWZF2yUtyYK9kQZ/8=";
  };
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
      owner = "Vali0004";
      repo = "sirit";
      rev = "e9f33a21066234f9479383233d4883837b7ea807";
      hash = "sha256-o/Cju5KHu1Dh+swO1K4HFRBn+dxqYvqU3CgZoUjebek=";
    }
    else {};
  microprofile = fetchFromGitHub {
    fetchSubmodules = true;
    owner = "jonasmr";
    repo = "microprofile";
    rev = "9ecdd59ca514ef56e95e9285c74f6bde4c6e1c97";
    hash = "sha256-/RWgtPLu5GLe3fkLRjI8SURs0hjQa0eleWRSieYYeCo=";
  };
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
      rm -rf $sourceRoot/Deps/ThirdParty/ImGui
      rm -rf $sourceRoot/Deps/ThirdParty/Sirit
      cp -r ${imgui} $sourceRoot/Deps/ThirdParty/ImGui
      cp -r ${sirit} $sourceRoot/Deps/ThirdParty/Sirit
    ''}
    rm -rf $sourceRoot/Deps/ThirdParty/asmjit
    rm -rf $sourceRoot/Deps/ThirdParty/microprofile
    cp -r ${asmjit} $sourceRoot/Deps/ThirdParty/asmjit
    cp -r ${microprofile} $sourceRoot/Deps/ThirdParty/microprofile
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
