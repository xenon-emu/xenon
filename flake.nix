{
  inputs = {
    utils.url = "github:numtide/flake-utils";
    # to update: nix flake lock --update-input nixpkgs
    nixpkgs.url = "github:nixos/nixpkgs";
  };
  outputs = { self, utils, nixpkgs }:
  (utils.lib.eachSystem [ "x86_64-linux" "ppc64" "ppc32" ] (system:
  let
    pkgsLut = {
      x86_64-linux = nixpkgs.legacyPackages.${system}.extend self.overlay;
      ppc32 = import nixpkgs {
        crossSystem.config = "powerpc-none-eabi";
        system = "x86_64-linux";
        overlays = [ self.overlay ];
      };
      ppc64 = import nixpkgs {
        crossSystem.config = "powerpc64-unknown-linux-gnuabielfv2";
        system = "x86_64-linux";
        overlays = [ self.overlay ];
        config.allowUnsupportedSystem = true;
      };
    };
    pkgs = pkgsLut.${system};
  in {
    packages = {
      inherit (pkgs) xenon xenon-cli xenon-static xenon-cli-static;
    };
    devShell = pkgs.xenon;
  })) // {
    overlay = self: super:
    let
      staticOverlay = self: super: {
        waylandSupport = false;
        libayatana-appindicator = null;
        libdecorSupport = false;
        libudevSupport = false;
        ibusSupport = false;
        pipewireSupport = false;
        pulseaudioSupport = false;
      };
    in {
      xenon-static = (self.extend staticOverlay).pkgsStatic.xenon;
      xenon-cli-static = (self.extend staticOverlay).pkgsStatic.xenon-cli;
      xenon = self.callPackage ./xenon.nix {};
      xenon-cli = self.callPackage ./xenon.nix { withGraphics = false; };
      sdl3 = super.sdl3.overrideDerivation (old: {
        version = "3.2.6";
        src = self.fetchFromGitHub {
          owner = "libsdl-org";
          repo = "SDL";
          tag = "release-3.2.6";
          hash = "sha256-SEL/JIenmueYayxZlWlMO3lTUOcqiaZZC6RJbbH4DmE=";
        };
      });
    };
  };
}
