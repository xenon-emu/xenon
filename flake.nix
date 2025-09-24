{
  inputs = {
    utils.url = "github:numtide/flake-utils";
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
      inherit (pkgs) default xenon xenon-cli xenon-static xenon-cli-static;
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
      xenon = self.callPackage ./xenon.nix {};
    in {
      xenon-static = (self.extend staticOverlay).pkgsStatic.xenon;
      xenon-cli-static = (self.extend staticOverlay).pkgsStatic.xenon-cli;
      default = xenon;
      inherit xenon;
      xenon-cli = self.callPackage ./xenon.nix { withGraphics = false; };
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