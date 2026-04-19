{
  inputs = {
    utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs";
  };
  outputs = { self, utils, nixpkgs }:
  (utils.lib.eachSystem [ "x86_64-linux" "ppc64" "ppc32" ] (system:
  let
    overlays = [
      self.overlay
      #(self: super: {
      #  asmjit = super.asmjit.overrideAttrs {
      #    version = "0-unstable-2026-02-15";
      #    src = self.fetchFromGitHub {
      #      owner = "asmjit";
      #      repo = "asmjit";
      #      rev = "a3199e8857792cd10b7589ff5d58343d2c9008ea";
      #      hash = "sha256-qb0lM1N1FIvoADNsZZdlg8HAheePv/LvSDvRhOAqZc0=";
      #    };
      #  };
      #})
    ];
    pkgsLut = {
      x86_64-linux  = import nixpkgs {
        system = "x86_64-linux";
        inherit overlays;
      };
      ppc32 = import nixpkgs {
        crossSystem.config = "powerpc-none-eabi";
        system = "x86_64-linux";
        inherit overlays;
      };
      ppc64 = import nixpkgs {
        crossSystem.config = "powerpc64-unknown-linux-gnuabielfv2";
        system = "x86_64-linux";
        inherit overlays;
        config.allowUnsupportedSystem = true;
      };
    };
    pkgs = pkgsLut.${system};
  in {
    packages = {
      inherit (pkgs) default xenon xenon-tools xenon-cli xenon-static xenon-tools-static xenon-cli-static;
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
      xenon-tools-static = (self.extend staticOverlay).pkgsStatic.xenon-tools;
      xenon-cli-static = (self.extend staticOverlay).pkgsStatic.xenon-cli;
      default = xenon;
      inherit xenon;
      xenon-tools = self.callPackage ./xenon-tools.nix {};
      xenon-cli = self.callPackage ./xenon.nix { withGraphics = false; };
    };
    nix.settings = {
      substituters = [
        "https://hydra.flab004.dev"
        "https://cache.nixos.org"
      ];
      trusted-public-keys = [
        "hydra.lab004.dev:6+mPv9GwAFx/9J+mIL0I41pU8k4HX0KiGi1LUHJf7LY="
        "cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY="
      ];
    };
  };
}