{ stdenv
, cmake
, fetchFromGitHub
, makeWrapper
, lib
, ninja
, pkg-config
, roboto
, sdl3
, fmt_11
, toml11
, python3
, vulkan-headers
, vulkan-loader
, withGraphics ? true
}:

let
  asmjit = fetchFromGitHub {
    owner = "asmjit";
    repo = "asmjit";
    rev = "32b5f78700e68684066feb99d583d6fb2a4e3b22";
    hash = "sha256-00000000000000000000000000000000000000000000";
  };
  cryptopp = fetchFromGitHub {
    owner = "weidai11";
    repo = "cryptopp";
    rev = "60f81a77e0c9a0e7ffc1ca1bc438ddfa2e43b78e";
    hash = "sha256-I94xGY0XVQu/RAtccf3dPbIuo1vtgVWvu56G7CaSth0=";
  };
  glslang = if withGraphics
    then fetchFromGitHub {
      owner = "KhronosGroup";
      repo = "glslang";
      rev = "9d764997360b202d2ba7aaad9a401e57d8df56b3";
      hash = "sha256-mRpwvzW3YPjaFvWLOLQHUqrsAJoHEkM+v2fXY3IUxyc=";
    }
    else {};
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
  vk-bootstrap = if withGraphics
    then fetchFromGitHub {
      owner = "charles-lunarg";
      repo = "vk-bootstrap";
      rev = "fe2cf07474bff6d7b7285e7af20b21656789dc07";
      hash = "sha256-DgDfYNGIdemyLaedJk25EWAt1aFccb8rhCtw25RSGm4=";
    }
    else {};
  vulkan-memory-allocator = if withGraphics
    then fetchFromGitHub {
      owner = "GPUOpen-LibrariesAndSDKs";
      repo = "VulkanMemoryAllocator";
      rev = "1076b348abd17859a116f4b111c43d58a588a086";
      hash = "sha256-WOx9upf1wn+f07kWoi3CV8X2NKSL5Fmm8d7KvPLrU8o=";
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
  nativeBuildInputs = [ makeWrapper cmake pkg-config ninja ];

  buildInputs = [
    fmt_11 toml11
  ] ++ lib.optionals withGraphics [
    sdl3
    python3 # Needed for glslang
    vulkan-headers
  ];

  cmakeFlags = if withGraphics
    then [ "-DGFX_ENABLED=True" ]
    else [ "-DGFX_ENABLED=False" ];

  postUnpack = ''
    ${lib.optionalString withGraphics ''
      echo graphics present
      rm -rf $sourceRoot/Deps/ThirdParty/glslang
      rm -rf $sourceRoot/Deps/ThirdParty/ImGui
      rm -rf $sourceRoot/Deps/ThirdParty/Sirit
      rm -rf $sourceRoot/Deps/ThirdParty/vk-bootstrap
      rm -rf $sourceRoot/Deps/ThirdParty/VulkanMemoryAllocator
      cp -r ${glslang} $sourceRoot/Deps/ThirdParty/glslang
      cp -r ${imgui} $sourceRoot/Deps/ThirdParty/ImGui
      cp -r ${sirit} $sourceRoot/Deps/ThirdParty/Sirit
      cp -r ${vk-bootstrap} $sourceRoot/Deps/ThirdParty/vk-bootstrap
      cp -r ${vulkan-memory-allocator} $sourceRoot/Deps/ThirdParty/VulkanMemoryAllocator
    ''}
    rm -rf $sourceRoot/Deps/ThirdParty/asmjit
    rm -rf $sourceRoot/Deps/ThirdParty/cryptopp
    rm -rf $sourceRoot/Deps/ThirdParty/microprofile
    cp -r ${asmjit} $sourceRoot/Deps/ThirdParty/asmjit
    cp -r ${cryptopp} $sourceRoot/Deps/ThirdParty/cryptopp
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
    wrapProgram $out/bin/Xenon \
      --prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ vulkan-loader ]}
  '';
}
