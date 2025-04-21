# Compiling Xenon with NixOS (or any Nix environment)
### You have a few options here:
#### You can add xenon into your configuration.nix and just build that way
```nix
{ config, modulesPath, lib, pkgs, ... }:

let
  xenon = builtins.getFlake "github:xenon-emu/xenon";
  xenonPkgs = xenon.packages.${pkgs.hostPlatform.system};
in {
  # ...
  environment.systemPackages = with pkgs; [
    xenonPkgs.xenon
  ];
}
```
#### Or, you can build with nix build in two different ways
```bash
nix build .#xenon
nix build github:xenon-emu/xenon#xenon
```
##### Note, you do not need to clone the repo when using the second method