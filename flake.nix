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
    hydraJobs = {
      inherit (self) packages;
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
      toml11 = super.toml11.overrideDerivation (old: {
        version = "4.4.0";
        src = self.fetchFromGitHub {
          owner = "ToruNiina";
          repo = "toml11";
          tag = "v4.4.0";
          hash = "sha256-sgWKYxNT22nw376ttGsTdg0AMzOwp8QH3E8mx0BZJTQ=";
        };
      });
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
    nix.settings = {
      substituters = [
        "https://hydra.fuckk.lol"
        "https://cache.nixos.org/"
      ];
      trusted-public-keys = [
        "hydra.fuckk.lol:6+mPv9GwAFx/9J+mIL0I41pU8k4HX0KiGi1LUHJf7LY="
        "cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY="
      ];
    };
  };
}
