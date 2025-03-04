{
  inputs = {
    utils.url = "github:numtide/flake-utils";
    # to update: nix flake lock --update-input nixpkgs
    nixpkgs.url = "github:nixos/nixpkgs";
  };
  outputs = { self, utils, nixpkgs }:
  (utils.lib.eachDefaultSystem (system:
  let
    pkgs = nixpkgs.legacyPackages.${system}.extend self.overlay;
  in {
    packages = {
      inherit (pkgs) xenon;
    };
    devShell = pkgs.xenon;
  })) // {
    overlay = self: super: {
      xenon = self.callPackage ./xenon.nix {};
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
