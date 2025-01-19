{
  description = "Flake to dev Minecraft4k";

  inputs = {
    nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.2405.*.tar.gz";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    allSystems = [
      "x86_64-linux"
    ];

    forAllSystems = f:
      nixpkgs.lib.genAttrs allSystems (system:
        f {
          pkgs = import nixpkgs {inherit system;};
        });
  in {
    devShells = forAllSystems ({pkgs}: {
      default = pkgs.mkShell {
        packages = with pkgs; [
          # build tools
          gcc
          elfkickers
          mono
          nasm
        ];

        buildInputs = [
          pkgs.SDL2
          pkgs.libGL
        ];

        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [pkgs.SDL2 pkgs.libGL];
      };
    });
  };
}
